#pragma once

#include <cstddef>
#include <utility>
#include <linux/sched.h>
#include <sched.h>
#include "felix86/common/types.hpp"

struct CloneArgs {
    ThreadState* parent_state = nullptr;
    u64 guest_flags = 0;
    pid_t* parent_tid = nullptr;
    pid_t* child_tid = nullptr;

    u64 new_tls = 0;
    u64 new_rsp = 0;
    u64 new_rip = 0;
    pthread_t new_thread{};

    u32 new_tid = 0; // to signal that clone_handler has finished using the pointer and get the tid
};

struct Threads {
    static int Unshare(int flags);

    static long Clone(ThreadState* current_state, CloneArgs* args);

    static void StartThread(ThreadState* state);

    static std::pair<u8*, size_t> AllocateStack(bool mode32);
};