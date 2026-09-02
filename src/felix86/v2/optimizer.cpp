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
