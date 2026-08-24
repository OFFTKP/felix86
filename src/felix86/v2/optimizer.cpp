#include "biscuit/assembler.hpp"
#include "felix86/common/decoder.h"
#include "felix86/v2/optimizer.hpp"

using namespace biscuit;

static bool is_compressed(u32 instr) {
    return (instr & 0x3) != 0x3;
}

static int instr_size(u32 instr) {
    return is_compressed(instr) ? 2 : 4;
}

static bool is_branch(RiscvMnemonic m) {
    switch (m) {
    case RISCV_BEQ:
    case RISCV_BNE:
    case RISCV_BLT:
    case RISCV_BGE:
    case RISCV_BLTU:
    case RISCV_BGEU:
    case RISCV_BEQI:
    case RISCV_BNEI:
    case RISCV_JAL:
    case RISCV_JALR:
    case RISCV_C_J:
    case RISCV_C_JAL:
    case RISCV_C_JR:
    case RISCV_C_JALR:
    case RISCV_C_BEQZ:
    case RISCV_C_BNEZ:
        return true;
    default:
        return false;
    }
}

static bool is_memory_access(RiscvMnemonic m) {
    switch (m) {
    case RISCV_LB:
    case RISCV_LBU:
    case RISCV_LB_AQ:
    case RISCV_LH:
    case RISCV_LHU:
    case RISCV_LH_AQ:
    case RISCV_LW:
    case RISCV_LWU:
    case RISCV_LW_AQ:
    case RISCV_LD:
    case RISCV_LD_AQ:
    case RISCV_SB:
    case RISCV_SB_RL:
    case RISCV_SH:
    case RISCV_SH_RL:
    case RISCV_SW:
    case RISCV_SW_RL:
    case RISCV_SD:
    case RISCV_SD_RL:
    case RISCV_C_LW:
    case RISCV_C_LWSP:
    case RISCV_C_LD:
    case RISCV_C_LDSP:
    case RISCV_C_LBU:
    case RISCV_C_LH:
    case RISCV_C_LHU:
    case RISCV_C_SW:
    case RISCV_C_SWSP:
    case RISCV_C_SD:
    case RISCV_C_SDSP:
    case RISCV_C_SB:
    case RISCV_C_SH:
    case RISCV_C_FLD:
    case RISCV_C_FLDSP:
    case RISCV_C_FLW:
    case RISCV_C_FLWSP:
    case RISCV_C_FSD:
    case RISCV_C_FSDSP:
    case RISCV_C_FSW:
    case RISCV_C_FSWSP:
    case RISCV_FLW:
    case RISCV_FLD:
    case RISCV_FLH:
    case RISCV_FLQ:
    case RISCV_FSW:
    case RISCV_FSD:
    case RISCV_FSH:
    case RISCV_FSQ:
    case RISCV_LR_B:
    case RISCV_LR_H:
    case RISCV_LR_W:
    case RISCV_LR_D:
    case RISCV_SC_B:
    case RISCV_SC_H:
    case RISCV_SC_W:
    case RISCV_SC_D:
    case RISCV_AMOADD_B:
    case RISCV_AMOADD_H:
    case RISCV_AMOADD_W:
    case RISCV_AMOADD_D:
    case RISCV_AMOSWAP_B:
    case RISCV_AMOSWAP_H:
    case RISCV_AMOSWAP_W:
    case RISCV_AMOSWAP_D:
    case RISCV_AMOOR_B:
    case RISCV_AMOOR_H:
    case RISCV_AMOOR_W:
    case RISCV_AMOOR_D:
    case RISCV_AMOAND_B:
    case RISCV_AMOAND_H:
    case RISCV_AMOAND_W:
    case RISCV_AMOAND_D:
    case RISCV_AMOXOR_B:
    case RISCV_AMOXOR_H:
    case RISCV_AMOXOR_W:
    case RISCV_AMOXOR_D:
    case RISCV_AMOMIN_B:
    case RISCV_AMOMIN_H:
    case RISCV_AMOMIN_W:
    case RISCV_AMOMIN_D:
    case RISCV_AMOMAX_B:
    case RISCV_AMOMAX_H:
    case RISCV_AMOMAX_W:
    case RISCV_AMOMAX_D:
    case RISCV_AMOMINU_B:
    case RISCV_AMOMINU_H:
    case RISCV_AMOMINU_W:
    case RISCV_AMOMINU_D:
    case RISCV_AMOMAXU_B:
    case RISCV_AMOMAXU_H:
    case RISCV_AMOMAXU_W:
    case RISCV_AMOMAXU_D:
    case RISCV_AMOCAS_B:
    case RISCV_AMOCAS_H:
    case RISCV_AMOCAS_W:
    case RISCV_AMOCAS_D:
    case RISCV_AMOCAS_Q:
    case RISCV_VSE8_V:
    case RISCV_VSE16_V:
    case RISCV_VSE32_V:
    case RISCV_VSE64_V:
    case RISCV_VLE8_V:
    case RISCV_VLE16_V:
    case RISCV_VLE32_V:
    case RISCV_VLE64_V:
    case RISCV_VLE8FF_V:
    case RISCV_VLE16FF_V:
    case RISCV_VLE32FF_V:
    case RISCV_VLE64FF_V:
    case RISCV_VLUXEI8_V:
    case RISCV_VLUXEI16_V:
    case RISCV_VLUXEI32_V:
    case RISCV_VLUXEI64_V:
    case RISCV_VLOXEI8_V:
    case RISCV_VLOXEI16_V:
    case RISCV_VLOXEI32_V:
    case RISCV_VLOXEI64_V:
    case RISCV_VSUXEI8_V:
    case RISCV_VSUXEI16_V:
    case RISCV_VSUXEI32_V:
    case RISCV_VSUXEI64_V:
    case RISCV_VSOXEI8_V:
    case RISCV_VSOXEI16_V:
    case RISCV_VSOXEI32_V:
    case RISCV_VSOXEI64_V:
    case RISCV_VLSE8_V:
    case RISCV_VLSE16_V:
    case RISCV_VLSE32_V:
    case RISCV_VLSE64_V:
    case RISCV_VSSE8_V:
    case RISCV_VSSE16_V:
    case RISCV_VSSE32_V:
    case RISCV_VSSE64_V:
    case RISCV_VLM_V:
    case RISCV_VSM_V:
    case RISCV_VL1RE8_V:
    case RISCV_VL1RE16_V:
    case RISCV_VL1RE32_V:
    case RISCV_VL1RE64_V:
    case RISCV_VL2RE8_V:
    case RISCV_VL2RE16_V:
    case RISCV_VL2RE32_V:
    case RISCV_VL2RE64_V:
    case RISCV_VL4RE8_V:
    case RISCV_VL4RE16_V:
    case RISCV_VL4RE32_V:
    case RISCV_VL4RE64_V:
    case RISCV_VL8RE8_V:
    case RISCV_VL8RE16_V:
    case RISCV_VL8RE32_V:
    case RISCV_VL8RE64_V:
    case RISCV_VS1R_V:
    case RISCV_VS2R_V:
    case RISCV_VS4R_V:
    case RISCV_VS8R_V:
        return true;
    default:
        return false;
    }
}

void Optimizer::native_pass(u8* start, u64 size) {
    u32 acq_fence, rel_fence;
    {
        Assembler as((u8*)&acq_fence, 4);
        as.FENCE(FenceOrder::R, FenceOrder::RW);
    }
    {
        Assembler as((u8*)&rel_fence, 4);
        as.FENCE(FenceOrder::RW, FenceOrder::W);
    }

    u32* acq_fence_pos = nullptr;
    u64 i = 0;
    while (i < size) {
        u32* pos = (u32*)(start + i);
        const u32 instr = *pos;
        RiscvMnemonic mnemonic = riscv_get_mnemonic(instr);

        if (is_branch(mnemonic)) {
            // We would need to have branch targets invalidate our fence positions, skip for now
            return;
        }

        if (mnemonic == RISCV_FENCE) {
            u32* prior_acq = acq_fence_pos;
            acq_fence_pos = nullptr;
            if (instr == acq_fence) {
                acq_fence_pos = pos;
            } else if (instr == rel_fence && prior_acq) {
                {
                    Assembler as((u8*)prior_acq, 4);
                    as.NOP();
                }
                {
                    Assembler as((u8*)pos, 4);
                    as.FENCETSO();
                }
            }
        } else if (is_memory_access(mnemonic)) {
            acq_fence_pos = nullptr;
        }

        i += instr_size(instr);
    }
}
