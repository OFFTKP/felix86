#pragma once

#include <string>
#include <stdbool.h>
#include <stdint.h>

using u64 = uint64_t;
using u32 = uint32_t;
using u16 = uint16_t;
using u8 = uint8_t;

using i64 = int64_t;
using i32 = int32_t;
using i16 = int16_t;
using i8 = int8_t;

std::string GetNameString(u32 name);

[[nodiscard]] constexpr bool IsValidSigned12BitImm(i64 value) {
    return value >= -2048 && value <= 2047;
}

[[nodiscard]] constexpr bool IsValidBTypeImm(ptrdiff_t value) {
    return value >= -4096 && value <= 4095;
}

[[nodiscard]] constexpr bool IsValidJTypeImm(ptrdiff_t value) {
    return value >= -0x80000 && value <= 0x7FFFF;
}

void felix86_div128(struct ThreadState* state, u64 divisor);
void felix86_divu128(struct ThreadState* state, u64 divisor);

u64 sext(u64 value, u8 size);
u64 sext_if_64(u64 value, u8 size_e);

void flush_icache(void* start, void* end);

int guest_breakpoint(const char* name, u64 address);

int clear_breakpoints();

void print_address(u64 address);

void felix86_fxsave(struct ThreadState* state, u64 address, bool fxsave64);

void felix86_fxrstor(struct ThreadState* state, u64 address, bool fxrstor64);

void felix86_packuswb(u8* dst, u8* src);
void felix86_packusdw(u16* dst, u8* src);
void felix86_packsswb(u8* dst, u8* src);
void felix86_packssdw(u16* dst, u8* src);
void felix86_pmaddwd(i16* dst, i16* src);

void push_calltrace(ThreadState* state);

void pop_calltrace(ThreadState* state);

void dump_states();

namespace biscuit {}
using namespace biscuit;

#define FELIX86_LOCK ASSERT(sem_wait(g_semaphore) == 0)
#define FELIX86_UNLOCK ASSERT(sem_post(g_semaphore) == 0)
