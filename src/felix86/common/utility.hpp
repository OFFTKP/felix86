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
