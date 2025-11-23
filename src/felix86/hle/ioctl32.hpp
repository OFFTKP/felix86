#pragma once

#include <string>
#include "felix86/common/types.hpp"

using ioctl_handler_type = int (*)(int, u32, u32);

struct Ioctl32 {
    static void registerFd(int fd, const std::string& name);

    static void duplicateFd(int fd, int new_fd);

    static ioctl_handler_type getHandler(int fd);
};

int ioctl32(int fd, u32 cmd, u32 args);