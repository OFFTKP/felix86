#include <Zydis/Zydis.h>
#include "felix86/common/types.hpp"
#include "felix86/common/utility.hpp"
#include "felix86/emulator.hpp"
#include "felix86/hle/thunks.hpp"
#include "felix86/v2/handlers.hpp"
#include "felix86/v2/recompiler.hpp"

void felix86_syscall(felix86_frame* frame);

void felix86_syscall32(felix86_frame* frame, u32 rip_nex);

void felix86_cpuid(ThreadState* state);

#define FAST_HANDLE(name)                                                                                                                            \
    void fast_##name(Recompiler& rec, u64 rip, Assembler& as, ZydisDecodedInstruction& instruction, ZydisDecodedOperand* operands)

#define HAS_VEX (instruction.attributes & (ZYDIS_ATTRIB_HAS_VEX))

#define HAS_REP (instruction.attributes & (ZYDIS_ATTRIB_HAS_REP | ZYDIS_ATTRIB_HAS_REPZ | ZYDIS_ATTRIB_HAS_REPNZ))

void UnimplementedHandler(Recompiler& rec, u64 rip, biscuit::Assembler& as, ZydisDecodedInstruction& instruction, ZydisDecodedOperand* operands) {
    ZydisDisassembledInstruction disassembled;
    auto mode = g_mode32 ? ZYDIS_MACHINE_MODE_LONG_COMPAT_32 : ZYDIS_MACHINE_MODE_LONG_64;
    if (ZYAN_SUCCESS(ZydisDisassembleIntel(mode, rip, (u8*)rip, 15, &disassembled))) {
        ERROR("Unhandled instruction %s (%02x)", disassembled.text, (int)instruction.opcode);
    } else {
        ERROR("Unhandled instruction %s (%02x)", ZydisMnemonicGetString(instruction.mnemonic), (int)instruction.opcode);
    }
}

#define X(name) HandlerPtr Handlers::ptr_##name = UnimplementedHandler;
#include "mnemonics.inc"
#undef X

void SetCmpFlags(u64 rip, Recompiler& rec, Assembler& as, biscuit::GPR dst, biscuit::GPR src, biscuit::GPR result, x86_size_e size,
                 bool zext_src = false, bool always_emit = false) {
    if (always_emit || rec.shouldEmitFlag(rip, X86_REF_CF)) {
        biscuit::GPR test = rec.scratch();
        if (zext_src) {
            rec.zext(test, src, size);
        } else {
            test = src;
        }
        rec.updateCarrySub(dst, test);
        rec.popScratch();
    }

    if (always_emit || rec.shouldEmitFlag(rip, X86_REF_PF)) {
        rec.updateParity(result);
    }

    if (always_emit || rec.shouldEmitFlag(rip, X86_REF_AF)) {
        rec.updateAuxiliarySub(dst, src);
    }

    if (always_emit || rec.shouldEmitFlag(rip, X86_REF_ZF)) {
        rec.updateZero(result, size);
    }

    if (always_emit || rec.shouldEmitFlag(rip, X86_REF_SF)) {
        rec.updateSign(result, size);
    }

    if (always_emit || rec.shouldEmitFlag(rip, X86_REF_OF)) {
        rec.updateOverflowSub(dst, src, result, size);
    }
}

bool is_segment(ZydisDecodedOperand& operand) {
    if (operand.type != ZYDIS_OPERAND_TYPE_REGISTER) {
        return false;
    }

    if (operand.reg.value >= ZYDIS_REGISTER_ES && operand.reg.value <= ZYDIS_REGISTER_GS) {
        return true;
    }

    return false;
}

int size_to_bytes(int size) {
    switch (size) {
    case 8: {
        return 1;
    }
    case 16: {
        return 2;
    }
    case 32: {
        return 4;
    }
    case 64: {
        return 8;
    }
    }

    UNREACHABLE();
    return 0;
}

enum CmpPredicate {
    EQ_OQ = 0x00,
    LT_OS = 0x01,
    LE_OS = 0x02,
    UNORD_Q = 0x03,
    NEQ_UQ = 0x04,
    NLT_US = 0x05,
    NLE_US = 0x06,
    ORD_Q = 0x07,
    EQ_UQ = 0x08,
    NGE_US = 0x09,
    NGT_US = 0x0A,
    FALSE_OQ = 0x0B,
    NEQ_OQ = 0x0C,
    GE_OS = 0x0D,
    GT_OS = 0x0E,
    TRUE_UQ = 0x0F,
    EQ_OS = 0x10,
    LT_OQ = 0x11,
    LE_OQ = 0x12,
    UNORD_S = 0x13,
    NEQ_US = 0x14,
    NLT_UQ = 0x15,
    NLE_UQ = 0x16,
    ORD_S = 0x17,
    EQ_US = 0x18,
    NGE_UQ = 0x19,
    NGT_UQ = 0x1A,
    FALSE_OS = 0x1B,
    NEQ_OS = 0x1C,
    GE_OQ = 0x1D,
    GT_OQ = 0x1E,
    TRUE_US = 0x1F,
};

void OP_noflags_destreg(Recompiler& rec, u64 rip, Assembler& as, ZydisDecodedInstruction& instruction, ZydisDecodedOperand* operands,
                        void (Assembler::*func64)(biscuit::GPR, biscuit::GPR, biscuit::GPR),
                        void (Assembler::*func32)(biscuit::GPR, biscuit::GPR, biscuit::GPR)) {
    biscuit::GPR dst = rec.getGPR(&operands[0], X86_SIZE_QWORD);
    biscuit::GPR src;
    if (operands[1].type == ZYDIS_OPERAND_TYPE_REGISTER) {
        src = rec.getGPR(&operands[1], X86_SIZE_QWORD);
    } else {
        src = rec.getGPR(&operands[1]);
    }

    switch (instruction.operand_width) {
    case 8: {
        // https://news.ycombinator.com/item?id=41364904 :)
        bool dst_high = rec.zydisToSize(operands[0].reg.value) == X86_SIZE_BYTE_HIGH;
        bool src_high = rec.zydisToSize(operands[1].reg.value) == X86_SIZE_BYTE_HIGH;
        biscuit::GPR temp = rec.scratch();
        if (!dst_high && !src_high) {
            as.SLLI(temp, src, 56);
            as.RORI(dst, dst, 8);
            (as.*func64)(dst, dst, temp);
            as.RORI(dst, dst, 56);
        } else if (!dst_high && src_high) {
            as.SRLI(temp, src, 8);
            as.SLLI(temp, temp, 56);
            as.RORI(dst, dst, 8);
            (as.*func64)(dst, dst, temp);
            as.RORI(dst, dst, 56);
        } else if (dst_high && !src_high) {
            as.SLLI(temp, src, 56);
            as.RORI(dst, dst, 16);
            (as.*func64)(dst, dst, temp);
            as.RORI(dst, dst, 48);
        } else if (dst_high && src_high) {
            as.SRLI(temp, src, 8);
            as.SLLI(temp, temp, 56);
            as.RORI(dst, dst, 16);
            (as.*func64)(dst, dst, temp);
            as.RORI(dst, dst, 48);
        }
        break;
    }
    case 16: {
        biscuit::GPR temp = rec.scratch();
        as.SLLI(temp, src, 48);
        as.RORI(dst, dst, 16);
        (as.*func64)(dst, dst, temp);
        as.RORI(dst, dst, 48);
        break;
    }
    case 32: {
        (as.*func32)(dst, dst, src);
        rec.zext(dst, dst, X86_SIZE_DWORD);
        break;
    }
    case 64: {
        (as.*func64)(dst, dst, src);
        break;
    }
    }

    rec.setGPR(rec.zydisToRef(operands[0].reg.value), X86_SIZE_QWORD, dst);
}

void SHIFT_noflags(Recompiler& rec, u64 rip, Assembler& as, ZydisDecodedInstruction& instruction, ZydisDecodedOperand* operands,
                   void (Assembler::*func64)(biscuit::GPR, biscuit::GPR, biscuit::GPR),
                   void (Assembler::*func32)(biscuit::GPR, biscuit::GPR, biscuit::GPR)) {
    biscuit::GPR result;
    biscuit::GPR dst;
    biscuit::GPR shift;
    x86_size_e size = rec.zydisToSize(operands[0].reg.value);
    if (operands[0].type == ZYDIS_OPERAND_TYPE_REGISTER) {
        switch (operands[0].size) {
        case 8: {
            if (func64 == &Assembler::SLL) {
                if (size == X86_SIZE_BYTE_HIGH) {
                    dst = rec.getGPR(&operands[0], X86_SIZE_QWORD);
                    biscuit::GPR dst_adjusted = rec.scratch();
                    as.SRLI(dst_adjusted, dst, 8);
                    dst = dst_adjusted;
                } else {
                    dst = rec.getGPR(&operands[0], X86_SIZE_QWORD);
                }
                result = rec.scratch();
            } else {
                dst = rec.getGPR(&operands[0]);
                result = rec.scratch();
            }
            break;
        }
        case 16: {
            if (func64 == &Assembler::SLL) {
                dst = rec.getGPR(&operands[0], X86_SIZE_QWORD);
                result = rec.scratch();
            } else {
                dst = rec.getGPR(&operands[0]);
                result = rec.scratch();
            }
            break;
        }
        case 32: {
            // Will save a zext if we get it this way
            dst = rec.getGPR(&operands[0], X86_SIZE_QWORD);
            result = dst;
            break;
        }
        case 64: {
            // Perform directly on the whole register
            dst = rec.getGPR(&operands[0]);
            result = dst;
            break;
        }
        }
    } else {
        dst = rec.getGPR(&operands[0]);
        result = dst;
    }

    if (operands[1].type == ZYDIS_OPERAND_TYPE_REGISTER) {
        ASSERT(rec.zydisToRef(operands[1].reg.value) == X86_REF_RCX);
        shift = rec.getGPR(X86_REF_RCX, X86_SIZE_QWORD);
    } else {
        shift = rec.getGPR(&operands[1]);
    }

    // The 64-bit shifts use 6 bits, the 32-bit shifts use 5 bits. Doing it this way means we don't
    // have to mask the shift amount
    if (instruction.operand_width == 64) {
        (as.*func64)(result, dst, shift);
    } else {
        (as.*func32)(result, dst, shift);
    }

    rec.setGPR(&operands[0], result);
}

FAST_HANDLE(MOV) {
    if (is_segment(operands[0])) {
        if (g_mode32) {
            biscuit::GPR src = rec.getGPR(&operands[1]);
            rec.writebackState();
            as.MV(a0, rec.threadStatePointer());
            as.MV(a1, src);
            as.LI(a2, operands[0].reg.value);
            rec.callPointer(offsetof(ThreadState, felix86_set_segment));
            rec.restoreState();
        } else {
            WARN("Setting segment register in 64-bit mode, ignoring");
        }
    } else if (is_segment(operands[1])) {
        if (g_mode32) {
            biscuit::GPR seg = rec.scratch();
            int offset = 0;
            switch (operands[1].reg.value) {
            case ZYDIS_REGISTER_CS: {
                offset = offsetof(ThreadState, cs);
                break;
            }
            case ZYDIS_REGISTER_DS: {
                offset = offsetof(ThreadState, ds);
                break;
            }
            case ZYDIS_REGISTER_SS: {
                offset = offsetof(ThreadState, ss);
                break;
            }
            case ZYDIS_REGISTER_ES: {
                offset = offsetof(ThreadState, es);
                break;
            }
            case ZYDIS_REGISTER_FS: {
                offset = offsetof(ThreadState, fs);
                break;
            }
            case ZYDIS_REGISTER_GS: {
                offset = offsetof(ThreadState, gs);
                break;
            }
            default: {
                UNREACHABLE();
                break;
            }
            }
            as.LHU(seg, offset, rec.threadStatePointer());
            rec.setGPR(&operands[0], seg);
        } else {
            WARN("Getting segment register in 64-bit mode, ignoring");
        }
    } else {
        bool reg_reg = operands[0].type == ZYDIS_OPERAND_TYPE_REGISTER && operands[1].type == ZYDIS_OPERAND_TYPE_REGISTER;
        bool not_same = rec.zydisToRef(operands[0].reg.value) != rec.zydisToRef(operands[1].reg.value);
        bool mem_reg = operands[0].type == ZYDIS_OPERAND_TYPE_MEMORY && operands[1].type == ZYDIS_OPERAND_TYPE_REGISTER;
        bool reg_mem = operands[0].type == ZYDIS_OPERAND_TYPE_REGISTER && operands[1].type == ZYDIS_OPERAND_TYPE_MEMORY;
        if (not_same && reg_reg) {
            // Save a mask by doing it this way
            biscuit::GPR src = rec.getGPR(&operands[1], X86_SIZE_QWORD);
            if (rec.zydisToSize(operands[1].reg.value) == X86_SIZE_BYTE_HIGH) {
                biscuit::GPR temp = rec.scratch();
                as.SRLI(temp, src, 8);
                src = temp;
            }
            rec.setGPR(&operands[0], src);
        } else if (mem_reg) {
            // Save a mask by doing it this way
            biscuit::GPR src = rec.getGPR(&operands[1], X86_SIZE_QWORD);
            if (rec.zydisToSize(operands[1].reg.value) == X86_SIZE_BYTE_HIGH) {
                biscuit::GPR temp = rec.scratch();
                as.SRLI(temp, src, 8);
                src = temp;
            }
            rec.setGPR(&operands[0], src);
        } else if (reg_mem) {
            switch (operands[0].size) {
            case 8:
            case 16: {
                biscuit::GPR src = rec.getGPR(&operands[1]);
                rec.setGPR(&operands[0], src);
                break;
            }
            case 32:
            case 64: {
                u64 immediate = operands[1].mem.disp.value;
                biscuit::GPR dst = rec.allocatedGPR(rec.zydisToRef(operands[0].reg.value));
                if (IsValidSigned12BitImm(immediate) && !(instruction.attributes & ZYDIS_ATTRIB_HAS_SEGMENT) &&
                    !g_config.paranoid) { // can't do this with seg+a32
                    // Remove the immediate from the operand and use it in the write memory instruction
                    // This can turn an ADDI+load into just a load if the LEA is just a register
                    ZydisDecodedOperand op = operands[1];
                    op.mem.disp.value = 0;
                    biscuit::GPR address = rec.lea(&op, false);
                    rec.readMemory(dst, address, immediate, rec.zydisToSize(operands[0].size));
                } else {
                    biscuit::GPR address = rec.lea(&operands[1], false);
                    rec.readMemory(dst, address, 0, rec.zydisToSize(operands[0].size));
                }
                break;
            }
            default: {
                UNREACHABLE();
            }
            }
        } else {
            biscuit::GPR src = rec.getGPR(&operands[1]);
            rec.setGPR(&operands[0], src);
        }
    }
}

FAST_HANDLE(ADD) {
    bool needs_cf = rec.shouldEmitFlag(rip, X86_REF_CF);
    bool needs_af = rec.shouldEmitFlag(rip, X86_REF_AF);
    bool needs_pf = rec.shouldEmitFlag(rip, X86_REF_PF);
    bool needs_zf = rec.shouldEmitFlag(rip, X86_REF_ZF);
    bool needs_sf = rec.shouldEmitFlag(rip, X86_REF_SF);
    bool needs_of = rec.shouldEmitFlag(rip, X86_REF_OF);
    bool needs_any_flag = needs_cf || needs_of || needs_pf || needs_sf || needs_zf || needs_af;
    bool dst_reg = operands[0].type == ZYDIS_OPERAND_TYPE_REGISTER;
    if (Extensions::B && g_config.noflag_opts && !needs_any_flag && dst_reg) {
        // We can do it faster if we don't need to calculate flags
        return OP_noflags_destreg(rec, rip, as, instruction, operands, &Assembler::ADD, &Assembler::ADDW);
    }

    biscuit::GPR result = rec.scratch();
    biscuit::GPR src = rec.getGPR(&operands[1]);
    biscuit::GPR dst;

    bool writeback = true;
    bool needs_atomic = operands[0].type == ZYDIS_OPERAND_TYPE_MEMORY && (instruction.attributes & ZYDIS_ATTRIB_HAS_LOCK);
    if (needs_atomic) {
        biscuit::GPR address = rec.lea(&operands[0]);
        dst = rec.scratch();
        switch (operands[0].size) {
        case 8: {
            if (Extensions::Zabha) {
                WARN("Atomic 8-bit ADD with Zabha, untested");
                as.AMOADD_B(Ordering::AQRL, dst, src, address);
            } else {
                /*
                    andi    a2, a0, -4
                    slli    a0, a0, 3
                    li      a3, 255
                    sllw    a3, a3, a0
                    sllw    a0, a1, a0
                .LBB0_1:
                    lr.w.aqrl       a1, (a2)
                    add     a4, a1, a0
                    xor     a4, a4, a1
                    and     a4, a4, a3
                    xor     a4, a4, a1
                    sc.w.rl a4, a4, (a2)
                    bnez    a4, .LBB0_1
                */
                biscuit::Label loop;
                biscuit::GPR masked_address = rec.scratch();
                biscuit::GPR mask = rec.scratch();
                biscuit::GPR src_shifted = rec.scratch();
                as.ANDI(masked_address, address, -4);
                as.SLLI(address, address, 3);
                as.LI(mask, 0xFF);
                as.SLLW(mask, mask, address);
                as.SLLW(src_shifted, src, address);

                as.Bind(&loop);
                as.LR_W(Ordering::AQRL, dst, masked_address);
                as.ADD(result, dst, src_shifted);
                as.XOR(result, result, dst);
                as.AND(result, result, mask);
                as.XOR(result, result, dst);
                as.SC_W(Ordering::AQRL, result, result, masked_address);
                as.BNEZ(result, &loop);

                as.SRLW(dst, dst, address);
                as.ANDI(dst, dst, 0xFF);

                rec.popScratch();
                rec.popScratch();
                rec.popScratch();
            }
            break;
        }
        case 16: {
            if (Extensions::Zabha) {
                WARN("Atomic 16-bit ADD with Zabha, untested");
                as.AMOADD_H(Ordering::AQRL, dst, src, address);
            } else {
                /*
                aadd(unsigned short*, unsigned short)
                    andi    a6, a0, -4
                    slli    a0, a0, 3
                    lui     a3, 16
                    addi    a3, a3, -1
                    sllw    a4, a3, a0
                    sllw    a1, a1, a0
                .LBB0_3:
                    lr.w.aqrl       a5, (a6)
                    add     a2, a5, a1
                    xor     a2, a2, a5
                    and     a2, a2, a4
                    xor     a2, a2, a5
                    sc.w.rl a2, a2, (a6)
                    bnez    a2, .LBB0_3
                    srlw    a0, a5, a0
                    and     a0, a0, a3
                */
                biscuit::Label loop, bad_alignment, end;
                biscuit::GPR masked_address = rec.scratch();
                biscuit::GPR mask = rec.scratch();
                as.LI(mask, 0b11);
                as.ANDI(masked_address, address, 0b11);
                as.BEQ(masked_address, mask, &bad_alignment);

                biscuit::GPR s_a1 = rec.scratch();
                biscuit::GPR s_a3 = mask;
                biscuit::GPR s_a2 = rec.flag(X86_REF_CF); // ran out of scratch and these get modified later
                biscuit::GPR s_a4 = rec.flag(X86_REF_SF);
                biscuit::GPR s_a5 = rec.flag(X86_REF_ZF);
                biscuit::GPR s_a6 = masked_address;
                as.ANDI(s_a6, address, -4);
                as.SLLI(address, address, 3);
                as.LI(s_a3, 0xFFFF);
                as.SLLW(s_a4, s_a3, address);
                as.SLLW(s_a1, src, address);
                as.Bind(&loop);
                as.LR_W(Ordering::AQRL, s_a5, s_a6);
                as.ADD(s_a2, s_a5, s_a1);
                as.XOR(s_a2, s_a2, s_a5);
                as.AND(s_a2, s_a2, s_a4);
                as.XOR(s_a2, s_a2, s_a5);
                as.SC_W(Ordering::AQRL, s_a2, s_a2, s_a6);
                as.BNEZ(s_a2, &loop);
                as.SRLW(dst, s_a5, address);
                as.AND(dst, dst, s_a3);

                as.J(&end);
                as.Bind(&bad_alignment);
                as.EBREAK();

                as.Bind(&end);
                rec.popScratch();
                rec.popScratch();
                rec.popScratch();
            }
            break;
        }
        case 32: {
            as.AMOADD_W(Ordering::AQRL, dst, src, address);
            break;
        }
        case 64: {
            as.AMOADD_D(Ordering::AQRL, dst, src, address);
            break;
        }
        }

        if (needs_any_flag || !g_config.noflag_opts) {
            as.ADD(result, dst, src);
        }

        writeback = false;
    } else {
        if (needs_atomic) {
            WARN("Atomic ADD with 8-bit operands encountered");
        }

        dst = rec.getGPR(&operands[0]);
        as.ADD(result, dst, src);
    }

    x86_size_e size = rec.getSize(&operands[0]);

    if (needs_cf) {
        rec.updateCarryAdd(dst, result, size);
    }

    if (needs_pf) {
        rec.updateParity(result);
    }

    if (needs_af) {
        rec.updateAuxiliaryAdd(dst, result);
    }

    if (needs_zf) {
        rec.updateZero(result, size);
    }

    if (needs_sf) {
        rec.updateSign(result, size);
    }

    if (needs_of) {
        rec.updateOverflowAdd(dst, src, result, size);
    }

    if (writeback) {
        rec.setGPR(&operands[0], result);
    }
}

FAST_HANDLE(SUB) {
    bool needs_cf = rec.shouldEmitFlag(rip, X86_REF_CF);
    bool needs_af = rec.shouldEmitFlag(rip, X86_REF_AF);
    bool needs_pf = rec.shouldEmitFlag(rip, X86_REF_PF);
    bool needs_zf = rec.shouldEmitFlag(rip, X86_REF_ZF);
    bool needs_sf = rec.shouldEmitFlag(rip, X86_REF_SF);
    bool needs_of = rec.shouldEmitFlag(rip, X86_REF_OF);
    bool needs_any_flag = needs_cf || needs_of || needs_pf || needs_sf || needs_zf || needs_af;
    bool dst_reg = operands[0].type == ZYDIS_OPERAND_TYPE_REGISTER;
    if (Extensions::B && g_config.noflag_opts && !needs_any_flag && dst_reg) {
        // We can do it faster if we don't need to calculate flags
        return OP_noflags_destreg(rec, rip, as, instruction, operands, &Assembler::SUB, &Assembler::SUBW);
    }

    biscuit::GPR result = rec.scratch();
    biscuit::GPR src = rec.getGPR(&operands[1]);
    biscuit::GPR dst;

    bool writeback = true;
    bool needs_atomic = operands[0].type == ZYDIS_OPERAND_TYPE_MEMORY && (instruction.attributes & ZYDIS_ATTRIB_HAS_LOCK);
    if (needs_atomic) {
        biscuit::GPR address = rec.lea(&operands[0]);
        dst = rec.scratch();
        switch (operands[0].size) {
        case 8: {
            /*
                andi    a2, a0, -4
                slli    a0, a0, 3
                li      a3, 255
                sllw    a3, a3, a0
                sllw    a0, a1, a0
            .LBB0_1:
                lr.w.aqrl       a1, (a2)
                sub     a4, a1, a0
                xor     a4, a4, a1
                and     a4, a4, a3
                xor     a4, a4, a1
                sc.w.rl a4, a4, (a2)
                bnez    a4, .LBB0_1
            */
            biscuit::Label loop;
            biscuit::GPR masked_address = rec.scratch();
            biscuit::GPR mask = rec.scratch();
            biscuit::GPR src_shifted = rec.scratch();
            as.ANDI(masked_address, address, -4);
            as.SLLI(address, address, 3);
            as.LI(mask, 0xFF);
            as.SLLW(mask, mask, address);
            as.SLLW(src_shifted, src, address);

            as.Bind(&loop);
            as.LR_W(Ordering::AQRL, dst, masked_address);
            as.SUB(result, dst, src_shifted);
            as.XOR(result, result, dst);
            as.AND(result, result, mask);
            as.XOR(result, result, dst);
            as.SC_W(Ordering::AQRL, result, result, masked_address);
            as.BNEZ(result, &loop);

            as.SRLW(dst, dst, address);
            as.ANDI(dst, dst, 0xFF);

            rec.popScratch();
            rec.popScratch();
            rec.popScratch();
            break;
        }
        case 16: {
            /*
            aadd(unsigned short*, unsigned short)
                andi    a6, a0, -4
                slli    a0, a0, 3
                lui     a3, 16
                addi    a3, a3, -1
                sllw    a4, a3, a0
                sllw    a1, a1, a0
            .LBB0_3:
                lr.w.aqrl       a5, (a6)
                sub     a2, a5, a1
                xor     a2, a2, a5
                and     a2, a2, a4
                xor     a2, a2, a5
                sc.w.rl a2, a2, (a6)
                bnez    a2, .LBB0_3
                srlw    a0, a5, a0
                and     a0, a0, a3
            */
            biscuit::Label loop, bad_alignment, end;
            biscuit::GPR masked_address = rec.scratch();
            biscuit::GPR mask = rec.scratch();
            as.LI(mask, 0b11);
            as.ANDI(masked_address, address, 0b11);
            as.BEQ(masked_address, mask, &bad_alignment);

            biscuit::GPR s_a1 = rec.scratch();
            biscuit::GPR s_a3 = mask;
            biscuit::GPR s_a2 = rec.flag(X86_REF_CF); // ran out of scratch and these get modified later
            biscuit::GPR s_a4 = rec.flag(X86_REF_SF);
            biscuit::GPR s_a5 = rec.flag(X86_REF_ZF);
            biscuit::GPR s_a6 = masked_address;
            as.ANDI(s_a6, address, -4);
            as.SLLI(address, address, 3);
            as.LI(s_a3, 0xFFFF);
            as.SLLW(s_a4, s_a3, address);
            as.SLLW(s_a1, src, address);
            as.Bind(&loop);
            as.LR_W(Ordering::AQRL, s_a5, s_a6);
            as.SUB(s_a2, s_a5, s_a1);
            as.XOR(s_a2, s_a2, s_a5);
            as.AND(s_a2, s_a2, s_a4);
            as.XOR(s_a2, s_a2, s_a5);
            as.SC_W(Ordering::AQRL, s_a2, s_a2, s_a6);
            as.BNEZ(s_a2, &loop);
            as.SRLW(dst, s_a5, address);
            as.AND(dst, dst, s_a3);

            as.J(&end);
            as.Bind(&bad_alignment);
            as.EBREAK();

            as.Bind(&end);
            rec.popScratch();
            rec.popScratch();
            rec.popScratch();
            break;
        }
        case 32: {
            biscuit::GPR src_neg = rec.scratch();
            as.NEG(src_neg, src);
            as.AMOADD_W(Ordering::AQRL, dst, src_neg, address);
            break;
        }
        case 64: {
            biscuit::GPR src_neg = rec.scratch();
            as.NEG(src_neg, src);
            as.AMOADD_D(Ordering::AQRL, dst, src_neg, address);
            break;
        }
        }
        rec.popScratch(); // pop src_neg

        // Still calculate result for flags
        as.SUB(result, dst, src);
        writeback = false;
    } else {
        if (needs_atomic) {
            WARN("Atomic SUB with 8 or 16 bit operands encountered");
        }

        dst = rec.getGPR(&operands[0]);
        as.SUB(result, dst, src);
    }

    x86_size_e size = rec.getSize(&operands[0]);

    SetCmpFlags(rip, rec, as, dst, src, result, size, operands[1].type == ZYDIS_OPERAND_TYPE_IMMEDIATE && size != X86_SIZE_QWORD);

    if (writeback) {
        rec.setGPR(&operands[0], result);
    }
}

FAST_HANDLE(SBB) {
    biscuit::GPR result = rec.scratch();
    biscuit::GPR result_2 = rec.scratch();
    biscuit::GPR src = rec.getGPR(&operands[1]);
    biscuit::GPR dst = rec.getGPR(&operands[0]);
    biscuit::GPR cf = rec.flag(X86_REF_CF);
    x86_size_e size = rec.getSize(&operands[0]);
    u64 sign_mask = rec.getSignMask(size);

    as.SUB(result, dst, src);
    as.SUB(result_2, result, cf);

    if (rec.shouldEmitFlag(rip, X86_REF_PF)) {
        rec.updateParity(result_2);
    }

    if (rec.shouldEmitFlag(rip, X86_REF_AF)) {
        rec.updateAuxiliarySbb(dst, src, result, cf);
    }

    if (rec.shouldEmitFlag(rip, X86_REF_OF)) {
        biscuit::GPR scratch = rec.scratch();
        biscuit::GPR scratch2 = rec.scratch();
        biscuit::GPR of = rec.flag(X86_REF_OF);
        as.LI(scratch2, sign_mask);
        as.XOR(scratch, dst, src);
        as.XOR(of, dst, result);
        as.AND(of, of, scratch);
        as.AND(of, of, scratch2);
        as.SNEZ(of, of);
        as.XOR(scratch, result, cf);
        as.XOR(scratch2, result, result_2);
        as.AND(scratch, scratch, scratch2);
        as.LI(scratch2, sign_mask);
        as.AND(scratch, scratch, scratch2);
        as.SNEZ(scratch, scratch);
        as.OR(of, of, scratch);
        rec.popScratch();
        rec.popScratch();
    }

    if (rec.shouldEmitFlag(rip, X86_REF_CF)) {
        biscuit::GPR scratch = rec.scratch();
        biscuit::GPR cf = rec.flag(X86_REF_CF);
        rec.zext(scratch, result, size);
        as.SLTU(scratch, scratch, cf);
        if (operands[1].type == ZYDIS_OPERAND_TYPE_IMMEDIATE && size != X86_SIZE_QWORD) {
            rec.zext(cf, src, size);
            as.SLTU(cf, dst, cf);
        } else {
            as.SLTU(cf, dst, src);
        }
        as.OR(cf, cf, scratch);
        rec.popScratch();
    }

    if (rec.shouldEmitFlag(rip, X86_REF_ZF)) {
        rec.updateZero(result_2, size);
    }

    if (rec.shouldEmitFlag(rip, X86_REF_SF)) {
        rec.updateSign(result_2, size);
    }

    rec.setGPR(&operands[0], result_2);
}

FAST_HANDLE(ADC) {
    biscuit::GPR result = rec.scratch();
    biscuit::GPR result_2 = rec.scratch();
    biscuit::GPR src = rec.getGPR(&operands[1]);
    biscuit::GPR dst = rec.getGPR(&operands[0]);
    biscuit::GPR cf = rec.flag(X86_REF_CF);
    x86_size_e size = rec.getSize(&operands[0]);

    as.ADD(result, dst, src);
    as.ADD(result_2, result, cf);

    if (rec.shouldEmitFlag(rip, X86_REF_PF)) {
        rec.updateParity(result_2);
    }

    if (rec.shouldEmitFlag(rip, X86_REF_AF)) {
        rec.updateAuxiliaryAdc(dst, result, cf, result_2);
    }

    if (rec.shouldEmitFlag(rip, X86_REF_OF)) {
        rec.updateOverflowAdd(dst, src, result_2, size);
    }

    if (rec.shouldEmitFlag(rip, X86_REF_CF)) {
        rec.updateCarryAdc(dst, result, result_2, size);
    }

    if (rec.shouldEmitFlag(rip, X86_REF_ZF)) {
        rec.updateZero(result_2, size);
    }

    if (rec.shouldEmitFlag(rip, X86_REF_SF)) {
        rec.updateSign(result_2, size);
    }

    rec.setGPR(&operands[0], result_2);
}

FAST_HANDLE(CMP) {
    bool needs_cf = rec.shouldEmitFlag(rip, X86_REF_CF);
    bool needs_af = rec.shouldEmitFlag(rip, X86_REF_AF);
    bool needs_pf = rec.shouldEmitFlag(rip, X86_REF_PF);
    bool needs_zf = rec.shouldEmitFlag(rip, X86_REF_ZF);
    bool needs_sf = rec.shouldEmitFlag(rip, X86_REF_SF);
    bool needs_of = rec.shouldEmitFlag(rip, X86_REF_OF);
    bool needs_any_flag = needs_cf || needs_of || needs_pf || needs_sf || needs_zf || needs_af;
    if (!needs_any_flag) {
        // Same as TEST, don't warn here
        return;
    }

    biscuit::GPR result = rec.scratch();
    biscuit::GPR src = rec.getGPR(&operands[1]);
    biscuit::GPR dst = rec.getGPR(&operands[0]);

    as.SUB(result, dst, src);

    x86_size_e size = rec.getSize(&operands[0]);

    SetCmpFlags(rip, rec, as, dst, src, result, size, operands[1].type == ZYDIS_OPERAND_TYPE_IMMEDIATE && size != X86_SIZE_QWORD);
}

FAST_HANDLE(OR) {
    bool needs_cf = rec.shouldEmitFlag(rip, X86_REF_CF);
    bool needs_pf = rec.shouldEmitFlag(rip, X86_REF_PF);
    bool needs_zf = rec.shouldEmitFlag(rip, X86_REF_ZF);
    bool needs_sf = rec.shouldEmitFlag(rip, X86_REF_SF);
    bool needs_of = rec.shouldEmitFlag(rip, X86_REF_OF);
    bool needs_any_flag = needs_cf || needs_of || needs_pf || needs_sf || needs_zf;
    bool dst_reg = operands[0].type == ZYDIS_OPERAND_TYPE_REGISTER;
    if (Extensions::B && g_config.noflag_opts && !needs_any_flag && dst_reg) {
        // We can do it faster if we don't need to calculate flags
        return OP_noflags_destreg(rec, rip, as, instruction, operands, &Assembler::OR, &Assembler::OR);
    }

    biscuit::GPR result = rec.scratch();
    biscuit::GPR src = rec.getGPR(&operands[1]);
    biscuit::GPR dst;

    bool writeback = true;
    bool needs_atomic = operands[0].type == ZYDIS_OPERAND_TYPE_MEMORY && (instruction.attributes & ZYDIS_ATTRIB_HAS_LOCK);
    bool too_small_for_atomic = operands[0].size == 8 || operands[0].size == 16;
    if (needs_atomic && !too_small_for_atomic) {
        biscuit::GPR address = rec.lea(&operands[0]);
        dst = rec.scratch();
        switch (operands[0].size) {
        case 32: {
            as.AMOOR_W(Ordering::AQRL, dst, src, address);
            break;
        }
        case 64: {
            as.AMOOR_D(Ordering::AQRL, dst, src, address);
            break;
        }
        }

        if (needs_any_flag || !g_config.noflag_opts) {
            as.OR(result, dst, src);
        }

        writeback = false;
    } else {
        if (needs_atomic) {
            WARN("Atomic OR with 8 or 16 bit operands encountered");
        }

        dst = rec.getGPR(&operands[0]);
        as.OR(result, dst, src);
    }

    x86_size_e size = rec.getSize(&operands[0]);

    if (needs_cf) {
        rec.clearFlag(X86_REF_CF);
    }

    if (needs_pf) {
        rec.updateParity(result);
    }

    if (needs_zf) {
        rec.updateZero(result, size);
    }

    if (needs_sf) {
        rec.updateSign(result, size);
    }

    if (needs_of) {
        rec.clearFlag(X86_REF_OF);
    }

    if (writeback) {
        rec.setGPR(&operands[0], result);
    }
}

FAST_HANDLE(XOR) {
    x86_size_e size = rec.getSize(&operands[0]);

    // Optimize this common case since xor is used to zero out a register frequently
    if ((size == X86_SIZE_DWORD || size == X86_SIZE_QWORD) && operands[0].type == ZYDIS_OPERAND_TYPE_REGISTER &&
        operands[1].type == ZYDIS_OPERAND_TYPE_REGISTER && operands[0].reg.value == operands[1].reg.value) {
        rec.setGPR(rec.zydisToRef(operands[0].reg.value), X86_SIZE_QWORD, x0);

        if (rec.shouldEmitFlag(rip, X86_REF_CF)) {
            rec.clearFlag(X86_REF_CF);
        }

        if (rec.shouldEmitFlag(rip, X86_REF_PF)) {
            biscuit::GPR pf = rec.scratch();
            as.LI(pf, 1);
            as.SB(pf, offsetof(ThreadState, pf), rec.threadStatePointer());
            rec.popScratch();
        }

        if (rec.shouldEmitFlag(rip, X86_REF_ZF)) {
            rec.setFlag(X86_REF_ZF);
        }

        if (rec.shouldEmitFlag(rip, X86_REF_SF)) {
            biscuit::GPR sf = rec.flag(X86_REF_SF);
            as.MV(sf, x0);
        }

        if (rec.shouldEmitFlag(rip, X86_REF_OF)) {
            rec.clearFlag(X86_REF_OF);
        }
        return;
    }

    bool needs_cf = rec.shouldEmitFlag(rip, X86_REF_CF);
    bool needs_pf = rec.shouldEmitFlag(rip, X86_REF_PF);
    bool needs_zf = rec.shouldEmitFlag(rip, X86_REF_ZF);
    bool needs_sf = rec.shouldEmitFlag(rip, X86_REF_SF);
    bool needs_of = rec.shouldEmitFlag(rip, X86_REF_OF);
    bool needs_any_flag = needs_cf || needs_of || needs_pf || needs_sf || needs_zf;
    bool dst_reg = operands[0].type == ZYDIS_OPERAND_TYPE_REGISTER;
    if (Extensions::B && g_config.noflag_opts && !needs_any_flag && dst_reg) {
        // We can do it faster if we don't need to calculate flags
        return OP_noflags_destreg(rec, rip, as, instruction, operands, &Assembler::XOR, &Assembler::XOR);
    }

    biscuit::GPR result = rec.scratch();
    biscuit::GPR dst;
    biscuit::GPR src = rec.getGPR(&operands[1]);

    bool writeback = true;
    bool needs_atomic = operands[0].type == ZYDIS_OPERAND_TYPE_MEMORY && (instruction.attributes & ZYDIS_ATTRIB_HAS_LOCK);
    bool too_small_for_atomic = operands[0].size == 8 || operands[0].size == 16;
    if (needs_atomic && !too_small_for_atomic) {
        dst = rec.scratch();
        biscuit::GPR address = rec.lea(&operands[0]);
        if (size == X86_SIZE_DWORD) {
            as.AMOXOR_W(Ordering::AQRL, dst, src, address);
        } else if (size == X86_SIZE_QWORD) {
            as.AMOXOR_D(Ordering::AQRL, dst, src, address);
        } else {
            UNREACHABLE();
        }

        if (needs_any_flag || !g_config.noflag_opts) {
            as.XOR(result, dst, src);
        }

        writeback = false;
    } else {
        if (needs_atomic) {
            WARN("Atomic XOR with 8 or 16 bit operands encountered");
        }

        dst = rec.getGPR(&operands[0]);
        as.XOR(result, dst, src);
    }

    if (needs_cf) {
        rec.clearFlag(X86_REF_CF);
    }

    if (needs_pf) {
        rec.updateParity(result);
    }

    if (needs_zf) {
        rec.updateZero(result, size);
    }

    if (needs_sf) {
        rec.updateSign(result, size);
    }

    if (needs_of) {
        rec.clearFlag(X86_REF_OF);
    }

    if (writeback) {
        rec.setGPR(&operands[0], result);
    }
}

FAST_HANDLE(AND) {
    biscuit::GPR result = rec.scratch();
    biscuit::GPR src = rec.getGPR(&operands[1]);
    biscuit::GPR dst;

    bool writeback = true;
    bool needs_atomic = operands[0].type == ZYDIS_OPERAND_TYPE_MEMORY && (instruction.attributes & ZYDIS_ATTRIB_HAS_LOCK);
    bool too_small_for_atomic = operands[0].size == 8 || operands[0].size == 16;
    if (needs_atomic && !too_small_for_atomic) {
        biscuit::GPR address = rec.lea(&operands[0]);
        dst = rec.scratch();
        switch (operands[0].size) {
        case 32: {
            as.AMOAND_W(Ordering::AQRL, dst, src, address);
            break;
        }
        case 64: {
            as.AMOAND_D(Ordering::AQRL, dst, src, address);
            break;
        }
        }

        // TODO: noflags opt
        as.AND(result, dst, src);
        writeback = false;
    } else {
        if (needs_atomic) {
            WARN("Atomic OR with 8 or 16 bit operands encountered");
        }

        dst = rec.getGPR(&operands[0]);
        as.AND(result, dst, src);
    }

    x86_size_e size = rec.getSize(&operands[0]);
    if (rec.shouldEmitFlag(rip, X86_REF_CF)) {
        rec.clearFlag(X86_REF_CF);
    }

    if (rec.shouldEmitFlag(rip, X86_REF_PF)) {
        rec.updateParity(result);
    }

    if (rec.shouldEmitFlag(rip, X86_REF_ZF)) {
        rec.updateZero(result, size);
    }

    if (rec.shouldEmitFlag(rip, X86_REF_SF)) {
        rec.updateSign(result, size);
    }

    if (rec.shouldEmitFlag(rip, X86_REF_OF)) {
        rec.clearFlag(X86_REF_OF);
    }

    if (writeback) {
        rec.setGPR(&operands[0], result);
    }
}

FAST_HANDLE(HLT) {
    rec.setExitReason(ExitReason::EXIT_REASON_HLT);
    rec.writebackState();
    as.LI(t5, (u64)Emulator::ExitDispatcher);
    as.MV(a0, sp);
    as.JR(t5);
    rec.stopCompiling();
}

FAST_HANDLE(UD2) {
    WARN_ONCE("UD2 instruction being compiled?");

    // UD2 will trigger SIGILL, so we need to do the same
    as.C_UNDEF();
    rec.stopCompiling();
}

FAST_HANDLE(CALL) {
    rec.pushCalltrace();

    switch (operands[0].type) {
    case ZYDIS_OPERAND_TYPE_REGISTER:
    case ZYDIS_OPERAND_TYPE_MEMORY: {
        biscuit::GPR src = rec.getGPR(&operands[0]);
        biscuit::GPR ripreg = rec.allocatedGPR(X86_REF_RIP);
        // Don't need to zero extend here as it's loaded as a DWORD
        as.MV(ripreg, src);
        biscuit::GPR rsp = rec.getGPR(X86_REF_RSP, rec.stackWidth());
        as.ADDI(rsp, rsp, -rec.stackPointerSize());
        rec.setGPR(X86_REF_RSP, rec.stackWidth(), rsp);

        biscuit::GPR scratch = rec.scratch();
        u64 return_address = rip + instruction.length;
        as.LI(scratch, return_address);
        rec.writeMemory(scratch, rsp, 0, rec.stackWidth());
        rec.backToDispatcher();
        rec.stopCompiling();
        break;
    }
    case ZYDIS_OPERAND_TYPE_IMMEDIATE: {
        u64 displacement = rec.sextImmediate(rec.getImmediate(&operands[0]), operands[0].imm.size);
        u64 return_address_offset = (rip - rec.getCurrentMetadata().guest_address) + instruction.length;

        biscuit::GPR rsp = rec.getGPR(X86_REF_RSP, rec.stackWidth());
        as.ADDI(rsp, rsp, -rec.stackPointerSize());
        rec.setGPR(X86_REF_RSP, rec.stackWidth(), rsp);

        biscuit::GPR ripreg = rec.allocatedGPR(X86_REF_RIP);
        rec.addi(ripreg, ripreg, return_address_offset);
        rec.writeMemory(ripreg, rsp, 0, rec.stackWidth());
        rec.addi(ripreg, ripreg, displacement);
        if (g_mode32) {
            rec.zext(ripreg, ripreg, X86_SIZE_DWORD);
            rec.jumpAndLink((u32)(rip + instruction.length + displacement));
        } else {
            rec.jumpAndLink(rip + instruction.length + displacement);
        }
        rec.stopCompiling();
        break;
    }
    default: {
        UNREACHABLE();
        break;
    }
    }
}

FAST_HANDLE(RET) {
    rec.popCalltrace();

    biscuit::GPR rsp = rec.getGPR(X86_REF_RSP, rec.stackWidth());
    biscuit::GPR scratch = rec.scratch();
    rec.readMemory(scratch, rsp, 0, rec.stackWidth());

    u64 imm = rec.stackPointerSize();
    if (operands[0].type == ZYDIS_OPERAND_TYPE_IMMEDIATE) {
        imm += rec.getImmediate(&operands[0]);
    }

    rec.addi(rsp, rsp, imm);

    rec.setGPR(X86_REF_RSP, rec.stackWidth(), rsp);

    biscuit::GPR ripreg = rec.allocatedGPR(X86_REF_RIP);
    // Don't need to zero extend here as it's loaded as a DWORD
    as.MV(ripreg, scratch);
    rec.backToDispatcher();
    rec.stopCompiling();
}

FAST_HANDLE(IRETD) {
    ASSERT(g_mode32);
    rec.writebackState();
    as.MV(a0, rec.threadStatePointer());
    rec.callPointer(offsetof(ThreadState, felix86_iret));
    rec.restoreState();
    rec.backToDispatcher();
    rec.stopCompiling();
}

FAST_HANDLE(IRETQ) {
    ASSERT(!g_mode32);
    rec.writebackState();
    as.MV(a0, rec.threadStatePointer());
    rec.callPointer(offsetof(ThreadState, felix86_iret));
    rec.restoreState();
    rec.backToDispatcher();
    rec.stopCompiling();
}

FAST_HANDLE(PUSH) {
    biscuit::GPR src;
    if (is_segment(operands[0])) {
        biscuit::GPR seg = rec.scratch();
        int offset = 0;
        switch (operands[0].reg.value) {
        case ZYDIS_REGISTER_CS: {
            offset = offsetof(ThreadState, cs);
            break;
        }
        case ZYDIS_REGISTER_DS: {
            offset = offsetof(ThreadState, ds);
            break;
        }
        case ZYDIS_REGISTER_SS: {
            offset = offsetof(ThreadState, ss);
            break;
        }
        case ZYDIS_REGISTER_ES: {
            offset = offsetof(ThreadState, es);
            break;
        }
        case ZYDIS_REGISTER_FS: {
            offset = offsetof(ThreadState, fs);
            break;
        }
        case ZYDIS_REGISTER_GS: {
            offset = offsetof(ThreadState, gs);
            break;
        }
        default: {
            UNREACHABLE();
            break;
        }
        }
        as.LHU(seg, offset, rec.threadStatePointer());
        src = seg;
    } else {
        src = rec.getGPR(&operands[0]);
    }

    biscuit::GPR rsp = rec.getGPR(X86_REF_RSP, rec.stackWidth());
    int imm = -size_to_bytes(instruction.operand_width);
    rec.writeMemory(src, rsp, imm, rec.zydisToSize(instruction.operand_width));

    as.ADDI(rsp, rsp, imm);
    rec.setGPR(X86_REF_RSP, rec.stackWidth(), rsp);
}

FAST_HANDLE(POP) {
    if (is_segment(operands[0])) {
        ASSERT_MSG(g_mode32, "Popping segment not in 32-bit mode?");
        biscuit::GPR src = rec.scratch();
        biscuit::GPR rsp = rec.getGPR(X86_REF_RSP, rec.stackWidth());
        int imm = size_to_bytes(instruction.operand_width);
        rec.readMemory(src, rsp, 0, X86_SIZE_WORD);
        rec.writebackState();
        as.MV(a0, rec.threadStatePointer());
        as.MV(a1, src);
        as.LI(a2, operands[0].reg.value);
        rec.callPointer(offsetof(ThreadState, felix86_set_segment));
        rec.restoreState();
        as.ADDI(rsp, rsp, imm);
        rec.setGPR(X86_REF_RSP, rec.stackWidth(), rsp);
    } else {
        biscuit::GPR result = rec.scratch();
        biscuit::GPR rsp = rec.getGPR(X86_REF_RSP, rec.stackWidth());
        int imm = size_to_bytes(instruction.operand_width);
        rec.readMemory(result, rsp, 0, rec.zydisToSize(instruction.operand_width));
        if (operands[0].type == ZYDIS_OPERAND_TYPE_REGISTER && rec.zydisToRef(operands[0].reg.value) == X86_REF_RSP) {
            // pop rsp special case
            WARN("pop rsp with width: %d", operands[0].size);
            rec.setGPR(&operands[0], result);
        } else {
            as.ADDI(rsp, rsp, imm);
            rec.setGPR(X86_REF_RSP, rec.stackWidth(), rsp);
            rec.setGPR(&operands[0], result);
        }
    }
}

FAST_HANDLE(NOP) {}

FAST_HANDLE(ENDBR32) {}

FAST_HANDLE(ENDBR64) {}

FAST_HANDLE(RDSSPD) {}

FAST_HANDLE(RDSSPQ) {}

FAST_HANDLE(RSTORSSP) {}

FAST_HANDLE(SAVEPREVSSP) {}

FAST_HANDLE(FNCLEX) {}

FAST_HANDLE(PREFETCHT0) {}

FAST_HANDLE(PREFETCHT1) {}

FAST_HANDLE(PREFETCHT2) {}

FAST_HANDLE(PREFETCHNTA) {}

FAST_HANDLE(PREFETCHW) {}

FAST_HANDLE(PREFETCHWT1) {}

FAST_HANDLE(SHL_imm) {
    x86_size_e size = rec.getSize(&operands[0]);
    biscuit::GPR result = rec.scratch();
    biscuit::GPR dst = rec.getGPR(&operands[0]);
    u8 shift = rec.getImmediate(&operands[1]);
    shift &= instruction.operand_width == 64 ? 0x3F : 0x1F;

    bool needs_cf = rec.shouldEmitFlag(rip, X86_REF_CF);
    bool needs_pf = rec.shouldEmitFlag(rip, X86_REF_PF);
    bool needs_zf = rec.shouldEmitFlag(rip, X86_REF_ZF);
    bool needs_sf = rec.shouldEmitFlag(rip, X86_REF_SF);
    bool needs_of = rec.shouldEmitFlag(rip, X86_REF_OF) && shift == 1;
    bool needs_any_flag = needs_cf || needs_of || needs_pf || needs_sf || needs_zf;
    if (!needs_any_flag && operands[0].size == 64 && g_config.noflag_opts) {
        result = dst; // shift the allocated register directly
    }

    if (shift != 0) {
        as.SLLI(result, dst, shift);

        if (needs_pf) {
            rec.updateParity(result);
        }

        if (needs_zf) {
            rec.updateZero(result, size);
        }

        if (needs_sf) {
            rec.updateSign(result, size);
        }

        if (needs_cf) {
            biscuit::GPR cf = rec.flag(X86_REF_CF);
            u8 shift_right = rec.getBitSize(size) - shift;
            shift_right &= 0x3F;
            as.SRLI(cf, dst, shift_right);
            as.ANDI(cf, cf, 1);
        }

        if (needs_of) {
            biscuit::GPR of = rec.flag(X86_REF_OF);
            u8 shift_right = rec.getBitSize(size) - 1;
            as.SRLI(of, dst, shift_right);
            as.ANDI(of, of, 1);
            as.XOR(of, of, rec.flag(X86_REF_CF));
        }

        rec.setGPR(&operands[0], result);
    } else if (operands[0].size == 32 && operands[0].type == ZYDIS_OPERAND_TYPE_REGISTER) {
        // Set it without zero extending again
        rec.setGPR(rec.zydisToRef(operands[0].reg.value), X86_SIZE_QWORD, dst);
    } else {
        return; // don't do nothing
    }
}

FAST_HANDLE(SHR_imm) {
    x86_size_e size = rec.getSize(&operands[0]);
    biscuit::GPR result = rec.scratch();
    biscuit::GPR dst = rec.getGPR(&operands[0]);
    u8 shift = rec.getImmediate(&operands[1]);
    shift &= instruction.operand_width == 64 ? 0x3F : 0x1F;

    if (shift != 0) {
        as.SRLI(result, dst, shift);

        if (rec.shouldEmitFlag(rip, X86_REF_PF)) {
            rec.updateParity(result);
        }

        if (rec.shouldEmitFlag(rip, X86_REF_ZF)) {
            rec.updateZero(result, size);
        }

        if (rec.shouldEmitFlag(rip, X86_REF_SF)) {
            rec.updateSign(result, size);
        }

        if (rec.shouldEmitFlag(rip, X86_REF_CF)) {
            biscuit::GPR cf = rec.flag(X86_REF_CF);
            u8 shift_right = shift - 1;
            as.SRLI(cf, dst, shift_right);
            as.ANDI(cf, cf, 1);
        }

        if (rec.shouldEmitFlag(rip, X86_REF_OF) && shift == 1) {
            biscuit::GPR of = rec.flag(X86_REF_OF);
            as.SRLI(of, dst, rec.getBitSize(size) - 1);
            as.ANDI(of, of, 1);
        }

        rec.setGPR(&operands[0], result);
    } else if (operands[0].size == 32 && operands[0].type == ZYDIS_OPERAND_TYPE_REGISTER) {
        // Set it without zero extending again
        rec.setGPR(rec.zydisToRef(operands[0].reg.value), X86_SIZE_QWORD, dst);
    } else {
        return; // don't do nothing
    }
}

FAST_HANDLE(SAR_imm) {
    x86_size_e size = rec.getSize(&operands[0]);
    biscuit::GPR result = rec.scratch();
    biscuit::GPR dst = rec.getGPR(&operands[0]);
    u8 shift = rec.getImmediate(&operands[1]);
    shift &= instruction.operand_width == 64 ? 0x3F : 0x1F;

    if (shift != 0) {
        switch (size) {
        case X86_SIZE_BYTE:
        case X86_SIZE_BYTE_HIGH: {
            as.SLLI(result, dst, 56);
            if (shift + 56 < 64) {
                as.SRAI(result, result, 56 + shift);
            } else {
                as.SRAI(result, result, 63);
            }
            break;
        }
        case X86_SIZE_WORD: {
            as.SLLI(result, dst, 48);
            if (shift + 48 < 64) {
                as.SRAI(result, result, 48 + shift);
            } else {
                as.SRAI(result, result, 63);
            }
            break;
        }
        case X86_SIZE_DWORD: {
            as.SRAIW(result, dst, shift);
            break;
        }
        case X86_SIZE_QWORD: {
            as.SRAI(result, dst, shift);
            break;
        }
        default: {
            UNREACHABLE();
            break;
        }
        }

        if (rec.shouldEmitFlag(rip, X86_REF_PF)) {
            rec.updateParity(result);
        }

        if (rec.shouldEmitFlag(rip, X86_REF_ZF)) {
            rec.updateZero(result, size);
        }

        if (rec.shouldEmitFlag(rip, X86_REF_SF)) {
            rec.updateSign(result, size);
        }

        if (rec.shouldEmitFlag(rip, X86_REF_CF)) {
            biscuit::GPR cf = rec.flag(X86_REF_CF);
            as.SRLI(cf, dst, shift - 1);
            as.ANDI(cf, cf, 1);
        }

        if (rec.shouldEmitFlag(rip, X86_REF_OF)) {
            biscuit::GPR of = rec.flag(X86_REF_OF);
            as.MV(of, x0);
        }

        rec.setGPR(&operands[0], result);
    } else if (operands[0].size == 32 && operands[0].type == ZYDIS_OPERAND_TYPE_REGISTER) {
        // Set it without zero extending again
        rec.setGPR(rec.zydisToRef(operands[0].reg.value), X86_SIZE_QWORD, dst);
    } else {
        return; // don't do nothing
    }
}

FAST_HANDLE(SHL) {
    if (operands[1].type == ZYDIS_OPERAND_TYPE_IMMEDIATE) {
        return fast_SHL_imm(rec, rip, as, instruction, operands);
    }

    bool needs_cf = rec.shouldEmitFlag(rip, X86_REF_CF);
    bool needs_pf = rec.shouldEmitFlag(rip, X86_REF_PF);
    bool needs_zf = rec.shouldEmitFlag(rip, X86_REF_ZF);
    bool needs_sf = rec.shouldEmitFlag(rip, X86_REF_SF);
    bool needs_of = rec.shouldEmitFlag(rip, X86_REF_OF);
    bool needs_any_flag = needs_cf || needs_of || needs_pf || needs_sf || needs_zf;

    if (g_config.noflag_opts && !needs_any_flag) {
        return SHIFT_noflags(rec, rip, as, instruction, operands, &Assembler::SLL, &Assembler::SLLW);
    }

    biscuit::GPR result = rec.scratch();
    x86_size_e size = rec.getSize(&operands[0]);
    biscuit::GPR dst = rec.getGPR(&operands[0]);
    biscuit::GPR src = rec.getGPR(&operands[1]);
    biscuit::GPR count = rec.scratch();

    if (instruction.operand_width == 64) {
        as.ANDI(count, src, 0x3F);
    } else {
        as.ANDI(count, src, 0x1F);
    }

    Label zero_source;

    as.SLL(result, dst, count);

    as.BEQZ(count, &zero_source);

    if (needs_pf) {
        rec.updateParity(result);
    }

    if (needs_zf) {
        rec.updateZero(result, size);
    }

    if (needs_sf) {
        rec.updateSign(result, size);
    }

    if (needs_cf) {
        biscuit::GPR cf = rec.flag(X86_REF_CF);
        as.LI(cf, rec.getBitSize(size));
        as.SUB(cf, cf, count);
        as.SRL(cf, dst, cf);
        as.ANDI(cf, cf, 1);
    }

    if (needs_of) {
        biscuit::GPR of = rec.flag(X86_REF_OF);
        as.SRLI(of, result, rec.getBitSize(size) - 1);
        as.ANDI(of, of, 1);
        as.XOR(of, of, rec.flag(X86_REF_CF));
    }

    as.Bind(&zero_source);

    rec.setGPR(&operands[0], result);
}

FAST_HANDLE(SHR) {
    if (operands[1].type == ZYDIS_OPERAND_TYPE_IMMEDIATE) {
        return fast_SHR_imm(rec, rip, as, instruction, operands);
    }

    bool needs_cf = rec.shouldEmitFlag(rip, X86_REF_CF);
    bool needs_pf = rec.shouldEmitFlag(rip, X86_REF_PF);
    bool needs_zf = rec.shouldEmitFlag(rip, X86_REF_ZF);
    bool needs_sf = rec.shouldEmitFlag(rip, X86_REF_SF);
    bool needs_of = rec.shouldEmitFlag(rip, X86_REF_OF);
    bool needs_any_flag = needs_cf || needs_of || needs_pf || needs_sf || needs_zf;

    if (g_config.noflag_opts && !needs_any_flag) {
        return SHIFT_noflags(rec, rip, as, instruction, operands, &Assembler::SRL, &Assembler::SRLW);
    }

    biscuit::GPR result = rec.scratch();
    x86_size_e size = rec.getSize(&operands[0]);
    biscuit::GPR dst = rec.getGPR(&operands[0]);
    biscuit::GPR src = rec.getGPR(&operands[1]);
    biscuit::GPR count = rec.scratch();

    if (instruction.operand_width == 64) {
        as.ANDI(count, src, 0x3F);
    } else {
        as.ANDI(count, src, 0x1F);
    }

    Label zero_source;

    as.SRL(result, dst, count);

    as.BEQZ(count, &zero_source);

    if (needs_pf) {
        rec.updateParity(result);
    }

    if (needs_zf) {
        rec.updateZero(result, size);
    }

    if (needs_sf) {
        rec.updateSign(result, size);
    }

    if (needs_cf) {
        biscuit::GPR cf = rec.flag(X86_REF_CF);
        as.ADDI(cf, count, -1);
        as.SRL(cf, dst, cf);
        as.ANDI(cf, cf, 1);
    }

    if (needs_of) {
        biscuit::GPR of = rec.flag(X86_REF_OF);
        as.SRLI(of, dst, rec.getBitSize(size) - 1);
        as.ANDI(of, of, 1);
    }

    as.Bind(&zero_source);

    rec.setGPR(&operands[0], result);
}

FAST_HANDLE(SAR) {
    if (operands[1].type == ZYDIS_OPERAND_TYPE_IMMEDIATE) {
        return fast_SAR_imm(rec, rip, as, instruction, operands);
    }

    biscuit::GPR result = rec.scratch();
    x86_size_e size = rec.getSize(&operands[0]);
    biscuit::GPR dst = rec.getGPR(&operands[0]);
    biscuit::GPR src = rec.getGPR(&operands[1]);
    biscuit::GPR count = rec.scratch();

    if (instruction.operand_width == 64) {
        as.ANDI(count, src, 0x3F);
    } else {
        as.ANDI(count, src, 0x1F);
    }

    Label zero_source;

    switch (size) {
    case X86_SIZE_BYTE:
    case X86_SIZE_BYTE_HIGH: {
        as.SLLI(result, dst, 56);
        as.SRAI(result, result, 56);
        as.SRA(result, result, count);
        break;
    }
    case X86_SIZE_WORD: {
        as.SLLI(result, dst, 48);
        as.SRAI(result, result, 48);
        as.SRA(result, result, count);
        break;
    }
    case X86_SIZE_DWORD: {
        as.SRAW(result, dst, count);
        break;
    }
    case X86_SIZE_QWORD: {
        as.SRA(result, dst, count);
        break;
    }
    default: {
        UNREACHABLE();
        break;
    }
    }

    as.BEQZ(count, &zero_source);

    if (rec.shouldEmitFlag(rip, X86_REF_PF)) {
        rec.updateParity(result);
    }

    if (rec.shouldEmitFlag(rip, X86_REF_ZF)) {
        rec.updateZero(result, size);
    }

    if (rec.shouldEmitFlag(rip, X86_REF_SF)) {
        rec.updateSign(result, size);
    }

    if (rec.shouldEmitFlag(rip, X86_REF_CF)) {
        biscuit::GPR cf = rec.flag(X86_REF_CF);
        as.ADDI(cf, count, -1);
        as.SRL(cf, dst, cf);
        as.ANDI(cf, cf, 1);
    }

    if (rec.shouldEmitFlag(rip, X86_REF_OF)) {
        biscuit::GPR of = rec.flag(X86_REF_OF);
        as.MV(of, x0);
    }

    as.Bind(&zero_source);

    rec.setGPR(&operands[0], result);
}

FAST_HANDLE(MOVQ) {
    if (operands[0].type == ZYDIS_OPERAND_TYPE_MEMORY) {
        ASSERT(operands[0].size == 64);
        biscuit::GPR dst = rec.scratch();
        biscuit::Vec src = rec.getVec(&operands[1]);

        rec.setVectorState(SEW::E64, 2);
        as.VMV_XS(dst, src);

        rec.setGPR(&operands[0], dst);
    } else if (operands[1].type == ZYDIS_OPERAND_TYPE_MEMORY) {
        ASSERT(operands[1].size == 64);
        biscuit::GPR src = rec.getGPR(&operands[1]);
        biscuit::Vec dst = rec.getVec(&operands[0]);

        rec.setVectorState(SEW::E64, 2);
        as.VMV(v0, 0b10);

        // Zero upper 64-bit elements (this will be useful for when we get to AVX)
        as.VXOR(dst, dst, dst, VecMask::Yes);
        as.VMV_SX(dst, src);

        rec.setVec(&operands[0], dst);
    } else if (operands[1].type == ZYDIS_OPERAND_TYPE_REGISTER) {
        ASSERT(operands[0].type == ZYDIS_OPERAND_TYPE_REGISTER && operands[1].type == ZYDIS_OPERAND_TYPE_REGISTER);

        if (rec.isGPR(operands[1].reg.value)) {
            biscuit::GPR src = rec.getGPR(&operands[1]);
            biscuit::Vec dst = rec.getVec(&operands[0]);

            rec.setVectorState(SEW::E64, 2);
            as.VMV(v0, 0b10);

            // Zero upper 64-bit elements (this will be useful for when we get to AVX)
            as.VXOR(dst, dst, dst, VecMask::Yes);
            as.VMV_SX(dst, src);

            rec.setVec(&operands[0], dst);
        } else if (rec.isGPR(operands[0].reg.value)) {
            biscuit::GPR dst = rec.getGPR(&operands[0]);
            biscuit::Vec src = rec.getVec(&operands[1]);

            rec.setVectorState(SEW::E64, 2);
            as.VMV_XS(dst, src);

            rec.setGPR(&operands[0], dst);
        } else {
            biscuit::Vec result = rec.scratchVec();
            biscuit::Vec src = rec.getVec(&operands[1]);

            rec.setVectorState(SEW::E64, 2);
            as.VMV(v0, 0b01);
            as.VMV(result, 0);
            as.VOR(result, src, 0, VecMask::Yes);

            rec.setVec(&operands[0], result);
        }
    }
}

FAST_HANDLE(MOVD) {
    if (operands[0].type == ZYDIS_OPERAND_TYPE_MEMORY) {
        ASSERT(operands[0].size == 32);
        biscuit::GPR dst = rec.scratch();
        biscuit::Vec src = rec.getVec(&operands[1]);

        rec.setVectorState(SEW::E32, 1);
        as.VMV_XS(dst, src);

        rec.setGPR(&operands[0], dst);
    } else if (operands[1].type == ZYDIS_OPERAND_TYPE_MEMORY) {
        ASSERT(operands[1].size == 32);
        biscuit::GPR src = rec.getGPR(&operands[1]);
        biscuit::Vec dst = rec.getVec(&operands[0]);

        rec.setVectorState(SEW::E32, 4);
        as.VMV(v0, 0b1110);

        // Zero upper 32-bit elements (this will be useful for when we get to AVX)
        as.VXOR(dst, dst, dst, VecMask::Yes);
        as.VMV_SX(dst, src);

        rec.setVec(&operands[0], dst);
    } else if (operands[1].type == ZYDIS_OPERAND_TYPE_REGISTER) {
        ASSERT(operands[0].type == ZYDIS_OPERAND_TYPE_REGISTER && operands[1].type == ZYDIS_OPERAND_TYPE_REGISTER);

        if (rec.isGPR(operands[1].reg.value)) {
            biscuit::GPR src = rec.getGPR(&operands[1]);
            biscuit::Vec dst = rec.getVec(&operands[0]);

            rec.setVectorState(SEW::E32, 4);
            as.VMV(v0, 0b1110);

            // Zero upper 32-bit elements (this will be useful for when we get to AVX)
            as.VXOR(dst, dst, dst, VecMask::Yes);
            as.VMV_SX(dst, src);

            rec.setVec(&operands[0], dst);
        } else if (rec.isGPR(operands[0].reg.value)) {
            biscuit::GPR dst = rec.getGPR(&operands[0]);
            biscuit::Vec src = rec.getVec(&operands[1]);

            rec.setVectorState(SEW::E32, 4);
            as.VMV_XS(dst, src);

            rec.setGPR(&operands[0], dst);
        } else {
            biscuit::Vec result = rec.scratchVec();
            biscuit::Vec src = rec.getVec(&operands[1]);

            rec.setVectorState(SEW::E32, 4);
            as.VMV(v0, 0b01);
            as.VMV(result, 0);
            as.VOR(result, src, 0, VecMask::Yes);

            rec.setVec(&operands[0], result);
        }
    }
}

FAST_HANDLE(JMP) {
    switch (operands[0].type) {
    case ZYDIS_OPERAND_TYPE_REGISTER:
    case ZYDIS_OPERAND_TYPE_MEMORY: {
        biscuit::GPR src = rec.getGPR(&operands[0]);
        biscuit::GPR ripreg = rec.allocatedGPR(X86_REF_RIP);
        // Don't need to zero extend here as it's loaded as a DWORD
        as.MV(ripreg, src);
        rec.backToDispatcher();
        rec.stopCompiling();
        break;
    }
    case ZYDIS_OPERAND_TYPE_IMMEDIATE: {
        u64 displacement = rec.sextImmediate(rec.getImmediate(&operands[0]), operands[0].imm.size);
        u64 address = rip + instruction.length + displacement;
        u64 offset = (rip - rec.getCurrentMetadata().guest_address) + instruction.length + displacement;
        biscuit::GPR ripreg = rec.allocatedGPR(X86_REF_RIP);
        rec.addi(ripreg, ripreg, offset);
        if (g_mode32) {
            rec.zext(ripreg, ripreg, X86_SIZE_DWORD);
            rec.jumpAndLink((u32)address);
        } else {
            rec.jumpAndLink(address);
        }
        rec.stopCompiling();
        break;
    }
    default: {
        UNREACHABLE();
        break;
    }
    }
}

FAST_HANDLE(LEA) {
    // The LEA instruction doesn't take into account segment override, but our lea function does
    // So hack the segment away
    ASSERT(operands[1].type == ZYDIS_OPERAND_TYPE_MEMORY);
    operands[1].mem.segment = ZYDIS_REGISTER_NONE;
    operands[1].attributes &= ~ZYDIS_ATTRIB_HAS_SEGMENT;
    biscuit::GPR address = rec.lea(&operands[1]);
    rec.setGPR(&operands[0], address);
}

FAST_HANDLE(RDFSBASE) {
    biscuit::GPR fs = rec.scratch();
    as.LD(fs, offsetof(ThreadState, fsbase), rec.threadStatePointer());
    rec.setGPR(&operands[0], fs);
}

FAST_HANDLE(RDGSBASE) {
    biscuit::GPR gs = rec.scratch();
    as.LD(gs, offsetof(ThreadState, gsbase), rec.threadStatePointer());
    rec.setGPR(&operands[0], gs);
}

FAST_HANDLE(DIV) {
    x86_size_e size = rec.getSize(&operands[0]);
    // we don't need to move src to scratch because the rdx and rax in all these cases are in scratches
    biscuit::GPR src = rec.getGPR(&operands[0]);

    switch (size) {
    case X86_SIZE_BYTE:
    case X86_SIZE_BYTE_HIGH: {
        biscuit::GPR mod = rec.scratch();
        biscuit::GPR ax = rec.getGPR(X86_REF_RAX, X86_SIZE_WORD);

        as.REMUW(mod, ax, src);
        as.DIVUW(ax, ax, src);

        rec.setGPR(X86_REF_RAX, X86_SIZE_BYTE, ax); // TODO: word write
        rec.setGPR(X86_REF_RAX, X86_SIZE_BYTE_HIGH, mod);
        break;
    }
    case X86_SIZE_WORD: {
        biscuit::GPR ax = rec.getGPR(X86_REF_RAX, X86_SIZE_WORD);
        biscuit::GPR dx = rec.getGPR(X86_REF_RDX, X86_SIZE_WORD);
        as.SLLIW(dx, dx, 16);
        as.OR(dx, dx, ax);

        as.DIVUW(ax, dx, src);
        as.REMUW(dx, dx, src);

        rec.setGPR(X86_REF_RAX, X86_SIZE_WORD, ax);
        rec.setGPR(X86_REF_RDX, X86_SIZE_WORD, dx);
        break;
    }
    case X86_SIZE_DWORD: {
        biscuit::GPR eax = rec.getGPR(X86_REF_RAX, X86_SIZE_DWORD);
        biscuit::GPR edx = rec.getGPR(X86_REF_RDX, X86_SIZE_QWORD);
        as.SLLI(edx, edx, 32);
        as.OR(edx, edx, eax);

        // This order is okay as RDX is modified last
        as.DIVU(eax, edx, src);
        as.REMU(edx, edx, src);

        rec.setGPR(X86_REF_RAX, X86_SIZE_DWORD, eax);
        rec.setGPR(X86_REF_RDX, X86_SIZE_DWORD, edx);
        break;
    }
    case X86_SIZE_QWORD: {
        // Most of the time the program doesn't actually want a 128-bit divide
        // and instead wants a 64-bit divide. If RDX is sign-extended from RAX then
        // we can use our RISC-V divide instruction rather than calling a function to emulate 128-bit div
        biscuit::GPR rax = rec.getGPR(X86_REF_RAX, X86_SIZE_QWORD);
        biscuit::GPR rdx = rec.getGPR(X86_REF_RDX, X86_SIZE_QWORD);
        biscuit::Label do_128bit, end;

        biscuit::GPR rax_sext = rec.scratch();
        as.SRAI(rax_sext, rax, 63);
        as.BNE(rax_sext, rdx, &do_128bit);
        rec.popScratch();

        // We need a slow 128-bit divide...

        biscuit::GPR mod = rec.scratch();
        biscuit::GPR div = rec.scratch();

        as.DIVU(div, rax, src);
        as.REMU(mod, rax, src);

        rec.setGPR(X86_REF_RAX, X86_SIZE_QWORD, div);
        rec.setGPR(X86_REF_RDX, X86_SIZE_QWORD, mod);

        rec.popScratch();
        rec.popScratch();

        as.J(&end);

        as.Bind(&do_128bit);
        rec.writebackState();
        as.MV(a1, src);
        as.MV(a0, rec.threadStatePointer());
        rec.callPointer(offsetof(ThreadState, felix86_divu128));
        rec.restoreState();

        as.Bind(&end);
        break;
    }
    default: {
        UNREACHABLE();
        break;
    }
    }
}

FAST_HANDLE(IDIV) {
    x86_size_e size = rec.getSize(&operands[0]);
    biscuit::GPR src = rec.getGPR(&operands[0]);

    switch (size) {
    case X86_SIZE_BYTE:
    case X86_SIZE_BYTE_HIGH: {
        biscuit::GPR mod = rec.scratch();
        biscuit::GPR divisor = rec.scratch();
        biscuit::GPR ax_sext = rec.scratch();
        biscuit::GPR ax = rec.getGPR(X86_REF_RAX, X86_SIZE_WORD);

        rec.sexth(ax_sext, ax);
        rec.sextb(divisor, src);

        as.REMW(mod, ax_sext, divisor);
        as.DIVW(ax, ax_sext, divisor);

        rec.popScratch();

        rec.setGPR(X86_REF_RAX, X86_SIZE_BYTE, ax);
        rec.setGPR(X86_REF_RAX, X86_SIZE_BYTE_HIGH, mod);
        break;
    }
    case X86_SIZE_WORD: {
        biscuit::GPR src_sext = rec.scratch();
        biscuit::GPR ax = rec.getGPR(X86_REF_RAX, X86_SIZE_WORD);
        biscuit::GPR dx = rec.getGPR(X86_REF_RDX, X86_SIZE_WORD);
        as.SLLIW(dx, dx, 16);
        as.OR(dx, dx, ax);

        rec.sexth(src_sext, src);

        as.DIVW(ax, dx, src_sext);
        as.REMW(dx, dx, src_sext);

        rec.setGPR(X86_REF_RAX, X86_SIZE_WORD, ax);
        rec.setGPR(X86_REF_RDX, X86_SIZE_WORD, dx);
        break;
    }
    case X86_SIZE_DWORD: {
        biscuit::GPR src_sext = rec.scratch();
        biscuit::GPR eax = rec.getGPR(X86_REF_RAX, X86_SIZE_DWORD);
        biscuit::GPR edx = rec.getGPR(X86_REF_RDX, X86_SIZE_QWORD);
        as.SLLI(edx, edx, 32);
        as.OR(edx, edx, eax);

        as.ADDIW(src_sext, src, 0);

        as.DIV(eax, edx, src_sext);
        as.REM(edx, edx, src_sext);

        rec.setGPR(X86_REF_RAX, X86_SIZE_DWORD, eax);
        rec.setGPR(X86_REF_RDX, X86_SIZE_DWORD, edx);
        break;
    }
    case X86_SIZE_QWORD: {
        // Most of the time the program doesn't actually want a 128-bit divide
        // and instead wants a 64-bit divide. If RDX is sign-extended from RAX then
        // we can use our RISC-V divide instruction rather than calling a function to emulate 128-bit div
        biscuit::GPR rax = rec.getGPR(X86_REF_RAX, X86_SIZE_QWORD);
        biscuit::GPR rdx = rec.getGPR(X86_REF_RDX, X86_SIZE_QWORD);
        biscuit::Label do_128bit, end;

        biscuit::GPR rax_sext = rec.scratch();
        as.SRAI(rax_sext, rax, 63);
        as.BNE(rax_sext, rdx, &do_128bit);
        rec.popScratch();

        // We need a slow 128-bit divide...

        biscuit::GPR mod = rec.scratch();
        biscuit::GPR div = rec.scratch();

        as.DIV(div, rax, src);
        as.REM(mod, rax, src);

        rec.setGPR(X86_REF_RAX, X86_SIZE_QWORD, div);
        rec.setGPR(X86_REF_RDX, X86_SIZE_QWORD, mod);

        rec.popScratch();
        rec.popScratch();

        as.J(&end);

        as.Bind(&do_128bit);
        rec.writebackState();
        as.MV(a1, src);
        as.MV(a0, rec.threadStatePointer());
        rec.callPointer(offsetof(ThreadState, felix86_div128));
        rec.restoreState();

        as.Bind(&end);
        break;
    }
    default: {
        UNREACHABLE();
        break;
    }
    }
}

FAST_HANDLE(TEST) {
    bool needs_pf = rec.shouldEmitFlag(rip, X86_REF_PF);
    bool needs_zf = rec.shouldEmitFlag(rip, X86_REF_ZF);
    bool needs_sf = rec.shouldEmitFlag(rip, X86_REF_SF);
    bool needs_cf = rec.shouldEmitFlag(rip, X86_REF_OF);
    bool needs_of = rec.shouldEmitFlag(rip, X86_REF_CF);
    bool needs_any_flag = needs_pf || needs_sf || needs_zf || needs_cf || needs_of;
    if (!needs_any_flag) {
        // Weirdly enough some x86 programs run test al, al and use no flags and overwrite them right after
        // Unsure if this is some sort of special nop or something, but in any case we don't warn about this
        return;
    }

    biscuit::GPR result = rec.scratch();

    biscuit::GPR src = rec.getGPR(&operands[1]);
    biscuit::GPR dst = rec.getGPR(&operands[0]);

    as.AND(result, dst, src);

    x86_size_e size = rec.getSize(&operands[0]);

    if (needs_cf) {
        rec.clearFlag(X86_REF_CF);
    }

    if (needs_pf) {
        rec.updateParity(result);
    }

    if (needs_zf) {
        rec.updateZero(result, size);
    }

    if (needs_sf) {
        rec.updateSign(result, size);
    }

    if (needs_of) {
        rec.clearFlag(X86_REF_OF);
    }
}

FAST_HANDLE(INC) {
    x86_size_e size = rec.getSize(&operands[0]);
    biscuit::GPR dst;
    biscuit::GPR res = rec.scratch();

    bool needs_atomic = operands[0].type == ZYDIS_OPERAND_TYPE_MEMORY && (instruction.attributes & ZYDIS_ATTRIB_HAS_LOCK);
    bool too_small_for_atomic = operands[0].size == 8 || operands[0].size == 16;
    bool writeback = true;
    if (needs_atomic && !too_small_for_atomic) {
        biscuit::GPR address = rec.lea(&operands[0]);
        biscuit::GPR one = rec.scratch();
        dst = rec.scratch();
        as.LI(one, 1);
        if (operands[0].size == 32) {
            as.AMOADD_W(Ordering::AQRL, dst, one, address);
        } else if (operands[0].size == 64) {
            as.AMOADD_D(Ordering::AQRL, dst, one, address);
        } else {
            UNREACHABLE();
        }
        as.ADDI(res, dst, 1); // Do the operation in the register as well to calculate the flags
        writeback = false;
    } else {
        if (needs_atomic) {
            WARN("Atomic INC with 8 or 16 bit operands encountered");
        }

        dst = rec.getGPR(&operands[0]);
        as.ADDI(res, dst, 1);
    }

    if (rec.shouldEmitFlag(rip, X86_REF_AF)) {
        rec.updateAuxiliaryAdd(dst, res);
    }

    if (rec.shouldEmitFlag(rip, X86_REF_OF)) {
        biscuit::GPR one = rec.scratch();
        as.LI(one, 1);
        rec.updateOverflowAdd(dst, one, res, size);
        rec.popScratch();
    }

    if (rec.shouldEmitFlag(rip, X86_REF_PF)) {
        rec.updateParity(res);
    }

    if (rec.shouldEmitFlag(rip, X86_REF_ZF)) {
        rec.updateZero(res, size);
    }

    if (rec.shouldEmitFlag(rip, X86_REF_SF)) {
        rec.updateSign(res, size);
    }

    if (writeback) {
        rec.setGPR(&operands[0], res);
    }
}

FAST_HANDLE(DEC) {
    x86_size_e size = rec.getSize(&operands[0]);
    biscuit::GPR dst;
    biscuit::GPR res = rec.scratch();

    bool needs_atomic = operands[0].type == ZYDIS_OPERAND_TYPE_MEMORY && (instruction.attributes & ZYDIS_ATTRIB_HAS_LOCK);
    bool too_small_for_atomic = operands[0].size == 8 || operands[0].size == 16;
    bool writeback = true;
    if (needs_atomic && !too_small_for_atomic) {
        biscuit::GPR address = rec.lea(&operands[0]);
        biscuit::GPR one = rec.scratch();
        dst = rec.scratch();
        as.LI(one, -1);
        if (operands[0].size == 32) {
            as.AMOADD_W(Ordering::AQRL, dst, one, address);
        } else if (operands[0].size == 64) {
            as.AMOADD_D(Ordering::AQRL, dst, one, address);
        } else {
            UNREACHABLE();
        }
        as.ADDI(res, dst, -1); // Do the operation in the register as well to calculate the flags
        writeback = false;
    } else {
        if (needs_atomic) {
            WARN("Atomic DEC with 8 or 16 bit operands encountered");
        }

        dst = rec.getGPR(&operands[0]);
        as.ADDI(res, dst, -1);
    }

    if (rec.shouldEmitFlag(rip, X86_REF_AF)) {
        biscuit::GPR one = rec.scratch();
        as.LI(one, 1);
        rec.updateAuxiliarySub(dst, one);
        rec.popScratch();
    }

    if (rec.shouldEmitFlag(rip, X86_REF_OF)) {
        biscuit::GPR one = rec.scratch();
        as.LI(one, 1);
        rec.updateOverflowSub(dst, one, res, size);
        rec.popScratch();
    }

    if (rec.shouldEmitFlag(rip, X86_REF_PF)) {
        rec.updateParity(res);
    }

    if (rec.shouldEmitFlag(rip, X86_REF_ZF)) {
        rec.updateZero(res, size);
    }

    if (rec.shouldEmitFlag(rip, X86_REF_SF)) {
        rec.updateSign(res, size);
    }

    if (writeback) {
        rec.setGPR(&operands[0], res);
    }
}

FAST_HANDLE(LAHF) {
    biscuit::GPR result = rec.scratch();
    biscuit::GPR scratch = rec.scratch();

    biscuit::GPR cf = rec.flag(X86_REF_CF);
    biscuit::GPR pf = rec.flag(X86_REF_PF);
    as.SLLI(scratch, pf, 2);
    as.OR(result, cf, scratch);

    biscuit::GPR af = rec.flag(X86_REF_AF);
    as.SLLI(scratch, af, 4);
    as.OR(result, result, scratch);

    biscuit::GPR zf = rec.flag(X86_REF_ZF);
    as.SLLI(scratch, zf, 6);
    as.OR(result, result, scratch);

    biscuit::GPR sf = rec.flag(X86_REF_SF);
    as.SLLI(scratch, sf, 7);
    as.OR(result, result, scratch);
    as.ORI(result, result, 0b10); // bit 1 is always set

    rec.setGPR(X86_REF_RAX, X86_SIZE_BYTE_HIGH, result);
}

FAST_HANDLE(SAHF) {
    biscuit::GPR cf = rec.flag(X86_REF_CF);
    biscuit::GPR af = rec.scratch();
    biscuit::GPR zf = rec.flag(X86_REF_ZF);
    biscuit::GPR sf = rec.flag(X86_REF_SF);
    biscuit::GPR ah = rec.getGPR(X86_REF_RAX, X86_SIZE_BYTE_HIGH);

    as.ANDI(cf, ah, 1);

    biscuit::GPR pf = rec.scratch();
    as.SRLI(pf, ah, 2);
    as.ANDI(pf, pf, 1);
    as.SB(pf, offsetof(ThreadState, pf), rec.threadStatePointer());

    as.SRLI(af, ah, 4);
    as.ANDI(af, af, 1);
    as.SB(af, offsetof(ThreadState, af), rec.threadStatePointer());

    as.SRLI(zf, ah, 6);
    as.ANDI(zf, zf, 1);

    as.SRLI(sf, ah, 7);
    as.ANDI(sf, sf, 1);
}

FAST_HANDLE(XCHG_lock) {
    biscuit::GPR address = rec.lea(&operands[0]);
    x86_size_e size = rec.getSize(&operands[0]);

    if (g_config.paranoid && size == X86_SIZE_WORD) {
        biscuit::Label ok;
        biscuit::GPR temp = rec.scratch();
        biscuit::GPR temp2 = rec.scratch();

        as.ANDI(temp, address, 0b11);
        as.LI(temp2, 0b11);
        as.BNE(temp, temp2, &ok);
        as.C_UNDEF();
        as.Bind(&ok);

        rec.popScratch();
        rec.popScratch();
    }

    biscuit::GPR src = rec.getGPR(&operands[1]);
    biscuit::GPR scratch = rec.scratch();
    biscuit::GPR dst = rec.scratch();

    switch (size) {
    case X86_SIZE_BYTE: {
        if (Extensions::Zabha) {
            WARN("Zabha is untested");
            as.AMOSWAP_B(Ordering::AQRL, dst, src, address);
        } else {
            Label loop;
            biscuit::GPR address_masked = rec.scratch();
            biscuit::GPR mask = rec.scratch();
            biscuit::GPR mask_shifted = rec.scratch();
            as.ANDI(address_masked, address, -4ll);
            as.SLLI(address, address, 3);
            as.LI(mask, 0xFF);
            as.SLLW(mask_shifted, mask, address);
            as.SLLW(src, src, address);

            as.Bind(&loop);
            as.LR_W(Ordering::AQRL, dst, address_masked);
            as.MV(scratch, src);
            as.XOR(scratch, scratch, dst);
            as.AND(scratch, scratch, mask_shifted);
            as.XOR(scratch, scratch, dst);
            as.SC_W(Ordering::AQRL, scratch, scratch, address_masked);
            as.BNEZ(scratch, &loop);
            as.SRLW(dst, dst, address);
            rec.popScratch();
            rec.popScratch();
            rec.popScratch();
        }
        break;
    }
    case X86_SIZE_WORD: {
        if (Extensions::Zabha) {
            WARN("Zabha is untested");
            as.AMOSWAP_H(Ordering::AQRL, dst, src, address);
        } else {
            Label loop, end, normal;
            biscuit::GPR address_masked = rec.scratch();
            biscuit::GPR mask = rec.scratch();
            biscuit::GPR mask_shifted = rec.scratch();

            as.ANDI(mask, address, 0b11);
            as.ADDI(mask, mask, -0b11);
            as.BNEZ(mask, &normal);

            // (Address & 0b11) == 0b11
            // This won't be properly emulated with lr.w/sc.w
            // We could use lr.d/sc.d, but then it wouldn't work for
            // (Address & 0b111) == 0b111
            // So whatever, let's handle both cases here
            as.LHU(scratch, 0, address);
            as.SH(src, 0, address);
            as.MV(dst, scratch);
            as.J(&end);

            as.Bind(&normal);
            as.ANDI(address_masked, address, -4ll);
            as.SLLI(address, address, 3);
            as.LI(mask, 0xFFFF);
            as.SLLW(mask_shifted, mask, address);
            as.SLLW(src, src, address);

            as.Bind(&loop);
            as.LR_W(Ordering::AQRL, dst, address_masked);
            as.MV(scratch, src);
            as.XOR(scratch, scratch, dst);
            as.AND(scratch, scratch, mask_shifted);
            as.XOR(scratch, scratch, dst);
            as.SC_W(Ordering::AQRL, scratch, scratch, address_masked);
            as.BNEZ(scratch, &loop);
            as.SRLW(dst, dst, address);

            as.Bind(&end);

            rec.popScratch();
            rec.popScratch();
            rec.popScratch();
        }
        break;
    }
    case X86_SIZE_DWORD: {
        as.MV(scratch, src);
        as.AMOSWAP_W(Ordering::AQRL, dst, scratch, address);
        break;
    }
    case X86_SIZE_QWORD: {
        as.MV(scratch, src);
        as.AMOSWAP_D(Ordering::AQRL, dst, scratch, address);
        break;
    }
    default: {
        UNREACHABLE();
        break;
    }
    }

    rec.setGPR(&operands[1], dst);
}

FAST_HANDLE(XCHG) {
    if (operands[0].type == ZYDIS_OPERAND_TYPE_MEMORY) {
        return fast_XCHG_lock(rec, rip, as, instruction, operands);
    }

    biscuit::GPR temp = rec.scratch();
    biscuit::GPR src = rec.getGPR(&operands[1]);
    biscuit::GPR dst = rec.getGPR(&operands[0]);

    as.MV(temp, src);

    rec.setGPR(&operands[1], dst);
    rec.setGPR(&operands[0], temp);
}

FAST_HANDLE(CLD) {
    as.SB(x0, offsetof(ThreadState, df), rec.threadStatePointer());
}

FAST_HANDLE(STD) {
    biscuit::GPR df = rec.scratch();
    as.LI(df, 1);
    as.SB(df, offsetof(ThreadState, df), rec.threadStatePointer());
}

FAST_HANDLE(CLC) {
    biscuit::GPR cf = rec.flag(X86_REF_CF);
    as.MV(cf, x0);
}

FAST_HANDLE(STC) {
    biscuit::GPR cf = rec.flag(X86_REF_CF);
    as.LI(cf, 1);
}

FAST_HANDLE(CBW) {
    biscuit::GPR al = rec.getGPR(X86_REF_RAX, X86_SIZE_BYTE);
    rec.sextb(al, al); // al is a scratch already
    rec.setGPR(X86_REF_RAX, X86_SIZE_WORD, al);
}

FAST_HANDLE(CWDE) {
    biscuit::GPR ax = rec.getGPR(X86_REF_RAX, X86_SIZE_QWORD);
    rec.sexth(ax, ax);
    rec.setGPR(X86_REF_RAX, X86_SIZE_DWORD, ax);
}

FAST_HANDLE(CDQE) {
    biscuit::GPR eax = rec.getGPR(X86_REF_RAX, X86_SIZE_QWORD);
    as.ADDIW(eax, eax, 0);
    rec.setGPR(X86_REF_RAX, X86_SIZE_QWORD, eax);
}

FAST_HANDLE(CWD) {
    biscuit::GPR sext = rec.scratch();
    biscuit::GPR ax = rec.getGPR(X86_REF_RAX, X86_SIZE_QWORD);
    rec.sexth(sext, ax);
    as.SRLI(sext, sext, 16);
    rec.setGPR(X86_REF_RDX, X86_SIZE_WORD, sext);
}

FAST_HANDLE(CDQ) {
    biscuit::GPR sext = rec.allocatedGPR(X86_REF_RDX);
    biscuit::GPR eax = rec.getGPR(X86_REF_RAX, X86_SIZE_QWORD);
    as.SRAIW(sext, eax, 31);
    rec.setGPR(X86_REF_RDX, X86_SIZE_DWORD, sext);
}

FAST_HANDLE(CQO) {
    biscuit::GPR sext = rec.allocatedGPR(X86_REF_RDX);
    biscuit::GPR rax = rec.getGPR(X86_REF_RAX, X86_SIZE_QWORD);
    as.SRAI(sext, rax, 63);
    rec.setGPR(X86_REF_RDX, X86_SIZE_QWORD, sext);
}

void JCC(Recompiler& rec, u64 rip, Assembler& as, ZydisDecodedInstruction& instruction, ZydisDecodedOperand* operands, biscuit::GPR cond) {
    u64 immediate = rec.sextImmediate(rec.getImmediate(&operands[0]), operands[0].imm.size);
    u64 address_false = rip + instruction.length;
    u64 address_true = address_false + immediate;
    rec.jumpAndLinkConditional(cond, address_true, address_false);
    rec.stopCompiling();
}

FAST_HANDLE(JO) {
    JCC(rec, rip, as, instruction, operands, rec.getCond(instruction.opcode & 0xF));
}

FAST_HANDLE(JNO) {
    JCC(rec, rip, as, instruction, operands, rec.getCond(instruction.opcode & 0xF));
}

FAST_HANDLE(JB) {
    JCC(rec, rip, as, instruction, operands, rec.getCond(instruction.opcode & 0xF));
}

FAST_HANDLE(JNB) {
    JCC(rec, rip, as, instruction, operands, rec.getCond(instruction.opcode & 0xF));
}

FAST_HANDLE(JZ) {
    JCC(rec, rip, as, instruction, operands, rec.getCond(instruction.opcode & 0xF));
}

FAST_HANDLE(JNZ) {
    JCC(rec, rip, as, instruction, operands, rec.getCond(instruction.opcode & 0xF));
}

FAST_HANDLE(JBE) {
    JCC(rec, rip, as, instruction, operands, rec.getCond(instruction.opcode & 0xF));
}

FAST_HANDLE(JNBE) {
    JCC(rec, rip, as, instruction, operands, rec.getCond(instruction.opcode & 0xF));
}

FAST_HANDLE(JP) {
    JCC(rec, rip, as, instruction, operands, rec.getCond(instruction.opcode & 0xF));
}

FAST_HANDLE(JNP) {
    JCC(rec, rip, as, instruction, operands, rec.getCond(instruction.opcode & 0xF));
}

FAST_HANDLE(JS) {
    JCC(rec, rip, as, instruction, operands, rec.getCond(instruction.opcode & 0xF));
}

FAST_HANDLE(JNS) {
    JCC(rec, rip, as, instruction, operands, rec.getCond(instruction.opcode & 0xF));
}

FAST_HANDLE(JL) {
    JCC(rec, rip, as, instruction, operands, rec.getCond(instruction.opcode & 0xF));
}

FAST_HANDLE(JNL) {
    JCC(rec, rip, as, instruction, operands, rec.getCond(instruction.opcode & 0xF));
}

FAST_HANDLE(JLE) {
    JCC(rec, rip, as, instruction, operands, rec.getCond(instruction.opcode & 0xF));
}

FAST_HANDLE(JNLE) {
    JCC(rec, rip, as, instruction, operands, rec.getCond(instruction.opcode & 0xF));
}

FAST_HANDLE(JRCXZ) {
    biscuit::GPR is_zero = rec.scratch();
    biscuit::GPR rcx = rec.getGPR(X86_REF_RCX, X86_SIZE_QWORD);
    as.SEQZ(is_zero, rcx);
    JCC(rec, rip, as, instruction, operands, is_zero);
}

FAST_HANDLE(JECXZ) {
    biscuit::GPR is_zero = rec.scratch();
    biscuit::GPR rcx = rec.getGPR(X86_REF_RCX, X86_SIZE_DWORD);
    as.SEQZ(is_zero, rcx);
    JCC(rec, rip, as, instruction, operands, is_zero);
}

FAST_HANDLE(JCXZ) {
    biscuit::GPR is_zero = rec.scratch();
    biscuit::GPR rcx = rec.getGPR(X86_REF_RCX, X86_SIZE_WORD);
    as.SEQZ(is_zero, rcx);
    JCC(rec, rip, as, instruction, operands, is_zero);
}

FAST_HANDLE(LOOP) {
    x86_size_e address_size = rec.zydisToSize(instruction.address_width);
    biscuit::GPR is_not_zero = rec.scratch();
    biscuit::GPR rcx = rec.getGPR(X86_REF_RCX, address_size);
    as.ADDI(rcx, rcx, -1);
    as.SNEZ(is_not_zero, rcx);
    rec.setGPR(X86_REF_RCX, address_size, rcx);
    JCC(rec, rip, as, instruction, operands, is_not_zero);
}

FAST_HANDLE(LOOPE) {
    biscuit::GPR zf = rec.flag(X86_REF_ZF);
    x86_size_e address_size = rec.zydisToSize(instruction.address_width);
    biscuit::GPR is_not_zero = rec.scratch();
    biscuit::GPR rcx = rec.getGPR(X86_REF_RCX, address_size);
    as.ADDI(rcx, rcx, -1);
    as.SNEZ(is_not_zero, rcx);
    as.AND(is_not_zero, is_not_zero, zf);
    rec.setGPR(X86_REF_RCX, address_size, rcx);
    JCC(rec, rip, as, instruction, operands, is_not_zero);
}

FAST_HANDLE(LOOPNE) {
    biscuit::GPR zf = rec.flag(X86_REF_ZF);
    biscuit::GPR not_zf = rec.scratch();
    x86_size_e address_size = rec.zydisToSize(instruction.address_width);
    biscuit::GPR is_not_zero = rec.scratch();
    biscuit::GPR rcx = rec.getGPR(X86_REF_RCX, address_size);
    as.ADDI(rcx, rcx, -1);
    as.SNEZ(is_not_zero, rcx);
    as.XORI(not_zf, zf, 1);
    as.AND(is_not_zero, is_not_zero, not_zf);
    rec.setGPR(X86_REF_RCX, address_size, rcx);
    JCC(rec, rip, as, instruction, operands, is_not_zero);
}

void CMOV(Recompiler& rec, u64 rip, Assembler& as, ZydisDecodedInstruction& instruction, ZydisDecodedOperand* operands, biscuit::GPR cond) {
    biscuit::GPR dst = rec.getGPR(&operands[0], X86_SIZE_QWORD);
    biscuit::GPR src;
    if (operands[1].type == ZYDIS_OPERAND_TYPE_REGISTER)
        src = rec.getGPR(&operands[1], X86_SIZE_QWORD);
    else
        src = rec.getGPR(&operands[1]);
    biscuit::GPR result = rec.scratch();
    if (instruction.operand_width == 64) {
        // Write directly to dst to save a move
        result = dst;
    }

    if (Extensions::Zicond) {
        biscuit::GPR tmp1 = rec.scratch();
        biscuit::GPR tmp2 = rec.scratch();
        as.CZERO_NEZ(tmp1, dst, cond);
        as.CZERO_EQZ(tmp2, src, cond);
        as.OR(result, tmp1, tmp2);
    } else {
        Label false_label;
        as.MV(result, dst);
        as.BEQZ(cond, &false_label);
        as.MV(result, src);
        as.Bind(&false_label);
    }

    rec.setGPR(&operands[0], result);
}

FAST_HANDLE(CMOVO) {
    CMOV(rec, rip, as, instruction, operands, rec.getCond(instruction.opcode & 0xF));
}

FAST_HANDLE(CMOVNO) {
    CMOV(rec, rip, as, instruction, operands, rec.getCond(instruction.opcode & 0xF));
}

FAST_HANDLE(CMOVB) {
    CMOV(rec, rip, as, instruction, operands, rec.getCond(instruction.opcode & 0xF));
}

FAST_HANDLE(CMOVNB) {
    CMOV(rec, rip, as, instruction, operands, rec.getCond(instruction.opcode & 0xF));
}

FAST_HANDLE(CMOVZ) {
    CMOV(rec, rip, as, instruction, operands, rec.getCond(instruction.opcode & 0xF));
}

FAST_HANDLE(CMOVNZ) {
    CMOV(rec, rip, as, instruction, operands, rec.getCond(instruction.opcode & 0xF));
}

FAST_HANDLE(CMOVBE) {
    CMOV(rec, rip, as, instruction, operands, rec.getCond(instruction.opcode & 0xF));
}

FAST_HANDLE(CMOVNBE) {
    CMOV(rec, rip, as, instruction, operands, rec.getCond(instruction.opcode & 0xF));
}

FAST_HANDLE(CMOVP) {
    CMOV(rec, rip, as, instruction, operands, rec.getCond(instruction.opcode & 0xF));
}

FAST_HANDLE(CMOVNP) {
    CMOV(rec, rip, as, instruction, operands, rec.getCond(instruction.opcode & 0xF));
}

FAST_HANDLE(CMOVS) {
    CMOV(rec, rip, as, instruction, operands, rec.getCond(instruction.opcode & 0xF));
}

FAST_HANDLE(CMOVNS) {
    CMOV(rec, rip, as, instruction, operands, rec.getCond(instruction.opcode & 0xF));
}

FAST_HANDLE(CMOVL) {
    CMOV(rec, rip, as, instruction, operands, rec.getCond(instruction.opcode & 0xF));
}

FAST_HANDLE(CMOVNL) {
    CMOV(rec, rip, as, instruction, operands, rec.getCond(instruction.opcode & 0xF));
}

FAST_HANDLE(CMOVLE) {
    CMOV(rec, rip, as, instruction, operands, rec.getCond(instruction.opcode & 0xF));
}

FAST_HANDLE(CMOVNLE) {
    CMOV(rec, rip, as, instruction, operands, rec.getCond(instruction.opcode & 0xF));
}

FAST_HANDLE(MOVSXD) {
    x86_size_e size = rec.getSize(&operands[1]);
    biscuit::GPR src = rec.getGPR(&operands[1]);

    if (size == X86_SIZE_DWORD) {
        biscuit::GPR dst = rec.allocatedGPR(rec.zydisToRef(operands[0].reg.value));
        as.ADDIW(dst, src, 0);
        rec.setGPR(&operands[0], dst);
    } else {
        UNREACHABLE(); // possible but why?
    }
}

FAST_HANDLE(IMUL_2_noflags) {
    x86_size_e size = rec.getSize(&operands[0]);
    biscuit::GPR dst = rec.getGPR(&operands[0], X86_SIZE_QWORD);
    biscuit::GPR src;
    if (operands[1].type == ZYDIS_OPERAND_TYPE_REGISTER)
        src = rec.getGPR(&operands[1], X86_SIZE_QWORD);
    else
        src = rec.getGPR(&operands[1]);

    switch (size) {
    case X86_SIZE_WORD: {
        biscuit::GPR result = rec.scratch();
        biscuit::GPR dst_sext = rec.scratch();
        biscuit::GPR src_sext = rec.scratch();
        rec.sexth(dst_sext, dst);
        rec.sexth(src_sext, src);
        as.MULW(result, dst_sext, src_sext);
        rec.setGPR(&operands[0], result);
        break;
    }
    case X86_SIZE_DWORD: {
        as.MULW(dst, dst, src);
        rec.setGPR(&operands[0], dst);
        break;
    }
    case X86_SIZE_QWORD: {
        as.MUL(dst, dst, src);
        rec.setGPR(&operands[0], dst);
        break;
    }
    default: {
        UNREACHABLE();
    }
    }
}

FAST_HANDLE(IMUL_3_noflags) {
    x86_size_e size = rec.getSize(&operands[0]);
    biscuit::GPR dst = rec.getGPR(&operands[0], X86_SIZE_QWORD);
    biscuit::GPR src1;
    if (operands[1].type == ZYDIS_OPERAND_TYPE_REGISTER)
        src1 = rec.getGPR(&operands[1], X86_SIZE_QWORD);
    else
        src1 = rec.getGPR(&operands[1]);
    biscuit::GPR src2 = rec.getGPR(&operands[2]);

    switch (size) {
    case X86_SIZE_WORD: {
        biscuit::GPR result = rec.scratch();
        biscuit::GPR src1_sext = rec.scratch();
        rec.sexth(src1_sext, src1);
        as.MULW(result, src1_sext, src2);
        rec.setGPR(&operands[0], result);
        break;
    }
    case X86_SIZE_DWORD: {
        as.MULW(dst, src1, src2);
        rec.setGPR(&operands[0], dst);
        break;
    }
    case X86_SIZE_QWORD: {
        as.MUL(dst, src1, src2);
        rec.setGPR(&operands[0], dst);
        break;
    }
    default: {
        UNREACHABLE();
    }
    }
}

FAST_HANDLE(IMUL) {
    x86_size_e size = rec.getSize(&operands[0]);
    u8 opcount = instruction.operand_count_visible;
    if (opcount == 1) {
        biscuit::GPR src = rec.getGPR(&operands[0]);
        switch (size) {
        case X86_SIZE_BYTE:
        case X86_SIZE_BYTE_HIGH: {
            biscuit::GPR result = rec.scratch();
            biscuit::GPR sext = rec.scratch();
            biscuit::GPR al = rec.getGPR(X86_REF_RAX, X86_SIZE_BYTE);
            rec.sextb(sext, al);
            rec.sextb(result, src);
            as.MULW(result, sext, result);
            rec.setGPR(X86_REF_RAX, X86_SIZE_WORD, result);

            if (rec.shouldEmitFlag(rip, X86_REF_CF) || rec.shouldEmitFlag(rip, X86_REF_OF)) {
                biscuit::GPR cf = rec.flag(X86_REF_CF);
                biscuit::GPR of = rec.flag(X86_REF_OF);
                rec.sextb(cf, result);
                as.XOR(of, cf, result);
                as.SNEZ(of, of);
                as.MV(cf, of);
            }
            break;
        }
        case X86_SIZE_WORD: {
            biscuit::GPR result = rec.scratch();
            biscuit::GPR sext = rec.scratch();
            biscuit::GPR ax = rec.getGPR(X86_REF_RAX, X86_SIZE_WORD);
            rec.sexth(sext, ax);
            rec.sexth(result, src);
            as.MULW(result, sext, result);
            rec.setGPR(X86_REF_RAX, X86_SIZE_WORD, result);

            if (rec.shouldEmitFlag(rip, X86_REF_CF) || rec.shouldEmitFlag(rip, X86_REF_OF)) {
                biscuit::GPR cf = rec.flag(X86_REF_CF);
                biscuit::GPR of = rec.flag(X86_REF_OF);

                rec.sexth(cf, result);
                as.XOR(of, cf, result);
                as.SNEZ(of, of);
                as.MV(cf, of);
            }

            as.SRAIW(result, result, 16);
            rec.setGPR(X86_REF_RDX, X86_SIZE_WORD, result);
            break;
        }
        case X86_SIZE_DWORD: {
            biscuit::GPR result = rec.scratch();
            biscuit::GPR sext = rec.scratch();
            biscuit::GPR eax = rec.getGPR(X86_REF_RAX, X86_SIZE_DWORD);
            as.ADDIW(sext, eax, 0);
            as.ADDIW(result, src, 0);
            as.MUL(result, sext, result);
            rec.setGPR(X86_REF_RAX, X86_SIZE_DWORD, result);

            if (rec.shouldEmitFlag(rip, X86_REF_CF) || rec.shouldEmitFlag(rip, X86_REF_OF)) {
                biscuit::GPR cf = rec.flag(X86_REF_CF);
                biscuit::GPR of = rec.flag(X86_REF_OF);

                as.ADDIW(cf, result, 0);
                as.XOR(of, cf, result);
                as.SNEZ(of, of);
                as.MV(cf, of);
            }

            as.SRLI(result, result, 32);
            rec.setGPR(X86_REF_RDX, X86_SIZE_DWORD, result);
            break;
        }
        case X86_SIZE_QWORD: {
            biscuit::GPR result = rec.scratch();
            biscuit::GPR rax = rec.getGPR(X86_REF_RAX, X86_SIZE_QWORD);
            as.MULH(result, rax, src);
            as.MUL(rax, rax, src);
            rec.setGPR(X86_REF_RAX, X86_SIZE_QWORD, rax);
            rec.setGPR(X86_REF_RDX, X86_SIZE_QWORD, result);

            if (rec.shouldEmitFlag(rip, X86_REF_CF) || rec.shouldEmitFlag(rip, X86_REF_OF)) {
                biscuit::GPR cf = rec.flag(X86_REF_CF);
                biscuit::GPR of = rec.flag(X86_REF_OF);

                as.SRAI(cf, rax, 63);
                as.XOR(of, cf, result);
                as.SNEZ(of, of);
                as.MV(cf, of);
            }
            break;
        }
        default: {
            UNREACHABLE();
            break;
        }
        }
    } else if (opcount == 2 || opcount == 3) {
        if (g_config.noflag_opts && !rec.shouldEmitFlag(rip, X86_REF_CF) && !rec.shouldEmitFlag(rip, X86_REF_OF)) {
            if (opcount == 2) {
                return fast_IMUL_2_noflags(rec, rip, as, instruction, operands);
            } else if (opcount == 3) {
                return fast_IMUL_3_noflags(rec, rip, as, instruction, operands);
            }
        }

        biscuit::GPR dst, src1, src2;
        if (opcount == 2) {
            dst = rec.getGPR(&operands[0]);
            src1 = dst;
            src2 = rec.getGPR(&operands[1]);
        } else {
            dst = rec.getGPR(&operands[0]);
            src1 = rec.getGPR(&operands[1]);
            src2 = rec.getGPR(&operands[2]);
        }
        switch (size) {
        case X86_SIZE_WORD: {
            biscuit::GPR result = rec.scratch();
            biscuit::GPR dst_sext = rec.scratch();
            rec.sexth(dst_sext, src1);
            rec.sexth(result, src2);
            as.MULW(result, result, dst_sext);
            rec.setGPR(&operands[0], result);

            biscuit::GPR cf = rec.flag(X86_REF_CF);
            biscuit::GPR of = rec.flag(X86_REF_OF);
            rec.sexth(cf, result);
            as.XOR(of, cf, result);
            as.SNEZ(of, of);
            as.MV(cf, of);
            break;
        }
        case X86_SIZE_DWORD: {
            biscuit::GPR result = rec.scratch();
            biscuit::GPR dst_sext = rec.scratch();
            as.ADDIW(dst_sext, src1, 0);
            as.ADDIW(result, src2, 0);
            as.MUL(result, result, dst_sext);
            rec.setGPR(&operands[0], result);

            biscuit::GPR cf = rec.flag(X86_REF_CF);
            biscuit::GPR of = rec.flag(X86_REF_OF);
            as.ADDIW(cf, result, 0);
            as.XOR(of, cf, result);
            as.SNEZ(of, of);
            as.MV(cf, of);
            break;
        }
        case X86_SIZE_QWORD: {
            biscuit::GPR result = rec.scratch();
            biscuit::GPR result_low = rec.scratch();
            as.MULH(result, src1, src2);
            as.MUL(result_low, src1, src2);
            rec.setGPR(&operands[0], result_low);

            biscuit::GPR cf = rec.flag(X86_REF_CF);
            biscuit::GPR of = rec.flag(X86_REF_OF);
            as.SRAI(cf, result_low, 63);
            as.XOR(of, cf, result);
            as.SNEZ(of, of);
            as.MV(cf, of);
            break;
        }
        default: {
            UNREACHABLE();
            break;
        }
        }
    } else {
        UNREACHABLE();
    }
}

FAST_HANDLE(MUL) {
    x86_size_e size = rec.getSize(&operands[0]);
    biscuit::GPR src = rec.getGPR(&operands[0]);
    switch (size) {
    case X86_SIZE_BYTE:
    case X86_SIZE_BYTE_HIGH: {
        biscuit::GPR result = rec.scratch();
        biscuit::GPR al = rec.getGPR(X86_REF_RAX, X86_SIZE_BYTE);
        as.MULW(result, al, src);
        rec.setGPR(X86_REF_RAX, X86_SIZE_WORD, result);

        if (rec.shouldEmitFlag(rip, X86_REF_CF) || rec.shouldEmitFlag(rip, X86_REF_OF)) {
            biscuit::GPR cf = rec.flag(X86_REF_CF);
            biscuit::GPR of = rec.flag(X86_REF_OF);
            // 8 * 8 bit can only be 16 bit so we don't need to zero extend
            as.SRLI(cf, result, 8);
            as.SNEZ(cf, cf);
            as.MV(of, cf);
        }
        break;
    }
    case X86_SIZE_WORD: {
        biscuit::GPR result = rec.scratch();
        biscuit::GPR ax = rec.getGPR(X86_REF_RAX, X86_SIZE_WORD);
        as.MULW(result, ax, src);
        rec.setGPR(X86_REF_RAX, X86_SIZE_WORD, result);

        as.SRLIW(result, result, 16);

        if (rec.shouldEmitFlag(rip, X86_REF_CF) || rec.shouldEmitFlag(rip, X86_REF_OF)) {
            biscuit::GPR cf = rec.flag(X86_REF_CF);
            biscuit::GPR of = rec.flag(X86_REF_OF);
            // Should be already zexted due to srliw
            as.SNEZ(cf, result);
            as.MV(of, cf);
        }

        rec.setGPR(X86_REF_RDX, X86_SIZE_WORD, result);
        break;
    }
    case X86_SIZE_DWORD: {
        biscuit::GPR result = rec.scratch();
        biscuit::GPR eax = rec.getGPR(X86_REF_RAX, X86_SIZE_DWORD);
        as.MUL(result, eax, src);
        rec.setGPR(X86_REF_RAX, X86_SIZE_DWORD, result);
        as.SRLI(result, result, 32);

        if (rec.shouldEmitFlag(rip, X86_REF_CF) || rec.shouldEmitFlag(rip, X86_REF_OF)) {
            biscuit::GPR cf = rec.flag(X86_REF_CF);
            biscuit::GPR of = rec.flag(X86_REF_OF);

            as.SNEZ(cf, result);
            as.MV(of, cf);
        }

        rec.setGPR(X86_REF_RDX, X86_SIZE_DWORD, result);
        break;
    }
    case X86_SIZE_QWORD: {
        biscuit::GPR result = rec.scratch();
        biscuit::GPR rax = rec.getGPR(X86_REF_RAX, X86_SIZE_QWORD);
        as.MULHU(result, rax, src);
        as.MUL(rax, rax, src);
        rec.setGPR(X86_REF_RAX, X86_SIZE_QWORD, rax);
        rec.setGPR(X86_REF_RDX, X86_SIZE_QWORD, result);

        if (rec.shouldEmitFlag(rip, X86_REF_CF) || rec.shouldEmitFlag(rip, X86_REF_OF)) {
            biscuit::GPR cf = rec.flag(X86_REF_CF);
            biscuit::GPR of = rec.flag(X86_REF_OF);

            as.SNEZ(cf, result);
            as.MV(of, cf);
        }
        break;
    }
    default: {
        UNREACHABLE();
        break;
    }
    }
}

void PUNPCKH(Recompiler& rec, Assembler& as, ZydisDecodedInstruction& instruction, ZydisDecodedOperand* operands, SEW sew, u8 vlen) {
    // Like PUNPCKL but we add a number to iota to pick the high elements
    bool is_mmx = operands[0].reg.value >= ZYDIS_REGISTER_MM0 && operands[0].reg.value <= ZYDIS_REGISTER_MM7;
    int num = 0;
    int size = 0;
    biscuit::GPR shift = rec.scratch();
    switch (sew) {
    case SEW::E8: {
        num = 8;
        size = 8;
        break;
    }
    case SEW::E16: {
        num = 4;
        size = 16;
        break;
    }
    case SEW::E32: {
        as.LI(shift, 32);
        num = 2;
        break;
    }
    default: {
        UNREACHABLE();
        break;
    }
    }

    if (is_mmx) {
        num /= 2;
    }

    biscuit::Vec dst = rec.getVec(&operands[0]);
    biscuit::Vec src = rec.getVec(&operands[1]);

    // Pick even scratch registers for the widening add (can't use MF2, ruins 128 VLEN)
    biscuit::Vec temp1 = v26;
    biscuit::Vec temp2 = v28;
    biscuit::Vec dst_down = v30;
    biscuit::Vec src_down = v31;

    rec.setVectorState(sew, vlen);
    as.VSLIDEDOWN(dst_down, dst, num);
    as.VSLIDEDOWN(src_down, src, num);
    as.VWADDU(temp1, dst_down, x0);
    as.VWADDU(temp2, src_down, x0);

    rec.setVectorState(SEW::E64, 2);
    if (sew == SEW::E32) {
        as.VSLL(temp2, temp2, shift);
    } else {
        as.VSLL(temp2, temp2, size);
    }
    as.VOR(dst, temp1, temp2);

    rec.setVec(&operands[0], dst);
}

FAST_HANDLE(PUNPCKLBW) {
    biscuit::Vec dst = rec.getVec(&operands[0]);
    biscuit::Vec src = rec.getVec(&operands[1]);
    biscuit::Vec temp1 = rec.scratchVec();
    biscuit::Vec temp2 = rec.scratchVec();

    rec.setVectorState(SEW::E8, 16, LMUL::MF2);
    as.VWADDU(temp1, dst, x0);
    as.VWADDU(temp2, src, x0);
    rec.setVectorState(SEW::E64, 2);
    as.VSLL(temp2, temp2, 8);
    as.VOR(dst, temp1, temp2);

    rec.setVec(&operands[0], dst);
}

FAST_HANDLE(PUNPCKLWD) {
    biscuit::Vec dst = rec.getVec(&operands[0]);
    biscuit::Vec src = rec.getVec(&operands[1]);
    biscuit::Vec temp1 = rec.scratchVec();
    biscuit::Vec temp2 = rec.scratchVec();

    rec.setVectorState(SEW::E16, 8, LMUL::MF2);
    as.VWADDU(temp1, dst, x0);
    as.VWADDU(temp2, src, x0);
    rec.setVectorState(SEW::E64, 2);
    as.VSLL(temp2, temp2, 16);
    as.VOR(dst, temp1, temp2);

    rec.setVec(&operands[0], dst);
}

FAST_HANDLE(PUNPCKLDQ) {
    biscuit::GPR shift = rec.scratch();
    biscuit::Vec dst = rec.getVec(&operands[0]);
    biscuit::Vec src = rec.getVec(&operands[1]);
    biscuit::Vec temp1 = rec.scratchVec();
    biscuit::Vec temp2 = rec.scratchVec();

    as.LI(shift, 32);
    rec.setVectorState(SEW::E32, 4, LMUL::MF2);
    as.VWADDU(temp1, dst, x0);
    as.VWADDU(temp2, src, x0);
    rec.setVectorState(SEW::E64, 2);
    as.VSLL(temp2, temp2, shift);
    as.VOR(dst, temp1, temp2);

    rec.setVec(&operands[0], dst);
}

FAST_HANDLE(PUNPCKLQDQ) {
    biscuit::Vec dst = rec.getVec(&operands[0]);
    biscuit::Vec src = rec.getVec(&operands[1]);

    rec.setVectorState(SEW::E64, 2);
    if (dst == src) { // VSLIDEUP dst/src overlap limitations
        src = rec.scratchVec();
        as.VMV(src, dst);
    }

    as.VSLIDEUP(dst, src, 1);

    rec.setVec(operands, dst);
}

FAST_HANDLE(PUNPCKHBW) {
    PUNPCKH(rec, as, instruction, operands, SEW::E8, 16);
}

FAST_HANDLE(PUNPCKHWD) {
    PUNPCKH(rec, as, instruction, operands, SEW::E16, 8);
}

FAST_HANDLE(PUNPCKHDQ) {
    PUNPCKH(rec, as, instruction, operands, SEW::E32, 4);
}

FAST_HANDLE(PUNPCKHQDQ) {
    biscuit::Vec temp = rec.scratchVec();
    biscuit::Vec dst = rec.getVec(&operands[0]);
    biscuit::Vec src = rec.getVec(&operands[1]);

    rec.setVectorState(SEW::E64, 2);
    as.VMV(v0, 0b10);
    as.VSLIDE1DOWN(temp, dst, x0);
    as.VMERGE(dst, temp, src);

    rec.setVec(&operands[0], dst);
}

FAST_HANDLE(UNPCKLPS) {
    biscuit::Vec scratch = rec.scratchVec();
    biscuit::Vec iota = rec.scratchVec();
    biscuit::Vec src1 = rec.getVec(&operands[0]);
    biscuit::Vec src2 = rec.getVec(&operands[1]);

    rec.setVectorState(SEW::E32, 4);
    as.VMV(scratch, 0);
    as.VMV(v0, 0b0101);
    as.VIOTA(iota, v0);
    as.VRGATHER(scratch, src1, iota, VecMask::Yes);
    as.VMV(v0, 0b1010);
    as.VIOTA(iota, v0);
    as.VRGATHER(scratch, src2, iota, VecMask::Yes);

    rec.setVec(&operands[0], scratch);
}

FAST_HANDLE(UNPCKHPS) {
    biscuit::Vec scratch = rec.scratchVec();
    biscuit::Vec iota = rec.scratchVec();
    biscuit::Vec src1 = rec.getVec(&operands[0]);
    biscuit::Vec src2 = rec.getVec(&operands[1]);

    rec.setVectorState(SEW::E32, 4);
    as.VMV(scratch, 0);
    as.VMV(v0, 0b0101);
    as.VIOTA(iota, v0);
    as.VADD(iota, iota, 2);
    as.VRGATHER(scratch, src1, iota, VecMask::Yes);
    as.VMV(v0, 0b1010);
    as.VIOTA(iota, v0);
    as.VADD(iota, iota, 2);
    as.VRGATHER(scratch, src2, iota, VecMask::Yes);

    rec.setVec(&operands[0], scratch);
}

FAST_HANDLE(UNPCKLPD) {
    biscuit::Vec scratch = rec.scratchVec();
    biscuit::Vec result = rec.scratchVec();
    biscuit::Vec src1 = rec.getVec(&operands[0]);
    biscuit::Vec src2 = rec.getVec(&operands[1]);

    rec.setVectorState(SEW::E64, 2);
    as.VSLIDEUP(scratch, src2, 1);
    as.VMV(v0, 0b10);
    as.VMERGE(result, src1, scratch);

    rec.setVec(&operands[0], result);
}

FAST_HANDLE(UNPCKHPD) {
    biscuit::Vec scratch = rec.scratchVec();
    biscuit::Vec result = rec.scratchVec();
    biscuit::Vec src1 = rec.getVec(&operands[0]);
    biscuit::Vec src2 = rec.getVec(&operands[1]);

    rec.setVectorState(SEW::E64, 2);
    as.VSLIDEDOWN(scratch, src1, 1);
    as.VMV(v0, 0b10);
    as.VMERGE(result, scratch, src2);

    rec.setVec(&operands[0], result);
}

FAST_HANDLE(VECTOR_MOV) {
    if (operands[1].type == ZYDIS_OPERAND_TYPE_REGISTER) {
        if (operands[0].type == ZYDIS_OPERAND_TYPE_REGISTER && operands[0].reg.value == operands[1].reg.value) {
            WARN("vmov from and to same reg?");
        }

        biscuit::Vec src = rec.getVec(&operands[1]);
        rec.setVec(&operands[0], src);
    } else {
        // Operand 1 is memory, so operand 0 must be register
        ASSERT(operands[0].type == ZYDIS_OPERAND_TYPE_REGISTER);
        // Load directly to register to avoid a vmv1r
        biscuit::Vec dst = rec.allocatedVec(rec.zydisToRef(operands[0].reg.value));
        int size = operands[0].size;
        ASSERT(operands[0].size == operands[1].size);
        ASSERT(operands[0].size > 64);
        biscuit::GPR address = rec.lea(&operands[1], false);
        rec.readMemory(dst, address, size);
        rec.setVec(&operands[0], dst);
    }
}

FAST_HANDLE(MOVAPD) {
    fast_VECTOR_MOV(rec, rip, as, instruction, operands);
}

FAST_HANDLE(MOVAPS) {
    fast_VECTOR_MOV(rec, rip, as, instruction, operands);
}

FAST_HANDLE(MOVUPD) {
    fast_VECTOR_MOV(rec, rip, as, instruction, operands);
}

FAST_HANDLE(MOVUPS) {
    fast_VECTOR_MOV(rec, rip, as, instruction, operands);
}

FAST_HANDLE(MOVDQA) {
    fast_VECTOR_MOV(rec, rip, as, instruction, operands);
}

FAST_HANDLE(MOVDQU) {
    fast_VECTOR_MOV(rec, rip, as, instruction, operands);
}

FAST_HANDLE(RDTSC) {
    biscuit::GPR tsc = rec.scratch();
    as.RDTIME(tsc);
    rec.setGPR(X86_REF_RAX, X86_SIZE_DWORD, tsc);
    as.SRLI(tsc, tsc, 32);
    rec.setGPR(X86_REF_RDX, X86_SIZE_QWORD, tsc);
}

FAST_HANDLE(CPUID) {
    rec.writebackState();
    as.MV(a0, rec.threadStatePointer());
    rec.callPointer(offsetof(ThreadState, felix86_cpuid));
    rec.restoreState();
}

FAST_HANDLE(SYSCALL) {
    if (!g_config.strace && g_config.inline_syscalls) {
        bool inlined = rec.tryInlineSyscall();
        if (inlined) {
            return;
        }
    }

    biscuit::GPR rcx = rec.allocatedGPR(X86_REF_RCX);
    as.LI(rcx, rip + instruction.length);
    rec.setGPR(X86_REF_RCX, X86_SIZE_QWORD, rcx);

    // Normally the syscall instruction also writes the flags to R11 but we don't need them in our syscall handler
    rec.writebackState();
    as.MV(a0, sp);
    rec.callPointer(offsetof(ThreadState, felix86_syscall));
    rec.restoreState();
}

FAST_HANDLE(INT) {
    ASSERT(operands[0].type == ZYDIS_OPERAND_TYPE_IMMEDIATE);
    ASSERT(operands[0].imm.value.u == 0x80);

    rec.writebackState();
    as.MV(a0, sp);
    as.LI(a1, rip + instruction.length);
    rec.callPointer(offsetof(ThreadState, felix86_syscall32));
    rec.restoreState();
}

FAST_HANDLE(MOVZX) {
    x86_size_e size_dst = rec.getSize(&operands[0]);
    x86_size_e size_src = rec.getSize(&operands[1]);
    biscuit::GPR dst = rec.getGPR(&operands[0], X86_SIZE_QWORD);
    if (operands[1].type == ZYDIS_OPERAND_TYPE_REGISTER) {
        biscuit::GPR src = rec.getGPR(&operands[1], X86_SIZE_QWORD);
        if (size_dst == X86_SIZE_WORD) {
            // Need to preserve top bits
            biscuit::GPR result = rec.scratch();
            if (size_src == X86_SIZE_BYTE_HIGH) {
                as.SRLI(result, src, 8);
                as.ANDI(result, result, 0xFF);
            } else if (size_src == X86_SIZE_BYTE) {
                as.ANDI(result, src, 0xFF);
            } else {
                UNREACHABLE();
            }
            as.SRLI(dst, dst, 16);
            as.SLLI(dst, dst, 16);
            as.OR(dst, dst, result);
        } else {
            if (size_src == X86_SIZE_BYTE_HIGH) {
                as.SRLI(dst, src, 8);
                as.ANDI(dst, dst, 0xFF);
            } else {
                rec.zext(dst, src, size_src);
            }
            rec.setGPR(rec.zydisToRef(operands[0].reg.value), X86_SIZE_QWORD, dst);
        }
    } else {
        biscuit::GPR address = rec.lea(&operands[1], false);
        if (size_dst == X86_SIZE_WORD) {
            biscuit::GPR result = rec.scratch();
            rec.readMemory(result, address, 0, size_src);
            as.SRLI(dst, dst, 16);
            as.SLLI(dst, dst, 16);
            as.OR(dst, dst, result);
        } else {
            rec.readMemory(dst, address, 0, size_src);
        }
    }
}

FAST_HANDLE(PXOR) {
    biscuit::Vec dst = rec.getVec(&operands[0]);
    biscuit::Vec src = rec.getVec(&operands[1]);
    rec.setVectorState(SEW::E64, 2);
    as.VXOR(dst, dst, src);
    rec.setVec(&operands[0], dst);
}

FAST_HANDLE(MOVNTDQ) {
    fast_VECTOR_MOV(rec, rip, as, instruction, operands);
}

FAST_HANDLE(MOVNTQ) {
    biscuit::Vec mm = rec.getVec(&operands[1]);
    rec.setVec(&operands[0], mm);
}

FAST_HANDLE(MOVNTDQA) {
    fast_VECTOR_MOV(rec, rip, as, instruction, operands);
}

FAST_HANDLE(MOVNTI) {
    biscuit::GPR src = rec.getGPR(&operands[1]);
    rec.setGPR(&operands[0], src);
}

FAST_HANDLE(MOVNTPD) {
    fast_VECTOR_MOV(rec, rip, as, instruction, operands);
}

FAST_HANDLE(MOVNTPS) {
    fast_VECTOR_MOV(rec, rip, as, instruction, operands);
}

FAST_HANDLE(PAND) {
    biscuit::Vec dst = rec.getVec(&operands[0]);
    biscuit::Vec src = rec.getVec(&operands[1]);
    rec.setVectorState(SEW::E64, 2);
    as.VAND(dst, dst, src);
    rec.setVec(&operands[0], dst);
}

FAST_HANDLE(POR) {
    biscuit::Vec dst = rec.getVec(&operands[0]);
    biscuit::Vec src = rec.getVec(&operands[1]);
    rec.setVectorState(SEW::E64, 2);
    as.VOR(dst, dst, src);
    rec.setVec(&operands[0], dst);
}

FAST_HANDLE(PANDN) {
    biscuit::Vec dst_not = rec.scratchVec();
    biscuit::Vec dst = rec.getVec(&operands[0]);
    biscuit::Vec src = rec.getVec(&operands[1]);
    rec.setVectorState(SEW::E64, 2);
    if (Extensions::Zvbb) {
        WARN_ONCE("PANDN + Zvbb is untested, please run tests and report results");
        as.VANDN(dst, src, dst);
    } else {
        as.VXOR(dst_not, dst, -1);
        as.VAND(dst, dst_not, src);
    }
    rec.setVec(&operands[0], dst);
}

FAST_HANDLE(ANDPS) {
    fast_PAND(rec, rip, as, instruction, operands);
}

FAST_HANDLE(ANDPD) {
    fast_PAND(rec, rip, as, instruction, operands);
}

FAST_HANDLE(ORPS) {
    fast_POR(rec, rip, as, instruction, operands);
}

FAST_HANDLE(ORPD) {
    fast_POR(rec, rip, as, instruction, operands);
}

FAST_HANDLE(XORPS) {
    fast_PXOR(rec, rip, as, instruction, operands);
}

FAST_HANDLE(XORPD) {
    fast_PXOR(rec, rip, as, instruction, operands);
}

FAST_HANDLE(ANDNPS) {
    fast_PANDN(rec, rip, as, instruction, operands);
}

FAST_HANDLE(ANDNPD) {
    fast_PANDN(rec, rip, as, instruction, operands);
}

void PADD(Recompiler& rec, u64 rip, Assembler& as, ZydisDecodedInstruction& instruction, ZydisDecodedOperand* operands, SEW sew, u8 vlen) {
    biscuit::Vec dst = rec.getVec(&operands[0]);
    biscuit::Vec src = rec.getVec(&operands[1]);
    rec.setVectorState(sew, vlen);
    as.VADD(dst, dst, src);
    rec.setVec(&operands[0], dst);
}

void PADDS(Recompiler& rec, u64 rip, Assembler& as, ZydisDecodedInstruction& instruction, ZydisDecodedOperand* operands, SEW sew, u8 vlen) {
    biscuit::Vec dst = rec.getVec(&operands[0]);
    biscuit::Vec src = rec.getVec(&operands[1]);
    rec.setVectorState(sew, vlen);
    as.VSADD(dst, dst, src);
    rec.setVec(&operands[0], dst);
}

void PADDSU(Recompiler& rec, u64 rip, Assembler& as, ZydisDecodedInstruction& instruction, ZydisDecodedOperand* operands, SEW sew, u8 vlen) {
    biscuit::Vec dst = rec.getVec(&operands[0]);
    biscuit::Vec src = rec.getVec(&operands[1]);
    rec.setVectorState(sew, vlen);
    as.VSADDU(dst, dst, src);
    rec.setVec(&operands[0], dst);
}

void PSUBS(Recompiler& rec, u64 rip, Assembler& as, ZydisDecodedInstruction& instruction, ZydisDecodedOperand* operands, SEW sew, u8 vlen) {
    biscuit::Vec dst = rec.getVec(&operands[0]);
    biscuit::Vec src = rec.getVec(&operands[1]);
    rec.setVectorState(sew, vlen);
    as.VSSUB(dst, dst, src);
    rec.setVec(&operands[0], dst);
}

void PSUBSU(Recompiler& rec, u64 rip, Assembler& as, ZydisDecodedInstruction& instruction, ZydisDecodedOperand* operands, SEW sew, u8 vlen) {
    biscuit::Vec dst = rec.getVec(&operands[0]);
    biscuit::Vec src = rec.getVec(&operands[1]);
    rec.setVectorState(sew, vlen);
    as.VSSUBU(dst, dst, src);
    rec.setVec(&operands[0], dst);
}

void PSUB(Recompiler& rec, u64 rip, Assembler& as, ZydisDecodedInstruction& instruction, ZydisDecodedOperand* operands, SEW sew, u8 vlen) {
    biscuit::Vec dst = rec.getVec(&operands[0]);
    biscuit::Vec src = rec.getVec(&operands[1]);
    rec.setVectorState(sew, vlen);
    as.VSUB(dst, dst, src);
    rec.setVec(&operands[0], dst);
}

FAST_HANDLE(PADDB) {
    PADD(rec, rip, as, instruction, operands, SEW::E8, 16);
}

FAST_HANDLE(PADDW) {
    PADD(rec, rip, as, instruction, operands, SEW::E16, 8);
}

FAST_HANDLE(PADDD) {
    PADD(rec, rip, as, instruction, operands, SEW::E32, 4);
}

FAST_HANDLE(PADDQ) {
    PADD(rec, rip, as, instruction, operands, SEW::E64, 2);
}

FAST_HANDLE(PADDSB) {
    PADDS(rec, rip, as, instruction, operands, SEW::E8, 16);
}

FAST_HANDLE(PADDSW) {
    PADDS(rec, rip, as, instruction, operands, SEW::E16, 8);
}

FAST_HANDLE(PSUBSB) {
    PSUBS(rec, rip, as, instruction, operands, SEW::E8, 16);
}

FAST_HANDLE(PSUBSW) {
    PSUBS(rec, rip, as, instruction, operands, SEW::E16, 8);
}

FAST_HANDLE(PADDUSB) {
    PADDSU(rec, rip, as, instruction, operands, SEW::E8, 16);
}

FAST_HANDLE(PADDUSW) {
    PADDSU(rec, rip, as, instruction, operands, SEW::E16, 8);
}

FAST_HANDLE(PSUBUSB) {
    PSUBSU(rec, rip, as, instruction, operands, SEW::E8, 16);
}

FAST_HANDLE(PSUBUSW) {
    PSUBSU(rec, rip, as, instruction, operands, SEW::E16, 8);
}

FAST_HANDLE(PSUBB) {
    PSUB(rec, rip, as, instruction, operands, SEW::E8, 16);
}

FAST_HANDLE(PSUBW) {
    PSUB(rec, rip, as, instruction, operands, SEW::E16, 8);
}

FAST_HANDLE(PSUBD) {
    PSUB(rec, rip, as, instruction, operands, SEW::E32, 4);
}

FAST_HANDLE(PSUBQ) {
    PSUB(rec, rip, as, instruction, operands, SEW::E64, 2);
}

FAST_HANDLE(ADDPS) {
    biscuit::Vec dst = rec.getVec(&operands[0]);
    biscuit::Vec src = rec.getVec(&operands[1]);
    rec.setVectorState(SEW::E32, 4);
    as.VFADD(dst, dst, src);
    rec.setVec(&operands[0], dst);
}

FAST_HANDLE(ADDPD) {
    biscuit::Vec dst = rec.getVec(&operands[0]);
    biscuit::Vec src = rec.getVec(&operands[1]);
    rec.setVectorState(SEW::E64, 2);
    as.VFADD(dst, dst, src);
    rec.setVec(&operands[0], dst);
}

FAST_HANDLE(SUBPS) {
    biscuit::Vec dst = rec.getVec(&operands[0]);
    biscuit::Vec src = rec.getVec(&operands[1]);
    rec.setVectorState(SEW::E32, 4);
    as.VFSUB(dst, dst, src);
    rec.setVec(&operands[0], dst);
}

FAST_HANDLE(SUBPD) {
    biscuit::Vec dst = rec.getVec(&operands[0]);
    biscuit::Vec src = rec.getVec(&operands[1]);
    rec.setVectorState(SEW::E64, 2);
    as.VFSUB(dst, dst, src);
    rec.setVec(&operands[0], dst);
}

FAST_HANDLE(MINPS) {
    if (g_config.inaccurate_minmax && !g_config.paranoid) {
        biscuit::Vec dst = rec.getVec(&operands[0]);
        biscuit::Vec src = rec.getVec(&operands[1]);
        rec.setVectorState(SEW::E32, 4);
        as.VFMIN(dst, dst, src);
        rec.setVec(&operands[0], dst);
        return;
    }

    biscuit::Vec dst = rec.getVec(&operands[0]);
    biscuit::Vec src = rec.getVec(&operands[1]);
    biscuit::Vec nan_mask_1 = rec.scratchVec();
    biscuit::Vec nan_mask_2 = rec.scratchVec();
    biscuit::Vec equal_mask = rec.scratchVec();
    biscuit::Vec zero_mask = rec.scratchVec();
    biscuit::Vec neg_zero_mask = rec.scratchVec();
    biscuit::FPR temp1 = rec.scratchFPR();
    biscuit::FPR temp2 = rec.scratchFPR();
    rec.setVectorState(SEW::E32, 4);

    // When either operand is NaN, or they are both 0.0 or both are -0.0, the result is the source
    as.VMFNE(nan_mask_1, dst, dst); // When a register isn't equal to itself, that element must be NaN
    as.VMFNE(nan_mask_2, src, src);
    as.VMOR(nan_mask_1, nan_mask_1, nan_mask_2);
    as.FMV_W_X(temp1, x0);                        // 0.0
    as.FSGNJN_S(temp2, temp1, temp1);             // -0.0
    as.VMFEQ(equal_mask, dst, src);               // Check where they are equal
    as.VMFEQ(zero_mask, dst, temp1);              // Check where dst is 0.0
    as.VMFEQ(neg_zero_mask, dst, temp2);          // Check where dst is -0.0
    as.VMOR(zero_mask, zero_mask, neg_zero_mask); // Either 0.0 or -0.0
    as.VMAND(equal_mask, equal_mask, zero_mask);  // Check where they are both zeroes
    as.VMOR(v0, nan_mask_1, equal_mask);          // Combine the masks

    as.VFMIN(nan_mask_2, dst, src);        // actual max result calculation
    as.VMERGE(zero_mask, nan_mask_2, src); // Where v0 is 1's, use src, otherwise use result of vfmax
    rec.setVec(&operands[0], zero_mask);
}

FAST_HANDLE(MINPD) {
    if (g_config.inaccurate_minmax && !g_config.paranoid) {
        biscuit::Vec dst = rec.getVec(&operands[0]);
        biscuit::Vec src = rec.getVec(&operands[1]);
        rec.setVectorState(SEW::E64, 2);
        as.VFMIN(dst, dst, src);
        rec.setVec(&operands[0], dst);
        return;
    }

    biscuit::Vec dst = rec.getVec(&operands[0]);
    biscuit::Vec src = rec.getVec(&operands[1]);
    biscuit::Vec nan_mask_1 = rec.scratchVec();
    biscuit::Vec nan_mask_2 = rec.scratchVec();
    biscuit::Vec equal_mask = rec.scratchVec();
    biscuit::Vec zero_mask = rec.scratchVec();
    biscuit::Vec neg_zero_mask = rec.scratchVec();
    biscuit::FPR temp1 = rec.scratchFPR();
    biscuit::FPR temp2 = rec.scratchFPR();
    rec.setVectorState(SEW::E64, 2);

    // When either operand is NaN, or they are both 0.0 or both are -0.0, the result is the source
    as.VMFNE(nan_mask_1, dst, dst); // When a register isn't equal to itself, that element must be NaN
    as.VMFNE(nan_mask_2, src, src);
    as.VMOR(nan_mask_1, nan_mask_1, nan_mask_2);
    as.FMV_D_X(temp1, x0);                        // 0.0
    as.FSGNJN_D(temp2, temp1, temp1);             // -0.0
    as.VMFEQ(equal_mask, dst, src);               // Check where they are equal
    as.VMFEQ(zero_mask, dst, temp1);              // Check where dst is 0.0
    as.VMFEQ(neg_zero_mask, dst, temp2);          // Check where dst is -0.0
    as.VMOR(zero_mask, zero_mask, neg_zero_mask); // Either 0.0 or -0.0
    as.VMAND(equal_mask, equal_mask, zero_mask);  // They are both zeroes
    as.VMOR(v0, nan_mask_1, equal_mask);          // Combine the masks

    as.VFMIN(nan_mask_2, dst, src);        // actual max result calculation
    as.VMERGE(zero_mask, nan_mask_2, src); // Where v0 is 1's, use src, otherwise use result of vfmax
    rec.setVec(&operands[0], zero_mask);
}

FAST_HANDLE(PMINUB) {
    biscuit::Vec dst = rec.getVec(&operands[0]);
    biscuit::Vec src = rec.getVec(&operands[1]);
    rec.setVectorState(SEW::E8, 16);
    as.VMINU(dst, dst, src);
    rec.setVec(&operands[0], dst);
}

FAST_HANDLE(PMINUW) {
    biscuit::Vec dst = rec.getVec(&operands[0]);
    biscuit::Vec src = rec.getVec(&operands[1]);
    rec.setVectorState(SEW::E16, 8);
    as.VMINU(dst, dst, src);
    rec.setVec(&operands[0], dst);
}

FAST_HANDLE(PMINUD) {
    biscuit::Vec dst = rec.getVec(&operands[0]);
    biscuit::Vec src = rec.getVec(&operands[1]);
    rec.setVectorState(SEW::E32, 4);
    as.VMINU(dst, dst, src);
    rec.setVec(&operands[0], dst);
}

FAST_HANDLE(PMAXUB) {
    biscuit::Vec dst = rec.getVec(&operands[0]);
    biscuit::Vec src = rec.getVec(&operands[1]);
    rec.setVectorState(SEW::E8, 16);
    as.VMAXU(dst, dst, src);
    rec.setVec(&operands[0], dst);
}

FAST_HANDLE(PMAXUW) {
    biscuit::Vec dst = rec.getVec(&operands[0]);
    biscuit::Vec src = rec.getVec(&operands[1]);
    rec.setVectorState(SEW::E16, 8);
    as.VMAXU(dst, dst, src);
    rec.setVec(&operands[0], dst);
}

FAST_HANDLE(PMAXUD) {
    biscuit::Vec dst = rec.getVec(&operands[0]);
    biscuit::Vec src = rec.getVec(&operands[1]);
    rec.setVectorState(SEW::E32, 4);
    as.VMAXU(dst, dst, src);
    rec.setVec(&operands[0], dst);
}

FAST_HANDLE(PMINSB) {
    biscuit::Vec dst = rec.getVec(&operands[0]);
    biscuit::Vec src = rec.getVec(&operands[1]);
    rec.setVectorState(SEW::E8, 16);
    as.VMIN(dst, dst, src);
    rec.setVec(&operands[0], dst);
}

FAST_HANDLE(PMINSW) {
    biscuit::Vec dst = rec.getVec(&operands[0]);
    biscuit::Vec src = rec.getVec(&operands[1]);
    rec.setVectorState(SEW::E16, 8);
    as.VMIN(dst, dst, src);
    rec.setVec(&operands[0], dst);
}

FAST_HANDLE(PMINSD) {
    biscuit::Vec dst = rec.getVec(&operands[0]);
    biscuit::Vec src = rec.getVec(&operands[1]);
    rec.setVectorState(SEW::E32, 4);
    as.VMIN(dst, dst, src);
    rec.setVec(&operands[0], dst);
}

FAST_HANDLE(PMAXSB) {
    biscuit::Vec dst = rec.getVec(&operands[0]);
    biscuit::Vec src = rec.getVec(&operands[1]);
    rec.setVectorState(SEW::E8, 16);
    as.VMAX(dst, dst, src);
    rec.setVec(&operands[0], dst);
}

FAST_HANDLE(PMAXSW) {
    biscuit::Vec dst = rec.getVec(&operands[0]);
    biscuit::Vec src = rec.getVec(&operands[1]);
    rec.setVectorState(SEW::E16, 8);
    as.VMAX(dst, dst, src);
    rec.setVec(&operands[0], dst);
}

FAST_HANDLE(PMAXSD) {
    biscuit::Vec dst = rec.getVec(&operands[0]);
    biscuit::Vec src = rec.getVec(&operands[1]);
    rec.setVectorState(SEW::E32, 4);
    as.VMAX(dst, dst, src);
    rec.setVec(&operands[0], dst);
}

FAST_HANDLE(PMULHW) {
    biscuit::Vec dst = rec.getVec(&operands[0]);
    biscuit::Vec src = rec.getVec(&operands[1]);
    rec.setVectorState(SEW::E16, 8);
    as.VMULH(dst, dst, src);
    rec.setVec(&operands[0], dst);
}

FAST_HANDLE(PMULHUW) {
    biscuit::Vec dst = rec.getVec(&operands[0]);
    biscuit::Vec src = rec.getVec(&operands[1]);
    rec.setVectorState(SEW::E16, 8);
    as.VMULHU(dst, dst, src);
    rec.setVec(&operands[0], dst);
}

FAST_HANDLE(PMULLW) {
    biscuit::Vec dst = rec.getVec(&operands[0]);
    biscuit::Vec src = rec.getVec(&operands[1]);
    rec.setVectorState(SEW::E16, 8);
    as.VMUL(dst, dst, src);
    rec.setVec(&operands[0], dst);
}

FAST_HANDLE(PMULLD) {
    biscuit::Vec dst = rec.getVec(&operands[0]);
    biscuit::Vec src = rec.getVec(&operands[1]);
    rec.setVectorState(SEW::E32, 4);
    as.VMUL(dst, dst, src);
    rec.setVec(&operands[0], dst);
}

FAST_HANDLE(PMULUDQ) {
    biscuit::GPR shift = rec.scratch();
    biscuit::Vec dst = rec.getVec(&operands[0]);
    biscuit::Vec src = rec.getVec(&operands[1]);
    biscuit::Vec dst_masked = rec.scratchVec();
    biscuit::Vec src_masked = rec.scratchVec();
    biscuit::Vec result = rec.scratchVec();

    rec.setVectorState(SEW::E64, 2);
    as.LI(shift, 32);
    as.VSLL(dst_masked, dst, shift);
    as.VSRL(dst_masked, dst_masked, shift);
    as.VSLL(src_masked, src, shift);
    as.VSRL(src_masked, src_masked, shift);
    as.VMUL(result, dst_masked, src_masked);

    rec.setVec(&operands[0], result);
}

FAST_HANDLE(PMULDQ) {
    biscuit::GPR shift = rec.scratch();
    biscuit::Vec dst = rec.getVec(&operands[0]);
    biscuit::Vec src = rec.getVec(&operands[1]);
    biscuit::Vec dst_masked = rec.scratchVec();
    biscuit::Vec src_masked = rec.scratchVec();
    biscuit::Vec result = rec.scratchVec();

    rec.setVectorState(SEW::E64, 2);
    as.LI(shift, 32);
    as.VSLL(dst_masked, dst, shift);
    as.VSRA(dst_masked, dst_masked, shift);
    as.VSLL(src_masked, src, shift);
    as.VSRA(src_masked, src_masked, shift);
    as.VMUL(result, dst_masked, src_masked);

    rec.setVec(&operands[0], result);
}

FAST_HANDLE(PMADDWD) {
    biscuit::Vec result = rec.scratchVec();
    biscuit::Vec dst_compress = rec.scratchVec();
    biscuit::Vec src_compress = rec.scratchVec();
    biscuit::Vec dst_compress2 = rec.scratchVec();
    biscuit::Vec src_compress2 = rec.scratchVec();
    biscuit::Vec vec_mask = rec.scratchVec();
    biscuit::GPR mask = rec.scratch();
    biscuit::Vec dst = rec.getVec(&operands[0]);
    biscuit::Vec src = rec.getVec(&operands[1]);

    rec.setVectorState(SEW::E16, 8);
    as.LI(mask, 0b01010101);
    as.VMV(v0, mask);
    as.VMNAND(vec_mask, v0, v0);
    as.VCOMPRESS(dst_compress, dst, v0);
    as.VCOMPRESS(src_compress, src, v0);
    as.VCOMPRESS(dst_compress2, dst, vec_mask);
    as.VCOMPRESS(src_compress2, src, vec_mask);

    rec.setVectorState(SEW::E16, 4, LMUL::MF2);
    as.VWMUL(result, dst_compress, src_compress);
    as.VWMACC(result, dst_compress2, src_compress2);

    rec.setVec(&operands[0], result);
}

FAST_HANDLE(MAXPS) {
    if (g_config.inaccurate_minmax && !g_config.paranoid) {
        biscuit::Vec dst = rec.getVec(&operands[0]);
        biscuit::Vec src = rec.getVec(&operands[1]);
        rec.setVectorState(SEW::E32, 4);
        as.VFMAX(dst, dst, src);
        rec.setVec(&operands[0], dst);
        return;
    }

    biscuit::Vec dst = rec.getVec(&operands[0]);
    biscuit::Vec src = rec.getVec(&operands[1]);
    biscuit::Vec nan_mask_1 = rec.scratchVec();
    biscuit::Vec nan_mask_2 = rec.scratchVec();
    biscuit::Vec equal_mask = rec.scratchVec();
    biscuit::Vec zero_mask = rec.scratchVec();
    biscuit::Vec neg_zero_mask = rec.scratchVec();
    biscuit::FPR temp1 = rec.scratchFPR();
    biscuit::FPR temp2 = rec.scratchFPR();
    rec.setVectorState(SEW::E32, 4);

    // When either operand is NaN, or they are both 0.0 or both are -0.0, the result is the source
    as.VMFNE(nan_mask_1, dst, dst); // When a register isn't equal to itself, that element must be NaN
    as.VMFNE(nan_mask_2, src, src);
    as.VMOR(nan_mask_1, nan_mask_1, nan_mask_2);
    as.FMV_W_X(temp1, x0);                        // 0.0
    as.FSGNJN_S(temp2, temp1, temp1);             // -0.0
    as.VMFEQ(equal_mask, dst, src);               // Check where they are equal
    as.VMFEQ(zero_mask, dst, temp1);              // Check where dst is 0.0
    as.VMFEQ(neg_zero_mask, dst, temp2);          // Check where dst is -0.0
    as.VMOR(zero_mask, zero_mask, neg_zero_mask); // Either 0.0 or -0.0
    as.VMAND(equal_mask, equal_mask, zero_mask);  // Check where they are both zeroes
    as.VMOR(v0, nan_mask_1, equal_mask);          // Combine the masks

    as.VFMAX(nan_mask_2, dst, src);        // actual max result calculation
    as.VMERGE(zero_mask, nan_mask_2, src); // Where v0 is 1's, use src, otherwise use result of vfmax
    rec.setVec(&operands[0], zero_mask);
}

FAST_HANDLE(MAXPD) {
    if (g_config.inaccurate_minmax && !g_config.paranoid) {
        biscuit::Vec dst = rec.getVec(&operands[0]);
        biscuit::Vec src = rec.getVec(&operands[1]);
        rec.setVectorState(SEW::E64, 2);
        as.VFMAX(dst, dst, src);
        rec.setVec(&operands[0], dst);
        return;
    }

    biscuit::Vec dst = rec.getVec(&operands[0]);
    biscuit::Vec src = rec.getVec(&operands[1]);
    biscuit::Vec nan_mask_1 = rec.scratchVec();
    biscuit::Vec nan_mask_2 = rec.scratchVec();
    biscuit::Vec equal_mask = rec.scratchVec();
    biscuit::Vec zero_mask = rec.scratchVec();
    biscuit::Vec neg_zero_mask = rec.scratchVec();
    biscuit::FPR temp1 = rec.scratchFPR();
    biscuit::FPR temp2 = rec.scratchFPR();
    rec.setVectorState(SEW::E64, 2);

    // When either operand is NaN, or they are both 0.0 or both are -0.0, the result is the source
    as.VMFNE(nan_mask_1, dst, dst); // When a register isn't equal to itself, that element must be NaN
    as.VMFNE(nan_mask_2, src, src);
    as.VMOR(nan_mask_1, nan_mask_1, nan_mask_2);
    as.FMV_D_X(temp1, x0);                        // 0.0
    as.FSGNJN_D(temp2, temp1, temp1);             // -0.0
    as.VMFEQ(equal_mask, dst, src);               // Check where they are equal
    as.VMFEQ(zero_mask, dst, temp1);              // Check where dst is 0.0
    as.VMFEQ(neg_zero_mask, dst, temp2);          // Check where dst is -0.0
    as.VMOR(zero_mask, zero_mask, neg_zero_mask); // Either 0.0 or -0.0
    as.VMAND(equal_mask, equal_mask, zero_mask);  // They are both zeroes
    as.VMOR(v0, nan_mask_1, equal_mask);          // Combine the masks

    as.VFMAX(nan_mask_2, dst, src);        // actual max result calculation
    as.VMERGE(zero_mask, nan_mask_2, src); // Where v0 is 1's, use src, otherwise use result of vfmax
    rec.setVec(&operands[0], zero_mask);
}

FAST_HANDLE(MULPS) {
    biscuit::Vec dst = rec.getVec(&operands[0]);
    biscuit::Vec src = rec.getVec(&operands[1]);
    rec.setVectorState(SEW::E32, 4);
    as.VFMUL(dst, dst, src);
    rec.setVec(&operands[0], dst);
}

FAST_HANDLE(MULPD) {
    biscuit::Vec dst = rec.getVec(&operands[0]);
    biscuit::Vec src = rec.getVec(&operands[1]);
    rec.setVectorState(SEW::E64, 2);
    as.VFMUL(dst, dst, src);
    rec.setVec(&operands[0], dst);
}

FAST_HANDLE(SQRTPS) {
    biscuit::Vec dst = rec.allocatedVec(rec.zydisToRef(operands[0].reg.value));
    biscuit::Vec src = rec.getVec(&operands[1]);
    rec.setVectorState(SEW::E32, 4);
    as.VFSQRT(dst, src);
    rec.setVec(&operands[0], dst);
}

FAST_HANDLE(SQRTPD) {
    biscuit::Vec dst = rec.allocatedVec(rec.zydisToRef(operands[0].reg.value));
    biscuit::Vec src = rec.getVec(&operands[1]);
    rec.setVectorState(SEW::E64, 2);
    as.VFSQRT(dst, src);
    rec.setVec(&operands[0], dst);
}

FAST_HANDLE(DIVPS) {
    biscuit::Vec dst = rec.getVec(&operands[0]);
    biscuit::Vec src = rec.getVec(&operands[1]);
    rec.setVectorState(SEW::E32, 4);
    as.VFDIV(dst, dst, src);
    rec.setVec(&operands[0], dst);
}

FAST_HANDLE(DIVPD) {
    biscuit::Vec dst = rec.getVec(&operands[0]);
    biscuit::Vec src = rec.getVec(&operands[1]);
    rec.setVectorState(SEW::E64, 2);
    as.VFDIV(dst, dst, src);
    rec.setVec(&operands[0], dst);
}

FAST_HANDLE(RCPPS) {
    biscuit::Vec dst = rec.allocatedVec(rec.zydisToRef(operands[0].reg.value));
    biscuit::Vec src = rec.getVec(&operands[1]);
    biscuit::Vec ones = rec.scratchVec();
    rec.setVectorState(SEW::E32, 4);
    biscuit::GPR scratch = rec.scratch();
    as.LI(scratch, 0x3f800000);
    as.VMV(ones, scratch);
    as.VFDIV(dst, ones, src);
    rec.setVec(&operands[0], dst);
}

FAST_HANDLE(RSQRTPS) {
    biscuit::Vec temp = rec.scratchVec();
    biscuit::Vec ones = rec.scratchVec();
    biscuit::Vec dst = rec.allocatedVec(rec.zydisToRef(operands[0].reg.value));
    biscuit::Vec src = rec.getVec(&operands[1]);
    rec.setVectorState(SEW::E32, 4);
    biscuit::GPR scratch = rec.scratch();
    as.LI(scratch, 0x3f800000);
    as.VMV(ones, scratch);
    as.VFSQRT(temp, src);
    as.VFDIV(dst, ones, temp);
    rec.setVec(&operands[0], dst);
}

FAST_HANDLE(MOVSB) {
    ASSERT(instruction.address_width > 16);
    u8 width = instruction.operand_width;
    x86_size_e address_width = rec.zydisToSize(instruction.address_width);
    biscuit::GPR rdi = rec.getGPR(X86_REF_RDI, address_width);
    biscuit::GPR rsi = rec.getGPR(X86_REF_RSI, address_width);
    biscuit::GPR rcx = rec.getGPR(X86_REF_RCX, address_width); // TODO: technically wrong, should use ecx/cx sometimes
    biscuit::GPR temp = rec.scratch();
    biscuit::GPR data = rec.scratch();
    biscuit::GPR df = rec.scratch();
    as.LBU(df, offsetof(ThreadState, df), rec.threadStatePointer());

    Label end;
    as.LI(temp, -width / 8);
    as.BNEZ(df, &end);
    as.LI(temp, width / 8);
    as.Bind(&end);

    Label loop_end, loop_body;
    if (HAS_REP) {
        rec.repPrologue(&loop_end, rcx);
        as.Bind(&loop_body);
    }

    rec.readMemory(data, rsi, 0, rec.zydisToSize(width));
    rec.writeMemory(data, rdi, 0, rec.zydisToSize(width));

    as.ADD(rdi, rdi, temp);
    as.ADD(rsi, rsi, temp);

    if (HAS_REP) {
        rec.repEpilogue(&loop_body, rcx);
        as.Bind(&loop_end);
    }

    rec.setGPR(X86_REF_RDI, address_width, rdi);
    rec.setGPR(X86_REF_RSI, address_width, rsi);
    rec.setGPR(X86_REF_RCX, address_width, rcx);
}

FAST_HANDLE(MOVSW) {
    fast_MOVSB(rec, rip, as, instruction, operands);
}

// The rep movsd and sse movsd have the same mnemonic, so we differentiate it like this
FAST_HANDLE(MOVSD_sse) {
    if (operands[0].type == ZYDIS_OPERAND_TYPE_MEMORY) {
        biscuit::Vec src = rec.getVec(&operands[1]);
        rec.setVec(&operands[0], src);
    } else {
        biscuit::Vec dst = rec.getVec(&operands[0]);
        biscuit::Vec src = rec.getVec(&operands[1]);
        if (operands[1].type == ZYDIS_OPERAND_TYPE_MEMORY) {
            rec.setVectorState(SEW::E64, 2);
            as.VMV(dst, 0);
        }
        rec.setVectorState(SEW::E64, 1);
        as.VMV(dst, src);
        rec.setVec(&operands[0], dst);
    }
}

FAST_HANDLE(MOVSD) {
    if (instruction.meta.isa_set == ZYDIS_ISA_SET_SSE2) {
        fast_MOVSD_sse(rec, rip, as, instruction, operands);
    } else if (instruction.meta.isa_set == ZYDIS_ISA_SET_I386) {
        fast_MOVSB(rec, rip, as, instruction, operands);
    } else {
        UNREACHABLE();
    }
}

FAST_HANDLE(MOVSQ) {
    fast_MOVSB(rec, rip, as, instruction, operands);
}

FAST_HANDLE(CMPSB) {
    ASSERT(instruction.address_width > 16);
    u8 width = instruction.operand_width;
    x86_size_e address_width = rec.zydisToSize(instruction.address_width);
    biscuit::GPR rdi = rec.getGPR(X86_REF_RDI, address_width);
    biscuit::GPR rsi = rec.getGPR(X86_REF_RSI, address_width);
    biscuit::GPR rcx = rec.getGPR(X86_REF_RCX, address_width);
    biscuit::GPR temp = rec.scratch();
    biscuit::GPR src1 = rec.scratch();
    biscuit::GPR src2 = rec.scratch();
    biscuit::GPR result = rec.scratch();

    x86_size_e size = rec.zydisToSize(width);
    biscuit::GPR df = rec.scratch();
    as.LBU(df, offsetof(ThreadState, df), rec.threadStatePointer());

    Label end;
    as.LI(temp, -width / 8);
    as.BNEZ(df, &end);
    as.LI(temp, width / 8);
    as.Bind(&end);
    rec.popScratch(); // pop df

    Label loop_end, loop_body;
    if (HAS_REP) {
        rec.repPrologue(&loop_end, rcx);
        as.Bind(&loop_body);
    }

    rec.readMemory(src1, rsi, 0, size);
    rec.readMemory(src2, rdi, 0, size);

    as.SUB(result, src1, src2);

    SetCmpFlags(rip, rec, as, src1, src2, result, size, false, HAS_REP /* always emit flags for rep */);

    as.ADD(rdi, rdi, temp);
    as.ADD(rsi, rsi, temp);

    if (HAS_REP) {
        rec.repzEpilogue(&loop_body, &loop_end, rcx, instruction.attributes & ZYDIS_ATTRIB_HAS_REPZ);
        as.Bind(&loop_end);
    }

    rec.setGPR(X86_REF_RDI, address_width, rdi);
    rec.setGPR(X86_REF_RSI, address_width, rsi);
    rec.setGPR(X86_REF_RCX, address_width, rcx);
}

FAST_HANDLE(CMPSW) {
    fast_CMPSB(rec, rip, as, instruction, operands);
}

FAST_HANDLE(CMPSD_string) {
    fast_CMPSB(rec, rip, as, instruction, operands);
}

FAST_HANDLE(CMPSQ) {
    fast_CMPSB(rec, rip, as, instruction, operands);
}

FAST_HANDLE(SCASB) {
    ASSERT(instruction.address_width > 16);
    u8 width = instruction.operand_width;
    x86_size_e size = rec.zydisToSize(width);
    x86_size_e address_width = rec.zydisToSize(instruction.address_width);
    biscuit::GPR rax = rec.getGPR(X86_REF_RAX, size);
    biscuit::GPR rdi = rec.getGPR(X86_REF_RDI, address_width);
    biscuit::GPR rcx = rec.getGPR(X86_REF_RCX, address_width);
    biscuit::GPR temp = rec.scratch();
    biscuit::GPR src2 = rec.scratch();
    biscuit::GPR result = rec.scratch();
    biscuit::GPR df = rec.scratch();
    as.LBU(df, offsetof(ThreadState, df), rec.threadStatePointer());

    Label end;
    as.LI(temp, -width / 8);
    as.BNEZ(df, &end);
    as.LI(temp, width / 8);
    as.Bind(&end);

    rec.popScratch();

    Label loop_end, loop_body;
    if (HAS_REP) {
        rec.repPrologue(&loop_end, rcx);
        as.Bind(&loop_body);
    }

    rec.readMemory(src2, rdi, 0, size);

    as.SUB(result, rax, src2);

    SetCmpFlags(rip, rec, as, rax, src2, result, size, false, HAS_REP /* always emit flags for rep */);

    as.ADD(rdi, rdi, temp);

    if (HAS_REP) {
        rec.repzEpilogue(&loop_body, &loop_end, rcx, instruction.attributes & ZYDIS_ATTRIB_HAS_REPZ);
        as.Bind(&loop_end);
    }

    rec.setGPR(X86_REF_RDI, address_width, rdi);
    rec.setGPR(X86_REF_RCX, address_width, rcx);
}

FAST_HANDLE(SCASW) {
    fast_SCASB(rec, rip, as, instruction, operands);
}

FAST_HANDLE(SCASD) {
    fast_SCASB(rec, rip, as, instruction, operands);
}

FAST_HANDLE(SCASQ) {
    fast_SCASB(rec, rip, as, instruction, operands);
}

FAST_HANDLE(LODSB) {
    ASSERT(!HAS_REP); // it can have rep, but it would be too silly
    ASSERT(instruction.address_width > 16);
    int width = instruction.operand_width;
    x86_size_e address_width = rec.zydisToSize(instruction.address_width);
    x86_size_e size = rec.zydisToSize(width);
    biscuit::GPR rsi = rec.getGPR(X86_REF_RSI, address_width);
    biscuit::GPR temp = rec.scratch();
    biscuit::GPR loaded = rec.scratch();
    biscuit::GPR df = rec.scratch();
    as.LBU(df, offsetof(ThreadState, df), rec.threadStatePointer());

    Label end;
    as.LI(temp, -width / 8);
    as.BNEZ(df, &end);
    as.LI(temp, width / 8);
    as.Bind(&end);

    rec.readMemory(loaded, rsi, 0, size);

    as.ADD(rsi, rsi, temp);

    rec.setGPR(X86_REF_RAX, size, loaded);
    rec.setGPR(X86_REF_RSI, address_width, rsi);
}

FAST_HANDLE(LODSW) {
    fast_LODSB(rec, rip, as, instruction, operands);
}

FAST_HANDLE(LODSD) {
    fast_LODSB(rec, rip, as, instruction, operands);
}

FAST_HANDLE(LODSQ) {
    fast_LODSB(rec, rip, as, instruction, operands);
}

FAST_HANDLE(STOSB) {
    ASSERT(instruction.address_width > 16);
    Label loop_end, loop_body;
    u8 width = instruction.operand_width;
    x86_size_e address_width = rec.zydisToSize(instruction.address_width);
    biscuit::GPR rdi = rec.getGPR(X86_REF_RDI, address_width);
    biscuit::GPR rcx = rec.getGPR(X86_REF_RCX, address_width);
    biscuit::GPR rax = rec.getGPR(X86_REF_RAX, rec.zydisToSize(width));
    biscuit::GPR temp = rec.scratch();
    biscuit::GPR df = rec.scratch();
    as.LBU(df, offsetof(ThreadState, df), rec.threadStatePointer());

    Label end;
    as.LI(temp, -width / 8);
    as.BNEZ(df, &end);
    as.LI(temp, width / 8);
    as.Bind(&end);
    rec.popScratch();

    if (HAS_REP) {
        rec.repPrologue(&loop_end, rcx);
        as.Bind(&loop_body);
    }

    rec.writeMemory(rax, rdi, 0, rec.zydisToSize(width));
    as.ADD(rdi, rdi, temp);

    if (HAS_REP) {
        rec.repEpilogue(&loop_body, rcx);
        as.Bind(&loop_end);
    }

    rec.setGPR(X86_REF_RDI, address_width, rdi);
    rec.setGPR(X86_REF_RCX, address_width, rcx);
}

FAST_HANDLE(STOSW) {
    fast_STOSB(rec, rip, as, instruction, operands);
}

FAST_HANDLE(STOSD) {
    fast_STOSB(rec, rip, as, instruction, operands);
}

FAST_HANDLE(STOSQ) {
    fast_STOSB(rec, rip, as, instruction, operands);
}

FAST_HANDLE(MOVHPS) {
    if (operands[0].type == ZYDIS_OPERAND_TYPE_MEMORY) {
        biscuit::Vec temp = rec.scratchVec();
        biscuit::Vec src = rec.getVec(&operands[1]);
        rec.setVectorState(SEW::E64, 2);
        as.VSLIDEDOWN(temp, src, 1);
        rec.setVec(&operands[0], temp);
    } else if (operands[1].type == ZYDIS_OPERAND_TYPE_MEMORY) {
        biscuit::Vec temp = rec.scratchVec();
        biscuit::Vec dst = rec.getVec(&operands[0]);
        biscuit::Vec src = rec.getVec(&operands[1]);
        rec.setVectorState(SEW::E64, 2);
        as.VSLIDEUP(temp, src, 1);
        as.VMV(v0, 0b10);
        as.VMERGE(dst, dst, temp);
        rec.setVec(&operands[0], dst);
    } else {
        UNREACHABLE();
    }
}

FAST_HANDLE(MOVHPD) {
    fast_MOVHPS(rec, rip, as, instruction, operands);
}

FAST_HANDLE(SHUFPD) {
    u8 imm = rec.getImmediate(&operands[2]);
    biscuit::GPR temp = rec.scratch();
    biscuit::Vec vtemp = rec.scratchVec();
    biscuit::Vec vsrc = rec.scratchVec();
    biscuit::Vec dst = rec.getVec(&operands[0]);
    biscuit::Vec src = rec.getVec(&operands[1]);

    rec.setVectorState(SEW::E64, 2);

    if ((imm & 0b1) == 0) {
        as.VMV_XS(temp, dst);
    } else {
        as.VSLIDEDOWN(vtemp, dst, 1);
        as.VMV_XS(temp, vtemp);
    }

    if ((imm & 0b10) != 0) {
        as.VSLIDEDOWN(vsrc, src, 1);
    } else {
        vsrc = src;
    }

    as.VSLIDE1UP(vtemp, vsrc, temp);

    rec.setVec(&operands[0], vtemp);
}

FAST_HANDLE(LEAVE) {
    x86_size_e size = rec.zydisToSize(instruction.operand_width);
    biscuit::GPR rbp = rec.getGPR(X86_REF_RBP, size);
    as.ADDI(rbp, rbp, rec.stackPointerSize());
    rec.setGPR(X86_REF_RSP, size, rbp);
    rec.readMemory(rbp, rbp, -rec.stackPointerSize(), size);
    rec.setGPR(X86_REF_RBP, size, rbp);
}

FAST_HANDLE(ENTER) {
    ERROR("ENTER instruction is broken");
    x86_size_e size = rec.zydisToSize(instruction.operand_width);
    int alloc_size = rec.getImmediate(&operands[0]);
    u8 nesting_level = rec.getImmediate(&operands[1]) & 0x1F;
    biscuit::GPR frame_temp = rec.scratch();
    biscuit::GPR rsp = rec.getGPR(X86_REF_RSP, X86_SIZE_QWORD);
    biscuit::GPR rbp = rec.getGPR(X86_REF_RBP, X86_SIZE_QWORD);
    int offset = instruction.operand_width / 8;
    as.ADDI(frame_temp, rsp, -offset);
    rec.writeMemory(rbp, rsp, -offset, size);

    if (nesting_level > 1) {
        biscuit::GPR mem = rec.scratch();
        for (u8 i = 1; i < nesting_level; i++) {
            rec.readMemory(mem, rbp, -i * offset, size);
            rec.writeMemory(mem, frame_temp, -i * offset, size);
        }
    } else if (nesting_level == 1) {
        rec.writeMemory(frame_temp, frame_temp, -offset, size);
    }

    rec.setGPR(X86_REF_RBP, size, frame_temp);
    biscuit::GPR new_rsp = rec.scratch();
    rec.addi(new_rsp, frame_temp, -alloc_size);
    rec.setGPR(X86_REF_RSP, size, new_rsp);
}

void SETCC(Recompiler& rec, u64 rip, ZydisDecodedInstruction& instruction, ZydisDecodedOperand* operands, biscuit::GPR cond) {
    rec.setGPR(&operands[0], cond);
}

FAST_HANDLE(SETO) {
    SETCC(rec, rip, instruction, operands, rec.getCond(instruction.opcode & 0xF));
}

FAST_HANDLE(SETNO) {
    SETCC(rec, rip, instruction, operands, rec.getCond(instruction.opcode & 0xF));
}

FAST_HANDLE(SETB) {
    SETCC(rec, rip, instruction, operands, rec.getCond(instruction.opcode & 0xF));
}

FAST_HANDLE(SETNB) {
    SETCC(rec, rip, instruction, operands, rec.getCond(instruction.opcode & 0xF));
}

FAST_HANDLE(SETZ) {
    SETCC(rec, rip, instruction, operands, rec.getCond(instruction.opcode & 0xF));
}

FAST_HANDLE(SETNZ) {
    SETCC(rec, rip, instruction, operands, rec.getCond(instruction.opcode & 0xF));
}

FAST_HANDLE(SETBE) {
    SETCC(rec, rip, instruction, operands, rec.getCond(instruction.opcode & 0xF));
}

FAST_HANDLE(SETNBE) {
    SETCC(rec, rip, instruction, operands, rec.getCond(instruction.opcode & 0xF));
}

FAST_HANDLE(SETP) {
    SETCC(rec, rip, instruction, operands, rec.getCond(instruction.opcode & 0xF));
}

FAST_HANDLE(SETNP) {
    SETCC(rec, rip, instruction, operands, rec.getCond(instruction.opcode & 0xF));
}

FAST_HANDLE(SETS) {
    SETCC(rec, rip, instruction, operands, rec.getCond(instruction.opcode & 0xF));
}

FAST_HANDLE(SETNS) {
    SETCC(rec, rip, instruction, operands, rec.getCond(instruction.opcode & 0xF));
}

FAST_HANDLE(SETL) {
    SETCC(rec, rip, instruction, operands, rec.getCond(instruction.opcode & 0xF));
}

FAST_HANDLE(SETNL) {
    SETCC(rec, rip, instruction, operands, rec.getCond(instruction.opcode & 0xF));
}

FAST_HANDLE(SETLE) {
    SETCC(rec, rip, instruction, operands, rec.getCond(instruction.opcode & 0xF));
}

FAST_HANDLE(SETNLE) {
    SETCC(rec, rip, instruction, operands, rec.getCond(instruction.opcode & 0xF));
}

FAST_HANDLE(NOT) {
    biscuit::GPR result = rec.scratch();
    biscuit::GPR dst = rec.getGPR(&operands[0]);
    as.NOT(result, dst);
    rec.setGPR(&operands[0], result);
}

FAST_HANDLE(NEG) {
    x86_size_e size = rec.getSize(&operands[0]);
    biscuit::GPR result = rec.scratch();
    biscuit::GPR dst = rec.getGPR(&operands[0]);
    if (size == X86_SIZE_BYTE || size == X86_SIZE_BYTE_HIGH) {
        rec.sextb(result, dst);
        as.NEG(result, result);
    } else if (size == X86_SIZE_WORD) {
        rec.sexth(result, dst);
        as.NEG(result, result);
    } else if (size == X86_SIZE_DWORD) {
        as.SUBW(result, x0, dst);
    } else if (size == X86_SIZE_QWORD) {
        as.NEG(result, dst);
    } else {
        UNREACHABLE();
    }

    if (rec.shouldEmitFlag(rip, X86_REF_CF)) {
        biscuit::GPR cf = rec.flag(X86_REF_CF);
        as.SNEZ(cf, dst);
    }

    if (rec.shouldEmitFlag(rip, X86_REF_ZF)) {
        rec.updateZero(result, size);
    }

    if (rec.shouldEmitFlag(rip, X86_REF_OF)) {
        rec.updateOverflowSub(x0, dst, result, size);
    }

    if (rec.shouldEmitFlag(rip, X86_REF_SF)) {
        rec.updateSign(result, size);
    }

    if (rec.shouldEmitFlag(rip, X86_REF_AF)) {
        biscuit::GPR af = rec.scratch();
        as.ANDI(af, dst, 0xF);
        as.SNEZ(af, af);
        as.SB(af, offsetof(ThreadState, af), rec.threadStatePointer());
        rec.popScratch();
    }

    if (rec.shouldEmitFlag(rip, X86_REF_PF)) {
        rec.updateParity(result);
    }

    rec.setGPR(&operands[0], result);
}

// There is no single instruction that can saturate a signed value into an unsigned destination. A sequence of two vector instructions that
// rst removes negative numbers by performing a max against 0 using vmax then clips the resulting unsigned value into the destination
// using vnclipu can be used if setting vxsat value for negative numbers is not required. A vsetvli is required inbetween these two
// instructions to change SEW.
FAST_HANDLE(PACKUSWB) {
    bool is_mmx = operands[0].reg.value >= ZYDIS_REGISTER_MM0 && operands[0].reg.value <= ZYDIS_REGISTER_MM7;
    biscuit::Vec result1 = rec.scratchVec();
    biscuit::Vec result2 = rec.scratchVec();
    biscuit::Vec result3 = rec.scratchVec();
    biscuit::Vec result4 = rec.scratchVec();
    biscuit::Vec result_up = rec.scratchVec();
    biscuit::Vec result = rec.scratchVec();
    biscuit::Vec dst = rec.getVec(&operands[0]);
    biscuit::Vec src = rec.getVec(&operands[1]);

    rec.setVectorState(SEW::E16, 8);
    as.VMAX(result1, dst, x0);
    as.VMAX(result2, src, x0);
    rec.setVectorState(SEW::E8, 8, LMUL::MF2);
    as.VNCLIPU(result3, result1, 0);
    as.VNCLIPU(result4, result2, 0);
    if (is_mmx) {
        rec.setVectorState(SEW::E32, 2);
    } else {
        rec.setVectorState(SEW::E64, 2);
    }
    as.VMV(v0, 0b10);
    as.VSLIDEUP(result_up, result4, 1);
    as.VMERGE(result, result3, result_up);
    rec.setVec(&operands[0], result);
}

FAST_HANDLE(PACKUSDW) {
    bool is_mmx = operands[0].reg.value >= ZYDIS_REGISTER_MM0 && operands[0].reg.value <= ZYDIS_REGISTER_MM7;
    biscuit::Vec result1 = rec.scratchVec();
    biscuit::Vec result2 = rec.scratchVec();
    biscuit::Vec result3 = rec.scratchVec();
    biscuit::Vec result4 = rec.scratchVec();
    biscuit::Vec result_up = rec.scratchVec();
    biscuit::Vec result = rec.scratchVec();
    biscuit::Vec dst = rec.getVec(&operands[0]);
    biscuit::Vec src = rec.getVec(&operands[1]);

    rec.setVectorState(SEW::E32, 4);
    as.VMAX(result1, dst, x0);
    as.VMAX(result2, src, x0);
    rec.setVectorState(SEW::E16, 4, LMUL::MF2);
    as.VNCLIPU(result3, result1, 0);
    as.VNCLIPU(result4, result2, 0);
    if (is_mmx) {
        rec.setVectorState(SEW::E32, 2);
    } else {
        rec.setVectorState(SEW::E64, 2);
    }
    as.VMV(v0, 0b10);
    as.VSLIDEUP(result_up, result4, 1);
    as.VMERGE(result, result3, result_up);
    rec.setVec(&operands[0], result);
}

FAST_HANDLE(PACKSSWB) {
    bool is_mmx = operands[0].reg.value >= ZYDIS_REGISTER_MM0 && operands[0].reg.value <= ZYDIS_REGISTER_MM7;
    biscuit::Vec result1 = rec.scratchVec();
    biscuit::Vec result2 = rec.scratchVec();
    biscuit::Vec result2_up = rec.scratchVec();
    biscuit::Vec result = rec.scratchVec();
    biscuit::Vec dst = rec.getVec(&operands[0]);
    biscuit::Vec src = rec.getVec(&operands[1]);

    // Use half the register group so we don't run into overlapping problems
    rec.setVectorState(SEW::E8, 8, LMUL::MF2);
    as.VNCLIP(result1, dst, 0);
    as.VNCLIP(result2, src, 0);
    if (is_mmx) {
        rec.setVectorState(SEW::E32, 2);
    } else {
        rec.setVectorState(SEW::E64, 2);
    }
    as.VMV(v0, 0b10);
    as.VSLIDEUP(result2_up, result2, 1);
    as.VMERGE(result, result1, result2_up);
    rec.setVec(&operands[0], result);
}

FAST_HANDLE(PACKSSDW) {
    bool is_mmx = operands[0].reg.value >= ZYDIS_REGISTER_MM0 && operands[0].reg.value <= ZYDIS_REGISTER_MM7;
    biscuit::Vec result1 = rec.scratchVec();
    biscuit::Vec result2 = rec.scratchVec();
    biscuit::Vec result2_up = rec.scratchVec();
    biscuit::Vec result = rec.scratchVec();
    biscuit::Vec dst = rec.getVec(&operands[0]);
    biscuit::Vec src = rec.getVec(&operands[1]);

    // Use half the register group so we don't run into overlapping problems
    rec.setVectorState(SEW::E16, 4, LMUL::MF2);
    as.VNCLIP(result1, dst, 0);
    as.VNCLIP(result2, src, 0);
    if (is_mmx) {
        rec.setVectorState(SEW::E32, 2);
    } else {
        rec.setVectorState(SEW::E64, 2);
    }
    as.VMV(v0, 0b10);
    as.VSLIDEUP(result2_up, result2, 1);
    as.VMERGE(result, result1, result2_up);
    rec.setVec(&operands[0], result);
}

void ROUND(Recompiler& rec, u64 rip, Assembler& as, ZydisDecodedInstruction& instruction, ZydisDecodedOperand* operands, SEW sew, u8 vlen) {
    u8 imm = rec.getImmediate(&operands[2]);
    biscuit::Vec dst = rec.getVec(&operands[0]);
    biscuit::Vec src = rec.getVec(&operands[1]);
    bool dyn_round = imm & 0b100;
    if (!(imm & 0b1000)) {
        WARN("Ignore precision bit not set for roundsd/roundss");
    }

    rec.setVectorState(sew, vlen);
    if (!dyn_round) {
        // Use floats so we can hardcode a different rounding mode without changing it
        // TODO: it may be faster to just change the rounding mode
        biscuit::FPR temp1 = rec.scratchFPR();
        biscuit::FPR temp2 = rec.scratchFPR();
        RMode rmode = rounding_mode((x86RoundingMode)(imm & 0b11));

        as.VFMV_FS(temp1, src);

        if (Extensions::Zfa) {
            WARN_ONCE("Zfa extension code, untested");
            if (sew == SEW::E64) {
                as.FROUND_D(temp2, temp1, rmode);
            } else if (sew == SEW::E32) {
                as.FROUND_S(temp2, temp1, rmode);
            } else {
                UNREACHABLE();
            }
        } else {
            biscuit::GPR temp = rec.scratch();
            if (sew == SEW::E64) {
                as.FCVT_L_D(temp, temp1, rmode);
                as.FCVT_D_L(temp2, temp, rmode);
            } else if (sew == SEW::E32) {
                as.FCVT_W_S(temp, temp1, rmode);
                as.FCVT_S_W(temp2, temp, rmode);
            } else {
                UNREACHABLE();
            }
        }

        as.VFMV_SF(dst, temp2);

        rec.setVec(&operands[0], dst);
    } else {
        // Dynamic rounding mode, use vectors directly
        biscuit::Vec temp = rec.scratchVec();
        as.VFCVT_X_F(temp, src);
        as.VFCVT_F_X(dst, temp);
        rec.setVec(&operands[0], dst);
    }
}

FAST_HANDLE(ROUNDSS) {
    ROUND(rec, rip, as, instruction, operands, SEW::E32, 1);
}

FAST_HANDLE(ROUNDSD) {
    ROUND(rec, rip, as, instruction, operands, SEW::E64, 1);
}

FAST_HANDLE(PMOVMSKB) {
    bool is_mmx = operands[1].reg.value >= ZYDIS_REGISTER_MM0 && operands[1].reg.value <= ZYDIS_REGISTER_MM7;
    biscuit::GPR scratch = rec.scratch();
    biscuit::Vec src = rec.getVec(&operands[1]);
    biscuit::Vec temp = rec.scratchVec();

    rec.setVectorState(SEW::E8, 16);
    as.VMSLT(temp, src, x0);

    rec.setVectorState(SEW::E64, 2);
    as.VMV_XS(scratch, temp);

    rec.zext(scratch, scratch, is_mmx ? X86_SIZE_BYTE : X86_SIZE_WORD);

    rec.setGPR(&operands[0], scratch);
}

FAST_HANDLE(MOVSHDUP) {
    biscuit::GPR shift = rec.scratch();
    biscuit::Vec dst = rec.getVec(&operands[0]);
    biscuit::Vec src = rec.getVec(&operands[1]);
    biscuit::Vec left = rec.scratchVec();
    biscuit::Vec right = rec.scratchVec();

    as.LI(shift, 32);
    rec.setVectorState(SEW::E64, 2);
    as.VSRL(right, src, shift);
    as.VSLL(left, right, shift);
    as.VOR(dst, left, right);

    rec.setVec(&operands[0], dst);
}

FAST_HANDLE(MOVSLDUP) {
    biscuit::GPR shift = rec.scratch();
    biscuit::Vec dst = rec.getVec(&operands[0]);
    biscuit::Vec src = rec.getVec(&operands[1]);
    biscuit::Vec left = rec.scratchVec();
    biscuit::Vec right = rec.scratchVec();

    as.LI(shift, 32);
    rec.setVectorState(SEW::E64, 2);
    as.VSLL(left, src, shift);
    as.VSRL(right, left, shift);
    as.VOR(dst, left, right);

    rec.setVec(&operands[0], dst);
}

FAST_HANDLE(MONITOR) {
    WARN_ONCE("This program uses MONITOR which is a NOP for us");
}

FAST_HANDLE(MWAIT) {
    WARN_ONCE("This program uses MWAIT which is a NOP for us");
}

FAST_HANDLE(PTEST) {
    biscuit::Vec zmask = rec.scratchVec();
    biscuit::Vec cmask = rec.scratchVec();
    biscuit::Vec resultz = rec.scratchVec();
    biscuit::Vec resultc = rec.scratchVec();
    biscuit::Vec dst = rec.getVec(&operands[0]);
    biscuit::Vec src = rec.getVec(&operands[1]);

    // PTEST with the same register is common to check if all the elements
    // of a single register are zero. In which case, CF is always 1 because (a & ~a) = 0,
    // and we don't need to perform a VAND
    bool same = dst == src;

    rec.setVectorState(SEW::E64, 2);
    if (!same) {
        as.VAND(resultz, dst, src);
        if (Extensions::Zvbb) {
            WARN_ONCE("PTEST + Zvbb is untested, please run tests and report results");
            as.VANDN(resultc, src, dst);
        } else {
            biscuit::Vec dst_not = rec.scratchVec();
            as.VXOR(dst_not, dst, -1);
            as.VAND(resultc, src, dst_not);
        }
    } else {
        resultz = dst;
    }

    // Set mask if not equal zero. Then we can check if that GPR is zero, to set the zero flag
    if (rec.shouldEmitFlag(rip, X86_REF_ZF)) {
        biscuit::GPR zf = rec.flag(X86_REF_ZF);
        as.VMSNE(zmask, resultz, 0);
        as.VMV_XS(zf, zmask);
        // No need to do a full zext, just shift left
        as.SLLI(zf, zf, 62); // only care about lower 2 bits for the 2 64-bit elements
        as.SEQZ(zf, zf);
    }

    if (rec.shouldEmitFlag(rip, X86_REF_CF)) {
        biscuit::GPR cf = rec.flag(X86_REF_CF);
        if (!same) {
            as.VMSNE(cmask, resultc, 0);
            as.VMV_XS(cf, cmask);
            as.SLLI(cf, cf, 62);
            as.SEQZ(cf, cf);
        } else {
            as.LI(cf, 1);
        }
    }

    if (rec.shouldEmitFlag(rip, X86_REF_AF)) {
        as.SB(x0, offsetof(ThreadState, af), rec.threadStatePointer());
    }

    if (rec.shouldEmitFlag(rip, X86_REF_OF)) {
        biscuit::GPR of = rec.flag(X86_REF_OF);
        as.MV(of, x0);
    }

    if (rec.shouldEmitFlag(rip, X86_REF_SF)) {
        biscuit::GPR sf = rec.flag(X86_REF_SF);
        as.MV(sf, x0);
    }

    if (rec.shouldEmitFlag(rip, X86_REF_PF)) {
        as.SB(x0, offsetof(ThreadState, pf), rec.threadStatePointer());
    }
}

FAST_HANDLE(MOVMSKPS) {
    biscuit::Vec mask = rec.scratchVec();
    biscuit::Vec src = rec.getVec(&operands[1]);
    biscuit::GPR dst = rec.scratch();

    rec.setVectorState(SEW::E32, 4);
    as.VMSLT(mask, src, x0);
    as.VMV_XS(dst, mask);
    as.ANDI(dst, dst, 0b1111);
    rec.setGPR(&operands[0], dst);
}

FAST_HANDLE(MOVMSKPD) {
    biscuit::Vec mask = rec.scratchVec();
    biscuit::Vec src = rec.getVec(&operands[1]);
    biscuit::GPR dst = rec.scratch();

    rec.setVectorState(SEW::E64, 2);
    as.VMSLT(mask, src, x0);
    as.VMV_XS(dst, mask);
    as.ANDI(dst, dst, 0b11);
    rec.setGPR(&operands[0], dst);
}

FAST_HANDLE(PMOVZXBQ) {
    biscuit::Vec dst = rec.getVec(&operands[0]);
    biscuit::Vec src = rec.getVec(&operands[1]);

    rec.setVectorState(SEW::E64, 2);
    if (dst != src) {
        as.VZEXTVF8(dst, src);
    } else {
        biscuit::Vec result = rec.scratchVec();
        as.VZEXTVF8(result, src);
        dst = result;
    }

    rec.setVec(&operands[0], dst);
}

FAST_HANDLE(PMOVZXBD) {
    biscuit::Vec dst = rec.getVec(&operands[0]);
    biscuit::Vec src = rec.getVec(&operands[1]);

    rec.setVectorState(SEW::E32, 4);
    if (dst != src) {
        as.VZEXTVF4(dst, src);
    } else {
        biscuit::Vec result = rec.scratchVec();
        as.VZEXTVF4(result, src);
        dst = result;
    }

    rec.setVec(&operands[0], dst);
}

FAST_HANDLE(PMOVZXBW) {
    biscuit::Vec dst = rec.getVec(&operands[0]);
    biscuit::Vec src = rec.getVec(&operands[1]);

    rec.setVectorState(SEW::E16, 8);
    if (dst != src) {
        as.VZEXTVF2(dst, src);
    } else {
        biscuit::Vec result = rec.scratchVec();
        as.VZEXTVF2(result, src);
        dst = result;
    }

    rec.setVec(&operands[0], dst);
}

FAST_HANDLE(PMOVZXWD) {
    biscuit::Vec dst = rec.getVec(&operands[0]);
    biscuit::Vec src = rec.getVec(&operands[1]);

    rec.setVectorState(SEW::E32, 4);
    if (dst != src) {
        as.VZEXTVF2(dst, src);
    } else {
        biscuit::Vec result = rec.scratchVec();
        as.VZEXTVF2(result, src);
        dst = result;
    }

    rec.setVec(&operands[0], dst);
}

FAST_HANDLE(PMOVZXWQ) {
    biscuit::Vec dst = rec.getVec(&operands[0]);
    biscuit::Vec src = rec.getVec(&operands[1]);

    rec.setVectorState(SEW::E64, 2);
    if (dst != src) {
        as.VZEXTVF4(dst, src);
    } else {
        biscuit::Vec result = rec.scratchVec();
        as.VZEXTVF4(result, src);
        dst = result;
    }

    rec.setVec(&operands[0], dst);
}

FAST_HANDLE(PMOVZXDQ) {
    biscuit::Vec dst = rec.getVec(&operands[0]);
    biscuit::Vec src = rec.getVec(&operands[1]);

    rec.setVectorState(SEW::E64, 2);
    if (dst != src) {
        as.VZEXTVF2(dst, src);
    } else {
        biscuit::Vec result = rec.scratchVec();
        as.VZEXTVF2(result, src);
        dst = result;
    }

    rec.setVec(&operands[0], dst);
}

FAST_HANDLE(PMOVSXBQ) {
    biscuit::Vec dst = rec.getVec(&operands[0]);
    biscuit::Vec src = rec.getVec(&operands[1]);

    rec.setVectorState(SEW::E64, 2);
    if (dst != src) {
        as.VSEXTVF8(dst, src);
    } else {
        biscuit::Vec result = rec.scratchVec();
        as.VSEXTVF8(result, src);
        dst = result;
    }

    rec.setVec(&operands[0], dst);
}

FAST_HANDLE(PMOVSXBD) {
    biscuit::Vec dst = rec.getVec(&operands[0]);
    biscuit::Vec src = rec.getVec(&operands[1]);

    rec.setVectorState(SEW::E32, 4);
    if (dst != src) {
        as.VSEXTVF4(dst, src);
    } else {
        biscuit::Vec result = rec.scratchVec();
        as.VSEXTVF4(result, src);
        dst = result;
    }

    rec.setVec(&operands[0], dst);
}

FAST_HANDLE(PMOVSXBW) {
    biscuit::Vec dst = rec.getVec(&operands[0]);
    biscuit::Vec src = rec.getVec(&operands[1]);

    rec.setVectorState(SEW::E16, 8);
    if (dst != src) {
        as.VSEXTVF2(dst, src);
    } else {
        biscuit::Vec result = rec.scratchVec();
        as.VSEXTVF2(result, src);
        dst = result;
    }

    rec.setVec(&operands[0], dst);
}

FAST_HANDLE(PMOVSXWD) {
    biscuit::Vec dst = rec.getVec(&operands[0]);
    biscuit::Vec src = rec.getVec(&operands[1]);

    rec.setVectorState(SEW::E32, 4);
    if (dst != src) {
        as.VSEXTVF2(dst, src);
    } else {
        biscuit::Vec result = rec.scratchVec();
        as.VSEXTVF2(result, src);
        dst = result;
    }

    rec.setVec(&operands[0], dst);
}

FAST_HANDLE(PMOVSXWQ) {
    biscuit::Vec dst = rec.getVec(&operands[0]);
    biscuit::Vec src = rec.getVec(&operands[1]);

    rec.setVectorState(SEW::E64, 2);
    if (dst != src) {
        as.VSEXTVF4(dst, src);
    } else {
        biscuit::Vec result = rec.scratchVec();
        as.VSEXTVF4(result, src);
        dst = result;
    }

    rec.setVec(&operands[0], dst);
}

FAST_HANDLE(PMOVSXDQ) {
    biscuit::Vec dst = rec.getVec(&operands[0]);
    biscuit::Vec src = rec.getVec(&operands[1]);

    rec.setVectorState(SEW::E64, 2);
    if (dst != src) {
        as.VSEXTVF2(dst, src);
    } else {
        biscuit::Vec result = rec.scratchVec();
        as.VSEXTVF2(result, src);
        dst = result;
    }

    rec.setVec(&operands[0], dst);
}

void PCMPEQ(Recompiler& rec, u64 rip, Assembler& as, ZydisDecodedInstruction& instruction, ZydisDecodedOperand* operands, SEW sew, u8 vlen) {
    biscuit::Vec zero = rec.scratchVec();
    biscuit::Vec dst = rec.getVec(&operands[0]);
    biscuit::Vec src = rec.getVec(&operands[1]);
    rec.setVectorState(sew, vlen);
    as.VMV(zero, 0);
    as.VMSEQ(v0, dst, src);
    as.VMERGE(dst, zero, -1ll);
    rec.setVec(&operands[0], dst);
}

void PCMPGT(Recompiler& rec, u64 rip, Assembler& as, ZydisDecodedInstruction& instruction, ZydisDecodedOperand* operands, SEW sew, u8 vlen) {
    biscuit::Vec zero = rec.scratchVec();
    biscuit::Vec dst = rec.getVec(&operands[0]);
    biscuit::Vec src = rec.getVec(&operands[1]);
    rec.setVectorState(sew, vlen);
    as.VMV(zero, 0);
    as.VMSLT(v0, src, dst);
    as.VMERGE(dst, zero, -1ll);
    rec.setVec(&operands[0], dst);
}

FAST_HANDLE(PCMPEQB) {
    PCMPEQ(rec, rip, as, instruction, operands, SEW::E8, 16);
}

FAST_HANDLE(PCMPEQW) {
    PCMPEQ(rec, rip, as, instruction, operands, SEW::E16, 8);
}

FAST_HANDLE(PCMPEQD) {
    PCMPEQ(rec, rip, as, instruction, operands, SEW::E32, 4);
}

FAST_HANDLE(PCMPEQQ) {
    PCMPEQ(rec, rip, as, instruction, operands, SEW::E64, 2);
}

FAST_HANDLE(PCMPGTB) {
    PCMPGT(rec, rip, as, instruction, operands, SEW::E8, 16);
}

FAST_HANDLE(PCMPGTW) {
    PCMPGT(rec, rip, as, instruction, operands, SEW::E16, 8);
}

FAST_HANDLE(PCMPGTD) {
    PCMPGT(rec, rip, as, instruction, operands, SEW::E32, 4);
}

FAST_HANDLE(PCMPGTQ) {
    PCMPGT(rec, rip, as, instruction, operands, SEW::E64, 2);
}

void CMPP(Recompiler& rec, u64 rip, Assembler& as, ZydisDecodedInstruction& instruction, ZydisDecodedOperand* operands, SEW sew, u8 vlen) {
    u8 imm = rec.getImmediate(&operands[2]);
    biscuit::Vec result = rec.scratchVec();
    biscuit::Vec temp1 = rec.scratchVec();
    biscuit::Vec temp2 = rec.scratchVec();
    biscuit::Vec dst = rec.getVec(&operands[0]);
    biscuit::Vec src = rec.getVec(&operands[1]);
    rec.setVectorState(sew, vlen);
    // TODO: technically wrong to use this enum I think but the operations are the same generally
    switch (imm) {
    case EQ_OQ: {
        as.VMFEQ(v0, dst, src);
        break;
    }
    case LT_OS: {
        as.VMFLT(v0, dst, src);
        break;
    }
    case LE_OS: {
        as.VMFLE(v0, dst, src);
        break;
    }
    case UNORD_Q: {
        // Set if either are NaN
        as.VMFNE(temp1, dst, dst);
        as.VMFNE(temp2, src, src);
        as.VMOR(v0, temp1, temp2);
        break;
    }
    case NEQ_UQ: {
        as.VMFNE(temp1, dst, dst);
        as.VMFNE(temp2, src, src);
        as.VMFNE(v0, dst, src);
        as.VMOR(v0, v0, temp1);
        as.VMOR(v0, v0, temp2);
        break;
    }
    case NLT_US: {
        as.VMFNE(temp1, dst, dst);
        as.VMFNE(temp2, src, src);
        as.VMFLE(v0, src, dst);
        as.VMOR(v0, v0, temp1);
        as.VMOR(v0, v0, temp2);
        break;
    }
    case NLE_US: {
        as.VMFNE(temp1, dst, dst);
        as.VMFNE(temp2, src, src);
        as.VMFLT(v0, src, dst);
        as.VMOR(v0, v0, temp1);
        as.VMOR(v0, v0, temp2);
        break;
    }
    case ORD_Q: {
        // Set if neither are NaN
        as.VMFEQ(temp1, dst, dst);
        as.VMFEQ(temp2, src, src);
        as.VMAND(v0, temp1, temp2);
        break;
    }
    default: {
        UNREACHABLE();
        break;
    }
    }

    // Set to 1s where the mask is set
    as.VMV(result, 0);
    as.VOR(result, result, -1, VecMask::Yes);

    rec.setVec(&operands[0], result);
}

FAST_HANDLE(CMPPS) {
    CMPP(rec, rip, as, instruction, operands, SEW::E32, 4);
}

FAST_HANDLE(CMPPD) {
    CMPP(rec, rip, as, instruction, operands, SEW::E64, 2);
}

// This instruction is MMX only
FAST_HANDLE(PSHUFW) {
    biscuit::Vec dst = rec.scratchVec();
    biscuit::Vec src = rec.getVec(&operands[1]);
    u8 imm = rec.getImmediate(&operands[2]);
    u64 el0 = imm & 0b11;
    u64 el1 = (imm >> 2) & 0b11;
    u64 el2 = (imm >> 4) & 0b11;
    u64 el3 = (imm >> 6) & 0b11;

    biscuit::GPR temp = rec.scratch();
    biscuit::Vec iota = rec.scratchVec();
    u64 mask = (el3 << 48) | (el2 << 32) | (el1 << 16) | el0;
    rec.setVectorState(SEW::E64, 1);
    as.LI(temp, mask);
    as.VMV_SX(iota, temp);

    rec.setVectorState(SEW::E16, 4);
    as.VRGATHER(dst, src, iota);

    rec.setVec(&operands[0], dst);
}

FAST_HANDLE(PSHUFD) {
    u8 imm = rec.getImmediate(&operands[2]);
    u64 el0 = imm & 0b11;
    u64 el1 = (imm >> 2) & 0b11;
    u64 el2 = (imm >> 4) & 0b11;
    u64 el3 = (imm >> 6) & 0b11;

    biscuit::Vec result = rec.scratchVec();
    biscuit::Vec iota = rec.scratchVec();
    biscuit::Vec src = rec.getVec(&operands[1]);

    rec.setVectorState(SEW::E64, 1);
    biscuit::GPR temp = rec.scratch();
    u64 mask = (el3 << 48) | (el2 << 32) | (el1 << 16) | el0;
    as.LI(temp, mask);
    as.VMV_SX(iota, temp);

    rec.setVectorState(SEW::E32, 4);
    as.VRGATHEREI16(result, src, iota);

    rec.setVec(&operands[0], result);
}

FAST_HANDLE(SHUFPS) {
    u8 imm = rec.getImmediate(&operands[2]);
    u64 el0 = imm & 0b11;
    u64 el1 = (imm >> 2) & 0b11;
    u64 el2 = (imm >> 4) & 0b11;
    u64 el3 = (imm >> 6) & 0b11;

    biscuit::Vec iota = rec.scratchVec();
    biscuit::Vec dst = rec.getVec(&operands[0]);
    biscuit::Vec src = rec.getVec(&operands[1]);
    biscuit::Vec result1 = rec.scratchVec();
    biscuit::Vec result2 = rec.scratchVec();

    u32 imm1 = el1 << 16 | el0;
    u32 imm2 = el3 << 16 | el2;
    rec.setVectorState(SEW::E32, 4);
    rec.vsplat(iota, imm1);
    as.VRGATHEREI16(result1, dst, iota);
    if (imm1 != imm2) {
        rec.vsplat(iota, imm2);
    }
    as.VRGATHEREI16(result2, src, iota);
    as.VSLIDEUP(result1, result2, 2);

    rec.setVec(&operands[0], result1);
}

FAST_HANDLE(PSHUFB) {
    biscuit::GPR bitmask = rec.scratch();
    biscuit::Vec tmp = rec.scratchVec();
    biscuit::Vec mask_masked = rec.scratchVec();
    biscuit::Vec dst = rec.getVec(&operands[0]);
    biscuit::Vec mask = rec.getVec(&operands[1]);

    bool is_mmx = operands[0].reg.value >= ZYDIS_REGISTER_MM0 && operands[0].reg.value <= ZYDIS_REGISTER_MM7;
    if (is_mmx) {
        as.LI(bitmask, 0b10000111);
        rec.setVectorState(SEW::E8, 8);
    } else {
        // Keep 0...3 for regular shifting and bit 7 which indicates resulting element goes to 0, maps well with vrgather this way
        as.LI(bitmask, 0b10001111);
        rec.setVectorState(SEW::E8, 16);
    }

    as.VAND(mask_masked, mask, bitmask);
    as.VRGATHER(tmp, dst, mask_masked);

    rec.setVec(&operands[0], tmp);

    ASSERT_MSG(Extensions::VLEN < 2048, "Woah... How did you get a 2048-bit VLEN device? Our PSHUFB implementation would break");
}

FAST_HANDLE(PBLENDVB) {
    biscuit::Vec dst = rec.getVec(&operands[0]);
    biscuit::Vec src = rec.getVec(&operands[1]);
    biscuit::Vec mask = rec.getVec(X86_REF_XMM0);

    rec.setVectorState(SEW::E8, 16);
    as.VMSLT(v0, mask, x0);
    as.VMERGE(dst, dst, src);

    rec.setVec(&operands[0], dst);
}

FAST_HANDLE(PBLENDW) {
    u8 imm = rec.getImmediate(&operands[2]);
    biscuit::GPR mask = rec.scratch();
    biscuit::Vec dst = rec.getVec(&operands[0]);
    biscuit::Vec src = rec.getVec(&operands[1]);

    rec.setVectorState(SEW::E16, 8);
    as.LI(mask, imm);
    as.VMV(v0, mask);
    as.VMERGE(dst, dst, src);

    rec.setVec(&operands[0], dst);
}

FAST_HANDLE(BLENDPS) {
    u8 imm = rec.getImmediate(&operands[2]) & 0b1111;
    biscuit::Vec dst = rec.getVec(&operands[0]);
    biscuit::Vec src = rec.getVec(&operands[1]);

    rec.setVectorState(SEW::E32, 4);
    as.VMV(v0, imm);
    as.VMERGE(dst, dst, src);

    rec.setVec(&operands[0], dst);
}

FAST_HANDLE(BLENDVPS) {
    biscuit::Vec dst = rec.getVec(&operands[0]);
    biscuit::Vec src = rec.getVec(&operands[1]);
    biscuit::Vec mask = rec.getVec(X86_REF_XMM0); // I see where VMERGE took inspiration from /j

    rec.setVectorState(SEW::E32, 4);
    as.VMSLT(v0, mask, x0);
    as.VMERGE(dst, dst, src);

    rec.setVec(&operands[0], dst);
}

FAST_HANDLE(BLENDPD) {
    u8 imm = rec.getImmediate(&operands[2]) & 0b11;
    biscuit::Vec dst = rec.getVec(&operands[0]);
    biscuit::Vec src = rec.getVec(&operands[1]);

    rec.setVectorState(SEW::E64, 2);
    as.VMV(v0, imm);
    as.VMERGE(dst, dst, src);

    rec.setVec(&operands[0], dst);
}

FAST_HANDLE(BLENDVPD) {
    biscuit::Vec dst = rec.getVec(&operands[0]);
    biscuit::Vec src = rec.getVec(&operands[1]);
    biscuit::Vec mask = rec.getVec(X86_REF_XMM0);

    rec.setVectorState(SEW::E64, 2);
    as.VMSLT(v0, mask, x0);
    as.VMERGE(dst, dst, src);

    rec.setVec(&operands[0], dst);
}

FAST_HANDLE(DPPS) {
    biscuit::GPR splat = rec.scratch();
    biscuit::Vec mul = rec.scratchVec();
    biscuit::Vec sum = rec.scratchVec();
    biscuit::Vec dst = rec.getVec(&operands[0]);
    biscuit::Vec src = rec.getVec(&operands[1]);
    u8 immediate = rec.getImmediate(&operands[2]);

    u8 mmask = immediate >> 4;
    u8 zmask = ~immediate & 0b1111;

    rec.setVectorState(SEW::E32, 4);
    as.VMV(v0, mmask);
    as.VMV(mul, 0);
    as.VMV(sum, 0);
    as.VFMUL(mul, dst, src, VecMask::Yes);
    as.VFREDUSUM(sum, mul, sum);
    as.VMV_XS(splat, sum);
    as.VMV(dst, splat);

    if (zmask != 0) {
        as.VMV(v0, zmask);
        as.VXOR(dst, dst, dst, VecMask::Yes);
    } else {
        // Using all elements
    }

    rec.setVec(&operands[0], dst);
}

FAST_HANDLE(PSHUFLW) {
    u8 imm = rec.getImmediate(&operands[2]);
    u64 el0 = imm & 0b11;
    u64 el1 = (imm >> 2) & 0b11;
    u64 el2 = (imm >> 4) & 0b11;
    u64 el3 = (imm >> 6) & 0b11;
    u64 low = el0 | el1 << 16 | el2 << 32 | el3 << 48;

    biscuit::Vec src = rec.getVec(&operands[1]);
    biscuit::Vec iota = rec.scratchVec();
    biscuit::Vec iota2 = rec.scratchVec();
    biscuit::Vec result = rec.scratchVec();
    biscuit::GPR low_gpr = rec.scratch();

    as.LI(low_gpr, low);
    rec.setVectorState(SEW::E16, 8);
    // Slide down 4 words, so then the register looks like 7 6 5 4, then we can slide up the other 4 elements
    as.VID(iota);
    as.VSLIDEDOWN(iota, iota, 4);
    rec.setVectorState(SEW::E64, 2);
    as.VSLIDE1UP(iota2, iota, low_gpr);
    as.VMV(result, 0);
    rec.setVectorState(SEW::E16, 8);
    as.VRGATHER(result, src, iota2);

    rec.setVec(&operands[0], result);
}

FAST_HANDLE(PSHUFHW) {
    u8 imm = rec.getImmediate(&operands[2]);
    biscuit::Vec result = rec.scratchVec();
    biscuit::Vec src = rec.getVec(&operands[1]);
    biscuit::GPR high_gpr = rec.scratch();
    biscuit::GPR tmp = rec.scratch();
    biscuit::Vec iota = rec.scratchVec();
    biscuit::Vec iota2 = rec.scratchVec();

    u64 el0 = 4 + (imm & 0b11);
    u64 el1 = 4 + ((imm >> 2) & 0b11);
    u64 el2 = 4 + ((imm >> 4) & 0b11);
    u64 el3 = 4 + ((imm >> 6) & 0b11);
    u64 high = el0 | el1 << 16 | el2 << 32 | el3 << 48;

    rec.setVectorState(SEW::E64, 2);
    as.LI(high_gpr, high);
    as.VMV_SX(iota, high_gpr);
    as.VSLIDE1UP(iota2, iota, x0);

    rec.setVectorState(SEW::E16, 8);
    as.VMV(result, src); // to move the low words

    as.LI(tmp, 0b11110000); // operate on top words only
    as.VMV(v0, tmp);

    as.VRGATHER(result, src, iota2, VecMask::Yes);

    rec.setVec(&operands[0], result);
}

FAST_HANDLE(PALIGNR) {
    x86_ref_e ref = rec.zydisToRef(operands[0].reg.value);
    if (ref >= X86_REF_MM0 && ref <= X86_REF_MM7) {
        ERROR("palignr not implemented for mmx registers");
    }
    u8 imm = rec.getImmediate(&operands[2]);
    biscuit::GPR temp = rec.scratch();
    biscuit::Vec result = rec.scratchVec();
    biscuit::Vec slide_up = rec.scratchVec();
    biscuit::Vec dst = rec.getVec(&operands[0]);
    biscuit::Vec src = rec.getVec(&operands[1]);

    rec.setVectorState(SEW::E64, 2);

    if (imm > 31) {
        as.VMV(dst, 0);
        rec.setVec(&operands[0], dst);
        return;
    }

    if (16 - imm > 0) {
        as.LI(temp, ~((1ull << (16 - imm)) - 1));
        as.VMV_SX(v0, temp);
        rec.setVectorState(SEW::E8, 16);
        as.VMV(result, 0);
        as.VSLIDEDOWN(result, src, imm);
        as.VAND(result, result, 0, VecMask::Yes);
        as.VMV(slide_up, 0);
        as.VSLIDEUP(slide_up, dst, 16 - imm);
        as.VOR(result, result, slide_up);
    } else {
        as.LI(temp, ~((1ull << (32 - imm)) - 1));
        as.VMV_SX(v0, temp);
        rec.setVectorState(SEW::E8, 16);
        as.VMV(result, 0);
        as.VSLIDEDOWN(result, dst, imm - 16);
        as.VAND(result, result, 0, VecMask::Yes);
    }

    rec.setVec(&operands[0], result);
}

void CTZ(Recompiler& rec, Assembler& as, biscuit::GPR result, biscuit::GPR src) {
    if (Extensions::B) {
        as.CTZ(result, src);
    } else {
        // This would infinitely loop if src is 0, but we know it's not
        biscuit::GPR scratch = rec.scratch();
        Label loop, escape;
        as.LI(result, 0);

        as.Bind(&loop);
        as.SRL(scratch, src, result);
        as.ANDI(scratch, scratch, 1);
        as.BNEZ(scratch, &escape);
        as.ADDI(result, result, 1);
        as.J(&loop);

        as.Bind(&escape);
    }
}

FAST_HANDLE(BSF) {
    biscuit::GPR result = rec.scratch();
    biscuit::GPR src = rec.getGPR(&operands[1]);
    biscuit::GPR dst = rec.getGPR(&operands[0]);
    (void)dst; // must be loaded since conditional code follows
    biscuit::GPR zf = rec.flag(X86_REF_ZF);

    Label end;
    as.SEQZ(zf, src);
    as.BEQZ(src, &end);

    CTZ(rec, as, result, src);

    rec.setGPR(&operands[0], result);

    as.Bind(&end);
}

FAST_HANDLE(TZCNT) {
    biscuit::GPR result = rec.scratch();
    biscuit::GPR src = rec.getGPR(&operands[1]);
    biscuit::GPR dst = rec.getGPR(&operands[0]);
    (void)dst; // must be loaded since conditional code follows
    biscuit::GPR zf = rec.flag(X86_REF_ZF);
    biscuit::GPR cf = rec.flag(X86_REF_CF);

    Label end;
    as.LI(result, instruction.operand_width);
    as.SEQZ(cf, src);
    as.BEQZ(src, &end);
    CTZ(rec, as, result, src);
    as.J(&end);

    as.Bind(&end);
    rec.setGPR(&operands[0], result);
    as.SEQZ(zf, result);
}

void BITSTRING_func(Recompiler& rec, u64 rip, Assembler& as, ZydisDecodedInstruction& instruction, ZydisDecodedOperand* operands, u64 func) {
    // Special case where the memory may index past the effective address, only when offset is a register
    biscuit::GPR base = rec.lea(&operands[0]);
    biscuit::GPR bit = rec.getGPR(&operands[1]);
    biscuit::GPR retval = rec.scratch();
    rec.writebackState();
    rec.sext(a1, bit, rec.zydisToSize(operands[1].size));
    as.MV(a0, base);
    rec.call(func);
    as.MV(retval, a0); // so restoreState doesn't ovewrite
    rec.restoreState();

    // Write result to CF
    biscuit::GPR cf = rec.flag(X86_REF_CF);
    as.MV(cf, retval);
}

FAST_HANDLE(BTC) {
    if (operands[0].type == ZYDIS_OPERAND_TYPE_MEMORY && operands[1].type == ZYDIS_OPERAND_TYPE_REGISTER) {
        BITSTRING_func(rec, rip, as, instruction, operands, (u64)&felix86_btc);
        return;
    }

    biscuit::GPR bit = rec.getGPR(&operands[1]);
    biscuit::GPR dst = rec.getGPR(&operands[0]);
    biscuit::GPR cf = rec.flag(X86_REF_CF);

    biscuit::GPR shift = rec.scratch();
    biscuit::GPR mask = rec.scratch();
    biscuit::GPR result = rec.scratch();

    u8 bit_size = operands[0].size;
    as.ANDI(shift, bit, bit_size - 1);
    as.SRL(cf, dst, shift);
    as.ANDI(cf, cf, 1);
    as.LI(mask, 1);
    as.SLL(mask, mask, shift);
    as.XOR(result, dst, mask);

    rec.setGPR(&operands[0], result);
}

FAST_HANDLE(BT) {
    if (operands[0].type == ZYDIS_OPERAND_TYPE_MEMORY && operands[1].type == ZYDIS_OPERAND_TYPE_REGISTER) {
        BITSTRING_func(rec, rip, as, instruction, operands, (u64)&felix86_bt);
        return;
    }

    biscuit::GPR shift = rec.scratch();
    biscuit::GPR bit = rec.getGPR(&operands[1]);
    biscuit::GPR cf = rec.flag(X86_REF_CF);
    biscuit::GPR dst = rec.getGPR(&operands[0]);

    u8 bit_size = operands[0].size;
    as.ANDI(shift, bit, bit_size - 1);

    as.SRL(cf, dst, shift);
    as.ANDI(cf, cf, 1);
}

FAST_HANDLE(BTS) {
    if (operands[0].type == ZYDIS_OPERAND_TYPE_MEMORY && operands[1].type == ZYDIS_OPERAND_TYPE_REGISTER) {
        BITSTRING_func(rec, rip, as, instruction, operands, (u64)&felix86_bts);
        return;
    }

    biscuit::GPR result = rec.scratch();
    biscuit::GPR bit = rec.getGPR(&operands[1]);
    biscuit::GPR dst = rec.getGPR(&operands[0]);
    biscuit::GPR shift = rec.scratch();

    u8 bit_size = operands[0].size;
    as.ANDI(shift, bit, bit_size - 1);
    if (rec.shouldEmitFlag(rip, X86_REF_CF)) {
        biscuit::GPR cf = rec.flag(X86_REF_CF);
        as.SRL(cf, dst, shift);
        as.ANDI(cf, cf, 1);
    }

    biscuit::GPR one = rec.scratch();
    as.LI(one, 1);
    as.SLL(one, one, shift);
    as.OR(result, dst, one);

    rec.setGPR(&operands[0], result);
}

FAST_HANDLE(BTR) {
    if (operands[0].type == ZYDIS_OPERAND_TYPE_MEMORY && operands[1].type == ZYDIS_OPERAND_TYPE_REGISTER) {
        BITSTRING_func(rec, rip, as, instruction, operands, (u64)&felix86_btr);
        return;
    }

    biscuit::GPR result = rec.scratch();
    biscuit::GPR bit = rec.getGPR(&operands[1]);
    biscuit::GPR dst = rec.getGPR(&operands[0]);
    biscuit::GPR cf = rec.flag(X86_REF_CF);
    biscuit::GPR shift = rec.scratch();

    u8 bit_size = operands[0].size;
    as.ANDI(shift, bit, bit_size - 1);
    as.SRL(cf, dst, shift);
    as.ANDI(cf, cf, 1);
    biscuit::GPR one = rec.scratch();
    as.LI(one, 1);
    as.SLL(one, one, shift);
    as.NOT(one, one);
    as.AND(result, dst, one);

    rec.setGPR(&operands[0], result);
}

FAST_HANDLE(BLSR) {
    WARN("BLSR is broken, check BLSR_flags");
    biscuit::GPR src = rec.getGPR(&operands[1]);
    biscuit::GPR result = rec.scratch();

    as.ADDI(result, src, -1);
    as.AND(result, src, result);

    if (rec.shouldEmitFlag(rip, X86_REF_CF)) {
        biscuit::GPR cf = rec.flag(X86_REF_CF);
        as.SEQZ(cf, src);
    }

    if (rec.shouldEmitFlag(rip, X86_REF_ZF)) {
        rec.updateZero(result, rec.zydisToSize(operands[0].size));
    }

    if (rec.shouldEmitFlag(rip, X86_REF_SF)) {
        rec.updateSign(result, rec.zydisToSize(operands[0].size));
    }

    rec.setGPR(&operands[0], result);
}

void BSR(Recompiler& rec, Assembler& as, biscuit::GPR result, biscuit::GPR src, int size) {
    if (Extensions::B) {
        if (size == 64) {
            as.CLZ(result, src);
            as.XORI(result, result, 63);
        } else if (size == 32) {
            as.CLZW(result, src);
            as.XORI(result, result, 31);
        } else if (size == 16) {
            as.SLLI(result, src, 16);
            as.CLZW(result, result);
            as.XORI(result, result, 15);
        } else {
            UNREACHABLE();
        }
    } else {
        // This would infinitely loop if src is 0, but we know it's not
        biscuit::GPR scratch = rec.scratch();
        Label loop, escape;
        as.LI(result, size - 1);
        as.Bind(&loop);
        as.SRL(scratch, src, result);
        as.ANDI(scratch, scratch, 1);
        as.BNEZ(scratch, &escape);
        as.ADDI(result, result, -1);
        as.J(&loop);
        as.Bind(&escape);
    }
}

FAST_HANDLE(BSR) {
    biscuit::GPR result = rec.scratch();
    biscuit::GPR src = rec.getGPR(&operands[1]);
    biscuit::GPR dst = rec.getGPR(&operands[0]);
    (void)dst; // must be loaded since conditional code follows
    biscuit::GPR zf = rec.flag(X86_REF_ZF);

    Label end;
    as.SEQZ(zf, src);
    as.BEQZ(src, &end);
    BSR(rec, as, result, src, instruction.operand_width);
    rec.setGPR(&operands[0], result);

    as.Bind(&end);
}

void REV8(Recompiler& rec, Assembler& as, biscuit::GPR result, biscuit::GPR src) {
    if (Extensions::B) {
        as.REV8(result, src);
    } else {
        biscuit::GPR a1 = rec.scratch();
        biscuit::GPR a2 = rec.scratch();
        biscuit::GPR a3 = rec.scratch();
        biscuit::GPR a4 = rec.scratch();
        biscuit::GPR a5 = rec.scratch();
        // Lifted from clang
        as.SRLI(a1, src, 40);
        as.LUI(a2, 16);
        as.SRLI(a3, src, 56);
        as.SRLI(a4, src, 24);
        as.LUI(a5, 4080);
        as.ADDIW(a2, a2, -256);
        as.AND(a1, a1, a2);
        as.OR(a1, a1, a3);
        as.SRLI(a3, src, 8);
        as.AND(a4, a4, a5);
        as.SRLIW(a3, a3, 24);
        as.SLLI(a3, a3, 24);
        as.OR(a3, a3, a4);
        as.SRLIW(a4, src, 24);
        as.AND(a5, a5, src);
        as.AND(a2, a2, src);
        as.SLLI(result, src, 56);
        as.SLLI(a4, a4, 32);
        as.SLLI(a5, a5, 24);
        as.OR(a4, a4, a5);
        as.SLLI(a2, a2, 40);
        as.OR(a1, a1, a3);
        as.OR(result, result, a2);
        as.OR(result, result, a4);
        as.OR(result, result, a1);
        rec.popScratch();
        rec.popScratch();
        rec.popScratch();
        rec.popScratch();
        rec.popScratch();
    }
}

FAST_HANDLE(BSWAP) {
    x86_size_e size = rec.getSize(&operands[0]);
    biscuit::GPR dst = rec.getGPR(&operands[0]);
    biscuit::GPR result = rec.scratch();

    if (size == X86_SIZE_DWORD) {
        REV8(rec, as, result, dst);
        as.SRLI(result, result, 32);
    } else if (size == X86_SIZE_QWORD) {
        REV8(rec, as, result, dst);
    } else {
        UNREACHABLE();
    }

    rec.setGPR(&operands[0], result);
}

FAST_HANDLE(MOVLPS) {
    if (operands[0].type == ZYDIS_OPERAND_TYPE_REGISTER) {
        ASSERT(operands[1].type == ZYDIS_OPERAND_TYPE_MEMORY);
        biscuit::Vec dst = rec.getVec(&operands[0]);
        biscuit::Vec src = rec.getVec(&operands[1]);

        rec.setVectorState(SEW::E64, 1);
        as.VMV(dst, src);

        rec.setVec(&operands[0], dst);
    } else if (operands[0].type == ZYDIS_OPERAND_TYPE_MEMORY) {
        biscuit::Vec src = rec.getVec(&operands[1]);
        rec.setVec(&operands[0], src);
    } else {
        UNREACHABLE();
    }
}

FAST_HANDLE(MOVLPD) {
    fast_MOVLPS(rec, rip, as, instruction, operands);
}

FAST_HANDLE(MOVHLPS) {
    ASSERT(operands[0].type == ZYDIS_OPERAND_TYPE_REGISTER);
    biscuit::Vec temp = rec.scratchVec();
    biscuit::Vec dst = rec.getVec(&operands[0]);
    biscuit::Vec src = rec.getVec(&operands[1]);

    rec.setVectorState(SEW::E64, 2);
    as.VSLIDEDOWN(temp, src, 1);
    as.VMV(v0, 0b10);
    as.VMERGE(dst, temp, dst);
    rec.setVec(&operands[0], dst);
}

FAST_HANDLE(ROL) {
    x86_size_e size = rec.getSize(&operands[0]);
    bool needs_cf = rec.shouldEmitFlag(rip, X86_REF_CF);
    bool needs_of = rec.shouldEmitFlag(rip, X86_REF_OF);
    bool needs_any_flag = needs_cf || needs_of;
    bool qword_or_dword = size == X86_SIZE_QWORD || size == X86_SIZE_DWORD;
    bool is_immediate = operands[1].type == ZYDIS_OPERAND_TYPE_IMMEDIATE;
    if (g_config.noflag_opts && qword_or_dword && is_immediate && !needs_any_flag) {
        switch (size) {
        case X86_SIZE_DWORD: {
            biscuit::GPR dst;
            if (operands[0].type == ZYDIS_OPERAND_TYPE_REGISTER) {
                // Don't zext, we use the W instructions so it's ok
                dst = rec.getGPR(&operands[0], X86_SIZE_QWORD);
            } else {
                dst = rec.getGPR(&operands[0]);
            }
            u8 immediate = rec.getImmediate(&operands[1]) & 31;
            u8 rotate_amount = (32 - immediate) & 31;
            if (Extensions::B) {
                as.RORIW(dst, dst, rotate_amount);
            } else {
                biscuit::GPR temp = rec.scratch();
                as.SRLIW(temp, dst, rotate_amount);
                as.SLLIW(dst, dst, immediate);
                as.OR(dst, dst, temp);
            }
            rec.setGPR(&operands[0], dst);
            break;
        }
        case X86_SIZE_QWORD: {
            biscuit::GPR dst = rec.getGPR(&operands[0]);
            u8 immediate = rec.getImmediate(&operands[1]) & 63;
            u8 rotate_amount = (64 - immediate) & 63;
            if (Extensions::B) {
                as.RORI(dst, dst, rotate_amount);
            } else {
                biscuit::GPR temp = rec.scratch();
                as.SRLI(temp, dst, rotate_amount);
                as.SLLI(dst, dst, immediate);
                as.OR(dst, dst, temp);
            }
            rec.setGPR(&operands[0], dst);
            break;
        }
        default: {
            UNREACHABLE();
        }
        }
        return;
    }

    biscuit::GPR dst = rec.getGPR(&operands[0]);
    biscuit::GPR src = rec.getGPR(&operands[1]);
    biscuit::GPR count = rec.scratch();

    Label zero_count;

    // TODO: optimize rotate with immediate to skip this check
    biscuit::GPR cf = rec.flag(X86_REF_CF);
    biscuit::GPR of = rec.flag(X86_REF_OF);
    as.ANDI(count, src, rec.getBitSize(size) == 64 ? 63 : 31);
    as.BEQZ(count, &zero_count);

    biscuit::GPR temp = rec.scratch();
    biscuit::GPR neg_count = rec.scratch();
    as.NEG(neg_count, count);
    as.ANDI(neg_count, neg_count, rec.getBitSize(size) - 1);
    as.SLL(temp, dst, count);
    as.SRL(neg_count, dst, neg_count);
    as.OR(dst, temp, neg_count);
    as.ANDI(cf, dst, 1);
    as.SRLI(of, dst, rec.getBitSize(size) - 1);
    as.XOR(of, of, cf);

    rec.setGPR(&operands[0], dst);

    as.Bind(&zero_count);
}

// TODO: optimize me for immediates and no flags like ROL
FAST_HANDLE(ROR) {
    x86_size_e size = rec.getSize(&operands[0]);
    biscuit::GPR dst = rec.getGPR(&operands[0]);
    biscuit::GPR src = rec.getGPR(&operands[1]);
    biscuit::GPR count = rec.scratch();

    Label zero_count;

    biscuit::GPR cf = rec.flag(X86_REF_CF);
    biscuit::GPR of = rec.flag(X86_REF_OF);
    as.ANDI(count, src, rec.getBitSize(size) == 64 ? 63 : 31);
    as.BEQZ(count, &zero_count);

    biscuit::GPR temp = rec.scratch();
    biscuit::GPR neg_count = rec.scratch();
    as.NEG(neg_count, count);
    as.ANDI(neg_count, neg_count, rec.getBitSize(size) - 1);
    as.SRL(temp, dst, count);
    as.SLL(neg_count, dst, neg_count);
    as.OR(dst, temp, neg_count);
    as.SRLI(cf, dst, rec.getBitSize(size) - 1);
    as.ANDI(cf, cf, 1);
    as.SRLI(of, dst, rec.getBitSize(size) - 2);
    as.ANDI(of, of, 1);
    as.XOR(of, of, cf);

    rec.setGPR(&operands[0], dst);

    as.Bind(&zero_count);
}

FAST_HANDLE(PSLLDQ) {
    u8 imm = rec.getImmediate(&operands[1]);
    biscuit::Vec dst = rec.getVec(&operands[0]);
    biscuit::Vec temp = rec.scratchVec();
    rec.setVectorState(SEW::E8, 16);
    if (imm > 15) {
        as.VMV(temp, 0);
    } else {
        as.VMV(temp, 0);
        as.VSLIDEUP(temp, dst, imm);
    }
    rec.setVec(&operands[0], temp);
}

FAST_HANDLE(PSRLDQ) {
    u8 imm = rec.getImmediate(&operands[1]);
    biscuit::Vec dst = rec.getVec(&operands[0]);
    biscuit::Vec temp = rec.scratchVec();
    if (imm > 15) {
        rec.setVectorState(SEW::E64, 2);
        as.VMV(temp, 0);
    } else {
        rec.setVectorState(SEW::E64, 2);
        biscuit::GPR mask = rec.scratch();
        as.LI(mask, ~((1ull << (16 - imm)) - 1));
        as.VMV_SX(v0, mask);
        rec.setVectorState(SEW::E8, 16);
        as.VSLIDEDOWN(temp, dst, imm);
        as.VAND(temp, temp, 0, VecMask::Yes);
    }
    rec.setVec(&operands[0], temp);
}

FAST_HANDLE(PSLLW) {
    biscuit::GPR shift = rec.scratch();
    biscuit::Vec dst = rec.getVec(&operands[0]);
    if (operands[1].type == ZYDIS_OPERAND_TYPE_IMMEDIATE) {
        u8 val = rec.getImmediate(&operands[1]);
        as.LI(shift, val);
    } else {
        biscuit::Vec src = rec.getVec(&operands[1]);
        rec.setVectorState(SEW::E64, 2);
        as.VMV_XS(shift, src);
    }
    rec.setVectorState(SEW::E16, 8);
    biscuit::GPR max = rec.scratch();
    biscuit::Label dont_zero;
    as.LI(max, 16);
    as.BLTU(shift, max, &dont_zero);
    as.VMV(dst, 0);
    as.Bind(&dont_zero);
    as.VSLL(dst, dst, shift);
    rec.setVec(&operands[0], dst);
}

FAST_HANDLE(PSLLQ) {
    biscuit::GPR shift = rec.scratch();
    if (operands[1].type == ZYDIS_OPERAND_TYPE_IMMEDIATE) {
        u8 val = rec.getImmediate(&operands[1]);
        as.LI(shift, val);
    } else {
        biscuit::Vec src = rec.getVec(&operands[1]);
        rec.setVectorState(SEW::E64, 2);
        as.VMV_XS(shift, src);
    }
    biscuit::Vec dst = rec.getVec(&operands[0]);
    biscuit::GPR max = rec.scratch();
    biscuit::Label dont_zero;
    rec.setVectorState(SEW::E64, 2);
    as.LI(max, 64);
    as.BLTU(shift, max, &dont_zero);
    as.VMV(dst, 0);
    as.Bind(&dont_zero);
    as.VSLL(dst, dst, shift);
    rec.setVec(&operands[0], dst);
}

FAST_HANDLE(PSLLD) {
    biscuit::GPR shift = rec.scratch();
    biscuit::Vec dst = rec.getVec(&operands[0]);
    if (operands[1].type == ZYDIS_OPERAND_TYPE_IMMEDIATE) {
        u8 val = rec.getImmediate(&operands[1]);
        as.LI(shift, val);
    } else {
        biscuit::Vec src = rec.getVec(&operands[1]);
        rec.setVectorState(SEW::E64, 2);
        as.VMV_XS(shift, src);
    }
    rec.setVectorState(SEW::E32, 4);
    biscuit::GPR max = rec.scratch();
    biscuit::Label dont_zero;
    as.LI(max, 32);
    as.BLTU(shift, max, &dont_zero);
    as.VMV(dst, 0);
    as.Bind(&dont_zero);
    as.VSLL(dst, dst, shift);
    rec.setVec(&operands[0], dst);
}

FAST_HANDLE(PSRLD) {
    biscuit::GPR shift = rec.scratch();
    biscuit::Vec dst = rec.getVec(&operands[0]);
    if (operands[1].type == ZYDIS_OPERAND_TYPE_IMMEDIATE) {
        u8 val = rec.getImmediate(&operands[1]);
        as.LI(shift, val);
    } else {
        biscuit::Vec src = rec.getVec(&operands[1]);
        rec.setVectorState(SEW::E64, 2);
        as.VMV_XS(shift, src);
    }
    rec.setVectorState(SEW::E32, 4);
    biscuit::GPR max = rec.scratch();
    biscuit::Label dont_zero;
    as.LI(max, 32);
    as.BLTU(shift, max, &dont_zero);
    as.VMV(dst, 0);
    as.Bind(&dont_zero);
    as.VSRL(dst, dst, shift);
    rec.setVec(&operands[0], dst);
}

FAST_HANDLE(PSRLW) {
    biscuit::GPR shift = rec.scratch();
    biscuit::Vec dst = rec.getVec(&operands[0]);
    if (operands[1].type == ZYDIS_OPERAND_TYPE_IMMEDIATE) {
        u8 val = rec.getImmediate(&operands[1]);
        as.LI(shift, val);
    } else {
        biscuit::Vec src = rec.getVec(&operands[1]);
        rec.setVectorState(SEW::E64, 2);
        as.VMV_XS(shift, src);
    }
    rec.setVectorState(SEW::E16, 8);
    biscuit::GPR max = rec.scratch();
    biscuit::Label dont_zero;
    as.LI(max, 16);
    as.BLTU(shift, max, &dont_zero);
    as.VMV(dst, 0);
    as.Bind(&dont_zero);
    as.VSRL(dst, dst, shift);
    rec.setVec(&operands[0], dst);
}

FAST_HANDLE(PSRLQ) {
    biscuit::GPR shift = rec.scratch();
    if (operands[1].type == ZYDIS_OPERAND_TYPE_IMMEDIATE) {
        u8 val = rec.getImmediate(&operands[1]);
        as.LI(shift, val);
    } else {
        biscuit::Vec src = rec.getVec(&operands[1]);
        rec.setVectorState(SEW::E64, 2);
        as.VMV_XS(shift, src);
    }
    biscuit::Vec dst = rec.getVec(&operands[0]);
    rec.setVectorState(SEW::E64, 2);
    biscuit::GPR max = rec.scratch();
    biscuit::Label dont_zero;
    as.LI(max, 64);
    as.BLTU(shift, max, &dont_zero);
    as.VMV(dst, 0);
    as.Bind(&dont_zero);
    as.VSRL(dst, dst, shift);
    rec.setVec(&operands[0], dst);
}

FAST_HANDLE(PSRAW) {
    biscuit::GPR shift = rec.scratch();
    biscuit::Vec dst = rec.getVec(&operands[0]);
    if (operands[1].type == ZYDIS_OPERAND_TYPE_IMMEDIATE) {
        u8 val = rec.getImmediate(&operands[1]);
        if (val > 15)
            val = 15;
        as.LI(shift, val);
    } else {
        biscuit::Vec src = rec.getVec(&operands[1]);
        rec.setVectorState(SEW::E64, 2);
        as.VMV_XS(shift, src);

        Label ok;
        biscuit::GPR max = rec.scratch();
        as.LI(max, 15);
        as.BLEU(shift, max, &ok);
        as.LI(shift, 15); // bigger than 15, set to 15
        as.Bind(&ok);
    }
    rec.setVectorState(SEW::E16, 8);
    as.VSRA(dst, dst, shift);
    rec.setVec(&operands[0], dst);
}

FAST_HANDLE(PSRAD) {
    biscuit::GPR shift = rec.scratch();
    biscuit::Vec dst = rec.getVec(&operands[0]);
    if (operands[1].type == ZYDIS_OPERAND_TYPE_IMMEDIATE) {
        u8 val = rec.getImmediate(&operands[1]);
        if (val > 31)
            val = 31;
        as.LI(shift, val);
    } else {
        biscuit::Vec src = rec.getVec(&operands[1]);
        rec.setVectorState(SEW::E64, 2);
        as.VMV_XS(shift, src);

        Label ok;
        biscuit::GPR max = rec.scratch();
        as.LI(max, 31);
        as.BLTU(shift, max, &ok);
        as.LI(shift, 31); // bigger than 31, set to 31
        as.Bind(&ok);
    }
    rec.setVectorState(SEW::E32, 4);
    as.VSRA(dst, dst, shift);
    rec.setVec(&operands[0], dst);
}

FAST_HANDLE(SFENCE) {
    as.FENCE(FenceOrder::RW, FenceOrder::RW); // just make a full fence for now, TODO: we can optimize this some day
}

FAST_HANDLE(LFENCE) {
    as.FENCE(FenceOrder::RW, FenceOrder::RW); // just make a full fence for now, TODO: we can optimize this some day
}

FAST_HANDLE(MFENCE) {
    as.FENCE(FenceOrder::RW, FenceOrder::RW); // just make a full fence for now, TODO: we can optimize this some day
}

FAST_HANDLE(MOVSX) {
    biscuit::GPR result = rec.scratch();
    biscuit::GPR src = rec.getGPR(&operands[1]);
    x86_size_e size = rec.getSize(&operands[1]);

    switch (size) {
    case X86_SIZE_BYTE:
    case X86_SIZE_BYTE_HIGH: {
        rec.sextb(result, src);
        break;
    }
    case X86_SIZE_WORD: {
        rec.sexth(result, src);
        break;
    }
    default: {
        UNREACHABLE();
        break;
    }
    }

    rec.setGPR(&operands[0], result);
}

void COMIS(Recompiler& rec, u64 rip, Assembler& as, ZydisDecodedInstruction& instruction, ZydisDecodedOperand* operands, SEW sew) {
    // If it's lhs < rhs, ZF remains 0 and CF gets set to 1
    biscuit::GPR cf = rec.flag(X86_REF_CF);
    biscuit::GPR zf = rec.flag(X86_REF_ZF);
    biscuit::GPR sf = rec.flag(X86_REF_SF);
    biscuit::GPR of = rec.flag(X86_REF_OF);

    if (rec.shouldEmitFlag(rip, X86_REF_AF)) {
        as.SB(x0, offsetof(ThreadState, af), rec.threadStatePointer());
    }

    if (rec.shouldEmitFlag(rip, X86_REF_OF)) {
        as.LI(of, 0);
    }

    if (rec.shouldEmitFlag(rip, X86_REF_SF)) {
        as.LI(sf, 0);
    }

    // Branch-less way to compute this
    // Explanation:
    // We calculate if either is NaN and OR cf and zf. If either is NaN they are all set to 1's so that makes sense
    // If it's lhs > rhs, cf pf zf are zero. So they retain their default value because
    // - FLT operates on CF, it will be false if it's greater than
    // - FEQ operates on ZF, it will be false if it's greater than
    // If it's lhs == rhs, ZF gets set to 1 and CF remains 0
    as.LI(cf, 0);
    as.LI(zf, 0);

    biscuit::Vec vlhs = rec.getVec(&operands[0]);
    biscuit::Vec vrhs = rec.getVec(&operands[1]);
    biscuit::FPR lhs = rec.scratchFPR();
    biscuit::FPR rhs = rec.scratchFPR();

    rec.setVectorState(sew, 1);
    as.VFMV_FS(lhs, vlhs);
    as.VFMV_FS(rhs, vrhs);

    biscuit::GPR nan_bit = rec.scratch();
    biscuit::GPR temp = rec.scratch();

    if (sew == SEW::E32) {
        as.FEQ_S(zf, lhs, rhs);
        as.FEQ_S(temp, lhs, lhs);
        as.FEQ_S(nan_bit, rhs, rhs);
    } else {
        as.FEQ_D(zf, lhs, rhs);
        as.FEQ_D(temp, lhs, lhs);
        as.FEQ_D(nan_bit, rhs, rhs);
    }

    if (sew == SEW::E32) {
        as.FLT_S(cf, lhs, rhs);
    } else {
        as.FLT_D(cf, lhs, rhs);
    }

    // Combine the NaN-ness of both operands into the NaN bit
    as.AND(nan_bit, nan_bit, temp);
    as.XORI(nan_bit, nan_bit, 1);

    as.SB(nan_bit, offsetof(ThreadState, pf), rec.threadStatePointer());

    // If the NaN bit is set we also overwrite the value of cf and zf with 1
    as.OR(cf, cf, nan_bit);
    as.OR(zf, zf, nan_bit);
}

FAST_HANDLE(COMISD) {
    COMIS(rec, rip, as, instruction, operands, SEW::E64);
}

FAST_HANDLE(UCOMISD) {
    COMIS(rec, rip, as, instruction, operands, SEW::E64);
}

FAST_HANDLE(COMISS) {
    COMIS(rec, rip, as, instruction, operands, SEW::E32);
}

FAST_HANDLE(UCOMISS) {
    COMIS(rec, rip, as, instruction, operands, SEW::E32);
}

FAST_HANDLE(PINSRB) {
    u8 imm = rec.getImmediate(&operands[2]) & 0b1111;
    biscuit::Vec dst = rec.getVec(&operands[0]);
    biscuit::GPR src = rec.getGPR(&operands[1]);
    biscuit::GPR mask = rec.scratch();
    biscuit::Vec tmp = rec.scratchVec();
    biscuit::Vec tmp2 = rec.scratchVec();
    biscuit::Vec result = rec.scratchVec();

    rec.setVectorState(SEW::E16, 1);
    as.LI(mask, (1 << imm));
    as.VMV(v0, mask);

    rec.setVectorState(SEW::E8, 16);
    as.VMV_SX(tmp, src);
    as.VSLIDEUP(tmp2, tmp, imm);
    as.VMERGE(result, dst, tmp2);

    rec.setVec(&operands[0], result);
}

FAST_HANDLE(PINSRW) {
    bool is_mmx = operands[0].reg.value >= ZYDIS_REGISTER_MM0 && operands[0].reg.value <= ZYDIS_REGISTER_MM7;
    u8 imm = rec.getImmediate(&operands[2]) & (is_mmx ? 0b11 : 0b111);
    biscuit::Vec dst = rec.getVec(&operands[0]);
    biscuit::GPR src = rec.getGPR(&operands[1]);
    biscuit::GPR mask = rec.scratch();
    biscuit::Vec tmp = rec.scratchVec();
    biscuit::Vec tmp2 = rec.scratchVec();
    biscuit::Vec result = rec.scratchVec();

    rec.setVectorState(SEW::E16, 8);
    as.LI(mask, (1 << imm));
    as.VMV(v0, mask);
    as.VMV_SX(tmp, src);
    as.VSLIDEUP(tmp2, tmp, imm);
    as.VMERGE(result, dst, tmp2);

    rec.setVec(&operands[0], result);
}

FAST_HANDLE(PINSRD) {
    u8 imm = rec.getImmediate(&operands[2]) & 0b11;
    biscuit::Vec dst = rec.getVec(&operands[0]);
    biscuit::GPR src = rec.getGPR(&operands[1]);
    biscuit::GPR mask = rec.scratch();
    biscuit::Vec tmp = rec.scratchVec();
    biscuit::Vec tmp2 = rec.scratchVec();
    biscuit::Vec result = rec.scratchVec();

    rec.setVectorState(SEW::E32, 4);
    as.LI(mask, (1 << imm));
    as.VMV(v0, mask);
    as.VMV_SX(tmp, src);
    as.VSLIDEUP(tmp2, tmp, imm);
    as.VMERGE(result, dst, tmp2);

    rec.setVec(&operands[0], result);
}

FAST_HANDLE(PINSRQ) {
    u8 imm = rec.getImmediate(&operands[2]) & 0b1;
    biscuit::Vec dst = rec.getVec(&operands[0]);
    biscuit::GPR src = rec.getGPR(&operands[1]);
    biscuit::GPR mask = rec.scratch();
    biscuit::Vec tmp = rec.scratchVec();
    biscuit::Vec tmp2 = rec.scratchVec();
    biscuit::Vec result = rec.scratchVec();

    rec.setVectorState(SEW::E64, 2);
    as.LI(mask, (1 << imm));
    as.VMV(v0, mask);
    as.VMV_SX(tmp, src);
    as.VSLIDEUP(tmp2, tmp, imm);
    as.VMERGE(result, dst, tmp2);

    rec.setVec(&operands[0], result);
}

FAST_HANDLE(PEXTRB) {
    u8 imm = rec.getImmediate(&operands[2]) & 0b1111;
    biscuit::Vec temp = rec.scratchVec();
    biscuit::GPR result = rec.scratch();
    biscuit::Vec src = rec.getVec(&operands[1]);

    rec.setVectorState(SEW::E8, 16);
    as.VSLIDEDOWN(temp, src, imm);
    as.VMV_XS(result, temp);
    rec.zext(result, result, X86_SIZE_BYTE);

    rec.setGPR(&operands[0], result);
}

FAST_HANDLE(PEXTRW) {
    bool is_mmx = operands[1].reg.value >= ZYDIS_REGISTER_MM0 && operands[1].reg.value <= ZYDIS_REGISTER_MM7;
    u8 imm = rec.getImmediate(&operands[2]) & (is_mmx ? 0b11 : 0b111);
    biscuit::Vec temp = rec.scratchVec();
    biscuit::GPR result = rec.scratch();
    biscuit::Vec src = rec.getVec(&operands[1]);

    rec.setVectorState(SEW::E16, 8);
    as.VSLIDEDOWN(temp, src, imm);
    as.VMV_XS(result, temp);
    rec.zext(result, result, X86_SIZE_WORD);

    rec.setGPR(&operands[0], result);
}

FAST_HANDLE(PEXTRD) {
    biscuit::Vec temp = rec.scratchVec();
    biscuit::Vec src = rec.getVec(&operands[1]);
    biscuit::GPR result = rec.scratch();
    u8 imm = rec.getImmediate(&operands[2]) & 0b11;

    rec.setVectorState(SEW::E32, 4);
    as.VSLIDEDOWN(temp, src, imm);
    as.VMV_XS(result, temp);

    rec.setGPR(&operands[0], result);
}

FAST_HANDLE(PEXTRQ) {
    biscuit::Vec temp = rec.scratchVec();
    biscuit::Vec src = rec.getVec(&operands[1]);
    biscuit::GPR result = rec.scratch();
    u8 imm = rec.getImmediate(&operands[2]) & 0b1;

    rec.setVectorState(SEW::E64, 2);
    as.VSLIDEDOWN(temp, src, imm);
    as.VMV_XS(result, temp);

    rec.setGPR(&operands[0], result);
}

FAST_HANDLE(CMPXCHG_lock) {
    x86_size_e size = rec.zydisToSize(instruction.operand_width);
    biscuit::GPR address = rec.lea(&operands[0]);
    biscuit::GPR src = rec.getGPR(&operands[1]);
    biscuit::GPR rax = rec.getGPR(X86_REF_RAX, size);
    biscuit::GPR dst = rec.scratch();

    switch (size) {
    case X86_SIZE_BYTE: {
        if (Extensions::Zacas && Extensions::Zabha) {
            // TODO: use AMOCAS here when we get to that point
            UNREACHABLE();
        } else {
            /*
                andi    a6, a0, -4
                slli    a0, a0, 3
                li      a4, 255
                sllw    a4, a4, a0
                sllw    a1, a1, a0
                sllw    a2, a2, a0
        .LBB0_1:
                lr.w.aqrl       a5, (a6)
                and     a3, a5, a4
                bne     a3, a1, .LBB0_3
                xor     a3, a5, a2
                and     a3, a3, a4
                xor     a3, a3, a5
                sc.w.rl a3, a3, (a6)
                bnez    a3, .LBB0_1
        .LBB0_3:
                srlw    a0, a5, a0
                zext.b  a0, a0
            */

            biscuit::GPR masked = rec.scratch();
            biscuit::GPR mask = rec.scratch();
            biscuit::GPR temp = rec.scratch();

            biscuit::GPR rax_shifted = rec.flag(X86_REF_CF);
            biscuit::GPR src_shifted = rec.flag(X86_REF_ZF);

            as.ANDI(masked, address, -4);
            as.SLLI(address, address, 3);
            as.LI(mask, 0xFF);
            as.SLLW(mask, mask, address);
            as.SLLW(src_shifted, src, address);
            as.SLLW(rax_shifted, rax, address);

            biscuit::Label loop, not_equal;
            as.Bind(&loop);
            as.LR_W(Ordering::AQRL, dst, masked);
            as.AND(temp, dst, mask);
            as.BNE(temp, rax_shifted, &not_equal);
            as.XOR(temp, dst, src_shifted);
            as.AND(temp, temp, mask);
            as.XOR(temp, temp, dst);
            as.SC_W(Ordering::AQRL, temp, temp, masked);
            as.BNEZ(temp, &loop);
            as.Bind(&not_equal);
            as.SRLW(dst, dst, address);
            as.ANDI(dst, dst, 0xFF);

            rec.popScratch();
            rec.popScratch();
            rec.popScratch();
        }
        break;
    }
    case X86_SIZE_WORD: {
        if (Extensions::Zacas && Extensions::Zabha && false) {
            // TODO: use AMOCAS here when we get to that point
            UNREACHABLE();
        } else {
            biscuit::Label aligned, end, not_equal;
            biscuit::GPR masked = rec.scratch();
            biscuit::GPR temp = rec.scratch();
            biscuit::GPR mask = rec.scratch();
            // If it's at end of dword we can't handle it
            as.ANDI(masked, address, 0b11);
            as.LI(temp, 0b11);
            as.BNE(masked, temp, &aligned);

            // Just crash
            as.EBREAK();

            /*
                andi    a6, a0, -4
                slli    a0, a0, 3
                lui     a4, 16
                addi    a4, a4, -1
                sllw    a7, a1, a0
                sllw    a5, a4, a0
                sllw    a2, a2, a0
        .LBB0_1:
                lr.w.aqrl       a3, (a6)
                and     a1, a3, a5
                bne     a1, a7, .LBB0_3
                xor     a1, a3, a2
                and     a1, a1, a5
                xor     a1, a1, a3
                sc.w.rl a1, a1, (a6)
                bnez    a1, .LBB0_1
        .LBB0_3:
                srlw    a0, a3, a0
                and     a0, a0, a4
                ret
            */
            as.Bind(&aligned);
            biscuit::Label loop;

            biscuit::GPR mask_shifted = rec.flag(X86_REF_SF);
            biscuit::GPR rax_shifted = rec.flag(X86_REF_CF);
            biscuit::GPR src_shifted = rec.flag(X86_REF_ZF);

            as.ANDI(masked, address, -4);
            as.SLLI(address, address, 3);
            as.LI(mask, 0xFFFF);
            as.SLLW(rax_shifted, rax, address);
            as.SLLW(mask_shifted, mask, address);
            as.SLLW(src_shifted, src, address);

            as.Bind(&loop);
            as.LR_W(Ordering::AQRL, dst, masked);
            as.AND(temp, dst, mask_shifted);
            as.BNE(temp, rax_shifted, &not_equal);
            as.XOR(temp, dst, src_shifted);
            as.AND(temp, temp, mask_shifted);
            as.XOR(temp, temp, dst);
            as.SC_W(Ordering::AQRL, temp, temp, masked);
            as.BNEZ(temp, &loop);
            as.Bind(&not_equal);
            as.SRLW(dst, dst, address);
            as.AND(dst, dst, mask);

            rec.popScratch();
            rec.popScratch();
            rec.popScratch();
        }
        break;
    }
    case X86_SIZE_DWORD: {
        if (Extensions::Zacas) {
            as.MV(dst, rax);
            as.AMOCAS_W(Ordering::AQRL, dst, src, address);
            rec.zext(dst, dst, X86_SIZE_DWORD);
            WARN_ONCE("Zacas & CMPXCHG, untested");
        } else {
            biscuit::Label not_equal;
            biscuit::Label start;
            biscuit::GPR scratch = rec.scratch();
            as.Bind(&start);
            as.LR_W(Ordering::AQRL, dst, address);
            rec.zext(dst, dst, X86_SIZE_DWORD); // LR sign extends
            as.BNE(dst, rax, &not_equal);
            as.SC_W(Ordering::AQRL, scratch, src, address);
            as.BNEZ(scratch, &start);
            as.Bind(&not_equal);
            rec.popScratch();
        }
        break;
    }
    case X86_SIZE_QWORD: {
        if (Extensions::Zacas) {
            as.MV(dst, rax);
            as.AMOCAS_D(Ordering::AQRL, dst, src, address); // TODO: unaligned support
            WARN_ONCE("Zacas & CMPXCHG, untested");
        } else {
            biscuit::Label unaligned, end;
            biscuit::GPR masked = rec.scratch();
            biscuit::GPR scratch = rec.scratch();
            as.ANDI(masked, address, 0b111);
            as.BNEZ(masked, &unaligned);

            biscuit::Label not_equal;
            biscuit::Label start;
            as.Bind(&start);
            as.LR_D(Ordering::AQRL, dst, address);
            as.BNE(dst, rax, &not_equal);
            as.SC_D(Ordering::AQRL, scratch, src, address);
            as.BNEZ(scratch, &start);
            as.Bind(&not_equal);
            as.J(&end);

            as.Bind(&unaligned);
            // If the address is not aligned, we can't use LR.D
            // Which means we also can't be technically correct atomically
            // Use LR.D/SC.D on the aligned address anyway to at least have a little bit of guarantee
            biscuit::Label not_equal_unaligned;
            biscuit::Label loop_unaligned;
            as.ANDI(masked, address, ~0b111);
            as.Bind(&loop_unaligned);
            as.LD(dst, 0, address);
            as.LR_D(Ordering::AQRL, scratch, masked);
            // We do the comparison on the load from the unaligned address, obviously
            as.BNE(dst, rax, &not_equal_unaligned);
            // If any of the bytes we can see with the aligned address are changed we retry
            // Of course this isn't actually atomic (we'd need hardware unaligned atomics support for that)
            // but it's better than nothing
            as.SC_D(Ordering::AQRL, scratch, scratch, masked); // Write the same thing we just loaded in scratch
            as.BNEZ(scratch, &loop_unaligned);
            as.SD(src, 0, address);

            as.Bind(&not_equal_unaligned);
            as.Bind(&end);

            rec.popScratch();
            rec.popScratch();
        }
        break;
    }
    default: {
        UNREACHABLE();
        break;
    }
    }

    biscuit::GPR result = rec.scratch();
    as.SUB(result, rax, dst);

    SetCmpFlags(rip, rec, as, rax, dst, result, size);

    Label dont_set;
    as.BEQZ(result, &dont_set);

    rec.setGPR(X86_REF_RAX, size, dst);

    as.Bind(&dont_set);
}

FAST_HANDLE(CMPXCHG) {
    if (operands[0].type == ZYDIS_OPERAND_TYPE_MEMORY) {
        return fast_CMPXCHG_lock(rec, rip, as, instruction, operands);
    }

    x86_size_e size = rec.zydisToSize(instruction.operand_width);
    biscuit::GPR dst = rec.getGPR(&operands[0]);
    biscuit::GPR src = rec.getGPR(&operands[1]);
    biscuit::GPR rax = rec.getGPR(X86_REF_RAX, size);

    Label end, equal;

    biscuit::GPR result = rec.scratch();

    as.SUB(result, rax, dst);

    SetCmpFlags(rip, rec, as, rax, dst, result, size);

    as.BEQ(dst, rax, &equal);

    // Not equal
    rec.setGPR(X86_REF_RAX, size, dst);
    as.J(&end);

    as.Bind(&equal);
    rec.setGPR(&operands[0], src);

    as.Bind(&end);
}

void SCALAR(Recompiler& rec, u64 rip, Assembler& as, ZydisDecodedInstruction& instruction, ZydisDecodedOperand* operands, SEW sew, u8 vlen,
            void (Assembler::*func)(Vec, Vec, Vec, VecMask)) {
    biscuit::Vec dst = rec.getVec(&operands[0]);
    biscuit::Vec src = rec.getVec(&operands[1]);
    rec.setVectorState(sew, vlen);
    (as.*func)(dst, dst, src, VecMask::No);
    rec.setVec(&operands[0], dst);
}

void SCALAR(Recompiler& rec, u64 rip, Assembler& as, ZydisDecodedInstruction& instruction, ZydisDecodedOperand* operands, SEW sew, u8 vlen,
            void (Assembler::*func)(Vec, Vec, VecMask)) {
    biscuit::Vec dst = rec.getVec(&operands[0]);
    biscuit::Vec src = rec.getVec(&operands[1]);
    rec.setVectorState(sew, vlen);
    (as.*func)(dst, src, VecMask::No);
    rec.setVec(&operands[0], dst);
}

FAST_HANDLE(EMMS) {
    rec.switchToX87();
}

FAST_HANDLE(DIVSS) {
    SCALAR(rec, rip, as, instruction, operands, SEW::E32, 1, &Assembler::VFDIV);
}

FAST_HANDLE(DIVSD) {
    SCALAR(rec, rip, as, instruction, operands, SEW::E64, 1, &Assembler::VFDIV);
}

FAST_HANDLE(ADDSS) {
    SCALAR(rec, rip, as, instruction, operands, SEW::E32, 1, &Assembler::VFADD);
}

FAST_HANDLE(ADDSD) {
    SCALAR(rec, rip, as, instruction, operands, SEW::E64, 1, &Assembler::VFADD);
}

FAST_HANDLE(SUBSS) {
    SCALAR(rec, rip, as, instruction, operands, SEW::E32, 1, &Assembler::VFSUB);
}

FAST_HANDLE(SUBSD) {
    SCALAR(rec, rip, as, instruction, operands, SEW::E64, 1, &Assembler::VFSUB);
}

FAST_HANDLE(MULSS) {
    SCALAR(rec, rip, as, instruction, operands, SEW::E32, 1, &Assembler::VFMUL);
}

FAST_HANDLE(MULSD) {
    SCALAR(rec, rip, as, instruction, operands, SEW::E64, 1, &Assembler::VFMUL);
}

FAST_HANDLE(MINSS) { // TODO: NaN handling
    SCALAR(rec, rip, as, instruction, operands, SEW::E32, 1, &Assembler::VFMIN);
}

FAST_HANDLE(MINSD) {
    SCALAR(rec, rip, as, instruction, operands, SEW::E64, 1, &Assembler::VFMIN);
}

FAST_HANDLE(MAXSS) {
    SCALAR(rec, rip, as, instruction, operands, SEW::E32, 1, &Assembler::VFMAX);
}

FAST_HANDLE(MAXSD) {
    SCALAR(rec, rip, as, instruction, operands, SEW::E64, 1, &Assembler::VFMAX);
}

FAST_HANDLE(CVTSI2SD) {
    x86_size_e gpr_size = rec.getSize(&operands[1]);
    biscuit::Vec dst = rec.getVec(&operands[0]);
    biscuit::GPR src = rec.getGPR(&operands[1]);
    biscuit::FPR temp = rec.scratchFPR();

    if (gpr_size == X86_SIZE_DWORD) {
        as.FCVT_D_W(temp, src);
        rec.setVectorState(SEW::E64, 1);
        as.VFMV_SF(dst, temp);
    } else if (gpr_size == X86_SIZE_QWORD) {
        as.FCVT_D_L(temp, src);
        rec.setVectorState(SEW::E64, 1);
        as.VFMV_SF(dst, temp);
    } else {
        UNREACHABLE();
    }

    rec.setVec(&operands[0], dst);
}

FAST_HANDLE(CVTSI2SS) {
    x86_size_e gpr_size = rec.getSize(&operands[1]);
    biscuit::Vec dst = rec.getVec(&operands[0]);
    biscuit::GPR src = rec.getGPR(&operands[1]);
    biscuit::FPR temp = rec.scratchFPR();

    if (gpr_size == X86_SIZE_DWORD) {
        as.FCVT_S_W(temp, src);
        rec.setVectorState(SEW::E32, 1);
        as.VFMV_SF(dst, temp);
    } else if (gpr_size == X86_SIZE_QWORD) {
        as.FCVT_S_L(temp, src);
        rec.setVectorState(SEW::E32, 1);
        as.VFMV_SF(dst, temp);
    } else {
        UNREACHABLE();
    }

    rec.setVec(&operands[0], dst);
}

FAST_HANDLE(CVTTSS2SI) {
    x86_size_e gpr_size = rec.getSize(&operands[0]);
    biscuit::Vec src = rec.getVec(&operands[1]);
    biscuit::GPR dst = rec.getGPR(&operands[0]);
    biscuit::FPR temp = rec.scratchFPR();

    if (gpr_size == X86_SIZE_DWORD) {
        rec.setVectorState(SEW::E32, 1);
        as.VFMV_FS(temp, src);
        as.FCVT_W_S(dst, temp, RMode::RTZ);
    } else if (gpr_size == X86_SIZE_QWORD) {
        rec.setVectorState(SEW::E32, 1);
        as.VFMV_FS(temp, src);
        as.FCVT_L_S(dst, temp, RMode::RTZ);
    } else {
        UNREACHABLE();
    }

    rec.setGPR(&operands[0], dst);
}

FAST_HANDLE(CVTTSD2SI) {
    x86_size_e gpr_size = rec.getSize(&operands[0]);
    biscuit::Vec src = rec.getVec(&operands[1]);
    biscuit::GPR dst = rec.getGPR(&operands[0]);
    biscuit::FPR temp = rec.scratchFPR();

    if (gpr_size == X86_SIZE_DWORD) {
        rec.setVectorState(SEW::E64, 1);
        as.VFMV_FS(temp, src);
        as.FCVT_W_D(dst, temp, RMode::RTZ);
    } else if (gpr_size == X86_SIZE_QWORD) {
        rec.setVectorState(SEW::E64, 1);
        as.VFMV_FS(temp, src);
        as.FCVT_L_D(dst, temp, RMode::RTZ);
    } else {
        UNREACHABLE();
    }

    rec.setGPR(&operands[0], dst);
}

FAST_HANDLE(CVTPD2PS) {
    biscuit::Vec result = rec.scratchVec();
    biscuit::Vec src = rec.getVec(&operands[1]);

    rec.setVectorState(SEW::E32, 4, LMUL::MF2);
    as.VFNCVT_F_F(result, src);
    rec.setVectorState(SEW::E32, 4);
    as.VMV(v0, 0b1100);
    as.VAND(result, result, 0, VecMask::Yes);
    rec.setVec(&operands[0], result);
}

FAST_HANDLE(CVTPS2PD) {
    biscuit::Vec result = rec.scratchVec();
    biscuit::Vec src = rec.getVec(&operands[1]);

    rec.setVectorState(SEW::E32, 4, LMUL::MF2);
    as.VFWCVT_F_F(result, src);
    rec.setVec(&operands[0], result);
}

FAST_HANDLE(CVTTPS2DQ) {
    biscuit::Vec dst = rec.getVec(&operands[0]);
    biscuit::Vec src = rec.getVec(&operands[1]);

    rec.setVectorState(SEW::E32, 4);
    as.VFCVT_RTZ_X_F(dst, src);

    rec.setVec(&operands[0], dst);
}

FAST_HANDLE(CVTPS2DQ) {
    biscuit::Vec dst = rec.getVec(&operands[0]);
    biscuit::Vec src = rec.getVec(&operands[1]);

    rec.setVectorState(SEW::E32, 4);
    as.VFCVT_X_F(dst, src);

    rec.setVec(&operands[0], dst);
}

FAST_HANDLE(CVTTPD2DQ) {
    biscuit::Vec dst = rec.getVec(&operands[0]);
    biscuit::Vec src = rec.getVec(&operands[1]);

    rec.setVectorState(SEW::E32, 4, LMUL::MF2);
    as.VFNCVT_RTZ_X_F(dst, src);
    rec.setVectorState(SEW::E32, 4);
    as.VMV(v0, 0b1100);
    as.VAND(dst, dst, 0, VecMask::Yes);

    rec.setVec(&operands[0], dst);
}

FAST_HANDLE(CVTPD2DQ) {
    biscuit::Vec result = rec.scratchVec();
    biscuit::Vec src = rec.getVec(&operands[1]);

    rec.setVectorState(SEW::E32, 4, LMUL::MF2);
    as.VFNCVT_X_F(result, src);

    rec.setVectorState(SEW::E32, 4);
    as.VMV(v0, 0b1100);
    as.VAND(result, result, 0, VecMask::Yes);

    rec.setVec(&operands[0], result);
}

FAST_HANDLE(CVTPI2PD) {
    biscuit::Vec dst = rec.getVec(&operands[0]);
    biscuit::Vec src = rec.getVec(&operands[1]);

    rec.setVectorState(SEW::E32, 2, LMUL::MF2);
    as.VFWCVT_F_X(dst, src);

    rec.setVec(&operands[0], dst);
}

FAST_HANDLE(CVTPD2PI) {
    biscuit::Vec dst = rec.getVec(&operands[0]);
    biscuit::Vec src = rec.getVec(&operands[1]);

    rec.setVectorState(SEW::E32, 2, LMUL::MF2);
    as.VFNCVT_X_F(dst, src);

    rec.setVec(&operands[0], dst);
}

FAST_HANDLE(CVTTPD2PI) {
    biscuit::Vec dst = rec.getVec(&operands[0]);
    biscuit::Vec src = rec.getVec(&operands[1]);

    rec.setVectorState(SEW::E32, 2, LMUL::MF2);
    as.VFNCVT_RTZ_X_F(dst, src);

    rec.setVec(&operands[0], dst);
}

FAST_HANDLE(CVTPI2PS) {
    biscuit::Vec dst = rec.getVec(&operands[0]);
    biscuit::Vec src = rec.getVec(&operands[1]);

    rec.setVectorState(SEW::E32, 2);
    as.VFCVT_F_X(dst, src);

    rec.setVec(&operands[0], dst);
}

FAST_HANDLE(CVTPS2PI) {
    biscuit::Vec dst = rec.getVec(&operands[0]);
    biscuit::Vec src = rec.getVec(&operands[1]);

    rec.setVectorState(SEW::E32, 2);
    as.VFCVT_X_F(dst, src);

    rec.setVec(&operands[0], dst);
}

FAST_HANDLE(CVTTPS2PI) {
    biscuit::Vec dst = rec.getVec(&operands[0]);
    biscuit::Vec src = rec.getVec(&operands[1]);

    rec.setVectorState(SEW::E32, 2);
    as.VFCVT_RTZ_X_F(dst, src);

    rec.setVec(&operands[0], dst);
}

FAST_HANDLE(XLAT) {
    if (!g_mode32) {
        biscuit::GPR rbx = rec.getGPR(X86_REF_RBX, X86_SIZE_QWORD);
        biscuit::GPR al = rec.getGPR(X86_REF_RAX, X86_SIZE_BYTE);
        biscuit::GPR address = rec.scratch();
        biscuit::GPR dest = rec.scratch();
        ZydisRegister seg = operands[0].mem.segment;
        biscuit::GPR segment = rec.scratch();
        WARN("XLAT in 64-bit mode?");
        as.ADD(address, rbx, al);
        switch (seg) {
        case ZYDIS_REGISTER_FS: {
            rec.readMemory(segment, Recompiler::threadStatePointer(), offsetof(ThreadState, fsbase), X86_SIZE_QWORD);
            as.ADD(address, address, segment);
            break;
        }
        case ZYDIS_REGISTER_GS: {
            rec.readMemory(segment, Recompiler::threadStatePointer(), offsetof(ThreadState, gsbase), X86_SIZE_QWORD);
            as.ADD(address, address, segment);
            break;
        }
        case ZYDIS_REGISTER_DS: {
            // Do nothing
            break;
        }
        default: {
            UNREACHABLE();
        }
        }
        rec.readMemory(dest, address, 0, X86_SIZE_BYTE);
        rec.setGPR(X86_REF_RAX, X86_SIZE_BYTE, dest);
    } else {
        ASSERT(operands[0].type == ZYDIS_OPERAND_TYPE_MEMORY);
        ZydisRegister seg = operands[0].mem.segment;
        biscuit::GPR segment = rec.scratch();
        switch (seg) {
        case ZYDIS_REGISTER_FS: {
            rec.readMemory(segment, Recompiler::threadStatePointer(), offsetof(ThreadState, fsbase), X86_SIZE_QWORD);
            break;
        }
        case ZYDIS_REGISTER_GS: {
            rec.readMemory(segment, Recompiler::threadStatePointer(), offsetof(ThreadState, gsbase), X86_SIZE_QWORD);
            break;
        }
        case ZYDIS_REGISTER_DS: {
            rec.readMemory(segment, Recompiler::threadStatePointer(), offsetof(ThreadState, dsbase), X86_SIZE_QWORD);
            break;
        }
        case ZYDIS_REGISTER_ES: {
            rec.readMemory(segment, Recompiler::threadStatePointer(), offsetof(ThreadState, esbase), X86_SIZE_QWORD);
            break;
        }
        case ZYDIS_REGISTER_SS: {
            rec.readMemory(segment, Recompiler::threadStatePointer(), offsetof(ThreadState, ssbase), X86_SIZE_QWORD);
            break;
        }
        case ZYDIS_REGISTER_CS: {
            rec.readMemory(segment, Recompiler::threadStatePointer(), offsetof(ThreadState, csbase), X86_SIZE_QWORD);
            break;
        }
        default: {
            UNREACHABLE();
        }
        }

        biscuit::GPR ebx = rec.getGPR(operands[0].mem.base);
        biscuit::GPR al = rec.getGPR(X86_REF_RAX, X86_SIZE_BYTE);
        biscuit::GPR address = rec.scratch();
        biscuit::GPR dest = rec.scratch();
        as.ADD(address, ebx, al);
        as.ADD(address, address, segment);
        rec.readMemory(dest, address, 0, X86_SIZE_BYTE);
        rec.setGPR(X86_REF_RAX, X86_SIZE_BYTE, dest);
    }
}

FAST_HANDLE(MOVQ2DQ) {
    biscuit::Vec dst = rec.getVec(&operands[0]);
    biscuit::Vec src = rec.getVec(&operands[1]);

    rec.setVectorState(SEW::E64, 2);
    as.VMV(dst, 0);
    rec.setVectorState(SEW::E64, 1);
    as.VMV(dst, src);

    rec.setVec(&operands[0], dst);
}

FAST_HANDLE(MOVDQ2Q) {
    biscuit::Vec dst = rec.getVec(&operands[0]);
    biscuit::Vec src = rec.getVec(&operands[1]);

    rec.setVectorState(SEW::E64, 1);
    as.VMV(dst, src);

    rec.setVec(&operands[0], dst);
}

FAST_HANDLE(XGETBV) {
    biscuit::GPR scratch = rec.scratch();
    as.LI(scratch, 0b11);
    rec.setGPR(X86_REF_RAX, X86_SIZE_QWORD, scratch);
    rec.setGPR(X86_REF_RDX, X86_SIZE_QWORD, x0);
}

FAST_HANDLE(MOVSS) {
    if (operands[0].type == ZYDIS_OPERAND_TYPE_MEMORY) {
        biscuit::Vec src = rec.getVec(&operands[1]);
        rec.setVec(&operands[0], src);
    } else if (operands[0].type == ZYDIS_OPERAND_TYPE_REGISTER) {
        biscuit::Vec dst = rec.getVec(&operands[0]);
        biscuit::Vec src = rec.getVec(&operands[1]);
        if (operands[1].type == ZYDIS_OPERAND_TYPE_MEMORY) {
            rec.setVectorState(SEW::E64, 2);
            as.VMV(dst, 0);
        }
        rec.setVectorState(SEW::E32, 1);
        as.VMV(dst, src);
        rec.setVec(&operands[0], dst);
    } else {
        UNREACHABLE();
    }
}

FAST_HANDLE(CVTSS2SI) {
    x86_size_e gpr_size = rec.getSize(&operands[0]);
    biscuit::Vec src = rec.getVec(&operands[1]);
    biscuit::GPR dst = rec.getGPR(&operands[0]);
    biscuit::FPR temp = rec.scratchFPR();

    if (gpr_size == X86_SIZE_DWORD) {
        rec.setVectorState(SEW::E32, 1);
        as.VFMV_FS(temp, src);
        as.FCVT_W_S(dst, temp);
    } else if (gpr_size == X86_SIZE_QWORD) {
        rec.setVectorState(SEW::E32, 1);
        as.VFMV_FS(temp, src);
        as.FCVT_L_S(dst, temp);
    } else {
        UNREACHABLE();
    }

    rec.setGPR(&operands[0], dst);
}

FAST_HANDLE(CVTSD2SI) {
    x86_size_e gpr_size = rec.getSize(&operands[0]);
    biscuit::Vec src = rec.getVec(&operands[1]);
    biscuit::GPR dst = rec.getGPR(&operands[0]);
    biscuit::FPR temp = rec.scratchFPR();

    rec.setVectorState(SEW::E64, 1);
    if (gpr_size == X86_SIZE_DWORD) {
        as.VFMV_FS(temp, src);
        as.FCVT_W_D(dst, temp);
    } else if (gpr_size == X86_SIZE_QWORD) {
        as.VFMV_FS(temp, src);
        as.FCVT_L_D(dst, temp);
    } else {
        UNREACHABLE();
    }

    rec.setGPR(&operands[0], dst);
}

FAST_HANDLE(CVTSS2SD) {
    biscuit::Vec dst = rec.getVec(&operands[0]);
    biscuit::Vec src = rec.getVec(&operands[1]);

    if (dst == src) {
        rec.setVectorState(SEW::E8, 16);
        biscuit::Vec temp = rec.scratchVec();
        as.VMV(temp, dst);
        dst = temp;
    }

    rec.setVectorState(SEW::E32, 1, LMUL::MF2);

    as.VFWCVT_F_F(dst, src);

    rec.setVec(&operands[0], dst);
}

FAST_HANDLE(CVTSD2SS) {
    biscuit::Vec dst = rec.getVec(&operands[0]);
    biscuit::Vec src = rec.getVec(&operands[1]);

    if (dst == src) {
        rec.setVectorState(SEW::E8, 16);
        biscuit::Vec temp = rec.scratchVec();
        as.VMV(temp, dst);
        dst = temp;
    }

    rec.setVectorState(SEW::E32, 1, LMUL::MF2);
    as.VFNCVT_F_F(dst, src);

    rec.setVec(&operands[0], dst);
}

FAST_HANDLE(SQRTSS) {
    SCALAR(rec, rip, as, instruction, operands, SEW::E32, 1, &Assembler::VFSQRT);
}

FAST_HANDLE(SQRTSD) {
    SCALAR(rec, rip, as, instruction, operands, SEW::E64, 1, &Assembler::VFSQRT);
}

FAST_HANDLE(RCPSS) {
    biscuit::Vec temp = rec.scratchVec();
    biscuit::Vec dst = rec.getVec(&operands[0]);
    biscuit::Vec src = rec.getVec(&operands[1]);
    rec.setVectorState(SEW::E32, 1);
    if (Extensions::Zfa) {
        WARN_ONCE("Zfa extension code, untested");
        biscuit::FPR one = rec.scratchFPR();
        as.FLI_S(one, 1.0);
        as.VFMV_SF(temp, one);
    } else {
        biscuit::GPR ones = rec.scratch();
        as.LI(ones, 0x3F800000);
        as.VMV(temp, ones);
    }
    as.VFDIV(dst, temp, src);
    rec.setVec(&operands[0], dst);
}

FAST_HANDLE(RSQRTSS) {
    biscuit::Vec temp = rec.scratchVec();
    biscuit::Vec temp2 = rec.scratchVec();
    biscuit::Vec dst = rec.getVec(&operands[0]);
    biscuit::Vec src = rec.getVec(&operands[1]);
    rec.setVectorState(SEW::E32, 1);
    biscuit::GPR ones = rec.scratch();
    as.LI(ones, 0x3F800000);
    as.VMV(temp, ones);
    as.VFSQRT(temp2, src);
    as.VFDIV(dst, temp, temp2);
    rec.setVec(&operands[0], dst);
}

FAST_HANDLE(MOVLHPS) {
    biscuit::Vec dst = rec.getVec(&operands[0]);
    biscuit::Vec src = rec.getVec(&operands[1]);
    rec.setVectorState(SEW::E64, 2);
    if (dst == src) { // VSLIDEUP dst/src overlap limitations
        src = rec.scratchVec();
        as.VMV(src, dst);
    }

    as.VSLIDEUP(dst, src, 1);
    rec.setVec(&operands[0], dst);
}

FAST_HANDLE(ADDSUBPS) {
    // NOTE: using dst directly saves a move but causes potentially
    // torn state if signal happens during vmnand
    biscuit::Vec result = rec.scratchVec();
    biscuit::Vec dst = rec.getVec(&operands[0]);
    biscuit::Vec src = rec.getVec(&operands[1]);
    rec.setVectorState(SEW::E32, 4);
    as.VMV(v0, 0b1010);
    as.VFADD(result, dst, src, VecMask::Yes);
    as.VMNAND(v0, v0, v0);
    as.VFSUB(result, dst, src, VecMask::Yes);
    rec.setVec(&operands[0], result);
}

FAST_HANDLE(ADDSUBPD) {
    // NOTE: using dst directly saves a move but causes potentially
    // torn state if signal happens during vmnand
    biscuit::Vec result = rec.scratchVec();
    biscuit::Vec dst = rec.getVec(&operands[0]);
    biscuit::Vec src = rec.getVec(&operands[1]);
    rec.setVectorState(SEW::E64, 2);
    as.VMV(v0, 0b10);
    as.VFADD(result, dst, src, VecMask::Yes);
    as.VMNAND(v0, v0, v0);
    as.VFSUB(result, dst, src, VecMask::Yes);
    rec.setVec(&operands[0], result);
}

FAST_HANDLE(HADDPD) {
    biscuit::Vec result1 = rec.scratchVec();
    biscuit::Vec result2 = rec.scratchVec();
    biscuit::Vec dst = rec.getVec(&operands[0]);
    biscuit::Vec src = rec.getVec(&operands[1]);

    rec.setVectorState(SEW::E64, 2);
    as.VMV(result1, 0);
    as.VMV(result2, 0);
    as.VFREDUSUM(result1, src, result1);
    as.VFREDUSUM(result2, dst, result2);
    as.VSLIDEUP(result2, result1, 1);

    rec.setVec(&operands[0], result2);
}

FAST_HANDLE(HSUBPD) {
    biscuit::Vec result1 = rec.scratchVec();
    biscuit::Vec result2 = rec.scratchVec();
    biscuit::Vec src_neg = rec.scratchVec();
    biscuit::Vec dst_neg = rec.scratchVec();
    biscuit::Vec dst = rec.getVec(&operands[0]);
    biscuit::Vec src = rec.getVec(&operands[1]);

    rec.setVectorState(SEW::E64, 2);
    as.VMV(src_neg, src);
    as.VMV(dst_neg, dst);
    as.VMV(v0, 0b10);
    as.VFNEG(src_neg, src_neg, VecMask::Yes);
    as.VFNEG(dst_neg, dst_neg, VecMask::Yes);
    as.VMV(result1, 0);
    as.VMV(result2, 0);
    as.VFREDUSUM(result1, src_neg, result1);
    as.VFREDUSUM(result2, dst_neg, result2);
    as.VSLIDEUP(result2, result1, 1);

    rec.setVec(&operands[0], result2);
}

FAST_HANDLE(HADDPS) {
    biscuit::Vec dst = rec.getVec(&operands[0]);
    biscuit::Vec src = rec.getVec(&operands[1]);

    biscuit::Vec dst_down = rec.scratchVec();
    biscuit::Vec src_down = rec.scratchVec();
    biscuit::Vec result1 = rec.scratchVec();
    biscuit::Vec result2 = rec.scratchVec();
    biscuit::Vec compress1 = rec.scratchVec();
    biscuit::Vec compress2 = rec.scratchVec();

    rec.setVectorState(SEW::E32, 4);
    as.VSLIDEDOWN(dst_down, dst, 1);
    as.VSLIDEDOWN(src_down, src, 1);
    as.VFADD(result1, dst, dst_down);
    as.VFADD(result2, src, src_down);
    // result1 and result2 have the elements we need in the 0th and 2nd position
    as.VMV(v0, 0b0101);
    as.VCOMPRESS(compress1, result1, v0);
    as.VCOMPRESS(compress2, result2, v0);
    as.VSLIDEUP(compress1, compress2, 2);

    rec.setVec(&operands[0], compress1);
}

FAST_HANDLE(HSUBPS) {
    biscuit::Vec dst = rec.getVec(&operands[0]);
    biscuit::Vec src = rec.getVec(&operands[1]);

    biscuit::Vec dst_down = rec.scratchVec();
    biscuit::Vec src_down = rec.scratchVec();
    biscuit::Vec result1 = rec.scratchVec();
    biscuit::Vec result2 = rec.scratchVec();
    biscuit::Vec compress1 = rec.scratchVec();
    biscuit::Vec compress2 = rec.scratchVec();

    rec.setVectorState(SEW::E32, 4);
    as.VSLIDEDOWN(dst_down, dst, 1);
    as.VSLIDEDOWN(src_down, src, 1);
    as.VFSUB(result1, dst, dst_down);
    as.VFSUB(result2, src, src_down);
    // result1 and result2 have the elements we need in the 0th and 2nd position
    as.VMV(v0, 0b0101);
    as.VCOMPRESS(compress1, result1, v0);
    as.VCOMPRESS(compress2, result2, v0);
    as.VSLIDEUP(compress1, compress2, 2);

    rec.setVec(&operands[0], compress1);
}

FAST_HANDLE(LDDQU) {
    biscuit::Vec src = rec.getVec(&operands[1]);
    rec.setVec(&operands[0], src);
}

void PSIGN(Recompiler& rec, u64 rip, Assembler& as, ZydisDecodedInstruction& instruction, ZydisDecodedOperand* operands, SEW sew, u8 vl) {
    biscuit::Vec result = rec.scratchVec();
    biscuit::Vec dst = rec.getVec(&operands[0]);
    biscuit::Vec src = rec.getVec(&operands[1]);
    rec.setVectorState(sew, vl);

    as.VMSLT(v0, src, x0);
    as.VMV(result, dst);
    as.VRSUB(result, dst, x0, VecMask::Yes);
    as.VMSEQ(v0, src, x0);
    as.VXOR(result, result, result, VecMask::Yes);

    rec.setVec(&operands[0], result);
}

FAST_HANDLE(PSIGND) {
    PSIGN(rec, rip, as, instruction, operands, SEW::E32, 4);
}

FAST_HANDLE(PSIGNW) {
    PSIGN(rec, rip, as, instruction, operands, SEW::E16, 8);
}

FAST_HANDLE(PSIGNB) {
    PSIGN(rec, rip, as, instruction, operands, SEW::E8, 16);
}

FAST_HANDLE(FXSAVE) {
    biscuit::GPR address = rec.lea(&operands[0]);
    rec.writebackState();
    as.MV(a1, address);
    as.MV(a0, rec.threadStatePointer());
    rec.callPointer(offsetof(ThreadState, felix86_fxsave));
    rec.restoreState();
}

FAST_HANDLE(FXSAVE64) {
    biscuit::GPR address = rec.lea(&operands[0]);
    rec.writebackState();
    as.MV(a1, address);
    as.MV(a0, rec.threadStatePointer());
    rec.callPointer(offsetof(ThreadState, felix86_fxsave));
    rec.restoreState();
}

FAST_HANDLE(FXRSTOR) {
    biscuit::GPR address = rec.lea(&operands[0]);
    rec.writebackState();
    as.MV(a1, address);
    as.MV(a0, rec.threadStatePointer());
    as.LI(a2, 0);
    rec.callPointer(offsetof(ThreadState, felix86_fxrstor));
    rec.restoreState();
}

FAST_HANDLE(FXRSTOR64) {
    biscuit::GPR address = rec.lea(&operands[0]);
    rec.writebackState();
    as.MV(a1, address);
    as.MV(a0, rec.threadStatePointer());
    as.LI(a2, 1);
    rec.callPointer(offsetof(ThreadState, felix86_fxrstor));
    rec.restoreState();
}

FAST_HANDLE(WRFSBASE) {
    biscuit::GPR reg = rec.getGPR(&operands[0]);

    if (instruction.operand_width == 32) {
        as.SW(reg, offsetof(ThreadState, fsbase), rec.threadStatePointer());
    } else if (instruction.operand_width == 64) {
        as.SD(reg, offsetof(ThreadState, fsbase), rec.threadStatePointer());
    } else {
        UNREACHABLE();
    }
}

FAST_HANDLE(WRGSBASE) {
    biscuit::GPR reg = rec.getGPR(&operands[0]);

    if (instruction.operand_width == 32) {
        as.SW(reg, offsetof(ThreadState, gsbase), rec.threadStatePointer());
    } else if (instruction.operand_width == 64) {
        as.SD(reg, offsetof(ThreadState, gsbase), rec.threadStatePointer());
    } else {
        UNREACHABLE();
    }
}

FAST_HANDLE(XADD_lock_32) {
    bool update_cf = rec.shouldEmitFlag(rip, X86_REF_CF);
    bool update_zf = rec.shouldEmitFlag(rip, X86_REF_ZF);
    bool update_af = rec.shouldEmitFlag(rip, X86_REF_AF);
    bool update_pf = rec.shouldEmitFlag(rip, X86_REF_PF);
    bool update_of = rec.shouldEmitFlag(rip, X86_REF_OF);
    bool update_sf = rec.shouldEmitFlag(rip, X86_REF_SF);
    bool update_any = update_af | update_cf | update_zf | update_pf | update_of | update_sf;

    biscuit::GPR dst = rec.scratch();
    biscuit::GPR src = rec.getGPR(&operands[1]);
    biscuit::GPR address = rec.lea(&operands[0]);
    as.AMOADD_W(Ordering::AQRL, dst, src, address);
    rec.zext(dst, dst, X86_SIZE_DWORD); // amoadd sign extends

    if (!g_config.noflag_opts || update_any) {
        biscuit::GPR result = rec.scratch();
        as.ADD(result, dst, src);

        x86_size_e size = rec.getSize(&operands[0]);

        if (update_cf) {
            rec.updateCarryAdd(dst, result, size);
        }

        if (update_pf) {
            rec.updateParity(result);
        }

        if (update_af) {
            rec.updateAuxiliaryAdd(dst, src);
        }

        if (update_zf) {
            rec.updateZero(result, size);
        }

        if (update_sf) {
            rec.updateSign(result, size);
        }

        if (update_of) {
            rec.updateOverflowAdd(dst, src, result, size);
        }
    }

    rec.setGPR(operands[1].reg.value, X86_SIZE_QWORD, dst);
}

FAST_HANDLE(XADD_lock_64) {
    bool update_cf = rec.shouldEmitFlag(rip, X86_REF_CF);
    bool update_zf = rec.shouldEmitFlag(rip, X86_REF_ZF);
    bool update_af = rec.shouldEmitFlag(rip, X86_REF_AF);
    bool update_pf = rec.shouldEmitFlag(rip, X86_REF_PF);
    bool update_of = rec.shouldEmitFlag(rip, X86_REF_OF);
    bool update_sf = rec.shouldEmitFlag(rip, X86_REF_SF);
    bool update_any = update_af | update_cf | update_zf | update_pf | update_of | update_sf;

    biscuit::GPR dst = rec.scratch();
    biscuit::GPR src = rec.getGPR(&operands[1]);
    biscuit::GPR address = rec.lea(&operands[0]);
    as.AMOADD_D(Ordering::AQRL, dst, src, address);

    if (!g_config.noflag_opts || update_any) {
        biscuit::GPR result = rec.scratch();
        as.ADD(result, dst, src);

        x86_size_e size = rec.getSize(&operands[0]);

        if (update_cf) {
            rec.updateCarryAdd(dst, result, size);
        }

        if (update_pf) {
            rec.updateParity(result);
        }

        if (update_af) {
            rec.updateAuxiliaryAdd(dst, src);
        }

        if (update_zf) {
            rec.updateZero(result, size);
        }

        if (update_sf) {
            rec.updateSign(result, size);
        }

        if (update_of) {
            rec.updateOverflowAdd(dst, src, result, size);
        }
    }

    rec.setGPR(operands[1].reg.value, X86_SIZE_QWORD, dst);
}

FAST_HANDLE(XADD) {
    bool needs_atomic = operands[0].type == ZYDIS_OPERAND_TYPE_MEMORY && (instruction.attributes & ZYDIS_ATTRIB_HAS_LOCK);
    if (needs_atomic) {
        switch (instruction.operand_width) {
        case 32: {
            return fast_XADD_lock_32(rec, rip, as, instruction, operands);
        }
        case 64: {
            return fast_XADD_lock_64(rec, rip, as, instruction, operands);
        }
        default: {
            WARN("Unhandled atomic width: %d for XADD", instruction.operand_width);
        }
        }
    }

    bool update_cf = rec.shouldEmitFlag(rip, X86_REF_CF);
    bool update_zf = rec.shouldEmitFlag(rip, X86_REF_ZF);
    bool update_af = rec.shouldEmitFlag(rip, X86_REF_AF);
    bool update_pf = rec.shouldEmitFlag(rip, X86_REF_PF);
    bool update_of = rec.shouldEmitFlag(rip, X86_REF_OF);
    bool update_sf = rec.shouldEmitFlag(rip, X86_REF_SF);
    // bool update_any = update_af | update_cf | update_zf | update_pf | update_of | update_sf;

    biscuit::GPR result = rec.scratch();
    biscuit::GPR dst = rec.getGPR(&operands[0]);
    biscuit::GPR src = rec.getGPR(&operands[1]);

    as.ADD(result, dst, src);

    x86_size_e size = rec.getSize(&operands[0]);

    if (update_cf) {
        rec.updateCarryAdd(dst, result, size);
    }

    if (update_pf) {
        rec.updateParity(result);
    }

    if (update_af) {
        rec.updateAuxiliaryAdd(dst, src);
    }

    if (update_zf) {
        rec.updateZero(result, size);
    }

    if (update_sf) {
        rec.updateSign(result, size);
    }

    if (update_of) {
        rec.updateOverflowAdd(dst, src, result, size);
    }

    // Set operands[1] first, as dst could be an allocated register, if we did it the other way
    // around it could cause problems -- result is a scratch so it won't be modified by this set
    rec.setGPR(&operands[1], dst);
    rec.setGPR(&operands[0], result);
}

FAST_HANDLE(CMPSD_sse) {
    u8 imm = rec.getImmediate(&operands[2]) & 0b111;
    biscuit::Vec dst = rec.getVec(&operands[0]);
    biscuit::Vec src = rec.getVec(&operands[1]);
    biscuit::FPR temp = rec.scratchFPR();
    biscuit::FPR temp2 = rec.scratchFPR();

    rec.setVectorState(SEW::E64, 1);
    as.VFMV_FS(temp, dst);
    as.VFMV_FS(temp2, src);

    biscuit::GPR result = rec.scratch();
    switch ((CmpPredicate)imm) {
    case EQ_OQ: {
        as.FEQ_D(result, temp, temp2);
        break;
    }
    case LT_OS: {
        as.FLT_D(result, temp, temp2);
        break;
    }
    case LE_OS: {
        as.FLE_D(result, temp, temp2);
        break;
    }
    case UNORD_Q: {
        // Check if it's a qNan or sNan, check bit 8 and 9
        biscuit::GPR nan = rec.scratch();
        biscuit::GPR mask = rec.scratch();
        as.FCLASS_D(result, temp);
        as.FCLASS_D(nan, temp2);
        as.OR(result, result, nan);
        as.LI(mask, 0b11 << 8);
        as.AND(result, result, mask);
        as.SNEZ(result, result);
        rec.popScratch();
        rec.popScratch();
        break;
    }
    case NEQ_UQ: {
        biscuit::GPR nan = rec.scratch();
        biscuit::GPR mask = rec.scratch();
        as.FCLASS_D(result, temp);
        as.FCLASS_D(nan, temp2);
        as.OR(result, result, nan);
        as.LI(mask, 0b11 << 8);
        as.AND(result, result, mask);
        as.SNEZ(result, result);
        rec.popScratch();
        rec.popScratch();

        // After checking if either are nan, also check if they are equal
        as.FEQ_D(nan, temp, temp2);
        as.XORI(nan, nan, 1);
        as.OR(result, result, nan);
        break;
    }
    case NLT_US: {
        biscuit::GPR nan = rec.scratch();
        biscuit::GPR mask = rec.scratch();
        as.FCLASS_D(result, temp);
        as.FCLASS_D(nan, temp2);
        as.OR(result, result, nan);
        as.LI(mask, 0b11 << 8);
        as.AND(result, result, mask);
        as.SNEZ(result, result);
        rec.popScratch();
        rec.popScratch();

        // After checking if either are nan, also check if they are equal
        as.FLT_D(nan, temp, temp2);
        as.XORI(nan, nan, 1);
        as.OR(result, result, nan);
        break;
    }
    case NLE_US: {
        biscuit::GPR nan = rec.scratch();
        biscuit::GPR mask = rec.scratch();
        as.FCLASS_D(result, temp);
        as.FCLASS_D(nan, temp2);
        as.OR(result, result, nan);
        as.LI(mask, 0b11 << 8);
        as.AND(result, result, mask);
        as.SNEZ(result, result);
        rec.popScratch();
        rec.popScratch();

        // After checking if either are nan, also check if they are equal
        as.FLE_D(nan, temp, temp2);
        as.XORI(nan, nan, 1);
        as.OR(result, result, nan);
        break;
    }
    case ORD_Q: {
        // Check if neither are NaN
        biscuit::GPR nan = rec.scratch();
        biscuit::GPR mask = rec.scratch();
        as.FCLASS_D(result, temp);
        as.FCLASS_D(nan, temp2);
        as.OR(result, result, nan);
        as.LI(mask, 0b11 << 8);
        as.AND(result, result, mask);
        as.SEQZ(result, result);
        rec.popScratch();
        rec.popScratch();
        break;
    }
    default: {
        UNREACHABLE();
        break;
    }
    }

    // Transform 0 or 1 to 0 or -1ull
    as.SUB(result, x0, result);
    as.VMV_SX(dst, result);

    rec.setVec(&operands[0], dst);
}

FAST_HANDLE(CMPSS) {
    u8 imm = rec.getImmediate(&operands[2]) & 0b111;
    biscuit::Vec dst = rec.getVec(&operands[0]);
    biscuit::Vec src = rec.getVec(&operands[1]);
    biscuit::FPR temp = rec.scratchFPR();
    biscuit::FPR temp2 = rec.scratchFPR();

    rec.setVectorState(SEW::E32, 1);
    as.VFMV_FS(temp, dst);
    as.VFMV_FS(temp2, src);

    biscuit::GPR result = rec.scratch();
    switch ((CmpPredicate)imm) {
    case EQ_OQ: {
        as.FEQ_S(result, temp, temp2);
        break;
    }
    case LT_OS: {
        as.FLT_S(result, temp, temp2);
        break;
    }
    case LE_OS: {
        as.FLE_S(result, temp, temp2);
        break;
    }
    case UNORD_Q: {
        // Check if it's a qNan or sNan, check bit 8 and 9
        biscuit::GPR nan = rec.scratch();
        biscuit::GPR mask = rec.scratch();
        as.FCLASS_S(result, temp);
        as.FCLASS_S(nan, temp2);
        as.OR(result, result, nan);
        as.LI(mask, 0b11 << 8);
        as.AND(result, result, mask);
        as.SNEZ(result, result);
        rec.popScratch();
        rec.popScratch();
        break;
    }
    case NEQ_UQ: {
        biscuit::GPR nan = rec.scratch();
        biscuit::GPR mask = rec.scratch();
        as.FCLASS_S(result, temp);
        as.FCLASS_S(nan, temp2);
        as.OR(result, result, nan);
        as.LI(mask, 0b11 << 8);
        as.AND(result, result, mask);
        as.SNEZ(result, result);
        rec.popScratch();
        rec.popScratch();

        // After checking if either are nan, also check if they are equal
        as.FEQ_S(nan, temp, temp2);
        as.XORI(nan, nan, 1);
        as.OR(result, result, nan);
        break;
    }
    case NLT_US: {
        biscuit::GPR nan = rec.scratch();
        biscuit::GPR mask = rec.scratch();
        as.FCLASS_S(result, temp);
        as.FCLASS_S(nan, temp2);
        as.OR(result, result, nan);
        as.LI(mask, 0b11 << 8);
        as.AND(result, result, mask);
        as.SNEZ(result, result);
        rec.popScratch();
        rec.popScratch();

        as.FLE_S(nan, temp2, temp);
        as.OR(result, result, nan);
        break;
    }
    case NLE_US: {
        biscuit::GPR nan = rec.scratch();
        biscuit::GPR mask = rec.scratch();
        as.FCLASS_S(result, temp);
        as.FCLASS_S(nan, temp2);
        as.OR(result, result, nan);
        as.LI(mask, 0b11 << 8);
        as.AND(result, result, mask);
        as.SNEZ(result, result);
        rec.popScratch();
        rec.popScratch();

        // After checking if either are nan, also check if they are equal
        as.FLT_S(nan, temp2, temp);
        as.OR(result, result, nan);
        break;
    }
    case ORD_Q: {
        // Check if neither are NaN
        biscuit::GPR nan = rec.scratch();
        biscuit::GPR mask = rec.scratch();
        as.FCLASS_S(result, temp);
        as.FCLASS_S(nan, temp2);
        as.OR(result, result, nan);
        as.LI(mask, 0b11 << 8);
        as.AND(result, result, mask);
        as.SEQZ(result, result);
        rec.popScratch();
        rec.popScratch();
        break;
    }
    default: {
        UNREACHABLE();
        break;
    }
    }

    // Transform 0 or 1 to 0 or -1ull
    as.SUB(result, x0, result);
    as.VMV_SX(dst, result);

    rec.setVec(&operands[0], dst);
}

FAST_HANDLE(CMPSD) {
    if (instruction.meta.isa_set == ZYDIS_ISA_SET_SSE2) {
        fast_CMPSD_sse(rec, rip, as, instruction, operands);
    } else if (instruction.meta.isa_set == ZYDIS_ISA_SET_I386) {
        fast_CMPSD_string(rec, rip, as, instruction, operands);
    } else {
        UNREACHABLE();
    }
}

FAST_HANDLE(CMC) {
    biscuit::GPR cf = rec.flag(X86_REF_CF);
    as.XORI(cf, cf, 1);
}

FAST_HANDLE(RCL) {
    biscuit::GPR temp_count = rec.scratch();
    biscuit::GPR dst = rec.getGPR(&operands[0]);
    biscuit::GPR dst_temp = rec.scratch();
    biscuit::GPR shift = rec.getGPR(&operands[1]);
    biscuit::GPR cf = rec.flag(X86_REF_CF);
    biscuit::GPR cf_temp = rec.scratch();

    as.ANDI(temp_count, shift, instruction.operand_width == 64 ? 63 : 31);
    if (instruction.operand_width == 8) {
        as.LI(cf_temp, 9);
        as.REMUW(temp_count, temp_count, cf_temp);
    } else if (instruction.operand_width == 16) {
        as.LI(cf_temp, 17);
        as.REMUW(temp_count, temp_count, cf_temp);
    }

    as.MV(dst_temp, dst);

    Label loop, end;
    as.Bind(&loop);
    as.BEQZ(temp_count, &end);

    as.SRLI(cf_temp, dst_temp, instruction.operand_width - 1);
    as.ANDI(cf_temp, cf_temp, 1);
    as.SLLI(dst_temp, dst_temp, 1);
    as.OR(dst_temp, dst_temp, cf);
    as.MV(cf, cf_temp);
    as.ADDI(temp_count, temp_count, -1);
    as.J(&loop);

    as.Bind(&end);

    if (rec.shouldEmitFlag(rip, X86_REF_OF)) {
        biscuit::GPR of = rec.flag(X86_REF_OF);
        as.SRLI(of, dst_temp, instruction.operand_width - 1);
        as.ANDI(of, of, 1);
        as.XOR(of, of, cf);
    }

    rec.setGPR(&operands[0], dst_temp);
}

FAST_HANDLE(RCR) {
    biscuit::GPR dst = rec.getGPR(&operands[0]);
    biscuit::GPR dst_temp = rec.scratch();
    biscuit::GPR shift = rec.getGPR(&operands[1]);
    biscuit::GPR cf = rec.flag(X86_REF_CF);
    biscuit::GPR cf_temp = rec.scratch();
    biscuit::GPR cf_shifted = rec.scratch();

    as.ANDI(shift, shift, instruction.operand_width == 64 ? 63 : 31); // shift is always a temporary reg
    if (instruction.operand_width == 8) {
        as.LI(cf_temp, 9);
        as.REMUW(shift, shift, cf_temp);
    } else if (instruction.operand_width == 16) {
        as.LI(cf_temp, 17);
        as.REMUW(shift, shift, cf_temp);
    }

    as.MV(dst_temp, dst);

    if (rec.shouldEmitFlag(rip, X86_REF_OF)) {
        biscuit::GPR of = rec.flag(X86_REF_OF);
        as.SRLI(of, dst_temp, instruction.operand_width - 1);
        as.ANDI(of, of, 1);
        as.XOR(of, of, cf);
    }

    Label loop, end;
    as.Bind(&loop);
    as.BEQZ(shift, &end);

    as.ANDI(cf_temp, dst_temp, 1);
    as.SRLI(dst_temp, dst_temp, 1);
    as.SLLI(cf_shifted, cf, instruction.operand_width - 1);
    as.OR(dst_temp, dst_temp, cf_shifted);
    as.MV(cf, cf_temp);
    as.ADDI(shift, shift, -1);
    as.J(&loop);

    as.Bind(&end);

    rec.setGPR(&operands[0], dst_temp);
}

FAST_HANDLE(SHLD) {
    u8 operand_size = instruction.operand_width;
    u8 mask = operand_size == 64 ? 63 : 31;
    biscuit::GPR dst = rec.getGPR(&operands[0]);
    biscuit::GPR src = rec.getGPR(&operands[1]);
    biscuit::GPR shift = rec.getGPR(&operands[2]); // it's ok to modify if reg, since it can only be cl and that comes as scratch
    biscuit::GPR result = rec.scratch();
    biscuit::GPR shift_sub = rec.scratch();

    Label end;
    as.ANDI(shift, shift, mask);
    as.MV(result, dst);
    as.BEQZ(shift, &end);
    as.LI(shift_sub, operand_size);
    as.SUB(shift_sub, shift_sub, shift);

    if (operand_size == 64) {
        biscuit::GPR temp = rec.scratch();
        as.SLL(result, dst, shift);
        as.SRL(temp, src, shift_sub);
        as.OR(result, result, temp);
        rec.popScratch();
    } else if (operand_size == 32 || operand_size == 16) {
        biscuit::GPR temp = rec.scratch();
        as.SLLW(result, dst, shift);
        as.SRLW(temp, src, shift_sub);
        as.OR(result, result, temp);
        rec.popScratch();
    } else {
        UNREACHABLE();
    }

    if (rec.shouldEmitFlag(rip, X86_REF_CF)) {
        biscuit::GPR cf = rec.flag(X86_REF_CF);
        as.SRL(cf, dst, shift_sub);
        as.ANDI(cf, cf, 1);
    }

    if (rec.shouldEmitFlag(rip, X86_REF_OF)) {
        biscuit::GPR of = rec.flag(X86_REF_OF);
        as.XOR(of, result, dst);
        as.SRLI(of, of, operand_size - 1);
        as.ANDI(of, of, 1);
    }

    if (rec.shouldEmitFlag(rip, X86_REF_ZF)) {
        rec.updateZero(result, rec.zydisToSize(operand_size));
    }

    if (rec.shouldEmitFlag(rip, X86_REF_SF)) {
        rec.updateSign(result, rec.zydisToSize(operand_size));
    }

    if (rec.shouldEmitFlag(rip, X86_REF_PF)) {
        rec.updateParity(result);
    }

    as.Bind(&end);
    rec.setGPR(&operands[0], result);
}

FAST_HANDLE(SHRD) {
    u8 operand_size = instruction.operand_width;
    u8 mask = operand_size == 64 ? 63 : 31;
    biscuit::GPR dst = rec.getGPR(&operands[0]);
    biscuit::GPR src = rec.getGPR(&operands[1]);
    biscuit::GPR shift = rec.getGPR(&operands[2]); // it's ok to modify if reg, since it can only be cl and that comes as scratch
    biscuit::GPR result = rec.scratch();
    biscuit::GPR shift_sub = rec.scratch();

    Label end;
    as.ANDI(shift, shift, mask);
    as.MV(result, dst);
    as.BEQZ(shift, &end);
    as.LI(shift_sub, operand_size);
    as.SUB(shift_sub, shift_sub, shift);

    if (operand_size == 64) {
        biscuit::GPR temp = rec.scratch();
        as.SRL(result, dst, shift);
        as.SLL(temp, src, shift_sub);
        as.OR(result, result, temp);
        rec.popScratch();
    } else if (operand_size == 32 || operand_size == 16) {
        biscuit::GPR temp = rec.scratch();
        as.SRLW(result, dst, shift);
        as.SLLW(temp, src, shift_sub);
        as.OR(result, result, temp);
        rec.popScratch();
    } else {
        UNREACHABLE();
    }

    if (rec.shouldEmitFlag(rip, X86_REF_CF)) {
        biscuit::GPR cf = rec.flag(X86_REF_CF);
        as.ADDI(shift, shift, -1);
        as.SRL(cf, dst, shift);
        as.ANDI(cf, cf, 1);
    }

    if (rec.shouldEmitFlag(rip, X86_REF_OF)) {
        biscuit::GPR of = rec.flag(X86_REF_OF);
        as.XOR(of, result, dst);
        as.SRLI(of, of, operand_size - 1);
        as.ANDI(of, of, 1);
    }

    if (rec.shouldEmitFlag(rip, X86_REF_ZF)) {
        rec.updateZero(result, rec.zydisToSize(operand_size));
    }

    if (rec.shouldEmitFlag(rip, X86_REF_SF)) {
        rec.updateSign(result, rec.zydisToSize(operand_size));
    }

    if (rec.shouldEmitFlag(rip, X86_REF_PF)) {
        rec.updateParity(result);
    }

    as.Bind(&end);
    rec.setGPR(&operands[0], result);
}

void PCMPXSTRX(Recompiler& rec, u64 rip, Assembler& as, ZydisDecodedInstruction& instruction, ZydisDecodedOperand* operands, pcmpxstrx type) {
    rec.writebackState();
    as.MV(a0, rec.threadStatePointer());
    as.LI(a1, (int)type);
    ASSERT(operands[0].reg.value >= ZYDIS_REGISTER_XMM0 && operands[0].reg.value <= ZYDIS_REGISTER_XMM15);
    as.ADDI(a2, rec.threadStatePointer(), offsetof(ThreadState, xmm) + (sizeof(XmmReg) * (operands[0].reg.value - ZYDIS_REGISTER_XMM0)));

    if (operands[1].type == ZYDIS_OPERAND_TYPE_REGISTER) {
        as.ADDI(a3, rec.threadStatePointer(), offsetof(ThreadState, xmm) + (sizeof(XmmReg) * (operands[1].reg.value - ZYDIS_REGISTER_XMM0)));
    } else {
        biscuit::GPR scratch = rec.lea(&operands[1]);
        ASSERT(scratch != a0 && scratch != a1 && scratch != a2);
        as.MV(a3, scratch);
    }

    as.LI(a4, operands[2].imm.value.u);

    rec.callPointer(offsetof(ThreadState, felix86_pcmpxstrx));
    rec.restoreState();
}

FAST_HANDLE(PCMPISTRI) {
    PCMPXSTRX(rec, rip, as, instruction, operands, pcmpxstrx::ImplicitIndex);
}

FAST_HANDLE(PCMPESTRI) {
    PCMPXSTRX(rec, rip, as, instruction, operands, pcmpxstrx::ExplicitIndex);
}

FAST_HANDLE(PCMPISTRM) {
    PCMPXSTRX(rec, rip, as, instruction, operands, pcmpxstrx::ImplicitMask);
}

FAST_HANDLE(PCMPESTRM) {
    PCMPXSTRX(rec, rip, as, instruction, operands, pcmpxstrx::ExplicitMask);
}

FAST_HANDLE(STMXCSR) {
    biscuit::GPR mxcsr = rec.scratch();
    biscuit::GPR address = rec.scratch();
    // TODO: are overflow/inexact/underflow etc flags set in fcsr? if then we need to copy them over
    as.ADDI(address, rec.threadStatePointer(), offsetof(ThreadState, mxcsr));
    as.LWU(mxcsr, 0, address);
    rec.setGPR(&operands[0], mxcsr);
}

FAST_HANDLE(LDMXCSR) {
    biscuit::GPR src = rec.getGPR(&operands[0]);
    biscuit::GPR rc = rec.scratch(); // rounding control
    biscuit::GPR temp = rec.scratch();

    // Extract rounding mode from MXCSR
    as.SRLI(rc, src, 13);
    as.ANDI(rc, rc, 0b11);

    // Here's how the rounding modes match up
    // 00 - Round to nearest (even) x86 -> 00 RISC-V
    // 01 - Round down (towards -inf) x86 -> 10 RISC-V
    // 10 - Round up (towards +inf) x86 -> 11 RISC-V
    // 11 - Round towards zero x86 -> 01 RISC-V
    // So we can shift the following bit sequence to the right and mask it
    // 01111000, shift by the rc * 2 and we get the RISC-V rounding mode
    as.SLLI(rc, rc, 1);
    as.LI(temp, 0b01111000);
    as.SRL(temp, temp, rc);
    as.ANDI(temp, temp, 0b11);
    as.FSRM(x0, temp); // load the equivalent RISC-V rounding mode

    // Also save the converted rounding mode for quick access
    as.SW(src, offsetof(ThreadState, mxcsr), rec.threadStatePointer());
    as.SB(temp, offsetof(ThreadState, rmode_sse), rec.threadStatePointer());

    rec.setFsrmSSE(true);
}

FAST_HANDLE(CVTDQ2PD) {
    biscuit::Vec scratch = rec.scratchVec();
    biscuit::Vec src = rec.getVec(&operands[1]);

    rec.setVectorState(SEW::E32, 4, LMUL::MF2);
    as.VFWCVT_F_X(scratch, src);

    rec.setVec(&operands[0], scratch);
}

FAST_HANDLE(CVTDQ2PS) {
    biscuit::Vec scratch = rec.scratchVec();
    biscuit::Vec src = rec.getVec(&operands[1]);

    rec.setVectorState(SEW::E32, 4);
    as.VFCVT_F_X(scratch, src);

    rec.setVec(&operands[0], scratch);
}

FAST_HANDLE(EXTRACTPS) {
    u8 imm = rec.getImmediate(&operands[2]) & 0b11;
    biscuit::GPR dst = rec.scratch();
    biscuit::Vec src = rec.getVec(&operands[1]);
    biscuit::Vec tmp = rec.scratchVec();

    rec.setVectorState(SEW::E32, 4);
    as.VSLIDEDOWN(tmp, src, imm);
    as.VMV_XS(dst, tmp);

    rec.setGPR(&operands[0], dst);
}

FAST_HANDLE(INSERTPS) {
    u8 immediate = rec.getImmediate(&operands[2]);
    biscuit::Vec dst = rec.getVec(&operands[0]);
    biscuit::Vec src = rec.getVec(&operands[1]);
    biscuit::Vec src_shifted = rec.scratchVec();

    u8 count_s = 0;
    u8 count_d = (immediate >> 4) & 0b11;
    u8 zmask = immediate & 0b1111;
    if (operands[1].type == ZYDIS_OPERAND_TYPE_REGISTER) {
        count_s = (immediate >> 6) & 0b11;
    }

    u8 mask = (1 << count_d) & 0b1111;

    // Need to shift src down by count_s, then shift it up by count_d to insert it there
    int count = count_s - count_d;

    rec.setVectorState(SEW::E32, 4);
    if (count < 0) {
        as.VSLIDEUP(src_shifted, src, -count);
    } else if (count > 0) {
        as.VSLIDEDOWN(src_shifted, src, count);
    } else {
        src_shifted = src;
    }

    as.VMV(v0, mask);
    as.VMERGE(dst, dst, src_shifted);

    if (zmask) {
        as.VMV(v0, zmask);
        as.VXOR(dst, dst, dst, VecMask::Yes);
    }

    rec.setVec(&operands[0], dst);
}

FAST_HANDLE(PUSHFQ) {
    biscuit::GPR src = rec.getFlags();
    biscuit::GPR rsp = rec.getGPR(X86_REF_RSP, rec.stackWidth());
    as.ADDI(rsp, rsp, -rec.stackPointerSize());
    rec.setGPR(X86_REF_RSP, rec.stackWidth(), rsp);
    rec.writeMemory(src, rsp, 0, rec.stackWidth());
}

FAST_HANDLE(POPFQ) {
    biscuit::GPR flags = rec.scratch();
    biscuit::GPR rsp = rec.getGPR(X86_REF_RSP, rec.stackWidth());
    as.LD(flags, 0, rsp);
    as.ADDI(rsp, rsp, rec.stackPointerSize());
    rec.setGPR(X86_REF_RSP, rec.stackWidth(), rsp);
    rec.setFlags(flags);
}

FAST_HANDLE(PUSHFD) {
    fast_PUSHFQ(rec, rip, as, instruction, operands);
}

FAST_HANDLE(POPFD) {
    fast_POPFQ(rec, rip, as, instruction, operands);
}

FAST_HANDLE(MOVDDUP) {
    biscuit::Vec result = rec.scratchVec();
    biscuit::Vec iota = rec.scratchVec();
    biscuit::Vec src = rec.getVec(&operands[1]);

    rec.setVectorState(SEW::E64, 2);
    as.VMV(iota, 0);
    as.VRGATHER(result, src, iota);

    rec.setVec(&operands[0], result);
}

FAST_HANDLE(PSADBW) {
    biscuit::Vec result = rec.scratchVec();
    biscuit::Vec result_high = rec.scratchVec();
    ASSERT(result.Index() % 2 == 0); // even register for widening ops
    ASSERT(result_high.Index() == result.Index() + 1);
    biscuit::Vec mask = rec.scratchVec();
    biscuit::Vec mask_high = rec.scratchVec();
    ASSERT(mask.Index() % 2 == 0);
    biscuit::Vec scratch = rec.scratchVec();
    ASSERT(scratch.Index() % 2 == 0);
    biscuit::Vec dst = rec.getVec(&operands[0]);
    biscuit::Vec src = rec.getVec(&operands[1]);

    bool is_mmx = operands[0].reg.value >= ZYDIS_REGISTER_MM0 && operands[0].reg.value <= ZYDIS_REGISTER_MM7;
    if (is_mmx) {
        rec.setVectorState(SEW::E8, 8);
    } else {
        rec.setVectorState(SEW::E8, 16);
    }

    as.VWSUBU(result, dst, src);
    rec.setVectorState(SEW::E16, 16, LMUL::M2);
    as.VSRA(mask, result, 15);
    as.VXOR(result, result, mask);
    as.VSUB(result, result, mask);

    if (!is_mmx) {
        as.VSLIDEDOWN(scratch, result, 8);
    }

    rec.setVectorState(SEW::E16, 8);

    biscuit::Vec reduction = mask;
    as.VMV(reduction, 0);

    if (is_mmx) {
        as.VREDSUM(reduction, result, reduction);
        rec.setVec(&operands[0], reduction);
    } else {
        biscuit::Vec reduction2 = mask_high;
        as.VMV(reduction2, 0);
        as.VREDSUM(reduction, result, reduction);
        as.VREDSUM(reduction2, scratch, reduction2);
        rec.setVectorState(SEW::E64, 2);
        as.VSLIDE1UP(result, reduction2, x0);
        as.VOR(dst, result, reduction);
        rec.setVec(&operands[0], dst);
    }
}

FAST_HANDLE(PAVGB) {
    biscuit::Vec dst = rec.getVec(&operands[0]);
    biscuit::Vec src = rec.getVec(&operands[1]);
    rec.setVectorState(SEW::E8, 16);
    as.VAADDU(dst, dst, src);
    rec.setVec(&operands[0], dst);
}

FAST_HANDLE(PAVGW) {
    biscuit::Vec dst = rec.getVec(&operands[0]);
    biscuit::Vec src = rec.getVec(&operands[1]);
    rec.setVectorState(SEW::E16, 8);
    as.VAADDU(dst, dst, src);
    rec.setVec(&operands[0], dst);
}

FAST_HANDLE(PHMINPOSUW) {
    biscuit::Vec min = rec.scratchVec();
    biscuit::Vec vs1 = rec.scratchVec();
    biscuit::Vec src = rec.getVec(&operands[1]);
    rec.setVectorState(SEW::E16, 8);
    as.VMV(vs1, -1); // vd[0] = minu( vs1[0] , vs2[*] ), set vs1 to max so we get min element of vs2
    as.VREDMINU(min, src, vs1);

    // We found the min but we also need to find the index
    biscuit::Label loop;
    biscuit::Vec src_copy = rec.scratchVec();
    biscuit::GPR min_gpr = rec.scratch();
    biscuit::GPR current = rec.scratch();
    biscuit::GPR index = rec.scratch();
    as.VMV(src_copy, src);
    as.VMV_XS(min_gpr, min);
    as.LI(index, -1);

    as.Bind(&loop);
    as.ADDI(index, index, 1);
    as.VMV_XS(current, src_copy);
    as.VSLIDE1DOWN(src_copy, src_copy, x0);
    as.BNE(current, min_gpr, &loop);

    biscuit::Vec dst = rec.scratchVec();
    biscuit::Vec dst_final = rec.scratchVec();
    as.VMV(dst, 0);
    as.VMV_SX(dst, index);
    as.VSLIDE1UP(dst_final, dst, min_gpr);

    rec.setVec(&operands[0], dst_final);
}

FAST_HANDLE(AESENC) {
    if (Extensions::Zvkned) {
        biscuit::Vec dst = rec.getVec(&operands[0]);
        biscuit::Vec src = rec.getVec(&operands[1]);
        rec.setVectorState(SEW::E32, 4);
        as.VAESEM_VV(dst, src);
        rec.setVec(&operands[0], dst);
    } else {
        ERROR("Hit AESENC instruction but system does not support Zvkned extension");
    }
}

FAST_HANDLE(AESENCLAST) {
    if (Extensions::Zvkned) {
        biscuit::Vec dst = rec.getVec(&operands[0]);
        biscuit::Vec src = rec.getVec(&operands[1]);
        rec.setVectorState(SEW::E32, 4);
        as.VAESEF_VV(dst, src);
        rec.setVec(&operands[0], dst);
    } else {
        ERROR("Hit AESENCLAST instruction but system does not support Zvkned extension");
    }
}

FAST_HANDLE(AESDECLAST) {
    if (Extensions::Zvkned) {
        biscuit::Vec dst = rec.getVec(&operands[0]);
        biscuit::Vec src = rec.getVec(&operands[1]);
        rec.setVectorState(SEW::E32, 4);
        as.VAESDF_VV(dst, src);
        rec.setVec(&operands[0], dst);
    } else {
        ERROR("Hit AESDECLAST instruction but system does not support Zvkned extension");
    }
}

FAST_HANDLE(CMPXCHG16B) {
    biscuit::GPR address = rec.lea(&operands[0]);
    if (Extensions::Zacas) {
        WARN_ONCE("cmpxchg16b with zacas, untested, please report results");
        // We are the luckiest emulator alive!
        // AMOCAS.Q needs a register group (meaning, 2 registers side by side like t0, t1) to work
        biscuit::GPR rax = rec.getGPR(X86_REF_RAX, X86_SIZE_QWORD);
        biscuit::GPR rdx = rec.getGPR(X86_REF_RDX, X86_SIZE_QWORD);
        biscuit::GPR rbx = rec.getGPR(X86_REF_RBX, X86_SIZE_QWORD);
        biscuit::GPR rcx = rec.getGPR(X86_REF_RCX, X86_SIZE_QWORD);
        biscuit::GPR rax_t = x28;
        biscuit::GPR rdx_t = x29;
        biscuit::GPR rbx_t = x30;
        biscuit::GPR rcx_t = x31;
        static_assert(Recompiler::isScratch(x28));
        static_assert(Recompiler::isScratch(x29));
        static_assert(Recompiler::isScratch(x30));
        static_assert(Recompiler::isScratch(x31));
        ASSERT(rax_t == x28 && rdx_t == x29 && rbx_t == x30 && rcx_t == x31); // in case we change the order
        as.MV(rax_t, rax);
        as.MV(rdx_t, rdx);
        as.MV(rbx_t, rbx);
        as.MV(rcx_t, rcx);
        as.AMOCAS_Q(Ordering::AQRL, rax_t, rbx_t, address);

        // Real value is now loaded into rdx_t:rax_t. Compare with rdx:rax to set the zero flag
        // We can overwrite the rbx_t/rcx_t scratches now
        biscuit::GPR zf = rec.flag(X86_REF_ZF);
        as.XOR(rbx_t, rax_t, rax);
        as.XOR(rcx_t, rdx_t, rdx);
        as.OR(rbx_t, rbx_t, rcx_t);
        as.SEQZ(zf, rbx_t);

        rec.setGPR(X86_REF_RAX, X86_SIZE_QWORD, rax_t);
        rec.setGPR(X86_REF_RDX, X86_SIZE_QWORD, rdx_t);
    } else {
        WARN_ONCE("This program uses CMPXCHG16B and your chip doesn't have the Zacas extension, execution may be unstable");
        biscuit::GPR rax = rec.getGPR(X86_REF_RAX, X86_SIZE_QWORD);
        biscuit::GPR rdx = rec.getGPR(X86_REF_RDX, X86_SIZE_QWORD);
        biscuit::GPR rbx = rec.getGPR(X86_REF_RBX, X86_SIZE_QWORD);
        biscuit::GPR rcx = rec.getGPR(X86_REF_RCX, X86_SIZE_QWORD);
        biscuit::GPR mem0 = rec.scratch();
        biscuit::GPR mem1 = rec.scratch();

        // Definitely not actually atomic, but better than nothing ...
        biscuit::Label spinloop, writeloop;
        biscuit::GPR lock_address = rec.scratch();
        biscuit::GPR lock = rec.scratch();
        as.LI(lock_address, (u64)&g_process_globals.cas128_lock);

        as.Bind(&spinloop);
        as.LI(lock, 1);
        as.AMOSWAP_W(Ordering::AQRL, lock, lock, lock_address);
        as.BNEZ(lock, &spinloop);

        // Again, not atomic, but at least checking if one of the two qwords checking is better than nothing
        as.Bind(&writeloop);
        as.LR_D(Ordering::AQRL, mem0, address);
        rec.readMemory(mem1, address, 8, X86_SIZE_QWORD);

        Label not_equal;
        biscuit::GPR zf = rec.flag(X86_REF_ZF);
        as.MV(zf, x0); // assume not equal
        as.BNE(mem0, rax, &not_equal);
        as.BNE(mem1, rdx, &not_equal);

        as.LI(zf, 1);
        as.SC_D(Ordering::AQRL, lock, rbx, address);
        as.BNEZ(lock, &writeloop);
        rec.writeMemory(rcx, address, 8, X86_SIZE_QWORD);

        as.Bind(&not_equal);

        as.MV(rax, mem0);
        as.MV(rdx, mem1);

        rec.setGPR(X86_REF_RAX, X86_SIZE_QWORD, rax);
        rec.setGPR(X86_REF_RDX, X86_SIZE_QWORD, rdx);

        as.AMOSWAP_W(Ordering::AQRL, x0, x0, lock_address);
    }
}

FAST_HANDLE(CMPXCHG8B) {
    // TODO: also implement using Zacas if available
    biscuit::Label unaligned;
    biscuit::GPR address = rec.lea(&operands[0]);
    biscuit::GPR eax = rec.getGPR(X86_REF_RAX, X86_SIZE_QWORD);
    biscuit::GPR edx = rec.getGPR(X86_REF_RDX, X86_SIZE_QWORD);
    biscuit::GPR ebx = rec.getGPR(X86_REF_RBX, X86_SIZE_QWORD);
    biscuit::GPR ecx = rec.getGPR(X86_REF_RCX, X86_SIZE_QWORD);
    biscuit::GPR masked = rec.scratch();
    biscuit::GPR dst = rec.scratch();
    biscuit::GPR edx_eax = rec.scratch();
    biscuit::GPR ecx_ebx = rec.scratch();
    biscuit::GPR bit = rec.scratch();

    as.SLLI(edx_eax, edx, 32);
    rec.zext(dst, eax, X86_SIZE_DWORD);
    as.OR(edx_eax, edx_eax, dst);

    as.SLLI(ecx_ebx, ecx, 32);
    rec.zext(dst, ebx, X86_SIZE_DWORD);
    as.OR(ecx_ebx, ecx_ebx, dst);

    as.ANDI(masked, address, 0b111);
    as.BNEZ(masked, &unaligned);

    biscuit::Label not_equal, loop, end;
    as.Bind(&loop);
    as.LR_D(Ordering::AQRL, dst, address);
    as.BNE(dst, edx_eax, &not_equal);
    as.SC_D(Ordering::AQRL, bit, ecx_ebx, address);
    as.BNEZ(bit, &loop);

    // If here EDX:EAX == m64, and ECX:EBX was loaded to m64, need to set ZF
    rec.setFlag(X86_REF_ZF);
    as.J(&end);

    as.Bind(&not_equal);
    rec.clearFlag(X86_REF_ZF);
    as.SRLI(edx, dst, 32);
    rec.setGPR(X86_REF_RAX, X86_SIZE_DWORD, dst); // will be zexted
    rec.setGPR(X86_REF_RDX, X86_SIZE_QWORD, edx); // don't zext

    as.J(&end);

    as.Bind(&unaligned);
    // If the address is not aligned, we can't use LR.D
    // Which means we also can't be technically correct atomically
    // Use LR.D/SC.D on the aligned address anyway to at least have a little bit of guarantee
    biscuit::Label not_equal_unaligned, loop_unaligned;
    as.ANDI(masked, address, ~0b111);
    as.Bind(&loop_unaligned);
    as.LD(dst, 0, address);
    as.LR_D(Ordering::AQRL, bit, masked);
    // We do the comparison on the load from the unaligned address, obviously
    as.BNE(dst, edx_eax, &not_equal_unaligned);
    // If any of the bytes we can see with the aligned address are changed we retry
    // Of course this isn't actually atomic (we'd need hardware unaligned atomics support for that)
    // but it's better than nothing
    as.SC_D(Ordering::AQRL, bit, bit, masked); // Write the same thing we just loaded in scratch
    as.BNEZ(bit, &loop_unaligned);
    as.SD(ecx_ebx, 0, address);

    // If here EDX:EAX == m64, and ECX:EBX was loaded to m64, need to set ZF
    rec.setFlag(X86_REF_ZF);
    as.J(&end);

    as.Bind(&not_equal_unaligned);
    rec.clearFlag(X86_REF_ZF);
    as.SRLI(edx, dst, 32);
    rec.setGPR(X86_REF_RAX, X86_SIZE_DWORD, dst); // will be zexted
    rec.setGPR(X86_REF_RDX, X86_SIZE_QWORD, edx);

    as.Bind(&end);
}

FAST_HANDLE(PAUSE) {
    if (Extensions::Zihintpause) {
        as.PAUSE();
    }
}

FAST_HANDLE(FLD) {
    if (operands[0].size == 80 && operands[0].type == ZYDIS_OPERAND_TYPE_MEMORY) {
        biscuit::GPR address = rec.lea(&operands[0]);
        rec.writebackState();
        as.MV(a0, address);
        rec.callPointer(offsetof(ThreadState, f80_to_64));
        rec.restoreState();
        rec.pushX87(fa0); // push return value
    } else {
        biscuit::FPR st = rec.getST(&operands[0]);
        biscuit::FPR temp = rec.scratchFPR();
        as.FMV_D(temp, st); // move to temp because getST could return allocated FPR
        rec.pushX87(temp);
    }
}

FAST_HANDLE(FILD) {
    biscuit::FPR ftemp = rec.scratchFPR();
    biscuit::GPR value = rec.getGPR(&operands[0]);
    switch (operands[0].size) {
    case 16: {
        rec.sext(value, value, X86_SIZE_WORD);
        as.FCVT_D_W(ftemp, value);
        break;
    }
    case 32: {
        as.FCVT_D_W(ftemp, value);
        break;
    }
    case 64: {
        as.FCVT_D_L(ftemp, value);
        break;
    }
    default: {
        UNREACHABLE();
    }
    }
    rec.pushX87(ftemp);
}

void OP(void (Assembler::*func)(FPR, FPR, FPR, RMode), Recompiler& rec, Assembler& as, ZydisDecodedInstruction& instruction,
        ZydisDecodedOperand* operands, bool pop, bool reverse = false) {
    biscuit::FPR lhs = rec.getST(&operands[0]);
    biscuit::FPR rhs = rec.getST(&operands[1]);

    ZydisDecodedOperand* result_operand = &operands[0];

    if (operands[0].type == ZYDIS_OPERAND_TYPE_MEMORY) {
        // Funnily, when the operand is memory the operation happens rhs op lhs
        std::swap(lhs, rhs);
        result_operand = &operands[1];
    }

    // TODO: don't use a separate FPR here
    biscuit::FPR result;
    if (!reverse) {
        (as.*func)(lhs, lhs, rhs, RMode::DYN);
        result = lhs;
    } else {
        (as.*func)(lhs, rhs, lhs, RMode::DYN);
        result = lhs;
    }
    rec.setST(result_operand, result);

    if (pop) {
        rec.popX87();
    }
}

FAST_HANDLE(FDIV) {
    OP(&Assembler::FDIV_D, rec, as, instruction, operands, false);
}

FAST_HANDLE(FDIVP) {
    OP(&Assembler::FDIV_D, rec, as, instruction, operands, true);
}

FAST_HANDLE(FIDIV) {
    biscuit::FPR st0 = rec.getST(0);
    biscuit::GPR integer = rec.getGPR(&operands[0]);
    ASSERT(operands[0].type == ZYDIS_OPERAND_TYPE_MEMORY);
    biscuit::FPR scratch = rec.scratchFPR();
    biscuit::FPR result = rec.scratchFPR();

    if (operands[0].size == 16) {
        rec.sext(integer, integer, X86_SIZE_WORD);
    }

    as.FCVT_D_W(scratch, integer);
    as.FDIV_D(result, st0, scratch);

    rec.setST(0, result);
}

FAST_HANDLE(FDIVR) {
    OP(&Assembler::FDIV_D, rec, as, instruction, operands, false, true);
}

FAST_HANDLE(FDIVRP) {
    OP(&Assembler::FDIV_D, rec, as, instruction, operands, true, true);
}

FAST_HANDLE(FIDIVR) {
    biscuit::FPR st0 = rec.getST(0);
    biscuit::GPR integer = rec.getGPR(&operands[0]);
    ASSERT(operands[0].type == ZYDIS_OPERAND_TYPE_MEMORY);
    biscuit::FPR scratch = rec.scratchFPR();
    biscuit::FPR result = rec.scratchFPR();

    if (operands[0].size == 16) {
        rec.sext(integer, integer, X86_SIZE_WORD);
    }

    as.FCVT_D_W(scratch, integer);
    as.FDIV_D(result, scratch, st0);

    rec.setST(0, result);
}

FAST_HANDLE(FMUL) {
    OP(&Assembler::FMUL_D, rec, as, instruction, operands, false);
}

FAST_HANDLE(FMULP) {
    OP(&Assembler::FMUL_D, rec, as, instruction, operands, true);
}

FAST_HANDLE(FIMUL) {
    biscuit::FPR st0 = rec.getST(0);
    biscuit::GPR integer = rec.getGPR(&operands[0]);
    ASSERT(operands[0].type == ZYDIS_OPERAND_TYPE_MEMORY);
    biscuit::FPR scratch = rec.scratchFPR();
    biscuit::FPR result = rec.scratchFPR();

    if (operands[0].size == 16) {
        rec.sext(integer, integer, X86_SIZE_WORD);
    }

    as.FCVT_D_W(scratch, integer);
    as.FMUL_D(result, st0, scratch);

    rec.setST(0, result);
}

FAST_HANDLE(FST) {
    biscuit::FPR st0 = rec.getST(0);
    rec.setST(&operands[0], st0);
}

FAST_HANDLE(FXCH) {
    u8 index = operands[0].reg.value - ZYDIS_REGISTER_ST0;
    ASSERT(index >= 1 && index <= 7);
    biscuit::FPR st0 = rec.getST(0);
    biscuit::FPR sti = rec.getST(index);
    biscuit::FPR temp = rec.scratchFPR();
    as.FMV_D(temp, st0);
    rec.setST(0, sti);
    rec.setST(index, temp);
}

FAST_HANDLE(FSTP) {
    biscuit::FPR st0 = rec.getST(0);
    rec.setST(&operands[0], st0);
    rec.popX87();
}

FAST_HANDLE(FADD) {
    OP(&Assembler::FADD_D, rec, as, instruction, operands, false);
}

FAST_HANDLE(FADDP) {
    OP(&Assembler::FADD_D, rec, as, instruction, operands, true);
}

FAST_HANDLE(FIADD) {
    biscuit::FPR st0 = rec.getST(0);
    biscuit::GPR integer = rec.getGPR(&operands[0]);
    ASSERT(operands[0].type == ZYDIS_OPERAND_TYPE_MEMORY);
    biscuit::FPR scratch = rec.scratchFPR();
    biscuit::FPR result = rec.scratchFPR();

    if (operands[0].size == 16) {
        rec.sext(integer, integer, X86_SIZE_WORD);
    }

    as.FCVT_D_W(scratch, integer);
    as.FADD_D(result, st0, scratch);

    rec.setST(0, result);
}

FAST_HANDLE(FSUB) {
    OP(&Assembler::FSUB_D, rec, as, instruction, operands, false);
}

FAST_HANDLE(FSUBP) {
    OP(&Assembler::FSUB_D, rec, as, instruction, operands, true);
}

FAST_HANDLE(FISUB) {
    biscuit::FPR st0 = rec.getST(0);
    biscuit::GPR integer = rec.getGPR(&operands[0]);
    ASSERT(operands[0].type == ZYDIS_OPERAND_TYPE_MEMORY);
    biscuit::FPR scratch = rec.scratchFPR();
    biscuit::FPR result = rec.scratchFPR();

    if (operands[0].size == 16) {
        rec.sext(integer, integer, X86_SIZE_WORD);
    }

    as.FCVT_D_W(scratch, integer);
    as.FSUB_D(result, st0, scratch);

    rec.setST(0, result);
}

FAST_HANDLE(FSUBR) {
    OP(&Assembler::FSUB_D, rec, as, instruction, operands, false, true);
}

FAST_HANDLE(FSUBRP) {
    OP(&Assembler::FSUB_D, rec, as, instruction, operands, true, true);
}

FAST_HANDLE(FISUBR) {
    biscuit::FPR st0 = rec.getST(0);
    biscuit::GPR integer = rec.getGPR(&operands[0]);
    ASSERT(operands[0].type == ZYDIS_OPERAND_TYPE_MEMORY);
    biscuit::FPR scratch = rec.scratchFPR();
    biscuit::FPR result = rec.scratchFPR();

    if (operands[0].size == 16) {
        rec.sext(integer, integer, X86_SIZE_WORD);
    }

    as.FCVT_D_W(scratch, integer);
    as.FSUB_D(result, scratch, st0);

    rec.setST(0, result);
}

FAST_HANDLE(FSQRT) {
    biscuit::FPR st0 = rec.getST(0);
    as.FSQRT_D(st0, st0);
    rec.setST(0, st0);
}

FAST_HANDLE(FSIN) {
    rec.writebackState();
    as.MV(a0, rec.threadStatePointer());
    rec.callPointer(offsetof(ThreadState, felix86_fsin));
    rec.restoreState();
}

FAST_HANDLE(FCOS) {
    rec.writebackState();
    as.MV(a0, rec.threadStatePointer());
    rec.callPointer(offsetof(ThreadState, felix86_fcos));
    rec.restoreState();
}

FAST_HANDLE(FPATAN) {
    rec.writebackState();
    as.MV(a0, rec.threadStatePointer());
    rec.callPointer(offsetof(ThreadState, felix86_fpatan));
    rec.restoreState();

    // FPATAN also pops the stack
    rec.popX87();
}

FAST_HANDLE(FPTAN) {
    rec.writebackState();
    as.MV(a0, rec.threadStatePointer());
    rec.callPointer(offsetof(ThreadState, felix86_fptan));
    rec.restoreState();

    // FPTAN also pushes 1.0 for compatibility reasons
    biscuit::GPR temp = rec.scratch();
    biscuit::FPR one = rec.scratchFPR();
    as.LI(temp, 0x3FF0000000000000ull);
    as.FMV_D_X(one, temp);
    rec.pushX87(one);
}

FAST_HANDLE(FWAIT) {
    WARN("FWAIT encountered, treating as NOP");
}

FAST_HANDLE(FPREM) {
    rec.writebackState();
    as.MV(a0, rec.threadStatePointer());
    rec.callPointer(offsetof(ThreadState, felix86_fprem));
    rec.restoreState();
}

FAST_HANDLE(F2XM1) {
    rec.writebackState();
    as.MV(a0, rec.threadStatePointer());
    rec.callPointer(offsetof(ThreadState, felix86_f2xm1));
    rec.restoreState();
}

FAST_HANDLE(FSCALE) {
    rec.writebackState();
    as.MV(a0, rec.threadStatePointer());
    rec.callPointer(offsetof(ThreadState, felix86_fscale));
    rec.restoreState();
}

FAST_HANDLE(FYL2X) {
    rec.writebackState();
    as.MV(a0, rec.threadStatePointer());
    rec.callPointer(offsetof(ThreadState, felix86_fyl2x));
    rec.restoreState();

    // FYL2X also pops the stack
    rec.popX87();
}

FAST_HANDLE(FYL2XP1) {
    rec.writebackState();
    as.MV(a0, rec.threadStatePointer());
    rec.callPointer(offsetof(ThreadState, felix86_fyl2xp1));
    rec.restoreState();

    // FYL2XP1 also pops the stack
    rec.popX87();
}

FAST_HANDLE(FXAM) {
    rec.writebackState();
    as.MV(a0, rec.threadStatePointer());
    rec.callPointer(offsetof(ThreadState, felix86_fxam));
    rec.restoreState();
}

FAST_HANDLE(FNSTENV) {
    biscuit::GPR address = rec.lea(&operands[0]);
    rec.writebackState();
    as.MV(a1, address);
    as.MV(a0, rec.threadStatePointer());
    if (instruction.attributes & ZYDIS_ATTRIB_HAS_OPERANDSIZE) {
        rec.callPointer(offsetof(ThreadState, felix86_fstenv_16));
    } else {
        rec.callPointer(offsetof(ThreadState, felix86_fstenv_32));
    }
    rec.restoreState();
}

FAST_HANDLE(FNSTSW) {
    biscuit::GPR temp = rec.scratch();
    as.LWU(temp, offsetof(ThreadState, fpu_sw), rec.threadStatePointer());
    rec.setGPR(&operands[0], temp);
}

FAST_HANDLE(FLDENV) {
    biscuit::GPR address = rec.lea(&operands[0]);
    rec.writebackState();
    as.MV(a1, address);
    as.MV(a0, rec.threadStatePointer());
    if (instruction.attributes & ZYDIS_ATTRIB_HAS_OPERANDSIZE) {
        rec.callPointer(offsetof(ThreadState, felix86_fldenv_16));
    } else {
        rec.callPointer(offsetof(ThreadState, felix86_fldenv_32));
    }
    rec.restoreState();
}

void FIST(Recompiler& rec, u64 rip, Assembler& as, ZydisDecodedOperand* operands, bool pop, RMode mode = RMode::DYN) {
    biscuit::FPR st0 = rec.getST(0);
    biscuit::GPR address = rec.lea(&operands[0]);
    biscuit::GPR integer = rec.scratch();

    if (operands[0].size == 16) {
        as.FCVT_W_D(integer, st0, mode);
        rec.writeMemory(integer, address, 0, X86_SIZE_WORD);
    } else if (operands[0].size == 32) {
        as.FCVT_W_D(integer, st0, mode);
        rec.writeMemory(integer, address, 0, X86_SIZE_DWORD);
    } else if (operands[0].size == 64) {
        as.FCVT_L_D(integer, st0, mode);
        rec.writeMemory(integer, address, 0, X86_SIZE_QWORD);
    } else {
        UNREACHABLE();
    }

    if (pop) {
        rec.popX87();
    }
}

FAST_HANDLE(FIST) {
    FIST(rec, rip, as, operands, false);
}

FAST_HANDLE(FISTP) {
    FIST(rec, rip, as, operands, true);
}

FAST_HANDLE(FISTTP) {
    FIST(rec, rip, as, operands, true, RMode::RTZ);
}

void FCOMI(Recompiler& rec, Assembler& as, ZydisDecodedOperand* operands, bool pop) {
    biscuit::GPR cond = rec.scratch();
    biscuit::GPR cond2 = rec.scratch();
    biscuit::FPR st0 = rec.getST(&operands[0]);
    biscuit::FPR sti = rec.getST(&operands[1]);

    biscuit::GPR zf = rec.flag(X86_REF_ZF);
    biscuit::GPR cf = rec.flag(X86_REF_CF);

    Label less_than, equal, greater_than, unordered, end;

    // Most likely result - not unordered
    as.SB(x0, offsetof(ThreadState, pf), rec.threadStatePointer());

    as.FEQ_D(cond, st0, st0);
    as.FEQ_D(cond2, sti, sti);
    as.AND(cond, cond, cond2);
    as.BEQZ(cond, &unordered);

    as.FLT_D(cond, st0, sti);
    as.BNEZ(cond, &less_than);

    as.FLT_D(cond, sti, st0);
    as.BNEZ(cond, &greater_than);

    // Implicit fallthrough for when comparison is equal (not less than, not greater than, not unordered)
    as.LI(zf, 1);
    as.LI(cf, 0);
    as.J(&end);

    as.Bind(&greater_than);
    as.LI(zf, 0);
    as.LI(cf, 0);
    as.J(&end);

    as.Bind(&unordered);
    as.LI(zf, 1);
    as.LI(cf, 1);
    as.SB(cf, offsetof(ThreadState, pf), rec.threadStatePointer());
    as.J(&end);

    as.Bind(&less_than);
    as.LI(zf, 0);
    as.LI(cf, 1);

    as.Bind(&end);

    if (pop) {
        rec.popX87();
    }
}

// We don't support exceptions ATM, same as FUCOMI
FAST_HANDLE(FCOMI) {
    FCOMI(rec, as, operands, false);
}

FAST_HANDLE(FUCOMI) {
    FCOMI(rec, as, operands, false);
}

FAST_HANDLE(FCOMIP) {
    FCOMI(rec, as, operands, true);
}

FAST_HANDLE(FUCOMIP) {
    FCOMI(rec, as, operands, true);
}

void FCOM(Recompiler& rec, Assembler& as, ZydisDecodedOperand* operands, int pop_count) {
    biscuit::FPR st0, src;
    if (operands[0].type == ZYDIS_OPERAND_TYPE_REGISTER && operands[0].reg.value == ZYDIS_REGISTER_ST0) {
        st0 = rec.getST(&operands[0]);
        src = rec.getST(&operands[1]);
    } else {
        st0 = rec.getST(&operands[1]);
        src = rec.getST(&operands[0]);
    }

    biscuit::GPR c0 = rec.scratch();
    biscuit::GPR c2 = rec.scratch();
    biscuit::GPR c3 = rec.scratch();
    biscuit::GPR nan1 = rec.scratch();
    biscuit::GPR nan2 = rec.scratch();

    // Branchless way of doing this
    as.LI(c0, 0);
    as.LI(c2, 0);
    as.LI(c3, 0);
    as.FEQ_D(nan1, st0, st0);
    as.XORI(nan1, nan1, 1);
    as.FEQ_D(nan2, src, src);
    as.XORI(nan2, nan2, 1);
    as.OR(nan1, nan1, nan2);

    as.FLT_D(c0, st0, src);
    as.FEQ_D(c3, st0, src);
    // If either is NaN set all to 1s
    as.OR(c0, c0, nan1);
    as.OR(c3, c3, nan1);
    as.OR(c2, c2, nan1);
    as.SLLI(c2, c2, 10);
    as.SLLI(c0, c0, 8);
    as.SLLI(c3, c3, 14);
    as.OR(c0, c0, c2);
    as.OR(c0, c0, c3);

    as.SW(c0, offsetof(ThreadState, fpu_sw), rec.threadStatePointer());

    if (pop_count == 1) {
        rec.popX87();
    } else if (pop_count == 2) {
        // TODO: optimize me please
        rec.popX87();
        rec.popX87();
    }
}

FAST_HANDLE(FCOM) {
    FCOM(rec, as, operands, 0);
}

FAST_HANDLE(FCOMP) {
    FCOM(rec, as, operands, 1);
}

FAST_HANDLE(FCOMPP) {
    FCOM(rec, as, operands, 2);
}

FAST_HANDLE(FUCOM) {
    FCOM(rec, as, operands, 0);
}

FAST_HANDLE(FUCOMP) {
    FCOM(rec, as, operands, 1);
}

FAST_HANDLE(FUCOMPP) {
    FCOM(rec, as, operands, 2);
}

FAST_HANDLE(FRNDINT) {
    biscuit::FPR st0 = rec.getST(0);

    if (Extensions::Zfa) {
        as.FROUND_D(st0, st0);
    } else {
        biscuit::GPR temp = rec.scratch();
        as.FCVT_L_D(temp, st0);
        as.FCVT_D_L(st0, temp);
    }

    rec.setST(0, st0);
}

FAST_HANDLE(FCHS) {
    biscuit::FPR st0 = rec.getST(0);
    as.FNEG_D(st0, st0);
    rec.setST(0, st0);
}

FAST_HANDLE(FLD1) {
    biscuit::FPR st = rec.scratchFPR();

    if (Extensions::Zfa) {
        as.FLI_D(st, 1.0);
    } else {
        biscuit::GPR temp = rec.scratch();
        as.LI(temp, 0x3FF0000000000000ull);
        as.FMV_D_X(st, temp);
    }

    rec.pushX87(st);
}

FAST_HANDLE(FLDL2T) {
    constexpr u64 value = 0x400A'934F'0979'A371ull;
    biscuit::FPR st = rec.scratchFPR();
    biscuit::GPR temp = rec.scratch();
    as.LI(temp, value);
    as.FMV_D_X(st, temp);
    rec.pushX87(st);
}

FAST_HANDLE(FLDL2E) {
    constexpr u64 value = 0x3FF7'1547'652B'82FEull;
    biscuit::FPR st = rec.scratchFPR();
    biscuit::GPR temp = rec.scratch();
    as.LI(temp, value);
    as.FMV_D_X(st, temp);
    rec.pushX87(st);
}

FAST_HANDLE(FLDPI) {
    constexpr u64 value = 0x4009'21FB'5444'2D18ull;
    biscuit::FPR st = rec.scratchFPR();
    biscuit::GPR temp = rec.scratch();
    as.LI(temp, value);
    as.FMV_D_X(st, temp);
    rec.pushX87(st);
}

FAST_HANDLE(FLDLG2) {
    constexpr u64 value = 0x3FD3'4413'509F'79FFull;
    biscuit::FPR st = rec.scratchFPR();
    biscuit::GPR temp = rec.scratch();
    as.LI(temp, value);
    as.FMV_D_X(st, temp);
    rec.pushX87(st);
}

FAST_HANDLE(FLDLN2) {
    constexpr u64 value = 0x3FE6'2E42'FEFA'39EFull;
    biscuit::FPR st = rec.scratchFPR();
    biscuit::GPR temp = rec.scratch();
    as.LI(temp, value);
    as.FMV_D_X(st, temp);
    rec.pushX87(st);
}

FAST_HANDLE(FLDZ) {
    biscuit::FPR st = rec.scratchFPR();
    as.FMV_D_X(st, x0);
    rec.pushX87(st);
}

FAST_HANDLE(FABS) {
    biscuit::FPR st0 = rec.getST(0);
    as.FABS_D(st0, st0);
    rec.setST(0, st0);
}

FAST_HANDLE(FNSTCW) {
    biscuit::GPR address = rec.lea(&operands[0]);
    biscuit::GPR temp = rec.scratch();
    as.LHU(temp, offsetof(ThreadState, fpu_cw), Recompiler::threadStatePointer());
    as.SH(temp, 0, address);
}

FAST_HANDLE(FLDCW) {
    biscuit::GPR address = rec.lea(&operands[0]);
    biscuit::GPR temp = rec.scratch();
    as.LHU(temp, 0, address);
    as.SH(temp, offsetof(ThreadState, fpu_cw), Recompiler::threadStatePointer());

    biscuit::GPR rc = rec.scratch();
    // Extract rounding mode from FPU control word
    as.SRLI(rc, temp, 10);
    as.ANDI(rc, rc, 0b11);

    // Here's how the rounding modes match up
    // 00 - Round to nearest (even) x86 -> 00 RISC-V
    // 01 - Round down (towards -inf) x86 -> 10 RISC-V
    // 10 - Round up (towards +inf) x86 -> 11 RISC-V
    // 11 - Round towards zero x86 -> 01 RISC-V
    // So we can shift the following bit sequence to the right and mask it
    // 01111000, shift by the rc * 2 and we get the RISC-V rounding mode
    as.SLLI(rc, rc, 1);
    as.LI(temp, 0b01111000);
    as.SRL(temp, temp, rc);
    as.ANDI(temp, temp, 0b11);
    as.FSRM(x0, temp);

    as.SB(temp, offsetof(ThreadState, rmode_x87), rec.threadStatePointer());

    rec.setFsrmSSE(false);
}

FAST_HANDLE(FNINIT) {
    biscuit::GPR temp = rec.scratch();
    as.LI(temp, 0x037F);
    as.SH(temp, offsetof(ThreadState, fpu_cw), Recompiler::threadStatePointer());

    // FINIT sets it to nearest neighbor which happens to be 0 in both x86 and RISC-V
    as.FSRM(x0);
    rec.setFsrmSSE(false);
}

void FCMOV(Recompiler& rec, Assembler& as, ZydisDecodedOperand* operands, biscuit::GPR cond) {
    biscuit::Label not_true;
    as.BEQZ(cond, &not_true);
    biscuit::FPR sti = rec.getST(&operands[1]);
    rec.setST(0, sti);
    as.Bind(&not_true);
}

FAST_HANDLE(FCMOVB) {
    biscuit::GPR cf = rec.flag(X86_REF_CF);
    FCMOV(rec, as, operands, cf);
}

FAST_HANDLE(FCMOVE) {
    biscuit::GPR zf = rec.flag(X86_REF_ZF);
    FCMOV(rec, as, operands, zf);
}

FAST_HANDLE(FCMOVBE) {
    biscuit::GPR cf = rec.flag(X86_REF_CF);
    biscuit::GPR zf = rec.flag(X86_REF_ZF);
    biscuit::GPR cond = rec.scratch();
    as.OR(cond, cf, zf);
    FCMOV(rec, as, operands, cond);
}

FAST_HANDLE(FCMOVU) {
    biscuit::GPR pf = rec.flag(X86_REF_PF);
    FCMOV(rec, as, operands, pf);
}

FAST_HANDLE(FCMOVNB) {
    biscuit::GPR cf = rec.flag(X86_REF_CF);
    biscuit::GPR cond = rec.scratch();
    as.XORI(cond, cf, 1);
    FCMOV(rec, as, operands, cond);
}

FAST_HANDLE(FCMOVNE) {
    biscuit::GPR zf = rec.flag(X86_REF_ZF);
    biscuit::GPR cond = rec.scratch();
    as.XORI(cond, zf, 1);
    FCMOV(rec, as, operands, cond);
}

FAST_HANDLE(FCMOVNBE) {
    biscuit::GPR cf = rec.flag(X86_REF_CF);
    biscuit::GPR zf = rec.flag(X86_REF_ZF);
    biscuit::GPR cond = rec.scratch();
    as.OR(cond, cf, zf);
    as.XORI(cond, cond, 1);
    FCMOV(rec, as, operands, cond);
}

FAST_HANDLE(FCMOVNU) {
    biscuit::GPR pf = rec.flag(X86_REF_PF);
    biscuit::GPR cond = rec.scratch();
    as.XORI(cond, pf, 1);
    FCMOV(rec, as, operands, cond);
}

FAST_HANDLE(FNSAVE) {
    biscuit::GPR address = rec.lea(&operands[0]);
    rec.writebackState();
    as.MV(a1, address);
    as.MV(a0, rec.threadStatePointer());
    if (instruction.attributes & ZYDIS_ATTRIB_HAS_OPERANDSIZE) {
        rec.callPointer(offsetof(ThreadState, felix86_fsave_16));
    } else {
        rec.callPointer(offsetof(ThreadState, felix86_fsave_32));
    }
    rec.restoreState();
}

FAST_HANDLE(FRSTOR) {
    biscuit::GPR address = rec.lea(&operands[0]);
    rec.writebackState();
    as.MV(a1, address);
    as.MV(a0, rec.threadStatePointer());
    if (instruction.attributes & ZYDIS_ATTRIB_HAS_OPERANDSIZE) {
        rec.callPointer(offsetof(ThreadState, felix86_frstor_16));
    } else {
        rec.callPointer(offsetof(ThreadState, felix86_frstor_32));
    }
    rec.restoreState();
}

// INVLPG is used during thunking to do various special stuff based on the operand
// It is an instruction that no userspace program should ever use which is why it was picked
// ----------------------------------------------------------------------------------------------------
// <!> <!> See src/felix86/hle/guest_libs/README.md for more info on these functions <!> <!>
FAST_HANDLE(INVLPG) {
    if (g_config.thunks_path.empty()) {
        ERROR("INVLPG while thunking path not set?");
    }

    enum {
        INVLPG_GENERATE_TRAMPOLINE = ZYDIS_REGISTER_RAX,
        INVLPG_THUNK_CONSTRUCTOR = ZYDIS_REGISTER_RBX,
        INVLPG_GENERATE_TRAMPOLINE_PTR = ZYDIS_REGISTER_RCX,
        INVLPG_GUEST_CODE_FINISHED = ZYDIS_REGISTER_RDX,
    };

    ASSERT_MSG(instruction.length == 3, "Hit INVLPG instruction but it's not 3 bytes?");
    ASSERT(operands[0].type == ZYDIS_OPERAND_TYPE_MEMORY);

    switch (operands[0].mem.base) {
    case INVLPG_GENERATE_TRAMPOLINE: {
        const char* name = (const char*)(rip + instruction.length);
        size_t name_size = strlen(name);
        ASSERT(name_size > 0);
        VERBOSE("Generating trampoline for %s", name);
        rec.writebackState();
        void* trampoline = Thunks::generateTrampoline(rec, name);
        ASSERT_MSG(trampoline != nullptr, "Failed to install trampoline for \"%s\" (%lx)", name, (u64)name);
        rec.restoreState();
        rip += name_size + 1; // also skip null byte
        break;
    }
    case INVLPG_GENERATE_TRAMPOLINE_PTR: {
        // Instead of generating a trampoline using a name, generate one using a ptr
        // This is good when we want to generate a trampoline when we have a host ptr ie. from getprocaddr functions
        // and we know its signature but we need a way to make the guest switch to host code when it tries to jump to that host ptr
        u64* address_ptr = (u64*)(rip + instruction.length);
        u64 address = *address_ptr;
        const char* signature = (const char*)(rip + instruction.length + 8);
        size_t signature_size = strlen(signature);
        VERBOSE("Generating trampoline for %lx (%s)", address, signature);
        rec.writebackState();
        void* trampoline = Thunks::generateTrampoline(rec, signature, address);
        ASSERT_MSG(trampoline != nullptr, "Failed to install trampoline for %lx", address);
        rec.restoreState();
        rip += 8 + signature_size + 1; // also skip null byte
        break;
    }
    case INVLPG_THUNK_CONSTRUCTOR: {
        u8* signature = (u8*)(rip + instruction.length + 1);
        u64 pointers = (u64)signature + 4;
        ASSERT_MSG(*(u32*)signature == 0x12345678, "Signature check failed on library constructor");
        ASSERT_MSG((pointers & 0b111) == 0, "Pointer table not aligned?");

        const char* name = (const char*)*(u64*)pointers;
        GuestPointers* guest_pointers = (GuestPointers*)(pointers + 8);
        ASSERT_MSG(name, "Library name is null?");
        ASSERT_MSG(strlen(name) < 30, "Library name too long? For thunked library %s", name);
        VERBOSE("Running constructor for thunked library %s", name);

        Thunks::runConstructor(name, guest_pointers);
        break;
    }
    case INVLPG_GUEST_CODE_FINISHED: {
        rec.setExitReason(ExitReason::EXIT_REASON_GUEST_CODE_FINISHED);
        rec.writebackState();
        as.LI(t5, (u64)Emulator::ExitDispatcher);
        as.MV(a0, sp);
        as.JR(t5);
        rec.stopCompiling();
        break;
    }
    default: {
        ERROR("Unknown INVLPG instruction base operand?");
        break;
    }
    }
}

FAST_HANDLE(MOVSS_no_rvv) {
    if (operands[0].type == ZYDIS_OPERAND_TYPE_MEMORY) {
        biscuit::GPR src = rec.getElementGPR(&operands[1], X86_SIZE_DWORD, 0);
        rec.setElementGPR(&operands[0], X86_SIZE_DWORD, 0, src);
    } else if (operands[0].type == ZYDIS_OPERAND_TYPE_REGISTER) {
        biscuit::GPR src = rec.getElementGPR(&operands[1], X86_SIZE_DWORD, 0);
        if (operands[1].type == ZYDIS_OPERAND_TYPE_MEMORY) {
            rec.setElementGPR(&operands[0], X86_SIZE_QWORD, 0, x0);
            rec.setElementGPR(&operands[0], X86_SIZE_QWORD, 1, x0);
        }
        rec.setElementGPR(&operands[0], X86_SIZE_DWORD, 0, src);
    } else {
        UNREACHABLE();
    }
}

FAST_HANDLE(MOVAPS_no_rvv) {
    biscuit::GPR src0 = rec.getElementGPR(&operands[1], X86_SIZE_QWORD, 0);
    biscuit::GPR src1 = rec.getElementGPR(&operands[1], X86_SIZE_QWORD, 1);
    rec.setElementGPR(&operands[0], X86_SIZE_QWORD, 0, src0);
    rec.setElementGPR(&operands[0], X86_SIZE_QWORD, 1, src1);
}

FAST_HANDLE(MOVUPS_no_rvv) {
    fast_MOVAPS_no_rvv(rec, rip, as, instruction, operands);
}

FAST_HANDLE(MOVDQU_no_rvv) {
    fast_MOVAPS_no_rvv(rec, rip, as, instruction, operands);
}

FAST_HANDLE(MOVAPD_no_rvv) {
    fast_MOVAPS_no_rvv(rec, rip, as, instruction, operands);
}

FAST_HANDLE(MOVUPD_no_rvv) {
    fast_MOVAPS_no_rvv(rec, rip, as, instruction, operands);
}

FAST_HANDLE(MOVDQA_no_rvv) {
    fast_MOVAPS_no_rvv(rec, rip, as, instruction, operands);
}

FAST_HANDLE(MOVLPS_no_rvv) {
    biscuit::GPR src = rec.getElementGPR(&operands[1], X86_SIZE_QWORD, 0);
    rec.setElementGPR(&operands[0], X86_SIZE_QWORD, 0, src);
}

FAST_HANDLE(MOVHPS_no_rvv) {
    if (operands[0].type == ZYDIS_OPERAND_TYPE_MEMORY) {
        biscuit::GPR src = rec.getElementGPR(&operands[1], X86_SIZE_QWORD, 1);
        rec.setElementGPR(&operands[0], X86_SIZE_QWORD, 0, src);
    } else if (operands[1].type == ZYDIS_OPERAND_TYPE_MEMORY) {
        biscuit::GPR src = rec.getElementGPR(&operands[1], X86_SIZE_QWORD, 0);
        rec.setElementGPR(&operands[0], X86_SIZE_QWORD, 1, src);
    } else {
        UNREACHABLE();
    }
}

FAST_HANDLE(MOVLHPS_no_rvv) {
    biscuit::GPR src = rec.getElementGPR(&operands[1], X86_SIZE_QWORD, 0);
    rec.setElementGPR(&operands[0], X86_SIZE_QWORD, 1, src);
}

FAST_HANDLE(MOVHLPS_no_rvv) {
    biscuit::GPR src = rec.getElementGPR(&operands[1], X86_SIZE_QWORD, 1);
    rec.setElementGPR(&operands[0], X86_SIZE_QWORD, 0, src);
}

FAST_HANDLE(MOVMSKPS_no_rvv) {
    biscuit::GPR result = rec.scratch();
    biscuit::GPR val0 = rec.getElementGPR(&operands[1], X86_SIZE_QWORD, 0);
    biscuit::GPR val1 = rec.getElementGPR(&operands[1], X86_SIZE_QWORD, 1);
    biscuit::GPR temp0 = rec.scratch();
    biscuit::GPR temp1 = rec.scratch();

    as.MV(result, x0);

    if (Extensions::B) {
        as.BEXTI(temp1, val0, 31);
    } else {
        as.SRLI(temp0, val0, 31);
        as.ANDI(temp1, temp0, 1);
    }

    as.OR(result, result, temp1);

    as.SRLI(temp0, val0, 63 - 1);
    as.ANDI(temp1, temp0, 0b10);
    as.OR(result, result, temp1);

    as.SRLI(temp0, val1, 31 - 2);
    as.ANDI(temp1, temp0, 0b100);
    as.OR(result, result, temp1);

    as.SRLI(temp0, val1, 63 - 3);
    as.ANDI(temp1, temp0, 0b1000);
    as.OR(result, result, temp1);

    rec.setGPR(&operands[0], result);
}

FAST_HANDLE(ADDSS_no_rvv) {
    biscuit::FPR result = rec.scratchFPR();
    biscuit::FPR dst = rec.getElementFPR(&operands[0], X86_SIZE_DWORD, 0);
    biscuit::FPR src = rec.getElementFPR(&operands[1], X86_SIZE_DWORD, 0);
    as.FADD_S(result, dst, src);
    rec.setElementFPR(&operands[0], X86_SIZE_DWORD, 0, result);
}

FAST_HANDLE(SUBSS_no_rvv) {
    biscuit::FPR result = rec.scratchFPR();
    biscuit::FPR dst = rec.getElementFPR(&operands[0], X86_SIZE_DWORD, 0);
    biscuit::FPR src = rec.getElementFPR(&operands[1], X86_SIZE_DWORD, 0);
    as.FSUB_S(result, dst, src);
    rec.setElementFPR(&operands[0], X86_SIZE_DWORD, 0, result);
}

FAST_HANDLE(MULSS_no_rvv) {
    biscuit::FPR result = rec.scratchFPR();
    biscuit::FPR dst = rec.getElementFPR(&operands[0], X86_SIZE_DWORD, 0);
    biscuit::FPR src = rec.getElementFPR(&operands[1], X86_SIZE_DWORD, 0);
    as.FMUL_S(result, dst, src);
    rec.setElementFPR(&operands[0], X86_SIZE_DWORD, 0, result);
}

FAST_HANDLE(DIVSS_no_rvv) {
    biscuit::FPR result = rec.scratchFPR();
    biscuit::FPR dst = rec.getElementFPR(&operands[0], X86_SIZE_DWORD, 0);
    biscuit::FPR src = rec.getElementFPR(&operands[1], X86_SIZE_DWORD, 0);
    as.FDIV_S(result, dst, src);
    rec.setElementFPR(&operands[0], X86_SIZE_DWORD, 0, result);
}

FAST_HANDLE(RCPSS_no_rvv) {
    biscuit::FPR result = rec.scratchFPR();
    biscuit::FPR dst = rec.getElementFPR(&operands[0], X86_SIZE_DWORD, 0);

    // TODO: Use Zfa if available
    biscuit::GPR ones = rec.scratch();
    biscuit::FPR fones = rec.scratchFPR();
    as.LI(ones, 0x3F800000);
    as.FMV_W_X(fones, ones);
    as.FDIV_S(result, fones, dst);

    rec.setElementFPR(&operands[0], X86_SIZE_DWORD, 0, result);
}

FAST_HANDLE(SQRTSS_no_rvv) {
    biscuit::FPR result = rec.scratchFPR();
    biscuit::FPR dst = rec.getElementFPR(&operands[0], X86_SIZE_DWORD, 0);
    as.FSQRT_S(result, dst);
    rec.setElementFPR(&operands[0], X86_SIZE_DWORD, 0, result);
}

FAST_HANDLE(MAXSS_no_rvv) {
    biscuit::FPR result = rec.scratchFPR();
    biscuit::FPR dst = rec.getElementFPR(&operands[0], X86_SIZE_DWORD, 0);
    biscuit::FPR src = rec.getElementFPR(&operands[1], X86_SIZE_DWORD, 0);
    as.FMAX_S(result, dst, src);
    rec.setElementFPR(&operands[0], X86_SIZE_DWORD, 0, result);
}

FAST_HANDLE(MINSS_no_rvv) {
    biscuit::FPR result = rec.scratchFPR();
    biscuit::FPR dst = rec.getElementFPR(&operands[0], X86_SIZE_DWORD, 0);
    biscuit::FPR src = rec.getElementFPR(&operands[1], X86_SIZE_DWORD, 0);
    as.FMIN_S(result, dst, src);
    rec.setElementFPR(&operands[0], X86_SIZE_DWORD, 0, result);
}

FAST_HANDLE(RSQRTSS_no_rvv) {
    biscuit::FPR result = rec.scratchFPR();
    biscuit::FPR sqrted = rec.scratchFPR();
    biscuit::FPR dst = rec.getElementFPR(&operands[0], X86_SIZE_DWORD, 0);

    // TODO: Use Zfa if available
    biscuit::GPR ones = rec.scratch();
    biscuit::FPR fones = rec.scratchFPR();
    as.FSQRT_S(sqrted, dst);
    as.LI(ones, 0x3F800000);
    as.FMV_W_X(fones, ones);
    as.FDIV_S(result, fones, sqrted);

    rec.setElementFPR(&operands[0], X86_SIZE_DWORD, 0, result);
}

FAST_HANDLE(ADDPS_no_rvv) {
    for (int i = 0; i < 4; i++) {
        biscuit::FPR temp = rec.scratchFPR();
        biscuit::FPR lhs = rec.getElementFPR(&operands[0], X86_SIZE_DWORD, i);
        biscuit::FPR rhs = rec.getElementFPR(&operands[1], X86_SIZE_DWORD, i);

        as.FADD_S(temp, lhs, rhs);

        rec.setElementFPR(&operands[0], X86_SIZE_DWORD, i, temp);

        rec.popScratchFPR();
        rec.popScratchFPR();
        rec.popScratchFPR();
    }
}

FAST_HANDLE(SUBPS_no_rvv) {
    for (int i = 0; i < 4; i++) {
        biscuit::FPR temp = rec.scratchFPR();
        biscuit::FPR lhs = rec.getElementFPR(&operands[0], X86_SIZE_DWORD, i);
        biscuit::FPR rhs = rec.getElementFPR(&operands[1], X86_SIZE_DWORD, i);

        as.FSUB_S(temp, lhs, rhs);

        rec.setElementFPR(&operands[0], X86_SIZE_DWORD, i, temp);

        rec.popScratchFPR();
        rec.popScratchFPR();
        rec.popScratchFPR();
    }
}

FAST_HANDLE(MULPS_no_rvv) {
    for (int i = 0; i < 4; i++) {
        biscuit::FPR temp = rec.scratchFPR();
        biscuit::FPR lhs = rec.getElementFPR(&operands[0], X86_SIZE_DWORD, i);
        biscuit::FPR rhs = rec.getElementFPR(&operands[1], X86_SIZE_DWORD, i);

        as.FMUL_S(temp, lhs, rhs);

        rec.setElementFPR(&operands[0], X86_SIZE_DWORD, i, temp);

        rec.popScratchFPR();
        rec.popScratchFPR();
        rec.popScratchFPR();
    }
}

FAST_HANDLE(DIVPS_no_rvv) {
    for (int i = 0; i < 4; i++) {
        biscuit::FPR temp = rec.scratchFPR();
        biscuit::FPR lhs = rec.getElementFPR(&operands[0], X86_SIZE_DWORD, i);
        biscuit::FPR rhs = rec.getElementFPR(&operands[1], X86_SIZE_DWORD, i);

        as.FDIV_S(temp, lhs, rhs);

        rec.setElementFPR(&operands[0], X86_SIZE_DWORD, i, temp);

        rec.popScratchFPR();
        rec.popScratchFPR();
        rec.popScratchFPR();
    }
}

FAST_HANDLE(RCPPS_no_rvv) {
    // TODO: Zfa
    biscuit::FPR fones = rec.scratchFPR();
    biscuit::GPR ones = rec.scratch();
    as.LI(ones, 0x3f800000);
    as.FMV_W_X(fones, ones);
    for (int i = 0; i < 4; i++) {
        biscuit::FPR temp = rec.scratchFPR();
        biscuit::FPR lhs = rec.getElementFPR(&operands[0], X86_SIZE_DWORD, i);

        as.FDIV_S(temp, fones, lhs);

        rec.setElementFPR(&operands[0], X86_SIZE_DWORD, i, temp);

        rec.popScratchFPR();
        rec.popScratchFPR();
    }
}

FAST_HANDLE(SQRTPS_no_rvv) {
    for (int i = 0; i < 4; i++) {
        biscuit::FPR temp = rec.scratchFPR();
        biscuit::FPR lhs = rec.getElementFPR(&operands[0], X86_SIZE_DWORD, i);

        as.FSQRT_S(temp, lhs);

        rec.setElementFPR(&operands[0], X86_SIZE_DWORD, i, temp);

        rec.popScratchFPR();
        rec.popScratchFPR();
    }
}

FAST_HANDLE(MAXPS_no_rvv) {
    for (int i = 0; i < 4; i++) {
        biscuit::FPR temp = rec.scratchFPR();
        biscuit::FPR lhs = rec.getElementFPR(&operands[0], X86_SIZE_DWORD, i);
        biscuit::FPR rhs = rec.getElementFPR(&operands[1], X86_SIZE_DWORD, i);

        as.FMAX_S(temp, lhs, rhs);

        rec.setElementFPR(&operands[0], X86_SIZE_DWORD, i, temp);

        rec.popScratchFPR();
        rec.popScratchFPR();
        rec.popScratchFPR();
    }
}

FAST_HANDLE(MINPS_no_rvv) {
    for (int i = 0; i < 4; i++) {
        biscuit::FPR temp = rec.scratchFPR();
        biscuit::FPR lhs = rec.getElementFPR(&operands[0], X86_SIZE_DWORD, i);
        biscuit::FPR rhs = rec.getElementFPR(&operands[1], X86_SIZE_DWORD, i);

        as.FMIN_S(temp, lhs, rhs);

        rec.setElementFPR(&operands[0], X86_SIZE_DWORD, i, temp);

        rec.popScratchFPR();
        rec.popScratchFPR();
        rec.popScratchFPR();
    }
}

FAST_HANDLE(RSQRTPS_no_rvv) {
    // TODO: Zfa
    biscuit::FPR fones = rec.scratchFPR();
    biscuit::GPR ones = rec.scratch();
    as.LI(ones, 0x3f800000);
    as.FMV_W_X(fones, ones);
    for (int i = 0; i < 4; i++) {
        biscuit::FPR sqrted = rec.scratchFPR();
        biscuit::FPR temp = rec.scratchFPR();
        biscuit::FPR lhs = rec.getElementFPR(&operands[0], X86_SIZE_DWORD, i);

        as.FSQRT_S(sqrted, lhs);
        as.FDIV_S(temp, fones, sqrted);

        rec.setElementFPR(&operands[0], X86_SIZE_DWORD, i, temp);

        rec.popScratchFPR();
        rec.popScratchFPR();
    }
}

FAST_HANDLE(CMPSS_no_rvv) {
    UNIMPLEMENTED();
}

FAST_HANDLE(COMISS_no_rvv) {
    UNIMPLEMENTED();
}

FAST_HANDLE(UCOMISS_no_rvv) {
    UNIMPLEMENTED();
}

FAST_HANDLE(CMPPS_no_rvv) {
    UNIMPLEMENTED();
}

FAST_HANDLE(SHUFPS_no_rvv) {
    u8 imm = rec.getImmediate(&operands[2]);
    biscuit::GPR el0 = rec.getElementGPR(&operands[0], X86_SIZE_DWORD, imm & 0b11);
    biscuit::GPR el1 = rec.getElementGPR(&operands[0], X86_SIZE_DWORD, (imm >> 2) & 0b11);
    biscuit::GPR el2 = rec.getElementGPR(&operands[1], X86_SIZE_DWORD, (imm >> 4) & 0b11);
    biscuit::GPR el3 = rec.getElementGPR(&operands[1], X86_SIZE_DWORD, (imm >> 6) & 0b11);
    rec.setElementGPR(&operands[0], X86_SIZE_DWORD, 0, el0);
    rec.setElementGPR(&operands[0], X86_SIZE_DWORD, 1, el1);
    rec.setElementGPR(&operands[0], X86_SIZE_DWORD, 2, el2);
    rec.setElementGPR(&operands[0], X86_SIZE_DWORD, 3, el3);
}

FAST_HANDLE(SHUFPD_no_rvv) {
    u8 imm = rec.getImmediate(&operands[2]);
    biscuit::GPR el0 = rec.getElementGPR(&operands[0], X86_SIZE_QWORD, imm & 0b1);
    biscuit::GPR el1 = rec.getElementGPR(&operands[1], X86_SIZE_QWORD, (imm >> 2) & 0b1);
    rec.setElementGPR(&operands[0], X86_SIZE_QWORD, 0, el0);
    rec.setElementGPR(&operands[0], X86_SIZE_QWORD, 1, el1);
}

FAST_HANDLE(UNPCKHPS_no_rvv) {
    biscuit::GPR el0 = rec.getElementGPR(&operands[0], X86_SIZE_DWORD, 2);
    biscuit::GPR el1 = rec.getElementGPR(&operands[1], X86_SIZE_DWORD, 2);
    biscuit::GPR el2 = rec.getElementGPR(&operands[0], X86_SIZE_DWORD, 3);
    biscuit::GPR el3 = rec.getElementGPR(&operands[1], X86_SIZE_DWORD, 3);
    rec.setElementGPR(&operands[0], X86_SIZE_DWORD, 0, el0);
    rec.setElementGPR(&operands[0], X86_SIZE_DWORD, 1, el1);
    rec.setElementGPR(&operands[0], X86_SIZE_DWORD, 2, el2);
    rec.setElementGPR(&operands[0], X86_SIZE_DWORD, 3, el3);
}

FAST_HANDLE(UNPCKLPS_no_rvv) {
    biscuit::GPR el0 = rec.getElementGPR(&operands[0], X86_SIZE_DWORD, 0);
    biscuit::GPR el1 = rec.getElementGPR(&operands[1], X86_SIZE_DWORD, 0);
    biscuit::GPR el2 = rec.getElementGPR(&operands[0], X86_SIZE_DWORD, 1);
    biscuit::GPR el3 = rec.getElementGPR(&operands[1], X86_SIZE_DWORD, 1);
    rec.setElementGPR(&operands[0], X86_SIZE_DWORD, 0, el0);
    rec.setElementGPR(&operands[0], X86_SIZE_DWORD, 1, el1);
    rec.setElementGPR(&operands[0], X86_SIZE_DWORD, 2, el2);
    rec.setElementGPR(&operands[0], X86_SIZE_DWORD, 3, el3);
}

FAST_HANDLE(CVTSI2SS_no_rvv) {
    UNIMPLEMENTED();
}

FAST_HANDLE(CVTSS2SI_no_rvv) {
    UNIMPLEMENTED();
}

FAST_HANDLE(CVTTSS2SI_no_rvv) {
    UNIMPLEMENTED();
}

FAST_HANDLE(CVTPI2PS_no_rvv) {
    UNIMPLEMENTED();
}

FAST_HANDLE(CVTPS2PI_no_rvv) {
    UNIMPLEMENTED();
}

FAST_HANDLE(CVTTPS2PI_no_rvv) {
    UNIMPLEMENTED();
}

FAST_HANDLE(ANDPS_no_rvv) {
    for (int i = 0; i < 2; i++) {
        biscuit::GPR temp = rec.scratch();
        biscuit::GPR lhs = rec.getElementGPR(&operands[0], X86_SIZE_QWORD, i);
        biscuit::GPR rhs = rec.getElementGPR(&operands[1], X86_SIZE_QWORD, i);

        as.AND(temp, lhs, rhs);

        rec.setElementGPR(&operands[0], X86_SIZE_QWORD, i, temp);
    }
}

FAST_HANDLE(ORPS_no_rvv) {
    for (int i = 0; i < 2; i++) {
        biscuit::GPR temp = rec.scratch();
        biscuit::GPR lhs = rec.getElementGPR(&operands[0], X86_SIZE_QWORD, i);
        biscuit::GPR rhs = rec.getElementGPR(&operands[1], X86_SIZE_QWORD, i);

        as.OR(temp, lhs, rhs);

        rec.setElementGPR(&operands[0], X86_SIZE_QWORD, i, temp);
    }
}

FAST_HANDLE(XORPS_no_rvv) {
    for (int i = 0; i < 2; i++) {
        biscuit::GPR temp = rec.scratch();
        biscuit::GPR lhs = rec.getElementGPR(&operands[0], X86_SIZE_QWORD, i);
        biscuit::GPR rhs = rec.getElementGPR(&operands[1], X86_SIZE_QWORD, i);

        as.XOR(temp, lhs, rhs);

        rec.setElementGPR(&operands[0], X86_SIZE_QWORD, i, temp);
    }
}

FAST_HANDLE(ANDNPS_no_rvv) {
    for (int i = 0; i < 2; i++) {
        biscuit::GPR temp = rec.scratch();
        biscuit::GPR lhs = rec.getElementGPR(&operands[0], X86_SIZE_QWORD, i);
        biscuit::GPR rhs = rec.getElementGPR(&operands[1], X86_SIZE_QWORD, i);

        if (Extensions::B) {
            as.ANDN(temp, rhs, lhs);
        } else {
            as.NOT(temp, lhs);
            as.AND(temp, temp, rhs);
        }

        rec.setElementGPR(&operands[0], X86_SIZE_QWORD, i, temp);
    }
}

FAST_HANDLE(PMULHUW_no_rvv) {
    UNIMPLEMENTED();
}

FAST_HANDLE(PSADBW_no_rvv) {
    UNIMPLEMENTED();
}

FAST_HANDLE(PAVGB_no_rvv) {
    UNIMPLEMENTED();
}

FAST_HANDLE(PAVGW_no_rvv) {
    UNIMPLEMENTED();
}

FAST_HANDLE(PMAXUB_no_rvv) {
    for (int i = 0; i < 16; i++) {
        biscuit::GPR lhs = rec.getElementGPR(&operands[0], X86_SIZE_BYTE, i);
        biscuit::GPR rhs = rec.getElementGPR(&operands[1], X86_SIZE_BYTE, i);
        biscuit::GPR result = rec.scratch();

        if (Extensions::B) {
            as.MAXU(result, lhs, rhs);
        } else {
            biscuit::Label skip;
            as.MV(result, lhs);
            as.BGTU(lhs, rhs, &skip);
            as.MV(result, rhs);
            as.Bind(&skip);
        }

        rec.setElementGPR(&operands[0], X86_SIZE_BYTE, i, result);

        rec.popScratch();
        rec.popScratch();
        rec.popScratch();
    }
}

FAST_HANDLE(PMINUB_no_rvv) {
    for (int i = 0; i < 16; i++) {
        biscuit::GPR lhs = rec.getElementGPR(&operands[0], X86_SIZE_BYTE, i);
        biscuit::GPR rhs = rec.getElementGPR(&operands[1], X86_SIZE_BYTE, i);
        biscuit::GPR result = rec.scratch();

        if (Extensions::B) {
            as.MINU(result, lhs, rhs);
        } else {
            biscuit::Label skip;
            as.MV(result, lhs);
            as.BLTU(lhs, rhs, &skip);
            as.MV(result, rhs);
            as.Bind(&skip);
        }

        rec.setElementGPR(&operands[0], X86_SIZE_BYTE, i, result);

        rec.popScratch();
        rec.popScratch();
        rec.popScratch();
    }
}

FAST_HANDLE(PMAXSW_no_rvv) {
    for (int i = 0; i < 8; i++) {
        biscuit::GPR lhs = rec.getElementGPR(&operands[0], X86_SIZE_WORD, i, true);
        biscuit::GPR rhs = rec.getElementGPR(&operands[1], X86_SIZE_WORD, i, true);
        biscuit::GPR result = rec.scratch();

        if (Extensions::B) {
            as.MAX(result, lhs, rhs);
        } else {
            biscuit::Label skip;
            as.MV(result, lhs);
            as.BGT(lhs, rhs, &skip);
            as.MV(result, rhs);
            as.Bind(&skip);
        }

        rec.setElementGPR(&operands[0], X86_SIZE_WORD, i, result);

        rec.popScratch();
        rec.popScratch();
        rec.popScratch();
    }
}

FAST_HANDLE(PMINSW_no_rvv) {
    for (int i = 0; i < 8; i++) {
        biscuit::GPR lhs = rec.getElementGPR(&operands[0], X86_SIZE_WORD, i, true);
        biscuit::GPR rhs = rec.getElementGPR(&operands[1], X86_SIZE_WORD, i, true);
        biscuit::GPR result = rec.scratch();

        if (Extensions::B) {
            as.MIN(result, lhs, rhs);
        } else {
            biscuit::Label skip;
            as.MV(result, lhs);
            as.BLT(lhs, rhs, &skip);
            as.MV(result, rhs);
            as.Bind(&skip);
        }

        rec.setElementGPR(&operands[0], X86_SIZE_WORD, i, result);

        rec.popScratch();
        rec.popScratch();
        rec.popScratch();
    }
}

FAST_HANDLE(PEXTRW_no_rvv) {
    u8 imm = rec.getImmediate(&operands[2]);
    biscuit::GPR extracted = rec.getElementGPR(&operands[1], X86_SIZE_WORD, imm & 0b11);
    if (operands[0].type == ZYDIS_OPERAND_TYPE_REGISTER) {
        rec.setGPR(operands[0].reg.value, X86_SIZE_QWORD, extracted);
    } else {
        rec.setGPR(&operands[0], extracted);
    }
}

FAST_HANDLE(PINSRW_no_rvv) {
    u8 imm = rec.getImmediate(&operands[2]);
    biscuit::GPR reg = rec.getGPR(&operands[1]);
    if (operands[1].type == ZYDIS_OPERAND_TYPE_REGISTER) {
        rec.zext(reg, reg, X86_SIZE_WORD);
    }
    rec.setElementGPR(&operands[0], X86_SIZE_WORD, imm & 0b11, reg);
}

FAST_HANDLE(PMOVMSKB_no_rvv) {
    biscuit::GPR result = rec.scratch();
    biscuit::GPR bit = rec.scratch();
    as.MV(result, x0);

    biscuit::GPR el0 = rec.getElementGPR(&operands[1], X86_SIZE_QWORD, 0);
    for (int i = 0; i < 8; i++) {
        if (Extensions::B) {
            as.BEXTI(bit, el0, (8 * i) - 1);
        } else {
            as.SRLI(bit, el0, (8 * i) - 1);
            as.ANDI(bit, bit, 1);
        }
        as.SLLI(bit, bit, i);
        as.OR(result, result, bit);
    }

    biscuit::GPR el1 = rec.getElementGPR(&operands[1], X86_SIZE_QWORD, 1);
    for (int i = 0; i < 8; i++) {
        if (Extensions::B) {
            as.BEXTI(bit, el1, (8 * i) - 1);
        } else {
            as.SRLI(bit, el1, (8 * i) - 1);
            as.ANDI(bit, bit, 1);
        }
        as.SLLI(bit, bit, i);
        as.OR(result, result, bit);
    }
}

FAST_HANDLE(PSHUFW_no_rvv) {
    UNIMPLEMENTED();
}

FAST_HANDLE(MOVNTQ_no_rvv) {
    UNIMPLEMENTED();
}

FAST_HANDLE(MOVNTPS_no_rvv) {
    UNIMPLEMENTED();
}

FAST_HANDLE(PUNPCKLWD_no_rvv) {
    biscuit::GPR r0 = rec.scratch();

    as.MV(r0, x0);
    {
        biscuit::GPR temp = rec.getElementGPR(&operands[0], X86_SIZE_WORD, 0);
        as.OR(r0, r0, temp);
        rec.popScratch();
    }
    {
        biscuit::GPR temp = rec.getElementGPR(&operands[1], X86_SIZE_WORD, 0);
        as.SLLI(temp, temp, 16);
        as.OR(r0, r0, temp);
        rec.popScratch();
    }
    {
        biscuit::GPR temp = rec.getElementGPR(&operands[0], X86_SIZE_WORD, 1);
        as.SLLI(temp, temp, 32);
        as.OR(r0, r0, temp);
        rec.popScratch();
    }
    {
        biscuit::GPR temp = rec.getElementGPR(&operands[1], X86_SIZE_WORD, 1);
        as.SLLI(temp, temp, 48);
        as.OR(r0, r0, temp);
        rec.popScratch();
    }

    biscuit::GPR r1 = rec.scratch();
    as.MV(r1, x0);
    {
        biscuit::GPR temp = rec.getElementGPR(&operands[0], X86_SIZE_WORD, 2);
        as.OR(r1, r1, temp);
        rec.popScratch();
    }
    {
        biscuit::GPR temp = rec.getElementGPR(&operands[1], X86_SIZE_WORD, 2);
        as.SLLI(temp, temp, 16);
        as.OR(r1, r1, temp);
        rec.popScratch();
    }
    {
        biscuit::GPR temp = rec.getElementGPR(&operands[0], X86_SIZE_WORD, 3);
        as.SLLI(temp, temp, 32);
        as.OR(r1, r1, temp);
        rec.popScratch();
    }
    {
        biscuit::GPR temp = rec.getElementGPR(&operands[1], X86_SIZE_WORD, 3);
        as.SLLI(temp, temp, 48);
        as.OR(r1, r1, temp);
        rec.popScratch();
    }

    rec.setElementGPR(&operands[0], X86_SIZE_QWORD, 0, r0);
    rec.setElementGPR(&operands[0], X86_SIZE_QWORD, 1, r1);
}

FAST_HANDLE(PUNPCKLDQ_no_rvv) {
    biscuit::GPR el0 = rec.getElementGPR(&operands[0], X86_SIZE_DWORD, 0);
    biscuit::GPR el1 = rec.getElementGPR(&operands[1], X86_SIZE_DWORD, 0);
    biscuit::GPR el2 = rec.getElementGPR(&operands[0], X86_SIZE_DWORD, 1);
    biscuit::GPR el3 = rec.getElementGPR(&operands[1], X86_SIZE_DWORD, 1);
    rec.setElementGPR(&operands[0], X86_SIZE_DWORD, 0, el0);
    rec.setElementGPR(&operands[0], X86_SIZE_DWORD, 1, el1);
    rec.setElementGPR(&operands[0], X86_SIZE_DWORD, 2, el2);
    rec.setElementGPR(&operands[0], X86_SIZE_DWORD, 3, el3);
}

FAST_HANDLE(PUNPCKLQDQ_no_rvv) {
    biscuit::GPR el0 = rec.getElementGPR(&operands[1], X86_SIZE_QWORD, 0);
    rec.setElementGPR(&operands[0], X86_SIZE_QWORD, 1, el0);
}

FAST_HANDLE(PXOR_no_rvv) {
    fast_XORPS_no_rvv(rec, rip, as, instruction, operands);
}

FAST_HANDLE(POR_no_rvv) {
    fast_ORPS_no_rvv(rec, rip, as, instruction, operands);
}

FAST_HANDLE(PAND_no_rvv) {
    fast_ANDPS_no_rvv(rec, rip, as, instruction, operands);
}

FAST_HANDLE(PANDN_no_rvv) {
    fast_ANDNPS_no_rvv(rec, rip, as, instruction, operands);
}

void Handlers::initialize() {
#define X(name) Handlers::ptr_##name = fast_##name;
#define SIMD(name)
#define X87(name)
#include "handlers.inc"
#undef X
#undef SIMD
#undef X87
    if (Extensions::V) {
#define X(name)
#define SIMD(name) Handlers::ptr_##name = fast_##name;
#define X87(name)
#include "handlers.inc"
#undef X
#undef SIMD
#undef X87
    } else {
#define MAP(name) Handlers::ptr_##name = fast_##name##_no_rvv
        MAP(MOVSS);
        MAP(MOVAPS);
        MAP(MOVUPS);
        MAP(MOVAPD);
        MAP(MOVUPD);
        MAP(MOVDQA);
        MAP(MOVDQU);
        MAP(MOVLPS);
        MAP(MOVHPS);
        MAP(MOVLHPS);
        MAP(MOVHLPS);
        MAP(MOVMSKPS);
        MAP(ADDSS);
        MAP(SUBSS);
        MAP(MULSS);
        MAP(DIVSS);
        MAP(RCPSS);
        MAP(SQRTSS);
        MAP(MAXSS);
        MAP(MINSS);
        MAP(RSQRTSS);
        MAP(ADDPS);
        MAP(SUBPS);
        MAP(MULPS);
        MAP(DIVPS);
        MAP(RCPPS);
        MAP(SQRTPS);
        MAP(MAXPS);
        MAP(MINPS);
        MAP(RSQRTPS);
        MAP(CMPSS);
        MAP(COMISS);
        MAP(UCOMISS);
        MAP(CMPPS);
        MAP(SHUFPS);
        MAP(SHUFPD);
        MAP(UNPCKHPS);
        MAP(UNPCKLPS);
        MAP(CVTSI2SS);
        MAP(CVTSS2SI);
        MAP(CVTTSS2SI);
        MAP(CVTPI2PS);
        MAP(CVTPS2PI);
        MAP(CVTTPS2PI);
        MAP(ANDPS);
        MAP(ORPS);
        MAP(XORPS);
        MAP(ANDNPS);
        MAP(PMULHUW);
        MAP(PSADBW);
        MAP(PAVGB);
        MAP(PAVGW);
        MAP(PMAXUB);
        MAP(PMINUB);
        MAP(PMAXSW);
        MAP(PMINSW);
        MAP(PEXTRW);
        MAP(PINSRW);
        MAP(PMOVMSKB);
        MAP(PSHUFW);
        MAP(MOVNTQ);
        MAP(MOVNTPS);
        MAP(PUNPCKLWD);
        MAP(PUNPCKLDQ);
        MAP(PUNPCKLQDQ);
        MAP(PXOR);
        MAP(POR);
        MAP(PAND);
        MAP(PANDN);
#undef MAP
    }

// When we support 80-bit mode, this will be changed
#define X(name)
#define SIMD(name)
#define X87(name) Handlers::ptr_##name = fast_##name;
#include "handlers.inc"
#undef X
#undef SIMD
#undef X87
}