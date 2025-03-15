#include <cstring>
#include <fcntl.h>
#include "felix86/hle/filesystem.hpp"

int Filesystem::OpenAt(int fd, const char* filename, int flags, u64 mode) {
    auto [new_fd, new_filename] = resolve(fd, filename);
    return openatInternal(new_fd, new_filename, flags, mode);
}

int Filesystem::FAccessAt(int fd, const char* filename, int mode, int flags) {
    auto [new_fd, new_filename] = resolve(fd, filename);
    return faccessatInternal(new_fd, new_filename, mode, flags);
}

int Filesystem::FStatAt(int fd, const char* filename, x64Stat* guest_stat, int flags) {
    auto [new_fd, new_filename] = resolve(fd, filename);

    struct stat host_stat;

    int result = fstatatInternal(new_fd, new_filename, &host_stat, flags);

    if (result == 0) {
        // This will do the marshalling, see stat.hpp
        *guest_stat = host_stat;
    }

    return result;
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
    auto [new_fd, new_filename] = resolve(fd, filename);

    int result = readlinkatInternal(new_fd, new_filename, buf, bufsiz);

    if (result > 0) {
        // Check if the path starts with rootfs (ie. when readlinking /proc stuff) and remove it
        char copy[PATH_MAX];
        memcpy(copy, buf, result);
        copy[result] = 0;
        std::string str = copy;
        if (str.find(g_rootfs_path.string()) == 0) {
            size_t rootfs_size = g_rootfs_path.string().size();
            size_t new_size = bufsiz - rootfs_size;
            memmove(copy, copy + rootfs_size, new_size);
            copy[new_size] = 0;
            VERBOSE("Readlink translation: %s", copy);

            memcpy(buf, copy, new_size);
            result = new_size;
        }
    }

    return result;
}

int Filesystem::Rename(const char* oldname, const char* newname) {
    if (!oldname || !newname) {
        return -EINVAL;
    }

    std::filesystem::path oldpath = resolve(oldname);
    std::filesystem::path newpath = resolve(newname);
    return ::rename(oldpath.c_str(), newpath.c_str());
}

int Filesystem::Symlink(const char* oldname, const char* newname) {
    if (!oldname || !newname) {
        return -EINVAL;
    }

    std::filesystem::path oldpath = resolve(oldname);
    std::filesystem::path newpath = resolve(newname);
    return ::symlink(oldpath.c_str(), newpath.c_str());
}

int Filesystem::Chmod(const char* filename, u64 mode) {
    if (!filename) {
        return -EINVAL;
    }

    std::filesystem::path path = resolve(filename);
    return ::chmod(path.c_str(), mode);
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
    return ::chown(path.c_str(), owner, group);
}

int Filesystem::Chdir(const char* filename) {
    std::filesystem::path path = resolve(filename);
    return chdir(path.c_str());
}

int Filesystem::LGetXAttr(const char* filename, const char* name, void* value, size_t size) {
    std::filesystem::path path = resolve(filename);
    return lgetxattrInternal(path.c_str(), name, value, size);
}

int Filesystem::UtimensAt(int fd, const char* filename, struct timespec* spec, int flags) {
    auto [new_fd, new_filename] = resolve(fd, filename);
    return utimensatInternal(new_fd, new_filename, spec, flags);
}

int Filesystem::openatInternal(int fd, const char* filename, int flags, u64 mode) {
    return ::syscall(SYS_openat, fd, filename, flags, mode);
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

int Filesystem::lgetxattrInternal(const char* filename, const char* name, void* value, size_t size) {
    return ::syscall(SYS_lgetxattr, filename, name, value, size);
}

int Filesystem::utimensatInternal(int fd, const char* filename, struct timespec* spec, int flags) {
    return ::syscall(SYS_utimensat, fd, filename, spec, flags);
}

std::pair<int, const char*> Filesystem::resolve(int fd, const char* path) {
    if (path == nullptr) {
        return {fd, nullptr};
    }

    // Special case for /proc/self/exe and similar variants
    std::string spath = path;
    std::string pidpath = "/proc/" + std::to_string(getpid()) + "/exe";
    if (spath == "/proc/self/exe" || spath == "/proc/thread-self/exe" || spath == pidpath) {
        return {AT_FDCWD, g_fs->GetExecutablePath().c_str()};
    }

    if (path[0] == '/') {
        return {g_rootfs_fd, &path[1]}; // return rootfs fd, skip the '/'
    } else {
        return {fd, path};
    }
}

std::filesystem::path Filesystem::resolve(const char* path) {
    ASSERT(path);

    // Special case for /proc/self/exe and similar variants
    std::string spath = path;
    std::string pidpath = "/proc/" + std::to_string(getpid()) + "/exe";
    if (spath == "/proc/self/exe" || spath == "/proc/thread-self/exe" || spath == pidpath) {
        return g_fs->GetExecutablePath();
    }

    if (path[0] == '/') {
        return g_rootfs_path / &path[1];
    }

    return path;
}