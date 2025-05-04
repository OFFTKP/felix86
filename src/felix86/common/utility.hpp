#pragma once

#include <climits>
#include <cstddef>
#include <filesystem>
#include <string>
#include <vector>
#include <stdbool.h>
#include <stdint.h>
#include "Zydis/Register.h"
#include "biscuit/isa.hpp"

using u64 = uint64_t;
using u32 = uint32_t;
using u16 = uint16_t;
using u8 = uint8_t;

using i64 = int64_t;
using i32 = int32_t;
using i16 = int16_t;
using i8 = int8_t;

[[nodiscard]] constexpr bool IsValidJTypeImm(ptrdiff_t value) {
    return value >= -0x80000 && value <= 0x7FFFF;
}

[[nodiscard]] constexpr bool IsValid2GBImm(i64 value) {
    return (i64)value >= (i64)INT_MIN && (i64)value <= (i64)INT_MAX;
}

void felix86_div128(struct ThreadState* state, u64 divisor);
void felix86_divu128(struct ThreadState* state, u64 divisor);

void felix86_iret(struct ThreadState* state);

u64 sext(u64 value, u8 size);
u64 sext_if_64(u64 value, u8 size_e);

std::string get_perf_symbol(u64 address);

void flush_icache();

void flush_icache_global(u64 start, u64 end);

int guest_breakpoint(const char* name, u64 address);

int clear_breakpoints();

void print_address(u64 address);

bool has_region(u64 address);

void update_symbols();

void felix86_fxsave(struct ThreadState* state, u64 address, bool fxsave64);

void felix86_fxrstor(struct ThreadState* state, u64 address, bool fxrstor64);

void felix86_pmaddwd(i16* dst, i16* src);

void push_calltrace(ThreadState* state, u64 address);

void pop_calltrace(ThreadState* state);

void dump_states();

namespace biscuit {}
using namespace biscuit;

enum class x86RoundingMode { Nearest = 0, Down = 1, Up = 2, Truncate = 3 };

inline RMode rounding_mode(x86RoundingMode mode) {
    switch (mode) {
    case x86RoundingMode::Nearest:
        return RMode::RNE;
    case x86RoundingMode::Down:
        return RMode::RDN;
    case x86RoundingMode::Up:
        return RMode::RUP;
    case x86RoundingMode::Truncate:
        return RMode::RTZ;
    }
    __builtin_unreachable();
}

typedef struct __attribute__((packed)) {
    uint64_t significand;
    uint16_t signExp;
} Float80;

Float80 f64_to_80(double);
double f80_to_64(Float80*);

bool felix86_bts(u64 address, i64 offset);
bool felix86_btr(u64 address, i64 offset);
bool felix86_btc(u64 address, i64 offset);
bool felix86_bt(u64 address, i64 offset);
void felix86_psadbw(u8* dst, u8* src);

void felix86_fsin(ThreadState* state);
void felix86_fcos(ThreadState* state);

const char* print_exit_reason(int reason);

inline std::vector<std::string> split_string(const std::string& txt, char ch) {
    std::vector<std::string> strs;
    size_t pos = txt.find(ch);
    size_t initialPos = 0;

    while (pos != std::string::npos) {
        strs.push_back(txt.substr(initialPos, pos - initialPos));
        initialPos = pos + 1;

        pos = txt.find(ch, initialPos);
    }

    strs.push_back(txt.substr(initialPos, std::min(pos, txt.size()) - initialPos + 1));

    return strs;
}

enum class pcmpxstrx { ImplicitIndex = 0b00, ImplicitMask = 0b01, ExplicitIndex = 0b10, ExplicitMask = 0b11 };

void felix86_pcmpxstrx(ThreadState* state, pcmpxstrx type, u8* dst, u8* src, u8 control);

inline bool is_subpath(const std::filesystem::path& path, const std::filesystem::path& base) {
    const auto mismatch_pair = std::mismatch(path.begin(), path.end(), base.begin(), base.end());
    return mismatch_pair.second == base.end();
}

u64 mmap_min_addr();

void felix86_set_segment(ThreadState* state, u64 value, ZydisRegister segment);

void felix86_fprem(ThreadState* state);

void felix86_fxam(ThreadState* state);

const std::string& felix86_cpuinfo();