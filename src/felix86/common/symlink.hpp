#pragma once

#include <filesystem>
#include "felix86/common/log.hpp"

struct Symlinker {
    static bool link(const std::filesystem::path& real_path, const std::filesystem::path& dest_path) {
        ASSERT(std::filesystem::exists(real_path));
        int result = symlink(real_path.c_str(), dest_path.c_str());
        return result == 0;
    }
};