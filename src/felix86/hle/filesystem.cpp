#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include "felix86/common/log.hpp"
#include "felix86/hle/filesystem.hpp"

const char* proc_self_exe = "/proc/self/exe";

bool Filesystem::LoadRootFS(const std::filesystem::path& path) {
    if (!rootfs_path.empty()) {
        ERROR("Rootfs already loaded");
        return false;
    }

    if (path.empty()) {
        ERROR("Empty rootfs path");
        return false;
    }

    std::filesystem::path root_path = "/";
    if (path == root_path) { // hopefully prevent someone from messing with their host root
        ERROR("You chose your own root path, you need to choose a sandboxed rootfs path");
        return false;
    }

    rootfs_path = std::filesystem::canonical(path);
    rootfs_path_string = rootfs_path.string();

    if (!std::filesystem::exists(rootfs_path) || !std::filesystem::is_directory(rootfs_path)) {
        ERROR("Rootfs path %s does not exist", rootfs_path.c_str());
        return false;
    }

    // Do some basic sanity checks to make sure the user didn't pick a
    // wrong directory
    auto dirs = {
        "bin", "etc", "lib", "lib64", "sbin", "usr", "var",
    };

    for (auto& dir : dirs) {
        if (!std::filesystem::exists(rootfs_path / dir)) {
            ERROR("Rootfs path %s is missing %s", rootfs_path.c_str(), dir);
            return false;
        }
    }

    FELIX86_LOCK;
    cwd_path = rootfs_path;
    FELIX86_UNLOCK;

    return true;
}

std::optional<std::filesystem::path> Filesystem::AtPath(int dirfd, const char* pathname) {
    // Check if it starts with /dev
    constexpr static const char* dev = "/dev";
    if (strncmp(pathname, dev, strlen(dev)) == 0) {
        return std::filesystem::path(pathname);
    }

    // Check if it starts with /run/user/1000
    constexpr static const char* run_user_1000 = "/run/user/1000";
    if (strncmp(pathname, run_user_1000, strlen(run_user_1000)) == 0) {
        return std::filesystem::path(pathname);
    }

    // Check if it starts with /sys/dev
    constexpr static const char* sys_dev = "/sys/dev";
    if (strncmp(pathname, sys_dev, strlen(sys_dev)) == 0) {
        return std::filesystem::path(pathname);
    }

    if (std::string(pathname) == proc_self_exe) { // TODO: remove this, AtPath should handle this
        return executable_path;                   // TODO: remove the rootfs from this path
    }

    std::filesystem::path path = pathname;
    if (path.is_relative()) {
        if (dirfd == AT_FDCWD) {
            FELIX86_LOCK;
            path = cwd_path / path;
            FELIX86_UNLOCK;
        } else {
            struct stat dirfd_stat;
            fstat(dirfd, &dirfd_stat);

            bool is_dir = S_ISDIR(dirfd_stat.st_mode);
            if (!is_dir) {
                WARN("dirfd is not a directory");
                error = -ENOTDIR;
                return std::nullopt;
            }

            // This is not POSIX portable but should work on Linux which is what we're targeting
            char dirfd_path[PATH_MAX];
            snprintf(dirfd_path, sizeof(dirfd_path), "/proc/self/fd/%d", dirfd);
            char result_path[PATH_MAX];
            ssize_t res = readlink(result_path, dirfd_path, sizeof(dirfd_path));
            if (res == -1) {
                WARN("Failed to readlink dirfd");
                error = -ENOENT;
                return std::nullopt;
            }
            result_path[res] = '\0';

            struct stat result_path_stat;
            stat(result_path, &result_path_stat);

            // Sanity check that the directory was not moved or something
            if (result_path_stat.st_dev != dirfd_stat.st_dev || result_path_stat.st_ino != dirfd_stat.st_ino) {
                WARN("dirfd sanity check failed");
                error = -ENOENT;
                return std::nullopt;
            }

            path = std::filesystem::path(result_path) / path;
        }
    } else if (path.is_absolute() && !validatePath(path)) {
        path = rootfs_path / path.relative_path();
    }

    if (std::filesystem::exists(path) && std::filesystem::is_symlink(path)) {
        char resolved_path[PATH_MAX];
        if (realpath(path.c_str(), resolved_path) == nullptr) {
            ERROR("Failed to resolve path %s", path.c_str());
        }

        path = resolved_path;
    }

    if (!validatePath(path)) {
        error = -ENOENT;
        return std::nullopt;
    }

    return path;
}

ssize_t Filesystem::ReadLinkAt(int dirfd, const char* pathname, char* buf, u32 bufsiz) {
    if (std::string(pathname) == proc_self_exe) { // TODO: remove this, AtPath should handle this
        std::string executable_path_string = executable_path.string();
        // readlink does not append a null terminator
        size_t written_size = std::min(executable_path_string.size() - 1, (size_t)bufsiz);
        memcpy(buf, executable_path_string.c_str(), written_size);
        return written_size;
    }

    auto path_opt = AtPath(dirfd, pathname);

    if (!path_opt) {
        return error;
    }

    std::filesystem::path path = path_opt.value();
    return readlink(path.c_str(), buf, bufsiz);
}

ssize_t Filesystem::ReadLink(const char* pathname, char* buf, u32 bufsiz) {
    return ReadLinkAt(AT_FDCWD, pathname, buf, bufsiz);
}

int Filesystem::FAccessAt(int dirfd, const char* pathname, int mode, int flags) {
    auto path_opt = AtPath(dirfd, pathname);

    if (!path_opt) {
        return error;
    }

    std::filesystem::path path = path_opt.value();
    return faccessat(AT_FDCWD, path.c_str(), mode, flags);
}

int Filesystem::Statx(int dirfd, const char* pathname, int flags, int mask, struct statx* statxbuf) {
    auto path_opt = AtPath(dirfd, pathname);

    if (!path_opt) {
        return error;
    }

    std::filesystem::path path = path_opt.value();
    return statx(AT_FDCWD, path.c_str(), flags, mask, statxbuf);
}

int Filesystem::OpenAt(int dirfd, const char* pathname, int flags, int mode) {
    auto path_opt = AtPath(dirfd, pathname);

    if (!path_opt) {
        return error;
    }

    std::filesystem::path path = path_opt.value();
    return openat(AT_FDCWD, path.c_str(), flags, mode);
}

bool Filesystem::validatePath(const std::filesystem::path& path) {
    if (rootfs_path_string.empty()) {
        ERROR("Filesystem not initialized");
        return false;
    }

    std::string string = path.lexically_normal().string();

    // To check that it's part of the sandbox, we check that the path is
    // a subpath of the rootfs path
    if (string.find(rootfs_path_string) != 0) {
        return false;
    }

    return true;
}

int Filesystem::Chdir(const char* path) {
    std::filesystem::path new_cwd = path;
    new_cwd = new_cwd.lexically_normal();
    if (new_cwd.is_relative()) {
        FELIX86_LOCK;
        new_cwd = cwd_path / new_cwd;
        FELIX86_UNLOCK;
    } else {
        new_cwd = rootfs_path / new_cwd.relative_path();
    }

    if (!validatePath(new_cwd)) {
        return -ENOENT;
    }

    FELIX86_LOCK;
    cwd_path = new_cwd;
    FELIX86_UNLOCK;
    return 0;
}

int Filesystem::GetCwd(char* buf, u32 bufsiz) {
    FELIX86_LOCK;
    std::string cwd_string = cwd_path.string();
    FELIX86_UNLOCK;

    if (cwd_string.size() < rootfs_path_string.size()) {
        ERROR("cwd is not part of the rootfs");
        return -ENOENT;
    }

    cwd_string = cwd_string.substr(rootfs_path_string.size());
    if (cwd_string.empty()) {
        cwd_string = "/";
    }

    if (cwd_string[0] != '/') {
        cwd_string = "/" + cwd_string;
    }

    size_t written_size = std::min(cwd_string.size(), (size_t)bufsiz);
    memcpy(buf, cwd_string.c_str(), written_size);
    return written_size;
}