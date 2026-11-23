#pragma once

#include "felix86/common/types.hpp"

struct QSBR {
    static void enter_quiescent_state();
    static u64 increment_global_epoch();
    static bool is_all_past_epoch(u64 target);
    static void wait_all_past_epoch(u64 target);
    // If a thread is in a spin loop it would never enter a quiescent state. In those cases we must force it to enter a quiescent state
    // so we can reclaim memory. We poke it by setting its deferred fault page and it will cause a fault on the next safepoint.
    static void poke_threads(u64 target);
    static void mark_offline();
    static void mark_online();
};
