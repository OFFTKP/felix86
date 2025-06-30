#include <cstring>
#include <fcntl.h>
#include <linux/openat2.h>
#include <sys/inotify.h>
#include <sys/mman.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/statfs.h>
#include <sys/xattr.h>
#include "felix86/common/overlay.hpp"
#include "felix86/common/types.hpp"
#include "felix86/common/utility.hpp"
#include "felix86/hle/filesystem.hpp"

#define FLAGS_SET(v, flags) ((~(v) & (flags)) == 0)

bool statx_inode_same(const struct statx* a, const struct statx* b) {
    return (a && a->stx_mask != 0) && (b && b->stx_mask != 0) && FLAGS_SET(a->stx_mask, STATX_TYPE | STATX_INO) &&
           FLAGS_SET(b->stx_mask, STATX_TYPE | STATX_INO) && ((a->stx_mode ^ b->stx_mode) & S_IFMT) == 0 && a->stx_dev_major == b->stx_dev_major &&
           a->stx_dev_minor == b->stx_dev_minor && a->stx_ino == b->stx_ino;
}

int generate_memfd(const char* path, int flags) {
    if (flags & O_CLOEXEC) {
        return memfd_create(path, MFD_ALLOW_SEALING | MFD_CLOEXEC);
    } else {
        return memfd_create(path, MFD_ALLOW_SEALING);
    }
}

void seal_memfd(int fd) {
    ASSERT(fcntl(fd, F_ADD_SEALS, F_SEAL_SEAL | F_SEAL_SHRINK | F_SEAL_GROW | F_SEAL_WRITE | F_SEAL_FUTURE_WRITE) == 0);
}

void Filesystem::initializeEmulatedNodes() {
    // clang-format off
    emulated_nodes[PROC_CPUINFO] = EmulatedNode {
        .path = "/proc/cpuinfo",
        .open_func = [](const char* path, int flags) {
            const std::string& cpuinfo = felix86_cpuinfo();
            int fd = generate_memfd("/proc/cpuinfo", flags);
            ASSERT(write(fd, cpuinfo.data(), cpuinfo.size()) == cpuinfo.size());
            lseek(fd, 0, SEEK_SET);
            seal_memfd(fd);
            return fd;
        },
    };

    emulated_nodes[PROC_SELF_MAPS] = EmulatedNode{
        .path = "/proc/self/maps",
        .open_func = [](const char* path, int flags) {
            std::string maps = felix86_maps();
            int fd = generate_memfd("/proc/self/maps", flags);
            ASSERT(write(fd, maps.data(), maps.size()) == maps.size());
            lseek(fd, 0, SEEK_SET);
            seal_memfd(fd);
            return fd;
        },
    };
    // clang-format on

    // Populate the stat field in each node
    for (int i = 0; i < EMULATED_NODE_COUNT; i++) {
        std::filesystem::path node_path = g_config.rootfs_path / emulated_nodes[i].path.relative_path();
        if (std::filesystem::exists(node_path)) { // if we are chrooted with no access to /proc then tough luck
            ASSERT(statx(AT_FDCWD, node_path.c_str(), 0, STATX_TYPE | STATX_INO | STATX_MNT_ID, &emulated_nodes[i].stat) == 0);
        }
    }
}

int Filesystem::OpenAt(int fd, const char* filename, int flags, u64 mode) {
    bool follow = !(flags & O_NOFOLLOW);
    auto [new_fd, new_filename] = resolve(fd, filename, follow);

    if (!g_mode32) {
        if (fd == AT_FDCWD && filename && filename[0] == '/') {
            // TODO: use our emulated node stuff instead of this
            // We may be opening a library, check if it's one of our overlays
            const char* overlay = Overlays::isOverlay(filename);
            if (overlay) {
                // Open the overlayed path instead of filename
                return openatInternal(AT_FDCWD, overlay, flags, mode);
            }
        }
    }

    return openatInternal(new_fd, new_filename.get_str(), flags, mode);
}

int Filesystem::FAccessAt(int fd, const char* filename, int mode, int flags) {
    bool follow = !(flags & AT_SYMLINK_NOFOLLOW);
    auto [new_fd, new_filename] = resolve(fd, filename, follow);
    return faccessatInternal(new_fd, new_filename.get_str(), mode, flags);
}

int Filesystem::FStatAt(int fd, const char* filename, struct stat* host_stat, int flags) {
    bool follow = !(flags & AT_SYMLINK_NOFOLLOW);
    auto [new_fd, new_filename] = resolve(fd, filename, follow);
    return fstatatInternal(new_fd, new_filename.get_str(), host_stat, flags);
}

int Filesystem::FStatAt64(int fd, const char* filename, struct stat64* host_stat, int flags) {
    bool follow = !(flags & AT_SYMLINK_NOFOLLOW);
    auto [new_fd, new_filename] = resolve(fd, filename, follow);
    return ::fstatat64(new_fd, new_filename.get_str(), host_stat, flags);
}

int Filesystem::StatFs(const char* filename, struct statfs* buf) {
    if (!filename) {
        WARN("statfs with null filename?");
        return -EINVAL;
    }

#ifndef ST_NOSYMFOLLOW
#define ST_NOSYMFOLLOW 0x2000
#endif
    bool follow = !(buf->f_flags & ST_NOSYMFOLLOW);
    NullablePath path = resolve(filename, follow);
    return statfsInternal(path.get_str(), buf);
}

int Filesystem::ReadlinkAt(int fd, const char* filename, char* buf, int bufsiz) {
    if (isProcSelfExe(filename)) {
        // If it's /proc/self/exe or similar, we don't want to resolve the path then readlink,
        // because readlink will fail as the resolved path would not be a link
        NullablePath npath = resolve(filename, false);
        ASSERT(npath.get_str());
        std::string path = npath.get_str();
        const size_t rootfs_size = g_config.rootfs_path.string().size();
        const size_t stem_size = path.size() - rootfs_size;
        // TODO!!!: g_fs->ExecutablePath() gets the path that's not inside the *mounted* rootfs but the original rootfs
        // Example: run felix86 /rootfs/usr/bin/readlink /proc/self/exe (bug doesn't happen inside emulated bash for obvious reasons)
        ASSERT_MSG(path.find(g_config.rootfs_path.string()) == 0, "Path: %s", path.c_str()); // it should be in rootfs but lets make sure
        int bytes = std::min((int)stem_size, bufsiz);
        memcpy(buf, path.c_str() + rootfs_size, bytes);
        return bytes;
    }

    auto [new_fd, new_filename] = resolve(fd, filename, false);

    int result = readlinkatInternal(new_fd, new_filename.get_str(), buf, bufsiz);

    if (result > 0) {
        std::string str(buf, result);
        removeRootfsPrefix(str);
        strncpy(buf, str.c_str(), result);
        return str.size();
    }

    return result;
}

int Filesystem::Getcwd(char* buf, size_t size) {
    int result = syscall(SYS_getcwd, buf, size);

    if (result > 0) {
        std::string str = buf;
        removeRootfsPrefix(str);
        strncpy(buf, str.c_str(), size);
        return strlen(buf);
    }

    return result;
}

int Filesystem::SymlinkAt(const char* oldname, int newfd, const char* newname) {
    if (!oldname || !newname) {
        return -EINVAL;
    }

    auto [newfd2, newpath] = resolve(newfd, newname, false);
    int result = ::symlinkat(oldname, newfd2, newpath.get_str());
    if (result == -1) {
        result = -errno;
    }
    return result;
}

int Filesystem::RenameAt2(int oldfd, const char* oldname, int newfd, const char* newname, int flags) {
    if (!oldname || !newname) {
        return -EINVAL;
    }

    auto [oldfd2, oldpath] = resolve(oldfd, oldname, false);
    auto [newfd2, newpath] = resolve(newfd, newname, false);
    int result = ::renameat2(oldfd2, oldpath.get_str(), newfd2, newpath.get_str(), flags);
    if (result == -1) {
        result = -errno;
    }
    return result;
}

int Filesystem::Chmod(const char* filename, u64 mode) {
    if (!filename) {
        return -EINVAL;
    }

    NullablePath path = resolve(filename, true);
    int result = ::chmod(path.get_str(), mode);
    if (result == -1) {
        result = -errno;
    }
    return result;
}

int Filesystem::Creat(const char* filename, u64 mode) {
    NullablePath path = resolve(filename, false);
    return ::creat(path.get_str(), mode);
}

int Filesystem::Statx(int fd, const char* filename, int flags, u32 mask, struct statx* statxbuf) {
    bool follow = !(flags & AT_SYMLINK_NOFOLLOW);
    auto [new_fd, new_filename] = resolve(fd, filename, follow);
    return statxInternal(new_fd, new_filename.get_str(), flags, mask, statxbuf);
}

int Filesystem::UnlinkAt(int fd, const char* filename, int flags) {
    if (!filename) {
        WARN("unlink with null filename?");
        return -EINVAL;
    }

    auto [new_fd, new_filename] = resolve(fd, filename, false);
    return unlinkatInternal(new_fd, new_filename.get_str(), flags);
}

int Filesystem::LinkAt(int oldfd, const char* oldpath, int newfd, const char* newpath, int flags) {
    bool follow = flags & AT_SYMLINK_FOLLOW;
    auto [roldfd, roldpath] = resolve(oldfd, oldpath, follow);
    auto [rnewfd, rnewpath] = resolve(newfd, newpath, follow);

    return linkatInternal(roldfd, roldpath.get_str(), rnewfd, rnewpath.get_str(), flags);
}

int Filesystem::Chown(const char* filename, u64 owner, u64 group) {
    NullablePath path = resolve(filename, true);
    int result = ::chown(path.get_str(), owner, group);
    if (result == -1) {
        result = -errno;
    }
    return result;
}

int Filesystem::LChown(const char* filename, u64 owner, u64 group) {
    NullablePath path = resolve(filename, false);
    int result = ::lchown(path.get_str(), owner, group);
    if (result == -1) {
        result = -errno;
    }
    return result;
}

int Filesystem::Chdir(const char* filename) {
    NullablePath path = resolve(filename, true);
    int result = ::syscall(SYS_chdir, path.get_str());
    if (result == -1) {
        result = -errno;
    }
    return result;
}

int Filesystem::MkdirAt(int fd, const char* filename, u64 mode) {
    auto [new_fd, new_path] = resolve(fd, filename, true);
    int result = ::mkdirat(new_fd, new_path.get_str(), mode);
    if (result == -1) {
        result = -errno;
    }
    return result;
}

int Filesystem::MknodAt(int fd, const char* filename, u64 mode, u64 dev) {
    auto [new_fd, new_path] = resolve(fd, filename, true);
    int result = ::mknodat(new_fd, new_path.get_str(), mode, dev);
    if (result == -1) {
        result = -errno;
    }
    return result;
}

int Filesystem::FChmodAt(int fd, const char* filename, u64 mode) {
    auto [new_fd, new_filename] = resolve(fd, filename, true);
    return fchmodatInternal(new_fd, new_filename.get_str(), mode);
}

int Filesystem::LGetXAttr(const char* filename, const char* name, void* value, size_t size) {
    NullablePath path = resolve(filename, false);
    return lgetxattrInternal(path.get_str(), name, value, size);
}

ssize_t Filesystem::Listxattr(const char* filename, char* list, size_t size, bool llist) {
    if (!llist) {
        NullablePath path = resolve(filename, true);
        return ::listxattr(path.get_str(), list, size);
    } else {
        NullablePath path = resolve(filename, false);
        return ::llistxattr(path.get_str(), list, size);
    }
}

int Filesystem::GetXAttr(const char* filename, const char* name, void* value, size_t size) {
    NullablePath path = resolve(filename, true);
    return getxattrInternal(path.get_str(), name, value, size);
}

int Filesystem::LSetXAttr(const char* filename, const char* name, void* value, size_t size, int flags) {
    NullablePath path = resolve(filename, false);
    return lsetxattrInternal(path.get_str(), name, value, size, flags);
}

int Filesystem::SetXAttr(const char* filename, const char* name, void* value, size_t size, int flags) {
    NullablePath path = resolve(filename, true);
    return setxattrInternal(path.get_str(), name, value, size, flags);
}

int Filesystem::RemoveXAttr(const char* filename, const char* name) {
    NullablePath path = resolve(filename, true);
    return removexattrInternal(path.get_str(), name);
}

int Filesystem::LRemoveXAttr(const char* filename, const char* name) {
    NullablePath path = resolve(filename, false);
    return lremovexattrInternal(path.get_str(), name);
}

int Filesystem::UtimensAt(int fd, const char* filename, struct timespec* spec, int flags) {
    auto [new_fd, new_filename] = resolve(fd, filename, true);
    return utimensatInternal(new_fd, new_filename.get_str(), spec, flags);
}

int Filesystem::Rmdir(const char* dir) {
    NullablePath path = resolve(dir, true);
    return rmdirInternal(path.get_str());
}

int Filesystem::Chroot(const char* path) {
    // First, do a no-op chroot to check if we have permissions at all
    int result = ::chroot("/");
    if (result != 0) {
        return -errno;
    }

    if (!path) {
        return -EINVAL;
    }

    NullablePath target = resolve(path, true);
    g_config.rootfs_path = target.get_str();
    close(g_rootfs_fd);
    g_rootfs_fd = open(target.get_str(), O_DIRECTORY);
    return 0;
}

int Filesystem::Mount(const char* source, const char* target, const char* fstype, u64 flags, const void* data) {
    const char* sptr = nullptr;
    const char* tptr = nullptr;

    bool follow = !(flags & MS_NOSYMFOLLOW);

    NullablePath rsource, rtarget;
    if (source) {
        rsource = resolve(source, follow);
        sptr = rsource.get_str();
    }
    if (target) {
        rtarget = resolve(target, follow);
        tptr = rtarget.get_str();
    }
    return ::mount(sptr, tptr, fstype, flags, data);
}

int Filesystem::Umount(const char* path, int flags) {
    bool follow = !(flags & UMOUNT_NOFOLLOW);
    NullablePath target = resolve(path, follow);
    return ::umount2(target.get_str(), flags);
}

int Filesystem::INotifyAddWatch(int fd, const char* path, u32 mask) {
    NullablePath file = resolve(path, true);
    return inotify_add_watch(fd, file.get_str(), mask);
}

int Filesystem::Truncate(const char* path, u64 length) {
    NullablePath file = resolve(path, true);
    return truncate(file.get_str(), length);
}

int Filesystem::openatInternal(int fd, const char* filename, int flags, u64 mode) {
    int opened_fd = ::syscall(SYS_openat, fd, filename, flags, mode);
    if (opened_fd != -1) {
        struct statx stat;
        ASSERT(statx(opened_fd, "", AT_EMPTY_PATH, STATX_TYPE | STATX_INO | STATX_MNT_ID, &stat) == 0);
        for (int i = 0; i < EMULATED_NODE_COUNT; i++) {
            EmulatedNode& node = emulated_nodes[i];
            if (statx_inode_same(&stat, &node.stat)) {
                // This is one of our emulated files, close the opened fd and replace it with our own
                close(opened_fd);
                int new_fd = node.open_func(filename, flags);
                ASSERT(new_fd > 0);
                return new_fd;
            }
        }
    }
    return opened_fd;
}

int Filesystem::faccessatInternal(int fd, const char* filename, int mode, int flags) {
    return ::syscall(SYS_faccessat2, fd, filename, mode, flags);
}

int Filesystem::fstatatInternal(int fd, const char* filename, struct stat* host_stat, int flags) {
    return ::syscall(SYS_newfstatat, fd, filename, host_stat, flags);
}

int Filesystem::statfsInternal(const std::filesystem::path& path, struct statfs* buf) {
    return ::syscall(SYS_statfs, path.c_str(), buf);
}

int Filesystem::readlinkatInternal(int fd, const char* filename, char* buf, int bufsiz) {
    return ::syscall(SYS_readlinkat, fd, filename, buf, bufsiz);
}

int Filesystem::statxInternal(int fd, const char* filename, int flags, u32 mask, struct statx* statxbuf) {
    return ::syscall(SYS_statx, fd, filename, flags, mask, statxbuf);
}

int Filesystem::linkatInternal(int oldfd, const char* oldpath, int newfd, const char* newpath, int flags) {
    return ::syscall(SYS_linkat, oldfd, oldpath, newfd, newpath, flags);
}

int Filesystem::unlinkatInternal(int fd, const char* filename, int flags) {
    return ::syscall(SYS_unlinkat, fd, filename, flags);
}

int Filesystem::getxattrInternal(const char* filename, const char* name, void* value, size_t size) {
    return ::syscall(SYS_getxattr, filename, name, value, size);
}

int Filesystem::lgetxattrInternal(const char* filename, const char* name, void* value, size_t size) {
    return ::syscall(SYS_lgetxattr, filename, name, value, size);
}

int Filesystem::setxattrInternal(const char* filename, const char* name, void* value, size_t size, int flags) {
    return ::syscall(SYS_setxattr, filename, name, value, size, flags);
}

int Filesystem::lsetxattrInternal(const char* filename, const char* name, void* value, size_t size, int flags) {
    return ::syscall(SYS_lsetxattr, filename, name, value, size, flags);
}

int Filesystem::removexattrInternal(const char* filename, const char* name) {
    return ::syscall(SYS_removexattr, filename, name);
}

int Filesystem::lremovexattrInternal(const char* filename, const char* name) {
    return ::syscall(SYS_lremovexattr, filename, name);
}

int Filesystem::utimensatInternal(int fd, const char* filename, struct timespec* spec, int flags) {
    return ::syscall(SYS_utimensat, fd, filename, spec, flags);
}

int Filesystem::fchmodatInternal(int fd, const char* filename, u64 mode) {
    return ::syscall(SYS_fchmodat, fd, filename, mode);
}

int Filesystem::rmdirInternal(const char* path) {
    return ::rmdir(path);
}

std::pair<int, NullablePath> Filesystem::resolve(int fd, const char* path, bool resolve_symlinks) {
    auto [new_fd, new_path] = resolveImpl(fd, path, resolve_symlinks);
    return {new_fd, new_path};
}

NullablePath Filesystem::resolve(const char* path, bool resolve_symlinks) {
    if (!path) {
        return nullptr;
    }

    if (path[0] == '/') {
        NullablePath npath = resolve(AT_FDCWD, path, resolve_symlinks).second;
        ASSERT(npath.get_str());
        ASSERT_MSG(npath.get_str()[0] == '/', "Bad path: %s -> %s", path, npath.get_str());
        return npath.get_str();
    } else {
        return path;
    }
}

void Filesystem::removeRootfsPrefix(std::string& path) {
    // Check if the path starts with rootfs (ie. when readlinking /proc stuff) and remove it
    std::string rootfs = g_config.rootfs_path.lexically_normal().string();

    if (path.find(rootfs) == 0) {
        if (path == g_config.rootfs_path) {
            // Special case, it is the rootfs path
            path = "/";
        } else {
            std::string sub = path.substr(rootfs.size());
            path = sub;
        }

        ASSERT(!path.empty());
        if (path[0] != '/') {
            path = '/' + path;
        }
    }
}

bool Filesystem::isProcSelfExe(const char* path) {
    if (!path) {
        return false;
    }

    std::string spath = path;
    std::string pidpath = "/proc/" + std::to_string(getpid()) + "/exe";
    if (spath == "/proc/self/exe" || spath == "/proc/thread-self/exe" || spath == pidpath) {
        return true;
    }
    return false;
}

std::pair<int, NullablePath> Filesystem::resolveImpl(int fd, const char* path, bool resolve_symlinks) {
    if (path == nullptr) {
        return {fd, nullptr};
    }

    if (path[0] == 0) {
        return {fd, path};
    }

    if (path[0] == '/' && path[1] == 0) {
        return {AT_FDCWD, g_config.rootfs_path};
    }

    if (isProcSelfExe(path)) {
        return {AT_FDCWD, g_executable_path_absolute};
    }

    // Convert the fd + path combo to an absolute path;
    std::filesystem::path resolve_me;
    if (path[0] == '/') {
        resolve_me = path;
    } else {
        char buffer[PATH_MAX];
        if (fd == AT_FDCWD) {
            char* cwd = getcwd(buffer, PATH_MAX);
            std::string file = std::filesystem::path(cwd) / path;
            removeRootfsPrefix(file);
            resolve_me = file;
        } else {
            std::string self_fd = "/proc/self/fd/" + std::to_string(fd);
            ssize_t size = readlink(self_fd.c_str(), buffer, PATH_MAX);
            if (size < 0) {
                WARN("Failed to read path for fd: %d and pathname %s", fd, path);
                return {fd, path};
            }
            buffer[size] = 0;
            std::string file = std::filesystem::path(buffer) / path;
            removeRootfsPrefix(file);
            resolve_me = file;
        }
    }

    ASSERT(resolve_me.is_absolute());

    if (resolve_symlinks) {
        // If we want to resolve symlinks anyway, then just resolve the entire thing in openat2
        struct open_how open_how;
        open_how.flags = O_PATH;
        open_how.resolve = RESOLVE_IN_ROOT | RESOLVE_NO_MAGICLINKS;
        open_how.mode = 0;
        int path_fd = syscall(SYS_openat2, g_rootfs_fd, resolve_me.c_str(), &open_how, sizeof(struct open_how));
        if (path_fd > 0) {
            char buffer[PATH_MAX];
            std::string self_fd = "/proc/self/fd/" + std::to_string(path_fd);
            ssize_t size = readlink(self_fd.c_str(), buffer, PATH_MAX - 1);
            ASSERT(size > 0);
            buffer[size] = 0;
            close(path_fd);
            return {AT_FDCWD, std::filesystem::path{buffer}};
        } else {
            return {AT_FDCWD, g_config.rootfs_path / resolve_me.relative_path()};
        }
    } else {
        // If we don't want to resolve symlinks on the last component, resolve just the basepath then add the final component
        const std::filesystem::path final_component = resolve_me.filename();
        const std::filesystem::path base_path = resolve_me.parent_path();
        struct open_how open_how;
        open_how.flags = O_PATH;
        open_how.resolve = RESOLVE_IN_ROOT | RESOLVE_NO_MAGICLINKS;
        open_how.mode = 0;
        int path_fd = syscall(SYS_openat2, g_rootfs_fd, base_path.c_str(), &open_how, sizeof(struct open_how));
        if (path_fd > 0) {
            char buffer[PATH_MAX];
            std::string self_fd = "/proc/self/fd/" + std::to_string(path_fd);
            ssize_t size = readlink(self_fd.c_str(), buffer, PATH_MAX - 1);
            ASSERT(size > 0);
            buffer[size] = 0;
            close(path_fd);

            std::filesystem::path final = buffer;
            final /= final_component;
            return {AT_FDCWD, final};
        } else {
            return {AT_FDCWD, g_config.rootfs_path / resolve_me.relative_path()};
        }
    }
}
