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