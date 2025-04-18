#pragma once

#include <array>
#include <biscuit/registers.hpp>
#include "felix86/common/utility.hpp"

constexpr static std::array saved_gprs = {ra, sp, gp, tp, s0, s1, s2, s3, s4, s5, s6, s7, s8, s9, s10, s11};

// A frame in the host stack that contains saved host registers
// This is used to restore the context before entering the dispatcher and to also store multiple contexts
// (for example when entering the dispatcher, then entering it again from a signal handler etc.)
struct felix86_frame {
    constexpr static u64 expected_magic = 0x6814'8664'0000'FE86;
    u64 magic; // to make sure this is indeed a frame
    ThreadState* state;
    u64 gprs[saved_gprs.size()];
    // We don't modify the saved FPRs so we don't need to save them
};