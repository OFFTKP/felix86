#pragma once

#include <filesystem>
#include "felix86/common/log.hpp"

struct Symlinker {
    static bool link(const std::filesystem::path& real_path, const std::filesystem::path& dest_path) {
        ASSERT(std::filesystem::exists(real_path));
        int result = symlink(real_path.c_str(), dest_path.c_str());

        if (result != 0 && errno == EEXIST) {
            char path[PATH_MAX];
            int size = readlink(dest_path.c_str(), path, PATH_MAX);
            if (size < 0) {
                ERROR("Failed to readlink %s", dest_path.c_str());
            }
            path[size] = 0;

            if (real_path.string() == path) {
                return true;
            } else {
                WARN("Symlink at %s already exist but points to %s instead of %s", dest_path.c_str(), path, real_path.c_str());
                return false;
            }
        }

        return result == 0;
    }

    // Resolve symlinks while placing results in rootfs to perpetually resolve them
    static std::filesystem::path resolve(const std::filesystem::path& path) {
        std::filesystem::path current = std::filesystem::absolute(path);
        if (!is_subpath(current, g_config.rootfs_path)) {
            current = g_config.rootfs_path / current.relative_path();
        }

        while (std::filesystem::is_symlink(current)) {
            std::error_code ec;
            std::filesystem::path resolved = std::filesystem::read_symlink(current, ec);
            if (ec) {
                ERROR("Failed to resolve symlink %s: %s", path.c_str(), ec.message().c_str());
            }

            if (!is_subpath(resolved, g_config.rootfs_path)) {
                if (resolved.is_absolute()) {
                    current = g_config.rootfs_path / resolved.relative_path();
                } else {
                    current = current.parent_path() / resolved.relative_path();
                    ASSERT_MSG(is_subpath(current, g_config.rootfs_path), "Resolved path is outside rootfs?");
                }
            } else {
                current = resolved;
            }
        }
        return current;
    }
};