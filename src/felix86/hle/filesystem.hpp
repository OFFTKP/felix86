#pragma once

#include <filesystem>
#include <linux/limits.h>
#include "felix86/common/utility.hpp"

struct Filesystem {
    Filesystem(const std::filesystem::path& path);

    bool Good() {
        return good;
    }

    ssize_t ReadLinkAt(u32 dirfd, const char* pathname, char* buf, u32 bufsiz);

private:
    bool validatePath(const std::filesystem::path& path);
    bool validatePath(const char* path) {
        return validatePath(std::filesystem::path(path));
    }

    bool good = false;
    std::filesystem::path rootfs_path;
    std::string rootfs_path_string;
};