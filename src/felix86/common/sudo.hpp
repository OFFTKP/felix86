#pragma once

#include <filesystem>
#include <grp.h>
#include "felix86/common/log.hpp"

struct Sudo {
    static bool hasPermissions();

    static bool dropPermissions();

    [[noreturn]] static void requestPermissions(int argc, char** argv);

    static bool chroot(const std::filesystem::path& path);

    static void mount(const char* path, const std::filesystem::path& dest, const char* fs_type, u32 flags = 0);

    static bool isMounted();
};