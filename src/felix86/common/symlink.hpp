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
};