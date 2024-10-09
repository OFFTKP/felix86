#pragma once

#include <span>
#include "biscuit/assembler.hpp"
#include "biscuit/registers.hpp"
#include "felix86/common/log.hpp"
#include "felix86/common/utility.hpp"

using namespace biscuit;

class Registers {
    constexpr static std::array total_gprs = {x1,  x5,  x6,  x7,  x9,  x10, x11, x12, x13, x14, x15, x16, x17, x18,
                                              x19, x20, x21, x22, x23, x24, x25, x26, x27, x28, x29, x30, x31};
    constexpr static std::array total_fprs = {f0,  f1,  f2,  f3,  f4,  f5,  f6,  f7,  f8,  f9,  f10, f11, f12, f13, f14, f15,
                                              f16, f17, f18, f19, f20, f21, f22, f23, f24, f25, f26, f27, f28, f29, f30, f31};
    constexpr static std::array total_vecs = {v0,  v1,  v2,  v3,  v4,  v5,  v6,  v7,  v8,  v9,  v10, v11, v12, v13, v14, v15,
                                              v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31};
    constexpr static std::array saved_gprs = {ra, sp, gp, tp, s0, s1, s2, s3, s4, s5, s6, s7, s8, s9, s10, s11};

    constexpr static std::array saved_fprs = {fs0, fs1, fs2, fs3, fs4, fs5, fs6, fs7, fs8, fs9, fs10, fs11};

public:
    biscuit::GPR AcquireScratchGPR() {
        if (scratch_gpr_index >= scratch_gprs.size()) {
            ERROR("Out of scratch GPRs");
        }

        return scratch_gprs[scratch_gpr_index++];
    }

    biscuit::GPR AcquireScratchGPRFromSpill(Assembler& as, u64 spill_location) {
        if (scratch_gpr_index > 0) {
            for (int i = 0; i < scratch_gpr_index; i++) {
                // Same spill location, return the same register
                if (scratch_spill_locations[i] == spill_location) {
                    return scratch_gprs[i];
                }
            }
        }

        biscuit::GPR reg = AcquireScratchGPR();
        if (spill_location > 2047) {
            ERROR("Spill location too large");
        }
        as.LD(reg, spill_location, Registers::SpillPointer());
        scratch_spill_locations[scratch_gpr_index - 1] = spill_location;
        return reg;
    }

    void ReleaseScratchRegs() {
        scratch_gpr_index = 0;
        scratch_fpr_index = 0;
        scratch_vec_index = 0;
        std::fill(scratch_spill_locations.begin(), scratch_spill_locations.end(), -1ull);
    }

    std::span<const biscuit::GPR> GetAvailableGPRs() const {
        return available_gprs;
    }
    std::span<const biscuit::FPR> GetAvailableFPRs() const {
        return available_fprs;
    }
    std::span<const biscuit::Vec> GetAvailableVecs() const {
        return available_vecs;
    }

    static biscuit::GPR Zero() {
        return x0;
    }

    // Pointer to the spill location, holding spilled registers
    static biscuit::GPR SpillPointer() {
        return x8; // saved register so that when we exit VM we don't have to save it
    }

    static biscuit::GPR ThreadStatePointer() {
        return x9; // saved register so that when we exit VM we don't have to save it
    }

    constexpr static bool IsCallerSaved(biscuit::GPR reg) {
        for (biscuit::GPR saved : saved_gprs) {
            if (reg == saved) {
                return false;
            }
        }

        return true;
    }

    constexpr static bool IsCallerSaved(biscuit::FPR reg) {
        for (biscuit::FPR saved : saved_fprs) {
            if (reg == saved) {
                return false;
            }
        }

        return true;
    }

    constexpr static auto GetSavedGPRs() {
        return saved_gprs;
    }

    constexpr static auto GetSavedFPRs() {
        return saved_fprs;
    }

    constexpr static u32 ScratchGPRCount = 3;
    constexpr static u32 ScratchFPRCount = 0;
    constexpr static u32 ScratchVecCount = 0;
    constexpr static u32 AvailableGPRCount = total_gprs.size() - ScratchGPRCount;
    constexpr static u32 AvailableFPRCount = total_fprs.size() - ScratchFPRCount;
    constexpr static u32 AvailableVecCount = total_vecs.size() - ScratchVecCount;

private:
    constexpr static std::span<const biscuit::GPR> available_gprs = {total_gprs.begin(), AvailableGPRCount};
    constexpr static std::span<const biscuit::GPR> scratch_gprs = {total_gprs.begin() + AvailableGPRCount, total_gprs.size() - AvailableGPRCount};
    std::array<u64, scratch_gprs.size()> scratch_spill_locations{};
    static_assert(scratch_gprs.size() + available_gprs.size() == total_gprs.size());

    constexpr static std::span<const biscuit::FPR> available_fprs = {total_fprs.begin(), AvailableFPRCount};
    constexpr static std::span<const biscuit::FPR> scratch_fprs = {total_fprs.begin() + AvailableFPRCount, total_fprs.size() - AvailableFPRCount};
    static_assert(scratch_fprs.size() + available_fprs.size() == total_fprs.size());

    constexpr static std::span<const biscuit::Vec> available_vecs = {total_vecs.begin(), AvailableVecCount};
    constexpr static std::span<const biscuit::Vec> scratch_vecs = {total_vecs.begin() + AvailableVecCount, total_vecs.size() - AvailableVecCount};
    static_assert(scratch_vecs.size() + available_vecs.size() == total_vecs.size());

    u8 scratch_gpr_index = 0;
    u8 scratch_fpr_index = 0;
    u8 scratch_vec_index = 0;
};

static_assert(Registers::IsCallerSaved(t0));
static_assert(!Registers::IsCallerSaved(s0));
static_assert(Registers::IsCallerSaved(ft0));
static_assert(!Registers::IsCallerSaved(fs0));