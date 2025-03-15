#pragma once

#include <filesystem>

struct Overlays {
    static void addOverlay(const char* lib_name, const std::filesystem::path& dest);

    static const char* isOverlay(int fd, const char* pathname);
};