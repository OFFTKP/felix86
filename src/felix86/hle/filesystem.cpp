#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include "felix86/common/log.hpp"
#include "felix86/hle/filesystem.hpp"

#define VALIDATE_PATH(path)                                                                                                                          \
    if (!validatePath(path)) {                                                                                                                       \
        return -ENOENT;                                                                                                                              \
    }

const char* proc_self_exe = "/proc/self/exe";

Filesystem::Filesystem(const std::filesystem::path& path) {
    if (path.empty()) {
        ERROR("Empty rootfs path");
        return;
    }

    std::filesystem::path root_path = "/";
    if (path == root_path) { // hopefully prevent someone from messing with their host root
        ERROR("You chose your own root path, you need to choose a sandboxed rootfs path");
        return;
    }

    rootfs_path = std::filesystem::canonical(path);
    rootfs_path_string = rootfs_path.string();

    if (!std::filesystem::exists(rootfs_path) || !std::filesystem::is_directory(rootfs_path)) {
        ERROR("Rootfs path %s does not exist", rootfs_path.c_str());
        return;
    }

    // Do some basic sanity checks to make sure the user didn't pick a
    // wrong directory
    auto dirs = {
        "bin", "etc", "lib", "lib64", "sbin", "usr", "var",
    };

    for (auto& dir : dirs) {
        if (!std::filesystem::exists(rootfs_path / dir)) {
            ERROR("Rootfs path %s is missing %s", rootfs_path.c_str(), dir);
            return;
        }
    }

    good = true;
}

ssize_t Filesystem::ReadLinkAt(u32 dirfd, const char* pathname, char* buf, u32 bufsiz) {
    VALIDATE_PATH(pathname);

    ERROR("Unsupported readlinkat call: (%d) %s", dirfd, pathname);
}

bool Filesystem::validatePath(const std::filesystem::path& path) {
    std::string string = path.lexically_normal().string();

    // To check that it's part of the sandbox, we check that the path is
    // a subpath of the rootfs path
    if (string.find(rootfs_path_string) != 0) {
        ERROR("Path %s is not part of the sandbox", string.c_str());
        return false;
    }

    return true;
}