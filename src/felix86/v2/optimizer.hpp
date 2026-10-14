#pragma once

#include "Zydis/DecoderTypes.h"
#include "felix86/common/log.hpp"
#include "felix86/common/types.hpp"

struct ZydisBookkeeping {

    ZydisBookkeeping() {
        reset();
    }
    void reset() {
        for (auto& copy : xmm_copy) {
            copy = NOT_A_COPY;
        }
    }

    bool is_xmm_copy(u8 index) const {
        return xmm_copy[index] != NOT_A_COPY;
    }

    u8 get_xmm_copy(u8 index) const {
        ASSERT(is_xmm_copy(index));
        return xmm_copy[index];
    }

    void remove_xmm_copy(u8 index) {
        ASSERT(index <= 15);
        xmm_copy[index] = NOT_A_COPY;
        for (auto& copy : xmm_copy) {
            if (copy == index) {
                copy = NOT_A_COPY;
            }
        }
    }

    void add_xmm_copy(u8 index, u8 copy) {
        remove_xmm_copy(index);
        if (index != copy) {
            xmm_copy[index] = copy;
        }
    }

private:
    constexpr static u8 NOT_A_COPY = 0xff;
    u8 xmm_copy[16];
};

namespace Optimizer {
void zydis_instruction_pass(ZydisBookkeeping& state, ZydisDecodedInstruction& instruction, ZydisDecodedOperand* operands);
void native_pass(u8* start, u64 size);
} // namespace Optimizer
