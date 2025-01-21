#include "felix86/common/x86.hpp"
#include "fmt/format.h"
#include "utility.hpp"

namespace {
std::string GetBlockName(u32 name) {
    u32 block_index = name >> 20;
    if (block_index == 0) {
        return "Entry";
    } else if (block_index == 1) {
        return "Exit";
    } else {
        return fmt::format("{}", block_index - 2);
    }
}

std::string ToVarName(u32 name) {
    return std::to_string(name & ((1 << 20) - 1));
}
} // namespace

std::string GetNameString(u32 name) {
    return fmt::format("%{}@{}", ToVarName(name), GetBlockName(name));
}

void felix86_div128(ThreadState* state, u64 divisor) {
    ASSERT(divisor != 0);
    __int128_t dividend = ((__int128_t)state->gprs[X86_REF_RDX - X86_REF_RAX] << 64) | state->gprs[X86_REF_RAX - X86_REF_RAX];
    u64 quotient = dividend / (i64)divisor;
    u64 remainder = dividend % (i64)divisor;
    state->gprs[X86_REF_RAX - X86_REF_RAX] = quotient;
    state->gprs[X86_REF_RDX - X86_REF_RAX] = remainder;
}

void felix86_divu128(ThreadState* state, u64 divisor) {
    ASSERT(divisor != 0);
    __uint128_t dividend = ((__uint128_t)state->gprs[X86_REF_RDX - X86_REF_RAX] << 64) | state->gprs[X86_REF_RAX - X86_REF_RAX];
    u64 quotient = dividend / divisor;
    u64 remainder = dividend % divisor;
    state->gprs[X86_REF_RAX - X86_REF_RAX] = quotient;
    state->gprs[X86_REF_RDX - X86_REF_RAX] = remainder;
}

u64 sext(u64 value, u8 size) {
    switch (size) {
    case X86_SIZE_BYTE:
        return (i64)(i8)value;
    case X86_SIZE_WORD:
        return (i64)(i16)value;
    case X86_SIZE_DWORD:
        return (i64)(i32)value;
    case X86_SIZE_QWORD:
        return value;
    default:
        UNREACHABLE();
        return 0;
    }
}

u64 sext_if_64(u64 value, u8 size_e) {
    switch (size_e) {
    case X86_SIZE_BYTE:
    case X86_SIZE_WORD:
    case X86_SIZE_DWORD:
        return value;
    case X86_SIZE_QWORD:
        return (i64)(i32)value;
    default:
        ERROR("Invalid immediate size");
        return 0;
    }
}

u64 current_rip() {
    return g_thread_state->rip;
}

// If you don't flush the cache the code will randomly SIGILL
void flush_icache() {
#if defined(__riscv)
    asm volatile("fence.i" ::: "memory");
#elif defined(__aarch64__)
#pragma message("Don't forget to implement me")
#elif defined(__x86_64__)
    // No need to flush the cache on x86
#endif
}

int guest_breakpoint(u64 address) {
    g_breakpoints[address] = {};
    return g_breakpoints.size();
}

int clear_breakpoints() {
    int count = g_breakpoints.size();
    g_breakpoints.clear();
    return count;
}