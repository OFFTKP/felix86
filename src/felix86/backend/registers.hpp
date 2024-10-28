#pragma once

#include <array>
#include "biscuit/assembler.hpp"
#include "biscuit/registers.hpp"
#include "felix86/common/log.hpp"
#include "felix86/common/utility.hpp"

using namespace biscuit;

class Registers {
    constexpr static std::array total_gprs = {x1,  x6,  x7,  x8,  x10, x11, x12, x13, x14, x15, x16, x17, x18,
                                              x19, x20, x21, x22, x23, x24, x25, x26, x27, x28, x29, x30, x31};
    constexpr static std::array total_fprs = {f0,  f1,  f2,  f3,  f4,  f5,  f6,  f7,  f8,  f9,  f10, f11, f12, f13, f14, f15,
                                              f16, f17, f18, f19, f20, f21, f22, f23, f24, f25, f26, f27, f28, f29, f30, f31};
    constexpr static std::array total_vecs = {v2,  v3,  v4,  v5,  v6,  v7,  v8,  v9,  v10, v11, v12, v13, v14, v15, v16,
                                              v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31};
    constexpr static std::array saved_gprs = {ra, sp, gp, tp, s0, s1, s2, s3, s4, s5, s6, s7, s8, s9, s10, s11};

    constexpr static std::array saved_fprs = {fs0, fs1, fs2, fs3, fs4, fs5, fs6, fs7, fs8, fs9, fs10, fs11};

    constexpr static std::array caller_saved_gprs = {ra, t0, t1, t2, t3, t4, t5, t6, a0, a1, a2, a3, a4, a5, a6, a7};

    constexpr static std::array caller_saved_fprs = {ft0,  ft1,  ft2, ft3, ft4, ft5, ft6, ft7, ft8, ft9,
                                                     ft10, ft11, fa0, fa1, fa2, fa3, fa4, fa5, fa6, fa7};

public:
    constexpr static biscuit::GPR Zero() {
        return x0;
    }

    constexpr static biscuit::GPR StackPointer() {
        return x2;
    }

    constexpr static biscuit::GPR ThreadStatePointer() {
        return x9; // saved register so that when we exit VM we don't have to save it
    }

    constexpr static bool IsCallerSaved(biscuit::GPR reg) {
        for (biscuit::GPR saved : caller_saved_gprs) {
            if (reg == saved) {
                return true;
            }
        }

        return false;
    }

    constexpr static bool IsCallerSaved(biscuit::FPR reg) {
        for (biscuit::FPR saved : caller_saved_fprs) {
            if (reg == saved) {
                return true;
            }
        }

        return false;
    }

    constexpr static const auto& GetCallerSavedGPRs() {
        return caller_saved_gprs;
    }

    constexpr static const auto& GetCallerSavedFPRs() {
        return caller_saved_fprs;
    }

    constexpr static const auto& GetSavedGPRs() {
        return saved_gprs;
    }

    constexpr static const auto& GetSavedFPRs() {
        return saved_fprs;
    }

    constexpr static const auto& GetAllocatableGPRs() {
        return total_gprs;
    }

    constexpr static const auto& GetAllocatableFPRs() {
        return total_fprs;
    }

    constexpr static const auto& GetAllocatableVecs() {
        return total_vecs;
    }

    static u8 GetGPRIndex(biscuit::GPR reg) {
        for (size_t i = 0; i < total_gprs.size(); i++) {
            if (total_gprs[i] == reg) {
                return i;
            }
        }

        UNREACHABLE();
        return 0;
    }
};

static_assert(Registers::IsCallerSaved(t0));
static_assert(!Registers::IsCallerSaved(s0));
static_assert(Registers::IsCallerSaved(ft0));
static_assert(!Registers::IsCallerSaved(fs0));