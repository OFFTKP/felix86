#include "biscuit/assembler.hpp"
#include "felix86/v2/optimizer.hpp"

using namespace biscuit;

static bool is_compressed(u32 instr) {
    return (instr & 0x3) != 0x3;
}

static int instr_size(u32 instr) {
    return is_compressed(instr) ? 2 : 4;
}

static bool is_branch_or_memory(u32 instr) {
    if (is_compressed(instr)) {
        // Check for compressed load or branch
        u32 bits = instr & 3;
        u32 funct3 = (instr >> 13) & 7;
        if (bits == 0) {
            return funct3 != 0;
        } else if (bits == 1) {
            return funct3 >= 5;
        }
        return funct3 == 4 ? (instr & 0x7c) == 0 && (instr & 0xf80) != 0 : funct3 != 0;
    }

    switch (instr & 0x7f) {
    case 0x03: // LOAD
    case 0x07: // LOAD-FP
    case 0x23: // STORE
    case 0x27: // STORE-FP
    case 0x2f: // AMO
    case 0x63: // BRANCH
    case 0x67: // JALR
    case 0x6f: // JAL
        return true;
    default:
        return false;
    }
}

void Optimizer::zydis_instruction_pass(ZydisBookkeeping& state, ZydisDecodedInstruction& instruction, ZydisDecodedOperand* operands) {
    const ZydisMnemonic mnemonic = instruction.mnemonic;
    if (mnemonic == ZYDIS_MNEMONIC_FXRSTOR || mnemonic == ZYDIS_MNEMONIC_FXRSTOR64 || mnemonic == ZYDIS_MNEMONIC_XRSTOR ||
        mnemonic == ZYDIS_MNEMONIC_XRSTOR64 || mnemonic == ZYDIS_MNEMONIC_VZEROALL) {
        state.reset();
        return;
    }

    // Check all operands instead of just the first, covers pcmpxstrm and vgather
    for (int i = 0; i < instruction.operand_count; i++) {
        const bool modified = operands[i].actions & ZYDIS_OPERAND_ACTION_MASK_WRITE;
        const bool is_reg = operands[i].type == ZYDIS_OPERAND_TYPE_REGISTER;
        const bool is_xmm = is_reg && operands[i].reg.value >= ZYDIS_REGISTER_XMM0 && operands[i].reg.value <= ZYDIS_REGISTER_XMM15;
        const bool is_ymm = is_reg && operands[i].reg.value >= ZYDIS_REGISTER_YMM0 && operands[i].reg.value <= ZYDIS_REGISTER_YMM15;
        if (modified) {
            if (is_xmm) {
                const int dst_index = operands[i].reg.value - ZYDIS_REGISTER_XMM0;
                if ((mnemonic == ZYDIS_MNEMONIC_MOVAPS || mnemonic == ZYDIS_MNEMONIC_MOVAPD || mnemonic == ZYDIS_MNEMONIC_MOVDQA ||
                     mnemonic == ZYDIS_MNEMONIC_MOVUPS || mnemonic == ZYDIS_MNEMONIC_MOVUPD || mnemonic == ZYDIS_MNEMONIC_MOVDQU) &&
                    operands[1].type == ZYDIS_OPERAND_TYPE_REGISTER) {
                    const int src_index = operands[1].reg.value - ZYDIS_REGISTER_XMM0;
                    state.add_xmm_copy(dst_index, src_index);
                } else if (mnemonic == ZYDIS_MNEMONIC_SHUFPS && operands[1].type == ZYDIS_OPERAND_TYPE_REGISTER) {
                    const int src_index = operands[1].reg.value - ZYDIS_REGISTER_XMM0;
                    const bool same_value = (state.is_xmm_copy(dst_index) && state.get_xmm_copy(dst_index) == src_index) ||
                                            (state.is_xmm_copy(src_index) && state.get_xmm_copy(src_index) == dst_index) ||
                                            (state.is_xmm_copy(dst_index) && state.is_xmm_copy(src_index) &&
                                             state.get_xmm_copy(dst_index) == state.get_xmm_copy(src_index));
                    if (same_value && src_index != dst_index) {
                        // Patterns like this is seen in some games:
                        //   movaps xmm1,xmm2
                        //   movaps xmm0,xmm2
                        //   shufps xmm0,xmm2,0x0
                        //   shufps xmm1,xmm2,0x55
                        //   shufps xmm2,xmm2,0xaa
                        // This pattern of move + shufps of moved register can become a single vgather.vi
                        // Unfortunately, the compilers would choose to emit `shufps xmm0, xmm2, 0` instead of `shufps xmm0, xmm0, 0` which
                        // would allow us to emit a vgather.vi without further analysis. Games would also not choose to emit the pshufd variant
                        // which would be one instruction instead of two because of domain crossing. For these reasons we detect it ourselves
                        // and replace it with a pshufd as we don't care about domains. Since pshufd only uses the src elements it is better
                        // for RVV translations, as the worst case will use 1 vrgather instead of 2.
                        // Replace mnemonic and operand, keep instruction length the same for translation_sizes reasons
                        // Also don't omit the copy, keeps synchronous signal state correct if a fault happens after the copy
                        instruction.mnemonic = ZYDIS_MNEMONIC_PSHUFD;
                    }
                    state.remove_xmm_copy(dst_index);
                } else {
                    state.remove_xmm_copy(dst_index);
                }
            } else if (is_ymm) {
                const int index = operands[i].reg.value - ZYDIS_REGISTER_YMM0;
                state.remove_xmm_copy(index);
            }
        }
    }
}

void Optimizer::native_pass(u8* start, u64 size) {
    static const u32 acq_fence = []() {
        u32 instr;
        Assembler as((u8*)&instr, sizeof(u32));
        as.FENCE(FenceOrder::R, FenceOrder::RW);
        return instr;
    }();
    static const u32 rel_fence = []() {
        u32 instr;
        Assembler as((u8*)&instr, sizeof(u32));
        as.FENCE(FenceOrder::RW, FenceOrder::W);
        return instr;
    }();

    u32* acq_fence_pos = nullptr;
    u64 i = 0;
    while (i < size) {
        u32* pos = (u32*)(start + i);
        const u32 instr = *pos;

        if (instr == acq_fence) {
            acq_fence_pos = pos;
        } else if (instr == rel_fence) {
            if (acq_fence_pos) {
                {
                    Assembler as((u8*)acq_fence_pos, 4);
                    as.NOP();
                }
                {
                    Assembler as((u8*)pos, 4);
                    as.FENCETSO();
                }
            }
            acq_fence_pos = nullptr;
        } else if (is_branch_or_memory(instr)) {
            acq_fence_pos = nullptr;
        }

        i += instr_size(instr);
    }
}
