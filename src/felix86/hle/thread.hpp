#pragma once

#include <cstddef>
#include <linux/sched.h>
#include <sched.h>
#include "felix86/common/utility.hpp"

struct Threads {
    static long Clone(ThreadState* current_state, clone_args* args);

    static std::pair<u8*, size_t> AllocateStack(size_t size = 0);
};