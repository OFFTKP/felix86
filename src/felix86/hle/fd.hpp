#pragma once

#include "felix86/common/types.hpp"

struct FD {
    // We move our emulator fds above this number
    static constexpr int min() {
        return 512;
    }

    static constexpr int max() {
        return min() + 128;
    }

    static void protect(int fd);

    static void unprotectAndClose(int fd);

    static int moveToHighNumber(int fd);

    static int close(int fd);

    static long getdents64(int fd, u64 dirp, u32 count);

    static int close_range(u32 start, u32 end, int flags);

    static int dup2(int old_fd, int new_fd);

    static int dup3(int old_fd, int new_fd, int flags);
};