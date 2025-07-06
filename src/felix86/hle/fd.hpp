#pragma once

#include "felix86/common/types.hpp"

struct FD {
    static void protect(int fd);

    static void unprotectAndClose(int fd);

    static int close(int fd);

    static int close_range(u32 start, u32 end, int flags);
};