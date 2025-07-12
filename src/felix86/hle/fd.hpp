#pragma once

#include "felix86/common/types.hpp"

struct FD {
    static void protect(int fd);

    static void unprotectAndClose(int fd);

    static int moveToHighNumber(int fd);

    static int close(int fd);

    static int close_range(u32 start, u32 end, int flags);

    static int dup2(int old_fd, int new_fd);

    static int dup3(int old_fd, int new_fd, int flags);
};