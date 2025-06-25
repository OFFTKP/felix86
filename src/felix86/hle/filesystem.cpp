#include <cstring>
#include <mutex>
#include <fcntl.h>
#include <sys/inotify.h>
#include <sys/mman.h>
#include <sys/mount.h>
#include <sys/stat.h>
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

enum class OurSymlink {
    No = 0,
    Proc,
    Run,
    Sys,
    Dev,
    Tmp,
};

OurSymlink isOurSymlinks(int fd, const char* path) {
    static struct statx proc_statx, run_statx, sys_statx, dev_statx, tmp_statx;
    static std::once_flag flag;
    std::call_once(flag, [&]() {
        ASSERT(statx(g_rootfs_fd, "proc", 0, STATX_TYPE | STATX_INO | STATX_MNT_ID, &proc_statx) == 0);
        ASSERT(statx(g_rootfs_fd, "run", 0, STATX_TYPE | STATX_INO | STATX_MNT_ID, &run_statx) == 0);
        ASSERT(statx(g_rootfs_fd, "sys", 0, STATX_TYPE | STATX_INO | STATX_MNT_ID, &sys_statx) == 0);
        ASSERT(statx(g_rootfs_fd, "dev", 0, STATX_TYPE | STATX_INO | STATX_MNT_ID, &dev_statx) == 0);
        ASSERT(statx(g_rootfs_fd, "tmp", 0, STATX_TYPE | STATX_INO | STATX_MNT_ID, &tmp_statx) == 0);
    });

    struct statx new_statx;
    int result = statx(fd, path, AT_EMPTY_PATH, STATX_TYPE | STATX_INO | STATX_MNT_ID, &new_statx);
    if (result == 0) {
        if (statx_inode_same(&proc_statx, &new_statx))
            return OurSymlink::Proc;
        if (statx_inode_same(&run_statx, &new_statx))
            return OurSymlink::Run;
        if (statx_inode_same(&sys_statx, &new_statx))
            return OurSymlink::Sys;
        if (statx_inode_same(&dev_statx, &new_statx))
            return OurSymlink::Dev;
        if (statx_inode_same(&tmp_statx, &new_statx))
            return OurSymlink::Tmp;
    }

    return OurSymlink::No;
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
        ASSERT(statx(AT_FDCWD, emulated_nodes[i].path.c_str(), 0, STATX_TYPE | STATX_INO | STATX_MNT_ID, &emulated_nodes[i].stat) == 0);
    }
}

int Filesystem::OpenAt(int fd, const char* filename, int flags, u64 mode) {
    auto [new_fd, new_filename] = resolve(fd, filename);

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

    return openatInternal(new_fd, new_filename, flags, mode);
}

int Filesystem::FAccessAt(int fd, const char* filename, int mode, int flags) {
    auto [new_fd, new_filename] = resolve(fd, filename);
    return faccessatInternal(new_fd, new_filename, mode, flags);
}

int Filesystem::FStatAt(int fd, const char* filename, struct stat* host_stat, int flags) {
    auto [new_fd, new_filename] = resolve(fd, filename);
    return fstatatInternal(new_fd, new_filename, host_stat, flags);
}

int Filesystem::FStatAt64(int fd, const char* filename, struct stat64* host_stat, int flags) {
    auto [new_fd, new_filename] = resolve(fd, filename);
    return ::fstatat64(new_fd, new_filename, host_stat, flags);
}

int Filesystem::StatFs(const char* filename, struct statfs* buf) {
    if (!filename) {
        WARN("statfs with null filename?");
        return -EINVAL;
    }

    std::filesystem::path path = resolve(filename);
    return statfsInternal(path.c_str(), buf);
}

int Filesystem::ReadlinkAt(int fd, const char* filename, char* buf, int bufsiz) {
    if (isProcSelfExe(filename)) {
        // If it's /proc/self/exe or similar, we don't want to resolve the path then readlink,
        // because readlink will fail as the resolved path would not be a link
        std::string path = resolve(filename);
        const size_t rootfs_size = g_config.rootfs_path.string().size();
        const size_t stem_size = path.size() - rootfs_size;
        ASSERT(path.find(g_config.rootfs_path.string()) == 0); // it should be in rootfs but lets make sure
        int bytes = std::min((int)stem_size, bufsiz);
        memcpy(buf, path.c_str() + rootfs_size, bytes);
        return bytes;
    }

    auto [new_fd, new_filename] = resolve(fd, filename);

    int result = readlinkatInternal(new_fd, new_filename, buf, bufsiz);

    if (result > 0) {
        std::string path(buf, result);
        removeRootfsPrefix(path);
        strncpy(buf, path.data(), path.size());
        return path.size();
    }

    return result;
}

int Filesystem::Getcwd(char* buf, size_t size) {
    int result = syscall(SYS_getcwd, buf, size);

    if (result > 0) {
        std::string path(buf, result);
        removeRootfsPrefix(path);
        strncpy(buf, path.data(), path.size());
        return path.size();
    }

    return result;
}

int Filesystem::Rename(const char* oldname, const char* newname) {
    if (!oldname || !newname) {
        return -EINVAL;
    }

    std::filesystem::path oldpath = resolve(oldname);
    std::filesystem::path newpath = resolve(newname);
    int result = ::rename(oldpath.c_str(), newpath.c_str());
    if (result == -1) {
        result = -errno;
    }
    return result;
}

int Filesystem::SymlinkAt(const char* oldname, int newfd, const char* newname) {
    if (!oldname || !newname) {
        return -EINVAL;
    }

    std::filesystem::path oldpath = resolve(oldname);
    auto [newfd2, newpath] = resolve(newfd, newname);
    int result = ::symlinkat(oldpath.c_str(), newfd2, newpath);
    if (result == -1) {
        result = -errno;
    }
    return result;
}

int Filesystem::RenameAt2(int oldfd, const char* oldname, int newfd, const char* newname, int flags) {
    if (!oldname || !newname) {
        return -EINVAL;
    }

    auto [oldfd2, oldpath] = resolve(oldfd, oldname);
    auto [newfd2, newpath] = resolve(newfd, newname);
    int result = ::renameat2(oldfd2, oldpath, newfd2, newpath, flags);
    if (result == -1) {
        result = -errno;
    }
    return result;
}

int Filesystem::Chmod(const char* filename, u64 mode) {
    if (!filename) {
        return -EINVAL;
    }

    std::filesystem::path path = resolve(filename);
    int result = ::chmod(path.c_str(), mode);
    if (result == -1) {
        result = -errno;
    }
    return result;
}

int Filesystem::Creat(const char* filename, u64 mode) {
    std::filesystem::path path = resolve(filename);
    return ::creat(path.c_str(), mode);
}

int Filesystem::Statx(int fd, const char* filename, int flags, u32 mask, struct statx* statxbuf) {
    if (!filename) {
        return -EINVAL;
    }

    auto [new_fd, new_filename] = resolve(fd, filename);
    return statxInternal(new_fd, new_filename, flags, mask, statxbuf);
}

int Filesystem::UnlinkAt(int fd, const char* filename, int flags) {
    if (!filename) {
        WARN("unlink with null filename?");
        return -EINVAL;
    }

    auto [new_fd, new_filename] = resolve(fd, filename);
    return unlinkatInternal(new_fd, new_filename, flags);
}

int Filesystem::LinkAt(int oldfd, const char* oldpath, int newfd, const char* newpath, int flags) {
    auto [roldfd, roldpath] = resolve(oldfd, oldpath);
    auto [rnewfd, rnewpath] = resolve(newfd, newpath);

    return linkatInternal(roldfd, roldpath, rnewfd, rnewpath, flags);
}

int Filesystem::Chown(const char* filename, u64 owner, u64 group) {
    std::filesystem::path path = resolve(filename);
    int result = ::chown(path.c_str(), owner, group);
    if (result == -1) {
        result = -errno;
    }
    return result;
}

int Filesystem::LChown(const char* filename, u64 owner, u64 group) {
    std::filesystem::path path = resolve(filename);
    int result = ::lchown(path.c_str(), owner, group);
    if (result == -1) {
        result = -errno;
    }
    return result;
}

int Filesystem::Chdir(const char* filename) {
    std::filesystem::path path = resolve(filename);
    int result = ::chdir(path.c_str());
    if (result == -1) {
        result = -errno;
    }
    return result;
}

int Filesystem::MkdirAt(int fd, const char* filename, u64 mode) {
    auto [new_fd, new_path] = resolve(fd, filename);
    int result = ::mkdirat(new_fd, new_path, mode);
    if (result == -1) {
        result = -errno;
    }
    return result;
}

int Filesystem::MknodAt(int fd, const char* filename, u64 mode, u64 dev) {
    auto [new_fd, new_path] = resolve(fd, filename);
    int result = ::mknodat(new_fd, new_path, mode, dev);
    if (result == -1) {
        result = -errno;
    }
    return result;
}

int Filesystem::FChmodAt(int fd, const char* filename, u64 mode) {
    auto [new_fd, new_filename] = resolve(fd, filename);
    return fchmodatInternal(new_fd, new_filename, mode);
}

int Filesystem::LGetXAttr(const char* filename, const char* name, void* value, size_t size) {
    std::filesystem::path path = resolve(filename);
    return lgetxattrInternal(path.c_str(), name, value, size);
}

ssize_t Filesystem::Listxattr(const char* filename, char* list, size_t size, bool llist) {
    std::filesystem::path path = resolve(filename);
    if (llist) {
        return ::listxattr(path.c_str(), list, size);
    } else {
        return ::llistxattr(path.c_str(), list, size);
    }
}

int Filesystem::GetXAttr(const char* filename, const char* name, void* value, size_t size) {
    std::filesystem::path path = resolve(filename);
    return getxattrInternal(path.c_str(), name, value, size);
}

int Filesystem::LSetXAttr(const char* filename, const char* name, void* value, size_t size, int flags) {
    std::filesystem::path path = resolve(filename);
    return lsetxattrInternal(path.c_str(), name, value, size, flags);
}

int Filesystem::SetXAttr(const char* filename, const char* name, void* value, size_t size, int flags) {
    std::filesystem::path path = resolve(filename);
    return setxattrInternal(path.c_str(), name, value, size, flags);
}

int Filesystem::RemoveXAttr(const char* filename, const char* name) {
    std::filesystem::path path = resolve(filename);
    return removexattrInternal(path.c_str(), name);
}

int Filesystem::LRemoveXAttr(const char* filename, const char* name) {
    std::filesystem::path path = resolve(filename);
    return lremovexattrInternal(path.c_str(), name);
}

int Filesystem::UtimensAt(int fd, const char* filename, struct timespec* spec, int flags) {
    auto [new_fd, new_filename] = resolve(fd, filename);
    return utimensatInternal(new_fd, new_filename, spec, flags);
}

int Filesystem::Rmdir(const char* dir) {
    std::filesystem::path path = resolve(dir);
    return rmdirInternal(path.c_str());
}

int Filesystem::Mount(const char* source, const char* target, const char* fstype, u64 flags, const void* data) {
    const char* sptr = nullptr;
    const char* tptr = nullptr;
    std::filesystem::path rsource, rtarget;
    if (source) {
        rsource = resolve(source);
        sptr = rsource.c_str();
    }
    if (target) {
        rtarget = resolve(target);
        tptr = rtarget.c_str();
    }
    return ::mount(sptr, tptr, fstype, flags, data);
}

int Filesystem::INotifyAddWatch(int fd, const char* path, u32 mask) {
    std::filesystem::path file = resolve(path);
    return inotify_add_watch(fd, file.c_str(), mask);
}

int Filesystem::Truncate(const char* path, u64 length) {
    std::filesystem::path file = resolve(path);
    return truncate(file.c_str(), length);
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

std::pair<int, const char*> Filesystem::resolveInner(int fd, const char* path) {
    if (path == nullptr) {
        return {fd, nullptr};
    }

    if (isProcSelfExe(path)) {
        return {AT_FDCWD, g_fs->GetExecutablePath().c_str()};
    }

    if (path[0] == '/') {
        if (path[1] == '\0') {
            return {g_rootfs_fd, "."};
        }

        return {g_rootfs_fd, &path[1]}; // return rootfs fd, skip the '/'
    } else {
        if (std::string(path) == "..") {
            static struct statx rootfs_statx;
            static std::once_flag flag;
            std::call_once(flag, [&]() { ASSERT(statx(g_rootfs_fd, "", AT_EMPTY_PATH, STATX_TYPE | STATX_INO | STATX_MNT_ID, &rootfs_statx) == 0); });

            bool is_same = false;
            struct statx new_fd_statx;
            if (statx(fd, "", AT_EMPTY_PATH, STATX_TYPE | STATX_INO | STATX_MNT_ID, &new_fd_statx) == 0) {
                is_same = statx_inode_same(&rootfs_statx, &new_fd_statx);
            }

            if (is_same) {
                // HACK: some programs like `systemd-tmpfiles --create` do some sort of root checking
                // via `fd = open("/")` and `fd2 = openat(fd, "..")` and comparing if the two fd's have same inode ids
                // among other things. We don't want this to happen, but a better solution might be possible.
                return {fd, "."};
            }
        }

        return {fd, path};
    }
}

std::pair<int, const char*> Filesystem::resolve(int fd, const char* path) {
    auto [new_fd, new_filename] = resolveInner(fd, path);

    // For the emulator to work we symlink some stuff like /proc to the rootfs, if we allow readlink to
    // return `/proc` when `readlink(/proc)` happens we'd get infinite recursion and stuff would not behave
    // For example the `realpath` function will cause problems if used on /proc
    // This was also noticed on systemd-tmpfiles which would do stuff like readlinkat(fd, proc) over and over
    // For our symlinks we are going to resolve them internally so a `stat` or `open` on them will open the original
    OurSymlink symlink = isOurSymlinks(new_fd, new_filename);
    if (symlink != OurSymlink::No) {
        switch (symlink) {
        case OurSymlink::Proc: {
            return {AT_FDCWD, "/proc"};
        }
        case OurSymlink::Run: {
            return {AT_FDCWD, "/run"};
        }
        case OurSymlink::Sys: {
            return {AT_FDCWD, "/sys"};
        }
        case OurSymlink::Dev: {
            return {AT_FDCWD, "/dev"};
        }
        case OurSymlink::Tmp: {
            return {AT_FDCWD, "/tmp"};
        }
        default: {
            UNREACHABLE();
        }
        }
    }

    return {new_fd, new_filename};
}

std::filesystem::path Filesystem::resolve(const char* path) {
    if (!path) {
        WARN("Filesystem::resolve with null path");
        return "";
    }

    if (isProcSelfExe(path)) {
        return g_fs->GetExecutablePath();
    }

    std::filesystem::path resolved_path;
    if (path[0] == '/') {
        if (path[1] == '\0') {
            resolved_path = g_config.rootfs_path;
        } else {
            resolved_path = g_config.rootfs_path / &path[1];
        }
    } else {
        resolved_path = path;
    }

    OurSymlink symlink = isOurSymlinks(AT_FDCWD, resolved_path.c_str());
    if (symlink != OurSymlink::No) {
        // TODO: reduce code duplication here and in other resolve
        switch (symlink) {
        case OurSymlink::Proc: {
            return "/proc";
        }
        case OurSymlink::Run: {
            return "/run";
        }
        case OurSymlink::Sys: {
            return "/sys";
        }
        case OurSymlink::Dev: {
            return "/dev";
        }
        case OurSymlink::Tmp: {
            return "/tmp";
        }
        default: {
            UNREACHABLE();
        }
        }
    }

    return resolved_path;
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
    std::string spath = path;
    std::string pidpath = "/proc/" + std::to_string(getpid()) + "/exe";
    if (spath == "/proc/self/exe" || spath == "/proc/thread-self/exe" || spath == pidpath) {
        return true;
    }
    return false;
}
