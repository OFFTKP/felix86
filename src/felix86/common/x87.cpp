#include "felix86/common/state.hpp"
extern "C" {
#include "softfloat.h"
}
#include "cephes.h"

static_assert(sizeof(extFloat80_t) == 10);

static bool isNaN80(extFloat80_t* v) {
    return ((v->signExp & 0x7FFF) == 0x7FFF) && (v->signif & 0x7FFFFFFFFFFFFFFFull);
}

static void clearC2(ThreadState* state) {
    state->ctx.fpu_sw &= ~C2_BIT;
}

using arithmetic_func_t = void (*)(const extFloat80_t*, const extFloat80_t*, extFloat80_t*);

struct PrecisionGuard {
    PrecisionGuard(ThreadState* state) : state(state) {
        state->use_precision_control = true;
    }

    ~PrecisionGuard() {
        state->use_precision_control = false;
    }

    PrecisionGuard(const PrecisionGuard&) = delete;
    PrecisionGuard& operator=(const PrecisionGuard&) = delete;
    PrecisionGuard(PrecisionGuard&&) = delete;
    PrecisionGuard& operator=(PrecisionGuard&&) = delete;

private:
    ThreadState* state;
};

static void markRegFull(ThreadState* state, const extFloat80_t* reg) {
    bool is_reg = (u64)reg >= (u64)&state->ctx.st[0] && (u64)reg <= (u64)&state->ctx.st[7];
    ASSERT(is_reg);
    int index = ((u64)reg - (u64)&state->ctx.st[0]) / sizeof(Float80);
    u8 tw = state->ctx.fpu_tw;
    tw |= (1 << index);
    state->ctx.fpu_tw = tw;
}

static void checkReg(ThreadState* state, const extFloat80_t* reg) {
    bool is_reg = (u64)reg >= (u64)&state->ctx.st[0] && (u64)reg <= (u64)&state->ctx.st[7];
    u8 tw = state->ctx.fpu_tw;
    if (is_reg) {
        int index = ((u64)reg - (u64)&state->ctx.st[0]) / sizeof(Float80);
        u8 status = (tw >> index) & 0b1;
        if (status == 0b0) {
            X87LOG("Reg %d is empty in tag word during x87 instruction, RIP: %lx", index, state->GetRip());
        }
    }
}

static void push(ThreadState* state, const extFloat80_t* value) {
    u8 top = state->ctx.fpu_top;
    top -= 1;
    top &= 0b111;

    // Ensure we are not overflowing
    u8 tw = state->ctx.fpu_tw;
    bool empty = ((tw >> top) & 1) == 0;
    if (!empty) {
        WARN("x87 stack overflows during push at RIP=%lx", state->GetRip());
        state->ctx.fpu_sw |= 0b1000001; // set SF and IE bits
        state->ctx.fpu_cw |= C1_BIT;    // set C1 to 1 == overflow
    }

    tw |= 1 << top;
    memcpy(&state->ctx.st[top], value, sizeof(Float80));
    state->ctx.fpu_tw = tw;
    state->ctx.fpu_top = top;
}

static void pop(ThreadState* state) {
    u8 top = state->ctx.fpu_top;
    u8 tw = state->ctx.fpu_tw;
    bool empty = ((tw >> top) & 1) == 0;
    if (empty) {
        WARN("x87 stack underflows during pop at RIP=%lx", state->GetRip());
        state->ctx.fpu_sw |= 0b1000001; // set SF and IE bits
        state->ctx.fpu_cw &= ~C1_BIT;   // set C1 to 0 == underflow
    }
    tw &= ~(1 << top);
    top += 1;
    top &= 0b111;
    state->ctx.fpu_tw = tw;
    state->ctx.fpu_top = top;
}

[[nodiscard]] static bool safe_memcpy(void* dst, void* src, size_t size) {
    ThreadState* state = ThreadState::Get();
    state->force_defer_synchronous = true;
    int ret = sigsetjmp(state->force_defer_buffer, 1);
    if (ret == 0) {
        memcpy(dst, src, size);
        asm volatile("" ::: "memory");
        state->force_defer_synchronous = false;
        return true;
    } else {
        // A signal happened during the memcpy and it was deferred, return false to return out of the function
        WARN("Signal %d happened during x87 function", ret);
        state->force_defer_synchronous = false;
        return false;
    }
}

[[nodiscard]] static bool fpu_operation(ThreadState* state, extFloat80_t* lhs, extFloat80_t* rhs, int size, arithmetic_func_t operation,
                                        bool reverse) {
    PrecisionGuard guard(state);
    if (size == 0) {
        checkReg(state, lhs);
        checkReg(state, rhs);
        if (reverse) {
            operation(rhs, lhs, lhs);
        } else {
            operation(lhs, rhs, lhs);
        }
    } else {
        extFloat80_t f80storage;
        extFloat80_t* f80p = &f80storage;
        if (size == 80) {
            bool read = safe_memcpy(f80p, lhs, sizeof(Float80));
            if (!read) {
                return false;
            }
        } else if (size == 64) {
            float64_t f64;
            bool read = safe_memcpy(&f64, lhs, sizeof(float64_t));
            if (!read) {
                return false;
            }
            f64_to_extF80M(f64, f80p);
        } else if (size == 32) {
            float32_t f32;
            bool read = safe_memcpy(&f32, lhs, sizeof(float32_t));
            if (!read) {
                return false;
            }
            f32_to_extF80M(f32, f80p);
        } else {
            UNREACHABLE();
        }
        checkReg(state, rhs);
        if (reverse) {
            operation(f80p, rhs, rhs);
        } else {
            operation(rhs, f80p, rhs);
        }
    }
    return true;
}

static bool fpu_operation_integer(ThreadState* state, extFloat80_t* lhs, extFloat80_t* rhs, int size, arithmetic_func_t operation, bool reverse) {
    PrecisionGuard guard(state);
    extFloat80_t f80storage;
    extFloat80_t* f80p = &f80storage;
    if (size == 16) {
        i16 num;
        bool read = safe_memcpy(&num, lhs, sizeof(i16));
        if (!read) {
            return false;
        }
        i32 snum = num;
        i32_to_extF80M(snum, f80p);
    } else if (size == 32) {
        i32 num;
        bool read = safe_memcpy(&num, lhs, sizeof(i32));
        if (!read) {
            return false;
        }
        i32_to_extF80M(num, f80p);
    } else {
        UNREACHABLE();
    }
    checkReg(state, rhs);
    if (reverse) {
        operation(f80p, rhs, rhs);
    } else {
        operation(rhs, f80p, rhs);
    }
    return true;
}

void felix86_x87_FLD(ThreadState* state, extFloat80_t* mem, int size) {
    FELIX86_PROFILE_INSTANT_INCREMENT(state->thread_stats, AccumulatedFloatFallbackCount, 1);
    extFloat80_t f80storage;
    extFloat80_t* f80p = &f80storage;
    if (size == 0) {
        f80p = mem;
    } else if (size == 80) {
        bool read = safe_memcpy(f80p, mem, sizeof(Float80));
        if (!read) {
            return;
        }
    } else if (size == 64) {
        float64_t f64;
        bool read = safe_memcpy(&f64, mem, sizeof(float64_t));
        if (!read) {
            return;
        }
        f64_to_extF80M(f64, f80p);
    } else if (size == 32) {
        float32_t f32;
        bool read = safe_memcpy(&f32, mem, sizeof(float32_t));
        if (!read) {
            return;
        }
        f32_to_extF80M(f32, f80p);
    } else {
        UNREACHABLE();
    }
    push(state, f80p);
}

void felix86_x87_FILD(ThreadState* state, extFloat80_t* mem, int size) {
    FELIX86_PROFILE_INSTANT_INCREMENT(state->thread_stats, AccumulatedFloatFallbackCount, 1);
    extFloat80_t f80;
    if (size == 16) {
        i16 num;
        bool read = safe_memcpy(&num, mem, sizeof(i16));
        if (!read) {
            return;
        }
        i32_to_extF80M((i32)num, &f80);
    } else if (size == 64) {
        i64 num;
        bool read = safe_memcpy(&num, mem, sizeof(i64));
        if (!read) {
            return;
        }
        i64_to_extF80M(num, &f80);
    } else if (size == 32) {
        i32 num;
        bool read = safe_memcpy(&num, mem, sizeof(i32));
        if (!read) {
            return;
        }
        i32_to_extF80M(num, &f80);
    } else {
        UNREACHABLE();
    }
    push(state, &f80);
}

#define COMMON_ARITHMETIC(name_lower, name_upper, reverse)                                                                                           \
    void felix86_x87_F##name_upper(ThreadState* state, extFloat80_t* lhs, extFloat80_t* rhs, int size) {                                             \
        FELIX86_PROFILE_INSTANT_INCREMENT(state->thread_stats, AccumulatedFloatFallbackCount, 1);                                                    \
        if (!fpu_operation(state, lhs, rhs, size, &extF80M_##name_lower, reverse)) {                                                                 \
            return;                                                                                                                                  \
        }                                                                                                                                            \
    }                                                                                                                                                \
    void felix86_x87_FI##name_upper(ThreadState* state, extFloat80_t* lhs, extFloat80_t* rhs, int size) {                                            \
        FELIX86_PROFILE_INSTANT_INCREMENT(state->thread_stats, AccumulatedFloatFallbackCount, 1);                                                    \
        if (!fpu_operation_integer(state, lhs, rhs, size, &extF80M_##name_lower, reverse)) {                                                         \
            return;                                                                                                                                  \
        }                                                                                                                                            \
    }                                                                                                                                                \
    void felix86_x87_F##name_upper##P(ThreadState* state, extFloat80_t* lhs, extFloat80_t* rhs, int size) {                                          \
        FELIX86_PROFILE_INSTANT_INCREMENT(state->thread_stats, AccumulatedFloatFallbackCount, 1);                                                    \
        if (!fpu_operation(state, lhs, rhs, size, &extF80M_##name_lower, reverse)) {                                                                 \
            return;                                                                                                                                  \
        }                                                                                                                                            \
        pop(state);                                                                                                                                  \
    }

COMMON_ARITHMETIC(add, ADD, false);
COMMON_ARITHMETIC(sub, SUB, false);
COMMON_ARITHMETIC(mul, MUL, false);
COMMON_ARITHMETIC(div, DIV, false);
COMMON_ARITHMETIC(sub, SUBR, true);
COMMON_ARITHMETIC(div, DIVR, true);

[[nodiscard]] bool FST_common(ThreadState* state, extFloat80_t* mem, int size) {
    if (size == 0) {
        memcpy(mem, &state->ctx.st[state->ctx.fpu_top], sizeof(Float80));
        markRegFull(state, mem);
    } else if (size == 32) {
        float32_t f32 = extF80M_to_f32((extFloat80_t*)&state->ctx.st[state->ctx.fpu_top]);
        bool write = safe_memcpy(mem, &f32, sizeof(u32));
        if (!write) {
            return false;
        }
    } else if (size == 64) {
        float64_t f64 = extF80M_to_f64((extFloat80_t*)&state->ctx.st[state->ctx.fpu_top]);
        bool write = safe_memcpy(mem, &f64, sizeof(u64));
        if (!write) {
            return false;
        }
    } else if (size == 80) {
        bool write = safe_memcpy(mem, &state->ctx.st[state->ctx.fpu_top], sizeof(Float80));
        if (!write) {
            return false;
        }
    } else {
        UNREACHABLE();
    }
    return true;
}

void felix86_x87_FST(ThreadState* state, extFloat80_t* mem, int size) {
    FELIX86_PROFILE_INSTANT_INCREMENT(state->thread_stats, AccumulatedFloatFallbackCount, 1);
    (void)FST_common(state, mem, size);
}

[[nodiscard]] static bool FIST_common(ThreadState* state, extFloat80_t* mem, int size, u8 rounding_mode) {
    extFloat80_t* src = (extFloat80_t*)&state->ctx.st[state->ctx.fpu_top];
    if (size == 16) {
        i32 num = extF80M_to_i32(src, rounding_mode, false);
        if (num > INT16_MAX || num < INT16_MIN) {
            num = INT16_MIN;
            state->ctx.fpu_sw |= 1;
        }
        bool write = safe_memcpy(mem, &num, sizeof(u16));
        if (!write) {
            return false;
        }
    } else if (size == 32) {
        i32 num = extF80M_to_i32(src, rounding_mode, false);
        bool write = safe_memcpy(mem, &num, sizeof(u32));
        if (!write) {
            return false;
        }
    } else if (size == 64) {
        i64 num = extF80M_to_i64(src, rounding_mode, false);
        bool write = safe_memcpy(mem, &num, sizeof(u64));
        if (!write) {
            return false;
        }
    } else {
        UNREACHABLE();
    }
    return true;
}

void felix86_x87_FIST(ThreadState* state, extFloat80_t* mem, int size) {
    FELIX86_PROFILE_INSTANT_INCREMENT(state->thread_stats, AccumulatedFloatFallbackCount, 1);
    (void)FIST_common(state, mem, size, softfloat_getRoundingMode());
}

void felix86_x87_FISTP(ThreadState* state, extFloat80_t* mem, int size) {
    FELIX86_PROFILE_INSTANT_INCREMENT(state->thread_stats, AccumulatedFloatFallbackCount, 1);
    bool success = FIST_common(state, mem, size, softfloat_getRoundingMode());
    if (!success) {
        return;
    }
    pop(state);
}

void felix86_x87_FISTTP(ThreadState* state, extFloat80_t* mem, int size) {
    FELIX86_PROFILE_INSTANT_INCREMENT(state->thread_stats, AccumulatedFloatFallbackCount, 1);
    bool success = FIST_common(state, mem, size, softfloat_round_minMag);
    if (!success) {
        return;
    }
    pop(state);
}

void felix86_x87_FSTP(ThreadState* state, extFloat80_t* mem, int size) {
    FELIX86_PROFILE_INSTANT_INCREMENT(state->thread_stats, AccumulatedFloatFallbackCount, 1);
    bool success = FST_common(state, mem, size);
    if (!success) {
        return;
    }
    pop(state);
}

void felix86_x87_FLDZ(ThreadState* state) {
    FELIX86_PROFILE_INSTANT_INCREMENT(state->thread_stats, AccumulatedFloatFallbackCount, 1);
    extFloat80_t value = {0, 0};
    push(state, &value);
}

void felix86_x87_FLD1(ThreadState* state) {
    FELIX86_PROFILE_INSTANT_INCREMENT(state->thread_stats, AccumulatedFloatFallbackCount, 1);
    extFloat80_t value = {0x8000'0000'0000'0000, 0x3FFF};
    push(state, &value);
}

void felix86_x87_FLDL2T(ThreadState* state) {
    FELIX86_PROFILE_INSTANT_INCREMENT(state->thread_stats, AccumulatedFloatFallbackCount, 1);
    extFloat80_t value = {0xD49A'784B'CD1B'8AFEULL, 0x4000};
    push(state, &value);
}

void felix86_x87_FLDL2E(ThreadState* state) {
    FELIX86_PROFILE_INSTANT_INCREMENT(state->thread_stats, AccumulatedFloatFallbackCount, 1);
    extFloat80_t value = {0xB8AA'3B29'5C17'F0BCULL, 0x3FFF};
    push(state, &value);
}

void felix86_x87_FLDPI(ThreadState* state) {
    FELIX86_PROFILE_INSTANT_INCREMENT(state->thread_stats, AccumulatedFloatFallbackCount, 1);
    extFloat80_t value = {0xC90F'DAA2'2168'C235ULL, 0x4000};
    push(state, &value);
}

void felix86_x87_FLDLG2(ThreadState* state) {
    FELIX86_PROFILE_INSTANT_INCREMENT(state->thread_stats, AccumulatedFloatFallbackCount, 1);
    extFloat80_t value = {0x9A20'9A84'FBCF'F799ULL, 0x3FFD};
    push(state, &value);
}

void felix86_x87_FLDLN2(ThreadState* state) {
    FELIX86_PROFILE_INSTANT_INCREMENT(state->thread_stats, AccumulatedFloatFallbackCount, 1);
    extFloat80_t value = {0xB172'17F7'D1CF'79ACULL, 0x3FFE};
    push(state, &value);
}

void felix86_x87_FABS(ThreadState* state) {
    FELIX86_PROFILE_INSTANT_INCREMENT(state->thread_stats, AccumulatedFloatFallbackCount, 1);
    extFloat80_t* reg = (extFloat80_t*)&state->ctx.st[state->ctx.fpu_top];
    checkReg(state, reg);
    reg->signExp &= ~0x8000;
}

void felix86_x87_FCHS(ThreadState* state) {
    FELIX86_PROFILE_INSTANT_INCREMENT(state->thread_stats, AccumulatedFloatFallbackCount, 1);
    extFloat80_t* reg = (extFloat80_t*)&state->ctx.st[state->ctx.fpu_top];
    checkReg(state, reg);
    reg->signExp ^= 0x8000;
}

void felix86_x87_FXCH(ThreadState* state, extFloat80_t* lhs, extFloat80_t* rhs, int) {
    FELIX86_PROFILE_INSTANT_INCREMENT(state->thread_stats, AccumulatedFloatFallbackCount, 1);
    checkReg(state, lhs);
    checkReg(state, rhs);
    extFloat80_t temp = *lhs;
    *lhs = *rhs;
    *rhs = temp;
    state->ctx.fpu_sw &= ~C1_BIT;
}

void felix86_x87_FTST(ThreadState* state) {
    FELIX86_PROFILE_INSTANT_INCREMENT(state->thread_stats, AccumulatedFloatFallbackCount, 1);
    extFloat80_t zero = {0, 0};
    extFloat80_t* reg = (extFloat80_t*)&state->ctx.st[state->ctx.fpu_top];
    checkReg(state, reg);
    bool nan = (reg->signExp & 0x7FFF) == 0x7FFF;
    bool c0, c2, c3;
    if (nan) {
        c3 = 1;
        c2 = 1;
        c0 = 1;
    } else if ((reg->signExp & 0x7FFF) == 0 && reg->signif == 0) {
        c3 = 1;
        c2 = 0;
        c0 = 0;
    } else if (extF80M_lt(reg, &zero)) {
        c3 = 0;
        c2 = 0;
        c0 = 1;
    } else {
        c3 = 0;
        c2 = 0;
        c0 = 0;
    }
    state->ctx.fpu_sw &= ~(C0_BIT | C1_BIT | C2_BIT | C3_BIT);
    state->ctx.fpu_sw |= c0 ? C0_BIT : 0;
    state->ctx.fpu_sw |= c2 ? C2_BIT : 0;
    state->ctx.fpu_sw |= c3 ? C3_BIT : 0;
}

static bool handle_infinity(extFloat80_t* reg) {
    if ((reg->signExp & 0x7FFF) == 0x7FFF && reg->signif == 0x8000000000000000ULL) {
        softfloat_raiseFlags(softfloat_flag_invalid);
        reg->signExp = 0x7FFF;
        reg->signif = 0xC000000000000000ULL;
        return true;
    }
    return false;
}

void felix86_x87_FSIN(ThreadState* state) {
    FELIX86_PROFILE_INSTANT_INCREMENT(state->thread_stats, AccumulatedFloatFallbackCount, 1);
    extFloat80_t* reg = (extFloat80_t*)&state->ctx.st[state->ctx.fpu_top];
    checkReg(state, reg);
    clearC2(state);
    if (handle_infinity(reg))
        return;
    float128_t f128;
    extF80M_to_f128M(reg, &f128);
    f128 = cephes_f128_sinl(f128);
    f128M_to_extF80M(&f128, reg);
}

void felix86_x87_FCOS(ThreadState* state) {
    FELIX86_PROFILE_INSTANT_INCREMENT(state->thread_stats, AccumulatedFloatFallbackCount, 1);
    extFloat80_t* reg = (extFloat80_t*)&state->ctx.st[state->ctx.fpu_top];
    checkReg(state, reg);
    clearC2(state);
    if (handle_infinity(reg))
        return;
    float128_t f128;
    extF80M_to_f128M(reg, &f128);
    f128 = cephes_f128_cosl(f128);
    f128M_to_extF80M(&f128, reg);
}

void felix86_x87_FSINCOS(ThreadState* state) {
    FELIX86_PROFILE_INSTANT_INCREMENT(state->thread_stats, AccumulatedFloatFallbackCount, 1);
    extFloat80_t* reg = (extFloat80_t*)&state->ctx.st[state->ctx.fpu_top];
    checkReg(state, reg);
    clearC2(state);
    if (handle_infinity(reg))
        return;
    float128_t f128;
    extF80M_to_f128M(reg, &f128);
    float128_t sin_result = cephes_f128_sinl(f128);
    float128_t cos_result = cephes_f128_cosl(f128);
    f128M_to_extF80M(&sin_result, reg);
    extFloat80_t cos_f80;
    f128M_to_extF80M(&cos_result, &cos_f80);
    push(state, &cos_f80);
}

void felix86_x87_FPTAN(ThreadState* state) {
    FELIX86_PROFILE_INSTANT_INCREMENT(state->thread_stats, AccumulatedFloatFallbackCount, 1);
    extFloat80_t* reg = (extFloat80_t*)&state->ctx.st[state->ctx.fpu_top];
    checkReg(state, reg);
    clearC2(state);
    if (handle_infinity(reg))
        return;
    float128_t f128;
    extF80M_to_f128M(reg, &f128);
    f128 = cephes_f128_tanl(f128);
    f128M_to_extF80M(&f128, reg);
    extFloat80_t one = {0x8000'0000'0000'0000, 0x3FFF};
    push(state, &one);
}

void felix86_x87_FSQRT(ThreadState* state) {
    FELIX86_PROFILE_INSTANT_INCREMENT(state->thread_stats, AccumulatedFloatFallbackCount, 1);
    PrecisionGuard guard(state);
    extFloat80_t* reg = (extFloat80_t*)&state->ctx.st[state->ctx.fpu_top];
    checkReg(state, reg);
    extF80M_sqrt(reg, reg);
}

void felix86_x87_FPATAN(ThreadState* state) {
    FELIX86_PROFILE_INSTANT_INCREMENT(state->thread_stats, AccumulatedFloatFallbackCount, 1);
    extFloat80_t* lhs = (extFloat80_t*)&state->ctx.st[state->ctx.fpu_top];
    extFloat80_t* rhs = (extFloat80_t*)&state->ctx.st[(state->ctx.fpu_top + 1) & 0b111];
    checkReg(state, lhs);
    checkReg(state, rhs);
    float128_t l, r, d;
    extF80M_to_f128M(lhs, &l);
    extF80M_to_f128M(rhs, &r);
    d = cephes_f128_atan2l(r, l);
    f128M_to_extF80M(&d, rhs);
    pop(state);
}

void felix86_x87_F2XM1(ThreadState* state) {
    FELIX86_PROFILE_INSTANT_INCREMENT(state->thread_stats, AccumulatedFloatFallbackCount, 1);
    extFloat80_t* lhs = (extFloat80_t*)&state->ctx.st[state->ctx.fpu_top];
    checkReg(state, lhs);
    float128_t f128;
    float128_t one = {0x0000000000000000, 0x3FFF000000000000};
    extF80M_to_f128M(lhs, &f128);
    f128 = f128_sub(cephes_f128_exp2l(f128), one);
    f128M_to_extF80M(&f128, lhs);
}

void felix86_x87_FYL2X(ThreadState* state) {
    FELIX86_PROFILE_INSTANT_INCREMENT(state->thread_stats, AccumulatedFloatFallbackCount, 1);
    extFloat80_t* lhs = (extFloat80_t*)&state->ctx.st[state->ctx.fpu_top];
    extFloat80_t* rhs = (extFloat80_t*)&state->ctx.st[(state->ctx.fpu_top + 1) & 0b111];
    checkReg(state, lhs);
    checkReg(state, rhs);
    float128_t l, r, d;
    extF80M_to_f128M(lhs, &l);
    extF80M_to_f128M(rhs, &r);
    d = f128_mul(r, cephes_f128_log2l(l));
    f128M_to_extF80M(&d, rhs);
    pop(state);
}

void felix86_x87_FYL2XP1(ThreadState* state) {
    FELIX86_PROFILE_INSTANT_INCREMENT(state->thread_stats, AccumulatedFloatFallbackCount, 1);
    extFloat80_t* lhs = (extFloat80_t*)&state->ctx.st[state->ctx.fpu_top];
    extFloat80_t* rhs = (extFloat80_t*)&state->ctx.st[(state->ctx.fpu_top + 1) & 0b111];
    checkReg(state, lhs);
    checkReg(state, rhs);
    float128_t l, r, d;
    float128_t one = {0x0000000000000000, 0x3FFF000000000000};
    extF80M_to_f128M(lhs, &l);
    extF80M_to_f128M(rhs, &r);
    d = f128_mul(r, cephes_f128_log2l(f128_add(l, one)));
    f128M_to_extF80M(&d, rhs);
    pop(state);
}

void felix86_x87_FRNDINT(ThreadState* state) {
    FELIX86_PROFILE_INSTANT_INCREMENT(state->thread_stats, AccumulatedFloatFallbackCount, 1);
    extFloat80_t* reg = (extFloat80_t*)&state->ctx.st[state->ctx.fpu_top];
    checkReg(state, reg);
    extF80M_roundToInt(reg, softfloat_getRoundingMode(), false, reg);
}

void felix86_x87_FCOM(ThreadState* state, extFloat80_t* rhs /* flipped */, extFloat80_t* lhs, int size) {
    FELIX86_PROFILE_INSTANT_INCREMENT(state->thread_stats, AccumulatedFloatFallbackCount, 1);
    checkReg(state, lhs);
    extFloat80_t rhsstorage;
    if (size != 0) {
        if (size == 64) {
            float64_t f64;
            bool read = safe_memcpy(&f64, rhs, sizeof(float64_t));
            if (!read) {
                return;
            }
            f64_to_extF80M(f64, &rhsstorage);
        } else if (size == 32) {
            float32_t f32;
            bool read = safe_memcpy(&f32, rhs, sizeof(float32_t));
            if (!read) {
                return;
            }
            f32_to_extF80M(f32, &rhsstorage);
        } else {
            UNREACHABLE();
        }
        rhs = &rhsstorage;
    } else {
        checkReg(state, rhs);
    }
    bool c0, c2, c3;
    bool nan = isNaN80(lhs) || isNaN80(rhs);
    bool eq = extF80M_eq(lhs, rhs);
    bool lt = extF80M_lt(lhs, rhs);
    if (nan) {
        c3 = 1;
        c2 = 1;
        c0 = 1;
    } else if (eq) {
        c3 = 1;
        c2 = 0;
        c0 = 0;
    } else if (lt) {
        c3 = 0;
        c2 = 0;
        c0 = 1;
    } else {
        c3 = 0;
        c2 = 0;
        c0 = 0;
    }
    state->ctx.fpu_sw &= ~(C0_BIT | C1_BIT | C2_BIT | C3_BIT);
    state->ctx.fpu_sw |= c0 ? C0_BIT : 0;
    state->ctx.fpu_sw |= c2 ? C2_BIT : 0;
    state->ctx.fpu_sw |= c3 ? C3_BIT : 0;
}

void felix86_x87_FCOMP(ThreadState* state, extFloat80_t* lhs, extFloat80_t* rhs, int size) {
    FELIX86_PROFILE_INSTANT_INCREMENT(state->thread_stats, AccumulatedFloatFallbackCount, 1);
    felix86_x87_FCOM(state, lhs, rhs, size);
    pop(state);
}

void felix86_x87_FCOMPP(ThreadState* state, extFloat80_t* st0, extFloat80_t* sti, int size) {
    FELIX86_PROFILE_INSTANT_INCREMENT(state->thread_stats, AccumulatedFloatFallbackCount, 1);
    felix86_x87_FCOM(state, sti, st0, 0);
    ASSERT(size == 0);
    pop(state);
    pop(state);
}

void felix86_x87_FUCOM(ThreadState* state, extFloat80_t* lhs, extFloat80_t* rhs, int size) {
    FELIX86_PROFILE_INSTANT_INCREMENT(state->thread_stats, AccumulatedFloatFallbackCount, 1);
    felix86_x87_FCOM(state, lhs, rhs, size);
    ASSERT(size == 0);
}

void felix86_x87_FUCOMP(ThreadState* state, extFloat80_t* st0, extFloat80_t* sti, int size) {
    FELIX86_PROFILE_INSTANT_INCREMENT(state->thread_stats, AccumulatedFloatFallbackCount, 1);
    felix86_x87_FCOM(state, sti, st0, 0);
    ASSERT(size == 0);
    pop(state);
}

void felix86_x87_FUCOMPP(ThreadState* state, extFloat80_t* st0, extFloat80_t* sti, int size) {
    FELIX86_PROFILE_INSTANT_INCREMENT(state->thread_stats, AccumulatedFloatFallbackCount, 1);
    felix86_x87_FCOM(state, sti, st0, 0);
    ASSERT(size == 0);
    pop(state);
    pop(state);
}

void felix86_x87_FUCOMI(ThreadState* state, extFloat80_t* lhs, extFloat80_t* rhs, int) {
    FELIX86_PROFILE_INSTANT_INCREMENT(state->thread_stats, AccumulatedFloatFallbackCount, 1);
    checkReg(state, lhs);
    checkReg(state, rhs);
    bool zf, pf, cf;
    bool unordered = isNaN80(lhs) || isNaN80(rhs);
    bool eq = extF80M_eq(lhs, rhs);
    bool lt = extF80M_lt(lhs, rhs);
    if (unordered) {
        zf = 1;
        pf = 1;
        cf = 1;
    } else if (eq) {
        zf = 1;
        pf = 0;
        cf = 0;
    } else if (lt) {
        zf = 0;
        pf = 0;
        cf = 1;
    } else {
        zf = 0;
        pf = 0;
        cf = 0;
    }
    state->ctx.zf = zf;
    state->ctx.pf = pf;
    state->ctx.cf = cf;
}

void felix86_x87_FUCOMIP(ThreadState* state, extFloat80_t* lhs, extFloat80_t* rhs, int) {
    FELIX86_PROFILE_INSTANT_INCREMENT(state->thread_stats, AccumulatedFloatFallbackCount, 1);
    felix86_x87_FUCOMI(state, lhs, rhs, 0);
    pop(state);
}

void felix86_x87_FCOMI(ThreadState* state, extFloat80_t* lhs, extFloat80_t* rhs, int) {
    FELIX86_PROFILE_INSTANT_INCREMENT(state->thread_stats, AccumulatedFloatFallbackCount, 1);
    felix86_x87_FUCOMI(state, lhs, rhs, 0);
}

void felix86_x87_FCOMIP(ThreadState* state, extFloat80_t* lhs, extFloat80_t* rhs, int) {
    FELIX86_PROFILE_INSTANT_INCREMENT(state->thread_stats, AccumulatedFloatFallbackCount, 1);
    felix86_x87_FUCOMI(state, lhs, rhs, 0);
    pop(state);
}

[[nodiscard]] bool FICOM_common(ThreadState* state, extFloat80_t* mem, int size) {
    extFloat80_t* lhs = (extFloat80_t*)&state->ctx.st[state->ctx.fpu_top];
    extFloat80_t rhsstorage;
    extFloat80_t* rhs = &rhsstorage;
    if (size == 16) {
        i16 num;
        bool read = safe_memcpy(&num, mem, sizeof(i16));
        if (!read) {
            return false;
        }
        i32_to_extF80M(num, rhs);
    } else if (size == 32) {
        i32 num;
        bool read = safe_memcpy(&num, mem, sizeof(i32));
        if (!read) {
            return false;
        }
        i32_to_extF80M(num, rhs);
    } else {
        UNREACHABLE();
    }

    checkReg(state, lhs);
    bool c0, c2, c3;
    bool nan = isNaN80(lhs) || isNaN80(rhs);
    bool eq = extF80M_eq(lhs, rhs);
    bool lt = extF80M_lt(lhs, rhs);
    if (nan) {
        c3 = 1;
        c2 = 1;
        c0 = 1;
    } else if (eq) {
        c3 = 1;
        c2 = 0;
        c0 = 0;
    } else if (lt) {
        c3 = 0;
        c2 = 0;
        c0 = 1;
    } else {
        c3 = 0;
        c2 = 0;
        c0 = 0;
    }
    state->ctx.fpu_sw &= ~(C0_BIT | C2_BIT | C3_BIT);
    state->ctx.fpu_sw |= c0 ? C0_BIT : 0;
    state->ctx.fpu_sw |= c2 ? C2_BIT : 0;
    state->ctx.fpu_sw |= c3 ? C3_BIT : 0;
    return true;
}

void felix86_x87_FICOM(ThreadState* state, extFloat80_t* mem, int size) {
    FELIX86_PROFILE_INSTANT_INCREMENT(state->thread_stats, AccumulatedFloatFallbackCount, 1);
    (void)FICOM_common(state, mem, size);
}

void felix86_x87_FICOMP(ThreadState* state, extFloat80_t* mem, int size) {
    FELIX86_PROFILE_INSTANT_INCREMENT(state->thread_stats, AccumulatedFloatFallbackCount, 1);
    bool success = FICOM_common(state, mem, size);
    if (!success) {
        return;
    }
    pop(state);
}

void felix86_x87_FPREM(ThreadState* state) {
    FELIX86_PROFILE_INSTANT_INCREMENT(state->thread_stats, AccumulatedFloatFallbackCount, 1);
    extFloat80_t* st0 = (extFloat80_t*)&state->ctx.st[state->ctx.fpu_top];
    extFloat80_t* st1 = (extFloat80_t*)&state->ctx.st[(state->ctx.fpu_top + 1) & 0b111];
    checkReg(state, st0);
    checkReg(state, st1);
    clearC2(state);

    bool st0_inf = (st0->signExp & 0x7FFF) == 0x7FFF && st0->signif == 0x8000000000000000ULL;
    bool st0_nan = (st0->signExp & 0x7FFF) == 0x7FFF && (st0->signif & 0x7FFFFFFFFFFFFFFFULL) != 0;
    bool st1_zero = (st1->signExp & 0x7FFF) == 0 && st1->signif == 0;

    if (st0_inf || st0_nan || st1_zero) {
        softfloat_raiseFlags(softfloat_flag_invalid);
        st0->signExp = 0x7FFF;
        st0->signif = 0xC000000000000000ULL;
        return;
    }

    extFloat80_t quotient, Q, product;
    extF80M_div(st0, st1, &quotient);
    extF80M_roundToInt(&quotient, softfloat_round_minMag, false, &Q);

    bool Q_zero = Q.signif == 0 && (Q.signExp & 0x7FFF) == 0;
    if (Q_zero) {
        return;
    }

    extF80M_mul(&Q, st1, &product);
    extF80M_sub(st0, &product, st0);

    i64 Qi = extF80M_to_i64(&Q, softfloat_round_minMag, false);
    state->ctx.fpu_sw &= ~(C0_BIT | C1_BIT | C2_BIT | C3_BIT);
    state->ctx.fpu_sw |= (Qi & 1) ? C1_BIT : 0;
    state->ctx.fpu_sw |= (Qi & 2) ? C3_BIT : 0;
    state->ctx.fpu_sw |= (Qi & 4) ? C0_BIT : 0;
}

void felix86_x87_FPREM1(ThreadState* state) {
    FELIX86_PROFILE_INSTANT_INCREMENT(state->thread_stats, AccumulatedFloatFallbackCount, 1);
    extFloat80_t* lhs = (extFloat80_t*)&state->ctx.st[state->ctx.fpu_top];
    extFloat80_t* rhs = (extFloat80_t*)&state->ctx.st[(state->ctx.fpu_top + 1) & 0b111];
    checkReg(state, lhs);
    checkReg(state, rhs);
    clearC2(state);
    extF80M_rem(lhs, rhs, lhs);
}

void felix86_x87_FSCALE(ThreadState* state) {
    FELIX86_PROFILE_INSTANT_INCREMENT(state->thread_stats, AccumulatedFloatFallbackCount, 1);
    extFloat80_t* st0 = (extFloat80_t*)&state->ctx.st[state->ctx.fpu_top];
    extFloat80_t* st1 = (extFloat80_t*)&state->ctx.st[(state->ctx.fpu_top + 1) & 0b111];
    checkReg(state, st0);
    checkReg(state, st1);

    extFloat80_t zero = {0, 0};
    if (extF80M_eq(st0, &zero)) {
        bool st1_pos_inf = (st1->signExp & 0x7FFF) == 0x7FFF && (st1->signExp & 0x8000) == 0 && (st1->signif & 0x7FFFFFFFFFFFFFFFULL) == 0;
        if (st1_pos_inf) {
            softfloat_raiseFlags(softfloat_flag_invalid);
            st0->signExp = 0x7FFF;
            st0->signif = 0xC000000000000000ULL;
        }
        return;
    }

    extFloat80_t truncated;
    extF80M_roundToInt(st1, softfloat_round_minMag, false, &truncated);
    float128_t f128;
    extF80M_to_f128M(&truncated, &f128);
    f128 = cephes_f128_exp2l(f128);
    extFloat80_t scale;
    f128M_to_extF80M(&f128, &scale);
    extF80M_mul(st0, &scale, st0);
}

void felix86_x87_FXTRACT(ThreadState* state) {
    FELIX86_PROFILE_INSTANT_INCREMENT(state->thread_stats, AccumulatedFloatFallbackCount, 1);
    extFloat80_t* st0 = (extFloat80_t*)&state->ctx.st[state->ctx.fpu_top];
    checkReg(state, st0);

    extFloat80_t original = *st0;
    bool is_zero = (original.signExp & 0x7FFF) == 0 && original.signif == 0;
    bool is_inf_nan = (original.signExp & 0x7FFF) == 0x7FFF;

    extFloat80_t exp_result;
    if (is_zero) {
        exp_result.signExp = 0xFFFF;
        exp_result.signif = 0x8000000000000000ULL;
    } else if (is_inf_nan) {
        if ((original.signif & 0x7FFFFFFFFFFFFFFFULL) == 0) {
            exp_result.signExp = 0x7FFF;
            exp_result.signif = 0x8000000000000000ULL;
        } else {
            exp_result = original;
        }
    } else {
        i32 true_exp = (original.signExp & 0x7FFF) - 16383;
        i32_to_extF80M(true_exp, &exp_result);
    }

    extFloat80_t sig_result;
    if (is_zero || is_inf_nan) {
        sig_result = original;
    } else {
        sig_result = original;
        sig_result.signExp = (original.signExp & 0x8000) | 0x3FFF;
    }

    *st0 = exp_result;
    push(state, &sig_result);
}

void felix86_x87_FFREEP(ThreadState* state, extFloat80_t* reg, int) {
    FELIX86_PROFILE_INSTANT_INCREMENT(state->thread_stats, AccumulatedFloatFallbackCount, 1);
    checkReg(state, reg);
    int index = ((u64)reg - (u64)&state->ctx.st[0]) / sizeof(Float80);
    pop(state);
    state->ctx.fpu_tw &= ~(1 << index);
}

void felix86_x87_FINCSTP(ThreadState* state) {
    FELIX86_PROFILE_INSTANT_INCREMENT(state->thread_stats, AccumulatedFloatFallbackCount, 1);
    state->ctx.fpu_top++;
    state->ctx.fpu_top &= 0b111;
}

void felix86_x87_FDECSTP(ThreadState* state) {
    FELIX86_PROFILE_INSTANT_INCREMENT(state->thread_stats, AccumulatedFloatFallbackCount, 1);
    state->ctx.fpu_top--;
    state->ctx.fpu_top &= 0b111;
}

void felix86_x87_FXAM(ThreadState* state) {
    FELIX86_PROFILE_INSTANT_INCREMENT(state->thread_stats, AccumulatedFloatFallbackCount, 1);
    u8 top = state->ctx.fpu_top;
    u8 tw = state->ctx.fpu_tw;
    bool empty = ((tw >> top) & 1) == 0;
    const extFloat80_t* st0 = (extFloat80_t*)&state->ctx.st[top];
    bool sign = (st0->signExp >> 15) & 1;
    u16 exponent = st0->signExp & 0x7FFF;
    u64 signif = st0->signif;
    bool integer_bit = (signif >> 63) & 1;
    u64 fraction = signif & 0x7FFFFFFFFFFFFFFFULL;
    bool c3 = 0, c2 = 0, c0 = 0;
    if (empty) {
        c3 = 1;
        c2 = 0;
        c0 = 1;
    } else if (exponent == 0x7FFF) {
        if (integer_bit) {
            if (fraction == 0) {
                // Infinity
                c3 = 0;
                c2 = 1;
                c0 = 1;
            } else {
                // NaN
                c3 = 0;
                c2 = 0;
                c0 = 1;
            }
        } else {
            // TODO: verify this
            // Unsupported
            c3 = 0;
            c2 = 0;
            c0 = 0;
        }
    } else if (exponent == 0) {
        if (!integer_bit && !fraction) {
            // Zero
            c3 = 1;
            c2 = 0;
            c0 = 0;
        } else {
            // Subnormal
            c3 = 1;
            c2 = 1;
            c0 = 0;
        }
    } else {
        if (!integer_bit) {
            // Unnormal
            c3 = 0;
            c2 = 0;
            c0 = 0;
        } else {
            // Normal
            c3 = 0;
            c2 = 1;
            c0 = 0;
        }
    }

    state->ctx.fpu_sw &= ~(C0_BIT | C1_BIT | C2_BIT | C3_BIT);
    state->ctx.fpu_sw |= sign ? C1_BIT : 0;
    state->ctx.fpu_sw |= c0 ? C0_BIT : 0;
    state->ctx.fpu_sw |= c2 ? C2_BIT : 0;
    state->ctx.fpu_sw |= c3 ? C3_BIT : 0;
}

void felix86_x87_FBLD(ThreadState* state, extFloat80_t* mem, int) {
    FELIX86_PROFILE_INSTANT_INCREMENT(state->thread_stats, AccumulatedFloatFallbackCount, 1);
    extFloat80_t buffer;
    bool copied = safe_memcpy(&buffer, mem, sizeof(extFloat80_t));
    if (!copied) {
        return;
    }

    bool sign = buffer.signExp & 0x8000;
    i64 val = 0;
    for (int i = 0; i < 9; i++) {
        val *= 100;
        u8 byte;
        memcpy(&byte, (u8*)&buffer + (8 - i), 1);
        val += (byte >> 4) * 10;
        val += byte & 0xF;
    }
    if (sign) {
        val = -val;
    }
    extFloat80_t result;
    i64_to_extF80M(val, &result);
    push(state, &result);
}

void felix86_x87_FBSTP(ThreadState* state, extFloat80_t* mem, int) {
    FELIX86_PROFILE_INSTANT_INCREMENT(state->thread_stats, AccumulatedFloatFallbackCount, 1);
    extFloat80_t buffer;
    extFloat80_t* st0 = (extFloat80_t*)&state->ctx.st[state->ctx.fpu_top];
    extFloat80_t rounded;
    extF80M_roundToInt(st0, softfloat_getRoundingMode(), false, &rounded);
    bool sign = rounded.signExp & 0x8000;
    rounded.signExp &= ~0x8000;
    u8* result = (u8*)&buffer;
    u64 value = extF80M_to_i64(&rounded, softfloat_getRoundingMode(), false);
    for (int i = 0; i < 9; i++) {
        u8 byte = value % 100;
        u8 high = byte / 10;
        u8 low = byte % 10;
        result[i] = (high << 4) | low;
        value /= 100;
    }
    result[9] = sign ? 0x80 : 0x00;
    bool copied = safe_memcpy(mem, &buffer, sizeof(extFloat80_t));
    if (!copied) {
        return;
    }
    pop(state);
}
