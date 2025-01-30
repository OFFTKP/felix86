#include "felix86/common/x86.hpp"
#include "felix86/emulator.hpp"
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

int guest_breakpoint(const char* region, u64 address) {
    auto [start, end] = MemoryMetadata::GetRegionByName(region);

    if (start == 0 && end == 0) {
        WARN("Region %s not found, breakpoint will be added later if loaded", region);
        MemoryMetadata::AddDeferredBreakpoint(region, address);
        return -1;
    }

    if (address >= (end - start)) {
        WARN("Address %016lx is out of bounds for region %s", address, region);
        return -1;
    }

    g_breakpoints[address + start] = {};
    return g_breakpoints.size();
}

int guest_breakpoint_abs(u64 address) {
    g_breakpoints[address] = {};
    return g_breakpoints.size();
}

int clear_breakpoints() {
    int count = g_breakpoints.size();
    g_breakpoints.clear();
    return count;
}

void felix86_fxsave(struct ThreadState* state, u64 address, bool fxsave64) {
    if (fxsave64) {
        memcpy((u8*)address + 160, state->xmm, 16 * 16);
    } else {
        memcpy((u8*)address + 160, state->xmm, 8 * 16);
    }
}

void felix86_fxrstor(struct ThreadState* state, u64 address, bool fxrstor64) {
    if (fxrstor64) {
        memcpy(state->xmm, (u8*)address + 160, 16 * 16);
    } else {
        memcpy(state->xmm, (u8*)address + 160, 8 * 16);
    }
}

void felix86_packuswb(u8* dst, u8* src) {
    i16* src16 = (i16*)src;
    i16* dst16 = (i16*)dst;
    for (int i = 0; i < 8; i++) {
        i16 value = *dst16++;
        u8 result;
        if (value < 0) {
            result = 0;
        } else if (value > SCHAR_MAX) {
            result = 255;
        } else {
            result = (u8)value;
        }
        dst[i] = result;
    }

    for (int i = 8; i < 16; i++) {
        i16 value = *src16++;
        u8 result;
        if (value < 0) {
            result = 0;
        } else if (value > SCHAR_MAX) {
            result = 255;
        } else {
            result = (u8)value;
        }
        dst[i] = result;
    }
}

void dump_states() {
    if (!g_emulator) {
        return;
    }

    auto& states = g_emulator->GetStates();
    int i = 0;
    for (auto& state : states) {
        dprintf(g_output_fd, ANSI_COLOR_RED "State %d: PC: %016lx - %s@0x%lx\n" ANSI_COLOR_RESET, i, state.GetRip(),
                MemoryMetadata::GetRegionName(state.GetRip()).c_str(), MemoryMetadata::GetOffset(state.GetRip()));
        i++;
    }
}