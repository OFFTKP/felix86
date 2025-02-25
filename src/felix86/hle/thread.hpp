#pragma once

#include <cstddef>
#include <utility>
#include <linux/sched.h>
#include <sched.h>
#include "felix86/common/utility.hpp"

struct Threads {
    static long Clone(ThreadState* current_state, clone_args* args);

    static void StartThread(ThreadState* state);

    static std::pair<u8*, size_t> AllocateStack(bool mode32);
};