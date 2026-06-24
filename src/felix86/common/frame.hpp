#pragma once

#include <array>
#include <biscuit/registers.hpp>
#include "felix86/common/types.hpp"

struct ThreadState;

constexpr static std::array saved_gprs = {biscuit::ra, biscuit::sp, biscuit::tp, biscuit::s0, biscuit::s1, biscuit::s2,  biscuit::s3, biscuit::s4,
                                          biscuit::s5, biscuit::s6, biscuit::s7, biscuit::s8, biscuit::s9, biscuit::s10, biscuit::s11};

// A frame in the host stack that contains saved host registers
// This is used to restore the context before entering the dispatcher and to also store multiple contexts
// (for example when entering the dispatcher, then entering it again from a signal handler etc.)
struct felix86_frame {
    constexpr static u64 expected_magic = 0x6814'8664'0000'FE86;
    u64 magic; // to make sure this is indeed a frame
    ThreadState* state;
    u64 invalidate_caller_thunk_ptr;
    u64 gprs[saved_gprs.size()];
    // We don't modify the saved FPRs so we don't need to save them
};