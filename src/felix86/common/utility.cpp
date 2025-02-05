#include "felix86/common/state.hpp"
#include "felix86/emulator.hpp"
#include "fmt/format.h"
#include "utility.hpp"

#ifdef __riscv
#include <sys/cachectl.h>
#endif

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
void flush_icache(void* start, void* end) {
#if defined(__riscv)
    // TODO: Code cache is local to each thread
    __riscv_flush_icache(start, end, 0);
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
        } else if (value > 255) {
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
        } else if (value > 255) {
            result = 255;
        } else {
            result = (u8)value;
        }
        dst[i] = result;
    }
}

void felix86_packusdw(u16* dst, u8* src) {
    i32* src32 = (i32*)src;
    i32* dst32 = (i32*)dst;
    for (int i = 0; i < 4; i++) {
        i32 value = *dst32++;
        u16 result;
        if (value < 0) {
            result = 0;
        } else if (value > 0xFFFF) {
            result = 0xFFFF;
        } else {
            result = (u16)value;
        }
        dst[i] = result;
    }

    for (int i = 4; i < 8; i++) {
        i32 value = *src32++;
        u16 result;
        if (value < 0) {
            result = 0;
        } else if (value > 0xFFFF) {
            result = 0xFFFF;
        } else {
            result = (u16)value;
        }
        dst[i] = result;
    }
}

void felix86_packsswb(u8* dst, u8* src) {
    i16* src16 = (i16*)src;
    i16* dst16 = (i16*)dst;
    for (int i = 0; i < 8; i++) {
        i16 value = *dst16++;
        u8 result;
        if (value < -127) {
            result = 128;
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
        if (value < -127) {
            result = 128;
        } else if (value > SCHAR_MAX) {
            result = 255;
        } else {
            result = (u8)value;
        }
        dst[i] = result;
    }
}

void felix86_packssdw(u16* dst, u8* src) {
    i32* src32 = (i32*)src;
    i32* dst32 = (i32*)dst;
    for (int i = 0; i < 4; i++) {
        i32 value = *dst32++;
        u16 result;
        if (value < -32767) {
            result = 0x8000;
        } else if (value > SHRT_MAX) {
            result = SHRT_MAX;
        } else {
            result = (u16)value;
        }
        dst[i] = result;
    }

    for (int i = 4; i < 8; i++) {
        i32 value = *src32++;
        u16 result;
        if (value < -32767) {
            result = 0x8000;
        } else if (value > SHRT_MAX) {
            result = SHRT_MAX;
        } else {
            result = (u16)value;
        }
        dst[i] = result;
    }
}

void felix86_pmaddwd(i16* dst, i16* src) {
    u32 temp[4];
    u32 result[4];

    temp[0] = dst[0] * src[0];
    temp[1] = dst[2] * src[2];
    temp[2] = dst[4] * src[4];
    temp[3] = dst[6] * src[6];

    result[0] = dst[1] * src[1];
    result[1] = dst[3] * src[3];
    result[2] = dst[5] * src[5];
    result[3] = dst[7] * src[7];

    u32* dst32 = (u32*)dst;
    dst32[0] = temp[0] + result[0];
    dst32[1] = temp[1] + result[1];
    dst32[2] = temp[2] + result[2];
    dst32[3] = temp[3] + result[3];
}

void dump_states() {
    if (!g_emulator) {
        return;
    }

    FELIX86_LOCK;
    auto& states = g_thread_states;
    int i = 0;
    for (auto& state : states) {
        dprintf(g_output_fd, ANSI_COLOR_RED "State %d: PC: %016lx - %s@0x%lx\n" ANSI_COLOR_RESET, i, state->GetRip(),
                MemoryMetadata::GetRegionName(state->GetRip()).c_str(), MemoryMetadata::GetOffset(state->GetRip()));

        if (g_calltrace) {
            dprintf(g_output_fd, ANSI_COLOR_RED "--- CALLTRACE ---\n" ANSI_COLOR_RESET);
            auto it = state->calltrace.rbegin();
            while (it != state->calltrace.rend()) {
                print_address(*it);
                it++;
            }
        }
        i++;
    }
    FELIX86_UNLOCK;
}

void print_address(u64 address) {
    dprintf(g_output_fd, ANSI_COLOR_RED "%s@0x%lx (%p)\n" ANSI_COLOR_RESET, MemoryMetadata::GetRegionName(address).c_str(),
            MemoryMetadata::GetOffset(address), (void*)address);
}

void push_calltrace(ThreadState* state) {
    state->calltrace.push_back(state->rip);
}

void pop_calltrace(ThreadState* state) {
    if (state->calltrace.empty()) {
        return;
    }

    state->calltrace.pop_back();
}