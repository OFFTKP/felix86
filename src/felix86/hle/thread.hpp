#pragma once

#include <cstddef>
#include <linux/sched.h>
#include <sched.h>
#include "felix86/common/utility.hpp"

struct Threads {
    static long Clone(ThreadState* current_state, clone_args* args);

    static u8* AllocateStack(size_t size = 0);
};