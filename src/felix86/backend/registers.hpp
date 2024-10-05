#pragma once

#include <span>
#include "biscuit/registers.hpp"
#include "felix86/common/log.hpp"
#include "felix86/common/utility.hpp"

using namespace biscuit;

class Registers {
    static constexpr std::array total_gprs = {x2,  x3,  x4,  x5,  x6,  x7,  x8,  x9,  x10, x11, x12, x13, x14, x15, x16,
                                              x17, x18, x19, x20, x21, x22, x23, x24, x25, x26, x27, x28, x29, x30, x31};
    static constexpr std::array total_fprs = {f0,  f1,  f2,  f3,  f4,  f5,  f6,  f7,  f8,  f9,  f10, f11, f12, f13, f14, f15,
                                              f16, f17, f18, f19, f20, f21, f22, f23, f24, f25, f26, f27, f28, f29, f30, f31};
    static constexpr std::array total_vecs = {v0,  v1,  v2,  v3,  v4,  v5,  v6,  v7,  v8,  v9,  v10, v11, v12, v13, v14, v15,
                                              v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31};

public:
    biscuit::GPR AcquireScratchGPR() {
        if (scratch_gpr_index >= scratch_gprs.size()) {
            ERROR("Out of scratch GPRs");
        }

        return scratch_gprs[scratch_gpr_index++];
    }
    void ReleaseScratchRegs() {
        scratch_gpr_index = 0;
        scratch_fpr_index = 0;
        scratch_vec_index = 0;
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

    static biscuit::GPR SpillPointer() {
        return x1;
    }

    static constexpr u32 ScratchGPRCount = 2;
    static constexpr u32 ScratchFPRCount = 0;
    static constexpr u32 ScratchVecCount = 0;
    static constexpr u32 AvailableGPRCount = total_gprs.size() - ScratchGPRCount;
    static constexpr u32 AvailableFPRCount = total_fprs.size() - ScratchFPRCount;
    static constexpr u32 AvailableVecCount = total_vecs.size() - ScratchVecCount;

private:
    static constexpr std::span<const biscuit::GPR> available_gprs = {total_gprs.begin(), AvailableGPRCount};
    static constexpr std::span<const biscuit::GPR> scratch_gprs = {total_gprs.begin() + AvailableGPRCount, total_gprs.size() - AvailableGPRCount};
    static_assert(scratch_gprs.size() + available_gprs.size() == total_gprs.size());

    static constexpr std::span<const biscuit::FPR> available_fprs = {total_fprs.begin(), AvailableFPRCount};
    static constexpr std::span<const biscuit::FPR> scratch_fprs = {total_fprs.begin() + AvailableFPRCount, total_fprs.size() - AvailableFPRCount};
    static_assert(scratch_fprs.size() + available_fprs.size() == total_fprs.size());

    static constexpr std::span<const biscuit::Vec> available_vecs = {total_vecs.begin(), AvailableVecCount};
    static constexpr std::span<const biscuit::Vec> scratch_vecs = {total_vecs.begin() + AvailableVecCount, total_vecs.size() - AvailableVecCount};
    static_assert(scratch_vecs.size() + available_vecs.size() == total_vecs.size());

    u8 scratch_gpr_index = 0;
    u8 scratch_fpr_index = 0;
    u8 scratch_vec_index = 0;
};