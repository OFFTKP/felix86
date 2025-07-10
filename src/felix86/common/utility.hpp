#pragma once

#include <climits>
#include <cstddef>
#include <filesystem>
#include <string>
#include <vector>
#include <stdbool.h>
#include "Zydis/Register.h"
#include "biscuit/isa.hpp"
#include "felix86/common/types.hpp"

[[nodiscard]] constexpr bool IsValidJTypeImm(ptrdiff_t value) {
    return value >= -0x80000 && value <= 0x7FFFF;
}

[[nodiscard]] constexpr bool IsValid2GBImm(i64 value) {
    return (i64)value >= (i64)INT_MIN && (i64)value <= (i64)INT_MAX;
}

[[nodiscard]] constexpr bool IsValidSigned12BitImm(ptrdiff_t value) {
    return value >= -2048 && value <= 2047;
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

std::string get_region(u64 address);
bool has_region(u64 address);

void update_symbols();

void felix86_fsave_16(struct ThreadState* state, u64 address);
void felix86_fsave_32(struct ThreadState* state, u64 address);
void felix86_frstor_16(struct ThreadState* state, u64 address);
void felix86_frstor_32(struct ThreadState* state, u64 address);
void felix86_fstenv_16(struct ThreadState* state, u64 address);
void felix86_fstenv_32(struct ThreadState* state, u64 address);
void felix86_fldenv_16(struct ThreadState* state, u64 address);
void felix86_fldenv_32(struct ThreadState* state, u64 address);
void felix86_fxsave(struct ThreadState* state, u64 address);
void felix86_fxrstor(struct ThreadState* state, u64 address);

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
void f64_to_80_mem(double value, u64 address);

bool felix86_bts(u64 address, i64 offset);
bool felix86_btr(u64 address, i64 offset);
bool felix86_btc(u64 address, i64 offset);
bool felix86_bt(u64 address, i64 offset);
void felix86_psadbw(u8* dst, u8* src);

void felix86_fsin(ThreadState* state);
void felix86_fcos(ThreadState* state);
void felix86_fptan(ThreadState* state);
void felix86_fpatan(ThreadState* state);
void felix86_f2xm1(ThreadState* state);
void felix86_fscale(ThreadState* state);
void felix86_fyl2x(ThreadState* state);
void felix86_fyl2xp1(ThreadState* state);

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

inline void replace_all(std::string& str, const std::string& from, const std::string& to) {
    if (from.empty())
        return;

    size_t start_pos = 0;
    while ((start_pos = str.find(from, start_pos)) != std::string::npos) {
        str.replace(start_pos, from.length(), to);
        start_pos += to.length();
    }
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

std::string felix86_maps();
const std::string& felix86_cpuinfo();

bool check_if_privileged_executable(const std::filesystem::path& path);

// TODO: move me to new binfmt.hpp file along with binfmt_misc function
bool unregister_binfmt_misc(const std::string& path);

std::string disassemble_one(u64 address);

void felix86_crash_and_burn();