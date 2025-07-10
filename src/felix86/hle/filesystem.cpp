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
#include "felix86/hle/fd.hpp"
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
    FdPath fd_path = resolve(fd, filename, follow);
    if (fd_path.is_error()) {
        VERBOSE("Error while resolving path during openat(%d, %s), error: %s", fd, filename, strerror(fd_path.get_errno()));
        return -fd_path.get_errno();
    }

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

    return openatInternal(fd_path.fd(), fd_path.path(), flags, mode);
}

int Filesystem::FAccessAt(int fd, const char* filename, int mode, int flags) {
    bool follow = !(flags & AT_SYMLINK_NOFOLLOW);
    FdPath fd_path = resolve(fd, filename, follow);
    if (fd_path.is_error()) {
        VERBOSE("Error while resolving path during faccessat(%d, %s), error: %s", fd, filename, strerror(fd_path.get_errno()));
        return -fd_path.get_errno();
    }
    return faccessatInternal(fd_path.fd(), fd_path.path(), mode, flags);
}

int Filesystem::FStatAt(int fd, const char* filename, struct stat* host_stat, int flags) {
    bool follow = !(flags & AT_SYMLINK_NOFOLLOW);
    FdPath fd_path = resolve(fd, filename, follow);
    if (fd_path.is_error()) {
        VERBOSE("Error while resolving path during fstatat(%d, %s. follow: %d), error: %s", fd, filename, follow, strerror(fd_path.get_errno()));
        return -fd_path.get_errno();
    }
    return fstatatInternal(fd_path.fd(), fd_path.path(), host_stat, flags);
}

int Filesystem::FStatAt64(int fd, const char* filename, struct stat64* host_stat, int flags) {
    bool follow = !(flags & AT_SYMLINK_NOFOLLOW);
    FdPath fd_path = resolve(fd, filename, follow);
    if (fd_path.is_error()) {
        VERBOSE("Error while resolving path during fstatat64(%d, %s), error: %s", fd, filename, strerror(fd_path.get_errno()));
        return -fd_path.get_errno();
    }
    return ::fstatat64(fd_path.fd(), fd_path.path(), host_stat, flags);
}

int Filesystem::StatFs(const char* filename, struct statfs* buf) {
    if (!filename) {
        WARN("statfs with null filename?");
        return -EINVAL;
    }

    FdPath fd_path = resolve(filename, false);
    if (fd_path.is_error()) {
        VERBOSE("Error while resolving path during statfs(%s), error: %s", filename, strerror(fd_path.get_errno()));
        return -fd_path.get_errno();
    }
    return statfsInternal(fd_path.full_path(), buf);
}

int Filesystem::ReadlinkAt(int fd, const char* filename, char* buf, int bufsiz) {
    if (isProcSelfExe(filename)) {
        // If it's /proc/self/exe or similar, we don't want to resolve the path then readlink,
        // because readlink will fail as the resolved path would not be a link
        FdPath npath = resolve(filename, false);
        ASSERT(!npath.is_error());
        ASSERT(npath.full_path());
        std::string path = npath.full_path();
        const size_t rootfs_size = g_config.rootfs_path.string().size();
        const size_t stem_size = path.size() - rootfs_size;
        ASSERT_MSG(path.find(g_config.rootfs_path.string()) == 0, "Path: %s", path.c_str()); // it should be in rootfs but lets make sure
        int bytes = std::min((int)stem_size, bufsiz);
        memcpy(buf, path.c_str() + rootfs_size, bytes);
        return bytes;
    }

    FdPath fd_path = resolve(fd, filename, false);
    if (fd_path.is_error()) {
        VERBOSE("Error while resolving path during readlinkat(%d, %s), error: %s", fd, filename, strerror(fd_path.get_errno()));
        return -fd_path.get_errno();
    }

    int result = readlinkatInternal(fd_path.fd(), fd_path.path(), buf, bufsiz);

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

    FdPath fd_path = resolve(newfd, newname, false);
    if (fd_path.is_error()) {
        VERBOSE("Error while resolving path during symlinkat(%d, %s), error: %s", newfd, newname, strerror(fd_path.get_errno()));
        return -fd_path.get_errno();
    }
    int result = ::symlinkat(oldname, fd_path.fd(), fd_path.path());
    if (result == -1) {
        result = -errno;
    }
    return result;
}

int Filesystem::RenameAt2(int oldfd, const char* oldname, int newfd, const char* newname, int flags) {
    if (!oldname || !newname) {
        return -EINVAL;
    }

    FdPath old_fd_path = resolve(oldfd, oldname, false);
    if (old_fd_path.is_error()) {
        VERBOSE("Error while resolving old path during renameat2(%d, %s), error: %s", oldfd, oldname, strerror(old_fd_path.get_errno()));
        return -old_fd_path.get_errno();
    }
    FdPath new_fd_path = resolve(newfd, newname, false);
    if (new_fd_path.is_error()) {
        VERBOSE("Error while resolving old path during renameat2(%d, %s), error: %s", newfd, newname, strerror(new_fd_path.get_errno()));
        return -new_fd_path.get_errno();
    }
    int result = ::renameat2(old_fd_path.fd(), old_fd_path.path(), new_fd_path.fd(), new_fd_path.path(), flags);
    if (result == -1) {
        result = -errno;
    }
    return result;
}

int Filesystem::Chmod(const char* filename, u64 mode) {
    if (!filename) {
        return -EINVAL;
    }

    FdPath fd_path = resolve(filename, true);
    if (fd_path.is_error()) {
        VERBOSE("Error while resolving path during chmod(%s), error: %s", filename, strerror(fd_path.get_errno()));
        return -fd_path.get_errno();
    }
    int result = ::chmod(fd_path.full_path(), mode);
    if (result == -1) {
        result = -errno;
    }
    return result;
}

int Filesystem::Creat(const char* filename, u64 mode) {
    FdPath fd_path = resolve(filename, false);
    if (fd_path.is_error()) {
        VERBOSE("Error while resolving path during creat(%s), error: %s", filename, strerror(fd_path.get_errno()));
        return -fd_path.get_errno();
    }
    return ::creat(fd_path.full_path(), mode);
}

int Filesystem::Statx(int fd, const char* filename, int flags, u32 mask, struct statx* statxbuf) {
    bool follow = !(flags & AT_SYMLINK_NOFOLLOW);
    FdPath fd_path = resolve(fd, filename, follow);
    if (fd_path.is_error()) {
        VERBOSE("Error while resolving old path during statx(%d, %s), error: %s", fd, filename, strerror(fd_path.get_errno()));
        return -fd_path.get_errno();
    }
    return statxInternal(fd_path.fd(), fd_path.path(), flags, mask, statxbuf);
}

int Filesystem::UnlinkAt(int fd, const char* filename, int flags) {
    if (!filename) {
        WARN("unlink with null filename?");
        return -EINVAL;
    }

    FdPath fd_path = resolve(fd, filename, false);
    if (fd_path.is_error()) {
        VERBOSE("Error while resolving old path during unlinkat(%d, %s), error: %s", fd, filename, strerror(fd_path.get_errno()));
        return -fd_path.get_errno();
    }
    return unlinkatInternal(fd_path.fd(), fd_path.path(), flags);
}

int Filesystem::LinkAt(int oldfd, const char* oldpath, int newfd, const char* newpath, int flags) {
    bool follow = flags & AT_SYMLINK_FOLLOW;
    FdPath old_fd_path = resolve(oldfd, oldpath, follow);
    if (old_fd_path.is_error()) {
        VERBOSE("Error while resolving old path during linkat(%d, %s), error: %s", oldfd, oldpath, strerror(old_fd_path.get_errno()));
        return -old_fd_path.get_errno();
    }
    FdPath new_fd_path = resolve(newfd, newpath, follow);
    if (new_fd_path.is_error()) {
        VERBOSE("Error while resolving old path during linkat(%d, %s), error: %s", newfd, newpath, strerror(new_fd_path.get_errno()));
        return -new_fd_path.get_errno();
    }

    return linkatInternal(old_fd_path.fd(), old_fd_path.path(), new_fd_path.fd(), new_fd_path.path(), flags);
}

int Filesystem::Chown(const char* filename, u64 owner, u64 group) {
    FdPath fd_path = resolve(filename, true);
    if (fd_path.is_error()) {
        VERBOSE("Error while resolving path during chown(%s), error: %s", filename, strerror(fd_path.get_errno()));
        return -fd_path.get_errno();
    }
    int result = ::chown(fd_path.full_path(), owner, group);
    if (result == -1) {
        result = -errno;
    }
    return result;
}

int Filesystem::LChown(const char* filename, u64 owner, u64 group) {
    FdPath fd_path = resolve(filename, false);
    if (fd_path.is_error()) {
        VERBOSE("Error while resolving path during lchown(%s), error: %s", filename, strerror(fd_path.get_errno()));
        return -fd_path.get_errno();
    }
    int result = ::lchown(fd_path.full_path(), owner, group);
    if (result == -1) {
        result = -errno;
    }
    return result;
}

int Filesystem::Chdir(const char* filename) {
    FdPath fd_path = resolve(filename, true);
    if (fd_path.is_error()) {
        VERBOSE("Error while resolving path during chdir(%s), error: %s", filename, strerror(fd_path.get_errno()));
        return -fd_path.get_errno();
    }
    int result = ::syscall(SYS_chdir, fd_path.full_path());
    if (result == -1) {
        result = -errno;
    }
    return result;
}

int Filesystem::MkdirAt(int fd, const char* filename, u64 mode) {
    FdPath fd_path = resolve(fd, filename, true);
    if (fd_path.is_error()) {
        VERBOSE("Error while resolving old path during mkdirat(%d, %s), error: %s", fd, filename, strerror(fd_path.get_errno()));
        return -fd_path.get_errno();
    }
    int result = ::mkdirat(fd_path.fd(), fd_path.path(), mode);
    if (result == -1) {
        result = -errno;
    }
    return result;
}

int Filesystem::MknodAt(int fd, const char* filename, u64 mode, u64 dev) {
    FdPath fd_path = resolve(fd, filename, true);
    if (fd_path.is_error()) {
        VERBOSE("Error while resolving old path during mknodat(%d, %s), error: %s", fd, filename, strerror(fd_path.get_errno()));
        return -fd_path.get_errno();
    }
    int result = ::mknodat(fd_path.fd(), fd_path.path(), mode, dev);
    if (result == -1) {
        result = -errno;
    }
    return result;
}

int Filesystem::FChmodAt(int fd, const char* filename, u64 mode) {
    FdPath fd_path = resolve(fd, filename, true);
    if (fd_path.is_error()) {
        VERBOSE("Error while resolving path during fchmodat(%d, %s), error: %s", fd, filename, strerror(fd_path.get_errno()));
        return -fd_path.get_errno();
    }
    return fchmodatInternal(fd_path.fd(), fd_path.path(), mode);
}

int Filesystem::LGetXAttr(const char* filename, const char* name, void* value, size_t size) {
    FdPath fd_path = resolve(filename, false);
    if (fd_path.is_error()) {
        VERBOSE("Error while resolving path during lgetxattr(%s), error: %s", filename, strerror(fd_path.get_errno()));
        return -fd_path.get_errno();
    }
    return lgetxattrInternal(fd_path.full_path(), name, value, size);
}

ssize_t Filesystem::Listxattr(const char* filename, char* list, size_t size, bool llist) {
    // TODO: make two functions
    if (!llist) {
        FdPath fd_path = resolve(filename, true);
        if (fd_path.is_error()) {
            VERBOSE("Error while resolving path during listxattr(%s), error: %s", filename, strerror(fd_path.get_errno()));
            return -fd_path.get_errno();
        }
        return ::listxattr(fd_path.full_path(), list, size);
    } else {
        FdPath fd_path = resolve(filename, false);
        if (fd_path.is_error()) {
            VERBOSE("Error while resolving path during llistxattr(%s), error: %s", filename, strerror(fd_path.get_errno()));
            return -fd_path.get_errno();
        }
        return ::llistxattr(fd_path.full_path(), list, size);
    }
}

int Filesystem::GetXAttr(const char* filename, const char* name, void* value, size_t size) {
    FdPath fd_path = resolve(filename, true);
    if (fd_path.is_error()) {
        VERBOSE("Error while resolving path during getxattr(%s), error: %s", filename, strerror(fd_path.get_errno()));
        return -fd_path.get_errno();
    }
    return getxattrInternal(fd_path.full_path(), name, value, size);
}

int Filesystem::LSetXAttr(const char* filename, const char* name, void* value, size_t size, int flags) {
    FdPath fd_path = resolve(filename, false);
    if (fd_path.is_error()) {
        VERBOSE("Error while resolving path during lsetxattr(%s), error: %s", filename, strerror(fd_path.get_errno()));
        return -fd_path.get_errno();
    }
    return lsetxattrInternal(fd_path.full_path(), name, value, size, flags);
}

int Filesystem::SetXAttr(const char* filename, const char* name, void* value, size_t size, int flags) {
    FdPath fd_path = resolve(filename, true);
    if (fd_path.is_error()) {
        VERBOSE("Error while resolving path during setxattr(%s), error: %s", filename, strerror(fd_path.get_errno()));
        return -fd_path.get_errno();
    }
    return setxattrInternal(fd_path.full_path(), name, value, size, flags);
}

int Filesystem::RemoveXAttr(const char* filename, const char* name) {
    FdPath fd_path = resolve(filename, true);
    if (fd_path.is_error()) {
        VERBOSE("Error while resolving path during removexattr(%s), error: %s", filename, strerror(fd_path.get_errno()));
        return -fd_path.get_errno();
    }
    return removexattrInternal(fd_path.full_path(), name);
}

int Filesystem::LRemoveXAttr(const char* filename, const char* name) {
    FdPath fd_path = resolve(filename, false);
    if (fd_path.is_error()) {
        VERBOSE("Error while resolving path during lremovexattr(%s), error: %s", filename, strerror(fd_path.get_errno()));
        return -fd_path.get_errno();
    }
    return lremovexattrInternal(fd_path.full_path(), name);
}

int Filesystem::UtimensAt(int fd, const char* filename, struct timespec* spec, int flags) {
    bool follow = !(flags & AT_SYMLINK_NOFOLLOW);
    FdPath fd_path = resolve(fd, filename, follow);
    if (fd_path.is_error()) {
        VERBOSE("Error while resolving path during utimensat(%d, %s), error: %s", fd, filename, strerror(fd_path.get_errno()));
        return -fd_path.get_errno();
    }
    return utimensatInternal(fd_path.fd(), fd_path.path(), spec, flags);
}

int Filesystem::Rmdir(const char* dir) {
    FdPath fd_path = resolve(dir, true);
    if (fd_path.is_error()) {
        VERBOSE("Error while resolving path during rmdir(%s), error: %s", dir, strerror(fd_path.get_errno()));
        return -fd_path.get_errno();
    }
    return rmdirInternal(fd_path.full_path());
}

int Filesystem::Chroot(const char* path) {
    WARN("chroot(%s)", path);
    if (!path) {
        return -EINVAL;
    }

    FdPath fd_path = resolve(path, true);
    if (fd_path.is_error()) {
        VERBOSE("Error while resolving path during chroot(%s), error: %s", path, strerror(fd_path.get_errno()));
        return -fd_path.get_errno();
    }

    // TODO: setting rootfs_path is most likely thread unsafe?
    g_config.rootfs_path = fd_path.full_path();
    int old_rootfs_fd = g_rootfs_fd;
    g_rootfs_fd = open(fd_path.full_path(), O_PATH | O_DIRECTORY);
    FD::unprotectAndClose(old_rootfs_fd);
    ASSERT_MSG(g_rootfs_fd > 0, "Failed to open new rootfs dir: %s", fd_path.full_path());
    FD::protect(g_rootfs_fd);
    return 0;
}

int Filesystem::Mount(const char* source, const char* target, const char* fstype, u64 flags, const void* data) {
    const char* sptr = nullptr;
    const char* tptr = nullptr;

    bool follow = !(flags & MS_NOSYMFOLLOW);

    FdPath rsource, rtarget;
    if (source) {
        rsource = resolve(source, follow);
        if (rsource.is_error()) {
            VERBOSE("Error while resolving path during mount(src=%s), error: %s", source, strerror(rsource.get_errno()));
            return -rsource.get_errno();
        }
        sptr = rsource.full_path();
    }
    if (target) {
        rtarget = resolve(target, follow);
        if (rtarget.is_error()) {
            VERBOSE("Error while resolving path during mount(dst=%s), error: %s", target, strerror(rtarget.get_errno()));
            return -rtarget.get_errno();
        }
        tptr = rtarget.full_path();
    }
    return ::mount(sptr, tptr, fstype, flags, data);
}

int Filesystem::Umount(const char* path, int flags) {
    bool follow = !(flags & UMOUNT_NOFOLLOW);
    FdPath fd_path = resolve(path, follow);
    if (fd_path.is_error()) {
        VERBOSE("Error while resolving path during umount(%s), error: %s", path, strerror(fd_path.get_errno()));
        return -fd_path.get_errno();
    }
    return ::umount2(fd_path.full_path(), flags);
}

int Filesystem::INotifyAddWatch(int fd, const char* path, u32 mask) {
    FdPath fd_path = resolve(path, true);
    if (fd_path.is_error()) {
        VERBOSE("Error while resolving path during inotifyaddwatch(%s), error: %s", path, strerror(fd_path.get_errno()));
        return -fd_path.get_errno();
    }
    return inotify_add_watch(fd, fd_path.full_path(), mask);
}

int Filesystem::Truncate(const char* path, u64 length) {
    FdPath fd_path = resolve(path, true);
    if (fd_path.is_error()) {
        VERBOSE("Error while resolving path during truncate(%s), error: %s", path, strerror(fd_path.get_errno()));
        return -fd_path.get_errno();
    }
    return truncate(fd_path.full_path(), length);
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

FdPath Filesystem::resolve(int fd, const char* path, bool resolve_symlinks) {
    FdPath fd_path = resolveImpl(fd, path, resolve_symlinks);
    return fd_path;
}

FdPath Filesystem::resolve(const char* path, bool resolve_symlinks) {
    if (!path) {
        WARN("Tried to resolve a nullptr path, returning EFAULT");
        return FdPath::error(EFAULT);
    }

    return resolve(AT_FDCWD, path, resolve_symlinks);
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

FdPath Filesystem::resolveImpl(int fd, const char* path, bool resolve_final) {
    if (path == nullptr) {
        return FdPath::create(fd, nullptr);
    }

    if (path[0] == 0) {
        return FdPath::create(fd, path);
    }

    if (path[0] == '/' && path[1] == 0) {
        return FdPath::create(AT_FDCWD, g_config.rootfs_path);
    }

    if (isProcSelfExe(path)) {
        return FdPath::create(AT_FDCWD, g_executable_path_absolute);
    }

    int current_fd;
    if (path[0] == '/') {
        current_fd = g_rootfs_fd;
    } else {
        current_fd = fd;
    }

    std::filesystem::path current_relative_path;

    std::filesystem::path resolve_me = std::filesystem::path(path).relative_path();
    std::deque<std::string> components;
    for (auto& entry : resolve_me) {
        components.push_back(entry);
    }

    struct statx root_statx;
    int result = statx(g_rootfs_fd, "", AT_EMPTY_PATH, STATX_TYPE | STATX_INO | STATX_MNT_ID, &root_statx);
    ASSERT(result == 0);

    int total_symlinks_resolved = 0; // goes up to 40 as per the kernel
    while (!components.empty()) {
        std::string current_component = components.front();
        components.pop_front();
        bool final_component = components.empty();

        struct statx current_statx;
        result = statx(current_fd, current_relative_path.c_str(), AT_EMPTY_PATH, STATX_TYPE | STATX_INO | STATX_MNT_ID, &current_statx);
        if (result != 0) {
            VERBOSE("Error while resolving statx %d %s, error: %s", current_fd, current_relative_path.c_str());
            return FdPath::error(errno);
        }

        if (current_component == ".." && statx_inode_same(&root_statx, &current_statx)) {
            // Warn about attempted container escapes as they could be a bug in our implementation
            // This way we still allow programs to use fd's outside the rootfs, as long as they
            // don't go inside and try to escape again
            // For example fd + a component with an absolute symlink shouldn't be able to then escape the rootfs
            VERBOSE("Tried to escape rootfs: %d %s", current_fd, current_relative_path.c_str());

            // Skip this component and point to rootfs
            current_fd = g_rootfs_fd;
            // Also clear this since we are starting resolution from rootfs now
            current_relative_path = ".";
            continue;
        }

        std::filesystem::path current = current_relative_path / current_component;
        struct stat current_stat;
        result = fstatat(current_fd, current.c_str(), &current_stat, AT_SYMLINK_NOFOLLOW);
        if (result != 0) {
            switch (errno) {
            case ENOENT: {
                if (final_component) {
                    // it is not necessarily an error if the final component is not found, maybe we are just creating it
                    break;
                } else {
                    return FdPath::error(ENOENT);
                }
            }
            case EACCES: {
                return FdPath::error(EACCES);
            }
            default: {
                WARN("Unknown error during path resolution: %s", strerror(errno));
                return FdPath::error(errno);
            }
            }
        }

        if (S_ISLNK(current_stat.st_mode)) {
            if (final_component && !resolve_final) {
                current_relative_path = current_relative_path / current_component;
                break;
            }

            total_symlinks_resolved++;
            if (total_symlinks_resolved > 40) {
                WARN("Symlink resolution hit limit for %d %s", fd, path);
                return FdPath::error(ELOOP);
            }

            // Check if current component is a magic-link
            // If it is, we don't resolve it and add it directly to the path and continue
            // This is because the kernel does special stuff when resolving magic-links which we can't do ourselves
            // Unfortunately there's no simple way of checking if it's a magic link that I can think of, other than using
            // openat2 with RESOLVE_NO_MAGICLINKS and openat and seeing whether there's a mismatch in results
            {
                // Open a directory to the current relative path
                // If we don't do that, then openat2 will fail if a magic-link is merely a component
                // For example if I do /proc/self/root/etc, I don't want the openat2 to fail here because etc
                // is not a magic-link, but it would fail because /proc/self/root is
                // We want to just check if the current component is a magic-link
                int dirfd = AT_FDCWD;
                if (current_relative_path.empty()) {
                    if (current_fd != AT_FDCWD) {
                        dirfd = dup(current_fd);
                    }
                } else {
                    dirfd = openat(current_fd, current_relative_path.c_str(), O_PATH | O_DIRECTORY);
                }
                ASSERT_MSG(dirfd == AT_FDCWD || dirfd >= 0, "Dirfd: %d %s", dirfd, strerror(errno));

                int result1 = openat(dirfd, current_component.c_str(), O_PATH);

                struct open_how how{
                    .flags = O_PATH,
                    .mode = 0,
                    .resolve = RESOLVE_NO_MAGICLINKS,
                };
                int result2 = syscall(SYS_openat2, dirfd, current_component.c_str(), &how, sizeof(open_how));
                int result2_error = errno;

                // TODO: maybe optimize some cases using close_range
                close(dirfd);
                if (result1 > 0) {
                    close(result1);
                }
                if (result2 > 0) {
                    close(result2);
                }

                if (result1 > 0 && result2 > 0) {
                    // Both succeeded... that's fine
                } else if (result1 < 0 && result2 < 0) {
                    // Both failed... that's fine
                } else {
                    // One succeeded and one failed
                    if (result2 > 0 && result1 < 0) {
                        // Shouldn't be possible
                        WARN("openat2 succeeded and openat failed during magic-link detection... what?");
                    } else {
                        ASSERT(result2 < 0 && result1 > 0);

                        // So this is a magic-link. Append it to the path without resolving it and continue to the next component
                        ASSERT(result2_error == ELOOP); // this is how openat2 should fail when a component is a magic-link

                        // Finally do what we need, don't resolve and append it to the path
                        current_relative_path = current_relative_path / current_component;
                        continue;
                    }
                }
            }

            // If it's a symlink, don't add to current_relative_path. Instead, readlink it and
            // add the components to the component buffer
            char buffer[PATH_MAX];
            result = readlinkat(current_fd, current.c_str(), buffer, PATH_MAX);
            if (result <= 0) {
                WARN("Failed during readlinkat: %d %s (original: %d %s), error: %s", current_fd, current.c_str(), fd, path, strerror(errno));
                return FdPath::error(errno);
            }
            buffer[result] = 0;

            std::filesystem::path resolved = buffer;

            if (resolved.is_absolute()) {
                // If a component is a symlink to an absolute path, set start_fd to g_rootfs_fd
                current_fd = g_rootfs_fd;
                // Also clear this since we are starting resolution from rootfs now
                current_relative_path = ".";
            }

            resolved = resolved.relative_path();
            // Push the components to the front in reversed order
            for (auto it = resolved.end(); it != resolved.begin();) {
                --it;
                components.push_front(*it);
            }
        } else if (S_ISDIR(current_stat.st_mode)) {
            // If it's a dir, just add to current_relative_path and go to the next component
            current_relative_path = current_relative_path / current_component;
        } else {
            if (final_component) {
                // See path_resolution(7) -> info about final entry
                current_relative_path = current_relative_path / current_component;
            } else {
                return FdPath::error(ENOTDIR);
            }
        }
    }

    current_relative_path = current_relative_path.lexically_normal();
    ASSERT(current_relative_path.is_relative());

    if (current_relative_path.empty()) {
        // We can't use an empty path in many syscalls and we can't convert it to null either
        // So in this case we will convert it to a full path and return that instead
        FdPath ret = FdPath::create(current_fd, current_relative_path);
        ret.full_path();
        return ret;
    }

    return FdPath::create(current_fd, current_relative_path);
}

std::pair<int, NullablePath> Filesystem::resolveImplOld(int fd, const char* path, bool resolve_symlinks) {
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
            if (resolve_me.is_absolute()) {
                return {AT_FDCWD, g_config.rootfs_path / resolve_me.relative_path()};
            } else {
                return {fd, resolve_me};
            }
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
            if (resolve_me.is_absolute()) {
                return {AT_FDCWD, g_config.rootfs_path / resolve_me.relative_path()};
            } else {
                return {fd, resolve_me};
            }
        }
    }
}
