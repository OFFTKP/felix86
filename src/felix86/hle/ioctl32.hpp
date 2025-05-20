#pragma once

#include "felix86/common/utility.hpp"

using ioctl_handler_type = int (*)(int, u32, u32);

struct Ioctl32 {
    static void registerFd(int fd, const std::string& name);

    static ioctl_handler_type getHandler(int fd);
};

int ioctl32(int fd, u32 cmd, u32 args);