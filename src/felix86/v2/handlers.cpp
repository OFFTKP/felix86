#include <Zydis/Zydis.h>
#include "felix86/hle/thunks.hpp"
#include "felix86/v2/recompiler.hpp"

void felix86_syscall(ThreadState* state);

void felix86_syscall32(ThreadState* state);

void felix86_cpuid(ThreadState* state);

#define FAST_HANDLE(name)                                                                                                                            \
    void fast_##name(Recompiler& rec, const HandlerMetadata& meta, Assembler& as, ZydisDecodedInstruction& instruction, ZydisDecodedOperand* operands)

#define IS_MMX (instruction.attributes & (ZYDIS_ATTRIB_FPU_STATE_CR | ZYDIS_ATTRIB_FPU_STATE_CW))

#define HAS_VEX (instruction.attributes & (ZYDIS_ATTRIB_HAS_VEX))

#define HAS_REP (instruction.attributes & (ZYDIS_ATTRIB_HAS_REP | ZYDIS_ATTRIB_HAS_REPZ | ZYDIS_ATTRIB_HAS_REPNZ))

void SetCmpFlags(const HandlerMetadata& meta, Recompiler& rec, Assembler& as, biscuit::GPR dst, biscuit::GPR src, biscuit::GPR result,
                 x86_size_e size, bool zext_src = false, bool always_emit = false) {
    if (always_emit || rec.shouldEmitFlag(meta.rip, X86_REF_CF)) {
        biscuit::GPR test = rec.scratch();
        if (zext_src) {
            rec.zext(test, src, size);
        } else {
            test = src;
        }
        rec.updateCarrySub(dst, test);
        rec.popScratch();
    }

    if (always_emit || rec.shouldEmitFlag(meta.rip, X86_REF_PF)) {
        rec.updateParity(result);
    }

    if (always_emit || rec.shouldEmitFlag(meta.rip, X86_REF_AF)) {
        rec.updateAuxiliarySub(dst, src);
    }

    if (always_emit || rec.shouldEmitFlag(meta.rip, X86_REF_ZF)) {
        rec.updateZero(result, size);
    }

    if (always_emit || rec.shouldEmitFlag(meta.rip, X86_REF_SF)) {
        rec.updateSign(result, size);
    }

    if (always_emit || rec.shouldEmitFlag(meta.rip, X86_REF_OF)) {
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

void OP_noflags_destreg(Recompiler& rec, const HandlerMetadata& meta, Assembler& as, ZydisDecodedInstruction& instruction,
                        ZydisDecodedOperand* operands, void (Assembler::*func64)(biscuit::GPR, biscuit::GPR, biscuit::GPR),
                        void (Assembler::*func32)(biscuit::GPR, biscuit::GPR, biscuit::GPR)) {
    biscuit::GPR dst = rec.getRefGPR(rec.zydisToRef(operands[0].reg.value), X86_SIZE_QWORD);
    biscuit::GPR src;
    if (operands[1].type == ZYDIS_OPERAND_TYPE_REGISTER) {
        src = rec.getRefGPR(rec.zydisToRef(operands[1].reg.value), X86_SIZE_QWORD);
    } else {
        src = rec.getOperandGPR(&operands[1]);
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

    rec.setRefGPR(rec.zydisToRef(operands[0].reg.value), X86_SIZE_QWORD, dst);
}

void SHIFT_noflags(Recompiler& rec, const HandlerMetadata& meta, Assembler& as, ZydisDecodedInstruction& instruction, ZydisDecodedOperand* operands,
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
                    dst = rec.getRefGPR(rec.zydisToRef(operands[0].reg.value), X86_SIZE_QWORD);
                    biscuit::GPR dst_adjusted = rec.scratch();
                    as.SRLI(dst_adjusted, dst, 8);
                    dst = dst_adjusted;
                } else {
                    dst = rec.getRefGPR(rec.zydisToRef(operands[0].reg.value), X86_SIZE_QWORD);
                }
                result = rec.scratch();
            } else {
                dst = rec.getOperandGPR(&operands[0]);
                result = rec.scratch();
            }
            break;
        }
        case 16: {
            if (func64 == &Assembler::SLL) {
                dst = rec.getRefGPR(rec.zydisToRef(operands[0].reg.value), X86_SIZE_QWORD);
                result = rec.scratch();
            } else {
                dst = rec.getOperandGPR(&operands[0]);
                result = rec.scratch();
            }
            break;
        }
        case 32: {
            // Will save a zext if we get it this way
            dst = rec.getRefGPR(rec.zydisToRef(operands[0].reg.value), X86_SIZE_QWORD);
            result = dst;
            break;
        }
        case 64: {
            // Perform directly on the whole register
            dst = rec.getOperandGPR(&operands[0]);
            result = dst;
            break;
        }
        }
    } else {
        dst = rec.getOperandGPR(&operands[0]);
        result = dst;
    }

    if (operands[1].type == ZYDIS_OPERAND_TYPE_REGISTER) {
        ASSERT(rec.zydisToRef(operands[1].reg.value) == X86_REF_RCX);
        shift = rec.getRefGPR(X86_REF_RCX, X86_SIZE_QWORD);
    } else {
        shift = rec.getOperandGPR(&operands[1]);
    }

    // The 64-bit shifts use 6 bits, the 32-bit shifts use 5 bits. Doing it this way means we don't
    // have to mask the shift amount
    if (instruction.operand_width == 64) {
        (as.*func64)(result, dst, shift);
    } else {
        (as.*func32)(result, dst, shift);
    }

    rec.setOperandGPR(&operands[0], result);
}

FAST_HANDLE(MOV) {
    if (is_segment(operands[0])) {
        biscuit::GPR src = rec.getOperandGPR(&operands[1]);
        rec.writebackDirtyState();
        rec.invalidStateUntilJump();
        as.MV(a0, rec.threadStatePointer());
        as.MV(a1, src);
        as.LI(a2, operands[0].reg.value);
        rec.call((u64)felix86_set_segment);
        rec.restoreRoundingMode();
    } else if (is_segment(operands[1])) {
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
        as.LWU(seg, offset, rec.threadStatePointer());
        rec.setOperandGPR(&operands[0], seg);
    } else {
        biscuit::GPR src = rec.getOperandGPR(&operands[1]);
        rec.setOperandGPR(&operands[0], src);
    }
}

FAST_HANDLE(ADD) {
    bool needs_cf = rec.shouldEmitFlag(meta.rip, X86_REF_CF);
    bool needs_af = rec.shouldEmitFlag(meta.rip, X86_REF_AF);
    bool needs_pf = rec.shouldEmitFlag(meta.rip, X86_REF_PF);
    bool needs_zf = rec.shouldEmitFlag(meta.rip, X86_REF_ZF);
    bool needs_sf = rec.shouldEmitFlag(meta.rip, X86_REF_SF);
    bool needs_of = rec.shouldEmitFlag(meta.rip, X86_REF_OF);
    bool needs_any_flag = needs_cf || needs_of || needs_pf || needs_sf || needs_zf || needs_af;
    bool dst_reg = operands[0].type == ZYDIS_OPERAND_TYPE_REGISTER;
    if (g_config.noflag_opts && !needs_any_flag && dst_reg) {
        // We can do it faster if we don't need to calculate flags
        return OP_noflags_destreg(rec, meta, as, instruction, operands, &Assembler::ADD, &Assembler::ADDW);
    }

    biscuit::GPR result = rec.scratch();
    biscuit::GPR src = rec.getOperandGPR(&operands[1]);
    biscuit::GPR dst = rec.getOperandGPR(&operands[0]);

    as.ADD(result, dst, src);

    x86_size_e size = rec.getOperandSize(&operands[0]);

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

    rec.setOperandGPR(&operands[0], result);
}

FAST_HANDLE(SUB) {
    bool needs_cf = rec.shouldEmitFlag(meta.rip, X86_REF_CF);
    bool needs_af = rec.shouldEmitFlag(meta.rip, X86_REF_AF);
    bool needs_pf = rec.shouldEmitFlag(meta.rip, X86_REF_PF);
    bool needs_zf = rec.shouldEmitFlag(meta.rip, X86_REF_ZF);
    bool needs_sf = rec.shouldEmitFlag(meta.rip, X86_REF_SF);
    bool needs_of = rec.shouldEmitFlag(meta.rip, X86_REF_OF);
    bool needs_any_flag = needs_cf || needs_of || needs_pf || needs_sf || needs_zf || needs_af;
    bool dst_reg = operands[0].type == ZYDIS_OPERAND_TYPE_REGISTER;
    if (g_config.noflag_opts && !needs_any_flag && dst_reg) {
        // We can do it faster if we don't need to calculate flags
        return OP_noflags_destreg(rec, meta, as, instruction, operands, &Assembler::SUB, &Assembler::SUBW);
    }

    biscuit::GPR result = rec.scratch();
    biscuit::GPR src = rec.getOperandGPR(&operands[1]);
    biscuit::GPR dst = rec.getOperandGPR(&operands[0]);

    as.SUB(result, dst, src);

    x86_size_e size = rec.getOperandSize(&operands[0]);

    SetCmpFlags(meta, rec, as, dst, src, result, size, operands[1].type == ZYDIS_OPERAND_TYPE_IMMEDIATE && size != X86_SIZE_QWORD);

    rec.setOperandGPR(&operands[0], result);
}

FAST_HANDLE(SBB) {
    biscuit::GPR result = rec.scratch();
    biscuit::GPR result_2 = rec.scratch();
    biscuit::GPR src = rec.getOperandGPR(&operands[1]);
    biscuit::GPR dst = rec.getOperandGPR(&operands[0]);
    biscuit::GPR cf = rec.flag(X86_REF_CF);
    x86_size_e size = rec.getOperandSize(&operands[0]);
    u64 sign_mask = rec.getSignMask(size);

    as.SUB(result, dst, src);
    as.SUB(result_2, result, cf);

    if (rec.shouldEmitFlag(meta.rip, X86_REF_PF)) {
        rec.updateParity(result_2);
    }

    if (rec.shouldEmitFlag(meta.rip, X86_REF_AF)) {
        rec.updateAuxiliarySbb(dst, src, result, cf);
    }

    if (rec.shouldEmitFlag(meta.rip, X86_REF_OF)) {
        biscuit::GPR scratch = rec.scratch();
        biscuit::GPR scratch2 = rec.scratch();
        biscuit::GPR of = rec.flagW(X86_REF_OF);
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

    if (rec.shouldEmitFlag(meta.rip, X86_REF_CF)) {
        biscuit::GPR scratch = rec.scratch();
        biscuit::GPR cf = rec.flagWR(X86_REF_CF);
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

    if (rec.shouldEmitFlag(meta.rip, X86_REF_ZF)) {
        rec.updateZero(result_2, size);
    }

    if (rec.shouldEmitFlag(meta.rip, X86_REF_SF)) {
        rec.updateSign(result_2, size);
    }

    rec.setOperandGPR(&operands[0], result_2);
}

FAST_HANDLE(ADC) {
    biscuit::GPR result = rec.scratch();
    biscuit::GPR result_2 = rec.scratch();
    biscuit::GPR src = rec.getOperandGPR(&operands[1]);
    biscuit::GPR dst = rec.getOperandGPR(&operands[0]);
    biscuit::GPR cf = rec.flag(X86_REF_CF);
    x86_size_e size = rec.getOperandSize(&operands[0]);

    as.ADD(result, dst, src);
    as.ADD(result_2, result, cf);

    if (rec.shouldEmitFlag(meta.rip, X86_REF_PF)) {
        rec.updateParity(result_2);
    }

    if (rec.shouldEmitFlag(meta.rip, X86_REF_AF)) {
        rec.updateAuxiliaryAdc(dst, result, cf, result_2);
    }

    if (rec.shouldEmitFlag(meta.rip, X86_REF_OF)) {
        rec.updateOverflowAdd(dst, src, result_2, size);
    }

    if (rec.shouldEmitFlag(meta.rip, X86_REF_CF)) {
        rec.updateCarryAdc(dst, result, result_2, size);
    }

    if (rec.shouldEmitFlag(meta.rip, X86_REF_ZF)) {
        rec.updateZero(result_2, size);
    }

    if (rec.shouldEmitFlag(meta.rip, X86_REF_SF)) {
        rec.updateSign(result_2, size);
    }

    rec.setOperandGPR(&operands[0], result_2);
}

FAST_HANDLE(CMP) {
    biscuit::GPR result = rec.scratch();
    biscuit::GPR src = rec.getOperandGPR(&operands[1]);
    biscuit::GPR dst = rec.getOperandGPR(&operands[0]);

    as.SUB(result, dst, src);

    x86_size_e size = rec.getOperandSize(&operands[0]);

    SetCmpFlags(meta, rec, as, dst, src, result, size, operands[1].type == ZYDIS_OPERAND_TYPE_IMMEDIATE && size != X86_SIZE_QWORD);
}

FAST_HANDLE(OR) {
    bool needs_cf = rec.shouldEmitFlag(meta.rip, X86_REF_CF);
    bool needs_pf = rec.shouldEmitFlag(meta.rip, X86_REF_PF);
    bool needs_zf = rec.shouldEmitFlag(meta.rip, X86_REF_ZF);
    bool needs_sf = rec.shouldEmitFlag(meta.rip, X86_REF_SF);
    bool needs_of = rec.shouldEmitFlag(meta.rip, X86_REF_OF);
    bool needs_any_flag = needs_cf || needs_of || needs_pf || needs_sf || needs_zf;
    bool dst_reg = operands[0].type == ZYDIS_OPERAND_TYPE_REGISTER;
    if (g_config.noflag_opts && !needs_any_flag && dst_reg) {
        // We can do it faster if we don't need to calculate flags
        return OP_noflags_destreg(rec, meta, as, instruction, operands, &Assembler::OR, &Assembler::OR);
    }

    biscuit::GPR result = rec.scratch();
    biscuit::GPR src = rec.getOperandGPR(&operands[1]);
    biscuit::GPR dst = rec.getOperandGPR(&operands[0]);

    as.OR(result, dst, src);

    x86_size_e size = rec.getOperandSize(&operands[0]);

    if (needs_cf) {
        rec.zeroFlag(X86_REF_CF);
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
        rec.zeroFlag(X86_REF_OF);
    }

    rec.setOperandGPR(&operands[0], result);
}

FAST_HANDLE(XOR) {
    x86_size_e size = rec.getOperandSize(&operands[0]);

    // Optimize this common case since xor is used to zero out a register frequently
    if ((size == X86_SIZE_DWORD || size == X86_SIZE_QWORD) && operands[0].type == ZYDIS_OPERAND_TYPE_REGISTER &&
        operands[1].type == ZYDIS_OPERAND_TYPE_REGISTER && operands[0].reg.value == operands[1].reg.value) {
        rec.setRefGPR(rec.zydisToRef(operands[0].reg.value), X86_SIZE_QWORD, x0);

        if (rec.shouldEmitFlag(meta.rip, X86_REF_CF)) {
            rec.zeroFlag(X86_REF_CF);
        }

        if (rec.shouldEmitFlag(meta.rip, X86_REF_PF)) {
            biscuit::GPR pf = rec.scratch();
            as.LI(pf, 1);
            as.SB(pf, offsetof(ThreadState, pf), rec.threadStatePointer());
            rec.popScratch();
        }

        if (rec.shouldEmitFlag(meta.rip, X86_REF_ZF)) {
            rec.setFlag(X86_REF_ZF);
        }

        if (rec.shouldEmitFlag(meta.rip, X86_REF_SF)) {
            biscuit::GPR sf = rec.flagW(X86_REF_SF);
            as.MV(sf, x0);
        }

        if (rec.shouldEmitFlag(meta.rip, X86_REF_OF)) {
            rec.zeroFlag(X86_REF_OF);
        }
        return;
    }

    bool needs_cf = rec.shouldEmitFlag(meta.rip, X86_REF_CF);
    bool needs_pf = rec.shouldEmitFlag(meta.rip, X86_REF_PF);
    bool needs_zf = rec.shouldEmitFlag(meta.rip, X86_REF_ZF);
    bool needs_sf = rec.shouldEmitFlag(meta.rip, X86_REF_SF);
    bool needs_of = rec.shouldEmitFlag(meta.rip, X86_REF_OF);
    bool needs_any_flag = needs_cf || needs_of || needs_pf || needs_sf || needs_zf;
    bool dst_reg = operands[0].type == ZYDIS_OPERAND_TYPE_REGISTER;
    if (g_config.noflag_opts && !needs_any_flag && dst_reg) {
        // We can do it faster if we don't need to calculate flags
        return OP_noflags_destreg(rec, meta, as, instruction, operands, &Assembler::XOR, &Assembler::XOR);
    }

    biscuit::GPR result = rec.scratch();
    biscuit::GPR dst;
    biscuit::GPR src = rec.getOperandGPR(&operands[1]);

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

        dst = rec.getOperandGPR(&operands[0]);
        as.XOR(result, dst, src);
    }

    if (needs_cf) {
        rec.zeroFlag(X86_REF_CF);
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
        rec.zeroFlag(X86_REF_OF);
    }

    if (writeback) {
        rec.setOperandGPR(&operands[0], result);
    }
}

FAST_HANDLE(AND) {
    biscuit::GPR result = rec.scratch();
    biscuit::GPR src = rec.getOperandGPR(&operands[1]);
    biscuit::GPR dst = rec.getOperandGPR(&operands[0]);

    as.AND(result, dst, src);

    x86_size_e size = rec.getOperandSize(&operands[0]);
    if (rec.shouldEmitFlag(meta.rip, X86_REF_CF)) {
        rec.zeroFlag(X86_REF_CF);
    }

    if (rec.shouldEmitFlag(meta.rip, X86_REF_PF)) {
        rec.updateParity(result);
    }

    if (rec.shouldEmitFlag(meta.rip, X86_REF_ZF)) {
        rec.updateZero(result, size);
    }

    if (rec.shouldEmitFlag(meta.rip, X86_REF_SF)) {
        rec.updateSign(result, size);
    }

    if (rec.shouldEmitFlag(meta.rip, X86_REF_OF)) {
        rec.zeroFlag(X86_REF_OF);
    }

    rec.setOperandGPR(&operands[0], result);
}

FAST_HANDLE(HLT) {
    rec.setExitReason(ExitReason::EXIT_REASON_HLT);
    rec.writebackDirtyState();
    rec.invalidStateUntilJump();
    rec.backToDispatcher();
    rec.stopCompiling();
}

FAST_HANDLE(CALL_rsb) {
    x86_size_e size = g_mode32 ? X86_SIZE_DWORD : X86_SIZE_QWORD;

    u64 displacement = 0;
    if (operands[0].type == ZYDIS_OPERAND_TYPE_IMMEDIATE) {
        biscuit::GPR temp = rec.scratch();
        displacement = operands[0].imm.value.s;
        as.LI(temp, meta.rip.add(instruction.length + displacement).toGuest().raw());
        rec.setRip(temp);
        rec.popScratch();
    } else {
        biscuit::GPR src = rec.getOperandGPR(&operands[0]);
        rec.setRip(src);
    }

    biscuit::GPR rsp = rec.getRefGPR(X86_REF_RSP, size);
    as.ADDI(rsp, rsp, -rec.stackPointerSize());
    rec.setRefGPR(X86_REF_RSP, size, rsp);

    biscuit::GPR guest_return_address = rec.scratch();
    GuestAddress return_address = meta.rip.add(instruction.length).toGuest();
    as.LI(guest_return_address, return_address.raw());
    as.ADDI(sp, sp, -16);
    as.SD(guest_return_address, 8, sp); // this is the prediction, the guest address we hope the RET jumps to

    rec.writeMemory(guest_return_address, rsp, 0, size);
    rec.writebackDirtyState();
    rec.invalidStateUntilJump();
    rec.pushCalltrace();

    // Instead of stopping and returning to dispatcher, continue compiling the current block
    // And perform an actual call, pushing our predicted return address to the stack
    // As long as each call corresponds to a ret this prediction will work out. If it doesn't,
    // it goes back to the dispatcher. There's cases where calls don't correspond 1:1 to rets such as exceptions.

    u64 start = (u64)as.GetCursorPointer();
    biscuit::GPR host_return_address = rec.scratch();

    // AUIPC + ADDI + SD + 2 instructions for jump = 20
    // If there's indirect linking, the jump will take 12 (AUIPC + ADDI + SD) + 12 * 4 + 3 * 8 for the linkIndirect
    int offset = (!g_config.link_indirect || operands[0].type == ZYDIS_OPERAND_TYPE_IMMEDIATE) ? 20 : 12 + (12 * 4 + 3 * 8);
    as.AUIPC(host_return_address, 0);
    as.ADDI(host_return_address, host_return_address, offset);
    as.SD(host_return_address, 0, sp);
    if (operands[0].type == ZYDIS_OPERAND_TYPE_IMMEDIATE) {
        rec.jumpAndLink(meta.rip.add(instruction.length + displacement), true /* push to rsb */);
    } else {
        if (!g_config.link_indirect) {
            rec.backToDispatcher(true); // true = push to rsb
        } else {
            rec.linkIndirect();
        }
    }
    u64 here = (u64)as.GetCursorPointer();
    ASSERT(here == start + offset);

    // We could continue compiling instructions in this block. It's a bit tricky with software that use jits though.
    // For example you compile a piece of code until a call, and then garbage may follow so you start compiling garbage instructions.
    // Or it's zeroed out and you compile a bunch of zeroes... not good. So for now we link to the next block after returning
    // and just stop compiling
    rec.jumpAndLink(meta.rip.add(instruction.length));
    rec.stopCompiling();
}

FAST_HANDLE(RET_rsb) {
    x86_size_e size = g_mode32 ? X86_SIZE_DWORD : X86_SIZE_QWORD;
    biscuit::GPR ra = rec.scratch();
    ASSERT(ra == biscuit::ra);
    biscuit::GPR rsp = rec.getRefGPR(X86_REF_RSP, size);
    biscuit::GPR scratch = rec.scratch();
    rec.readMemory(scratch, rsp, 0, size);

    u64 imm = rec.stackPointerSize();
    if (operands[0].type == ZYDIS_OPERAND_TYPE_IMMEDIATE) {
        imm += rec.getImmediate(&operands[0]);
    }

    rec.addi(rsp, rsp, imm);

    rec.setRefGPR(X86_REF_RSP, size, rsp);
    rec.setRip(scratch);

    biscuit::Label misprediction;
    rec.writebackDirtyState();
    rec.invalidStateUntilJump();

    biscuit::GPR prediction = rec.scratch();
    as.LD(prediction, 8, sp);
    as.BNE(scratch, prediction, &misprediction);
    // Our prediction was correct, just return to ra
    rec.popCalltrace();
    as.LD(ra, 0, sp);
    as.ADDI(sp, sp, 16);
    as.RET();

    // Prediction was incorrect, return to dispatcher
    as.Bind(&misprediction);

    rec.popCalltrace();
    as.ADDI(sp, sp, 16);
    rec.backToDispatcher();
    rec.stopCompiling();
}

FAST_HANDLE(CALL) {
    if (g_config.rsb) {
        return fast_CALL_rsb(rec, meta, as, instruction, operands);
    }

    // TODO: deduplicate code like in call_rsb
    switch (operands[0].type) {
    case ZYDIS_OPERAND_TYPE_REGISTER:
    case ZYDIS_OPERAND_TYPE_MEMORY: {
        biscuit::GPR src = rec.getOperandGPR(&operands[0]);
        rec.setRip(src);
        biscuit::GPR rsp = rec.getRefGPR(X86_REF_RSP, rec.stackWidth());
        as.ADDI(rsp, rsp, -rec.stackPointerSize());
        rec.setRefGPR(X86_REF_RSP, rec.stackWidth(), rsp);

        biscuit::GPR scratch = rec.scratch();
        GuestAddress return_address = meta.rip.add(instruction.length).toGuest();
        as.LI(scratch, return_address.raw());
        rec.writeMemory(scratch, rsp, 0, rec.stackWidth());

        rec.writebackDirtyState();
        rec.invalidStateUntilJump();
        rec.pushCalltrace();
        rec.backToDispatcher();
        rec.stopCompiling();
        break;
    }
    case ZYDIS_OPERAND_TYPE_IMMEDIATE: {
        u64 displacement = rec.sextImmediate(rec.getImmediate(&operands[0]), operands[0].imm.size);
        GuestAddress return_address = meta.rip.add(instruction.length).toGuest();

        biscuit::GPR rsp = rec.getRefGPR(X86_REF_RSP, rec.stackWidth());
        as.ADDI(rsp, rsp, -rec.stackPointerSize());
        rec.setRefGPR(X86_REF_RSP, rec.stackWidth(), rsp);

        biscuit::GPR scratch = rec.scratch();
        as.LI(scratch, return_address.raw());
        rec.writeMemory(scratch, rsp, 0, rec.stackWidth());

        rec.addi(scratch, scratch, displacement);

        rec.setRip(scratch);
        rec.writebackDirtyState();
        rec.invalidStateUntilJump();
        rec.pushCalltrace();
        rec.jumpAndLink(meta.rip.add(instruction.length + displacement));
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
    if (g_config.rsb) {
        return fast_RET_rsb(rec, meta, as, instruction, operands);
    }

    biscuit::GPR rsp = rec.getRefGPR(X86_REF_RSP, rec.stackWidth());
    biscuit::GPR scratch = rec.scratch();
    rec.readMemory(scratch, rsp, 0, rec.stackWidth());

    u64 imm = rec.stackPointerSize();
    if (operands[0].type == ZYDIS_OPERAND_TYPE_IMMEDIATE) {
        imm += rec.getImmediate(&operands[0]);
    }

    rec.addi(rsp, rsp, imm);

    rec.setRefGPR(X86_REF_RSP, rec.stackWidth(), rsp);
    rec.setRip(scratch);
    rec.writebackDirtyState();
    rec.invalidStateUntilJump();
    rec.popCalltrace();
    rec.backToDispatcher();
    rec.stopCompiling();
}

FAST_HANDLE(PUSH) {
    biscuit::GPR src = rec.getOperandGPR(&operands[0]);
    biscuit::GPR rsp = rec.getRefGPR(X86_REF_RSP, rec.stackWidth());
    int imm = -size_to_bytes(instruction.operand_width);
    rec.writeMemory(src, rsp, imm, rec.zydisToSize(instruction.operand_width));

    as.ADDI(rsp, rsp, imm);
    rec.setRefGPR(X86_REF_RSP, rec.stackWidth(), rsp);
}

FAST_HANDLE(POP) {
    biscuit::GPR result = rec.scratch();
    biscuit::GPR rsp = rec.getRefGPR(X86_REF_RSP, rec.stackWidth());

    rec.readMemory(result, rsp, 0, rec.zydisToSize(instruction.operand_width));

    int imm = size_to_bytes(instruction.operand_width);
    rec.setOperandGPR(&operands[0], result);

    x86_ref_e ref = rec.zydisToRef(operands[0].reg.value);
    if (ref == X86_REF_RSP) {
        // pop rsp special case
        rec.setRefGPR(X86_REF_RSP, rec.stackWidth(), result);
    } else {
        as.ADDI(rsp, rsp, imm);
        rec.setRefGPR(X86_REF_RSP, rec.stackWidth(), rsp);
    }
}

FAST_HANDLE(NOP) {}

FAST_HANDLE(ENDBR32) {}

FAST_HANDLE(ENDBR64) {}

FAST_HANDLE(RDSSPD) {}

FAST_HANDLE(RDSSPQ) {}

FAST_HANDLE(RSTORSSP) {}

FAST_HANDLE(SAVEPREVSSP) {}

FAST_HANDLE(SHL_imm) {
    x86_size_e size = rec.getOperandSize(&operands[0]);
    biscuit::GPR result = rec.scratch();
    biscuit::GPR dst = rec.getOperandGPR(&operands[0]);
    u8 shift = rec.getImmediate(&operands[1]);
    shift &= instruction.operand_width == 64 ? 0x3F : 0x1F;

    bool needs_cf = rec.shouldEmitFlag(meta.rip, X86_REF_CF);
    bool needs_pf = rec.shouldEmitFlag(meta.rip, X86_REF_PF);
    bool needs_zf = rec.shouldEmitFlag(meta.rip, X86_REF_ZF);
    bool needs_sf = rec.shouldEmitFlag(meta.rip, X86_REF_SF);
    bool needs_of = rec.shouldEmitFlag(meta.rip, X86_REF_OF) && shift == 1;
    bool needs_any_flag = needs_cf || needs_of || needs_pf || needs_sf || needs_zf;
    if (!needs_any_flag && operands[0].size == X86_SIZE_QWORD) {
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
            biscuit::GPR cf = rec.flagW(X86_REF_CF);
            u8 shift_right = rec.getBitSize(size) - shift;
            shift_right &= 0x3F;
            as.SRLI(cf, dst, shift_right);
            as.ANDI(cf, cf, 1);
        }

        if (needs_of) {
            biscuit::GPR of = rec.flagW(X86_REF_OF);
            u8 shift_right = rec.getBitSize(size) - 1;
            as.SRLI(of, dst, shift_right);
            as.ANDI(of, of, 1);
            as.XOR(of, of, rec.flag(X86_REF_CF));
        }

        rec.setOperandGPR(&operands[0], result);
    } else if (operands[0].size == 32 && operands[0].type == ZYDIS_OPERAND_TYPE_REGISTER) {
        // Set it without zero extending again
        rec.setRefGPR(rec.zydisToRef(operands[0].reg.value), X86_SIZE_QWORD, dst);
    } else {
        return; // don't do nothing
    }
}

FAST_HANDLE(SHR_imm) {
    x86_size_e size = rec.getOperandSize(&operands[0]);
    biscuit::GPR result = rec.scratch();
    biscuit::GPR dst = rec.getOperandGPR(&operands[0]);
    u8 shift = rec.getImmediate(&operands[1]);
    shift &= instruction.operand_width == 64 ? 0x3F : 0x1F;

    if (shift != 0) {
        as.SRLI(result, dst, shift);

        if (rec.shouldEmitFlag(meta.rip, X86_REF_PF)) {
            rec.updateParity(result);
        }

        if (rec.shouldEmitFlag(meta.rip, X86_REF_ZF)) {
            rec.updateZero(result, size);
        }

        if (rec.shouldEmitFlag(meta.rip, X86_REF_SF)) {
            rec.updateSign(result, size);
        }

        if (rec.shouldEmitFlag(meta.rip, X86_REF_CF)) {
            biscuit::GPR cf = rec.flagW(X86_REF_CF);
            u8 shift_right = shift - 1;
            as.SRLI(cf, dst, shift_right);
            as.ANDI(cf, cf, 1);
        }

        if (rec.shouldEmitFlag(meta.rip, X86_REF_OF) && shift == 1) {
            biscuit::GPR of = rec.flagW(X86_REF_OF);
            as.SRLI(of, dst, rec.getBitSize(size) - 1);
            as.ANDI(of, of, 1);
        }

        rec.setOperandGPR(&operands[0], result);
    } else if (operands[0].size == 32 && operands[0].type == ZYDIS_OPERAND_TYPE_REGISTER) {
        // Set it without zero extending again
        rec.setRefGPR(rec.zydisToRef(operands[0].reg.value), X86_SIZE_QWORD, dst);
    } else {
        return; // don't do nothing
    }
}

FAST_HANDLE(SAR_imm) {
    x86_size_e size = rec.getOperandSize(&operands[0]);
    biscuit::GPR result = rec.scratch();
    biscuit::GPR dst = rec.getOperandGPR(&operands[0]);
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

        if (rec.shouldEmitFlag(meta.rip, X86_REF_PF)) {
            rec.updateParity(result);
        }

        if (rec.shouldEmitFlag(meta.rip, X86_REF_ZF)) {
            rec.updateZero(result, size);
        }

        if (rec.shouldEmitFlag(meta.rip, X86_REF_SF)) {
            rec.updateSign(result, size);
        }

        if (rec.shouldEmitFlag(meta.rip, X86_REF_CF)) {
            biscuit::GPR cf = rec.flagW(X86_REF_CF);
            as.SRLI(cf, dst, shift - 1);
            as.ANDI(cf, cf, 1);
        }

        if (rec.shouldEmitFlag(meta.rip, X86_REF_OF)) {
            biscuit::GPR of = rec.flagW(X86_REF_OF);
            as.MV(of, x0);
        }

        rec.setOperandGPR(&operands[0], result);
    } else if (operands[0].size == 32 && operands[0].type == ZYDIS_OPERAND_TYPE_REGISTER) {
        // Set it without zero extending again
        rec.setRefGPR(rec.zydisToRef(operands[0].reg.value), X86_SIZE_QWORD, dst);
    } else {
        return; // don't do nothing
    }
}

FAST_HANDLE(SHL) {
    if (operands[1].type == ZYDIS_OPERAND_TYPE_IMMEDIATE) {
        return fast_SHL_imm(rec, meta, as, instruction, operands);
    }

    bool needs_cf = rec.shouldEmitFlag(meta.rip, X86_REF_CF);
    bool needs_pf = rec.shouldEmitFlag(meta.rip, X86_REF_PF);
    bool needs_zf = rec.shouldEmitFlag(meta.rip, X86_REF_ZF);
    bool needs_sf = rec.shouldEmitFlag(meta.rip, X86_REF_SF);
    bool needs_of = rec.shouldEmitFlag(meta.rip, X86_REF_OF);
    bool needs_any_flag = needs_cf || needs_of || needs_pf || needs_sf || needs_zf;

    if (g_config.noflag_opts && !needs_any_flag) {
        return SHIFT_noflags(rec, meta, as, instruction, operands, &Assembler::SLL, &Assembler::SLLW);
    }

    biscuit::GPR result = rec.scratch();
    x86_size_e size = rec.getOperandSize(&operands[0]);
    biscuit::GPR dst = rec.getOperandGPR(&operands[0]);
    biscuit::GPR src = rec.getOperandGPR(&operands[1]);
    biscuit::GPR count = rec.scratch();

    if (instruction.operand_width == 64) {
        as.ANDI(count, src, 0x3F);
    } else {
        as.ANDI(count, src, 0x1F);
    }

    Label zero_source;

    // Gotta load these values as we don't know if the instruction is gonna modify them until runtime
    if (needs_cf)
        rec.flag(X86_REF_CF);

    if (needs_of)
        rec.flag(X86_REF_OF);

    if (needs_zf)
        rec.flag(X86_REF_ZF);

    if (needs_sf)
        rec.flag(X86_REF_SF);

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
        biscuit::GPR cf = rec.flagW(X86_REF_CF);
        as.LI(cf, rec.getBitSize(size));
        as.SUB(cf, cf, count);
        as.SRL(cf, dst, cf);
        as.ANDI(cf, cf, 1);
    }

    if (needs_of) {
        biscuit::GPR of = rec.flagW(X86_REF_OF);
        as.SRLI(of, result, rec.getBitSize(size) - 1);
        as.ANDI(of, of, 1);
        as.XOR(of, of, rec.flag(X86_REF_CF));
    }

    as.Bind(&zero_source);

    rec.setOperandGPR(&operands[0], result);
}

FAST_HANDLE(SHR) {
    if (operands[1].type == ZYDIS_OPERAND_TYPE_IMMEDIATE) {
        return fast_SHR_imm(rec, meta, as, instruction, operands);
    }

    bool needs_cf = rec.shouldEmitFlag(meta.rip, X86_REF_CF);
    bool needs_pf = rec.shouldEmitFlag(meta.rip, X86_REF_PF);
    bool needs_zf = rec.shouldEmitFlag(meta.rip, X86_REF_ZF);
    bool needs_sf = rec.shouldEmitFlag(meta.rip, X86_REF_SF);
    bool needs_of = rec.shouldEmitFlag(meta.rip, X86_REF_OF);
    bool needs_any_flag = needs_cf || needs_of || needs_pf || needs_sf || needs_zf;

    if (g_config.noflag_opts && !needs_any_flag) {
        return SHIFT_noflags(rec, meta, as, instruction, operands, &Assembler::SRL, &Assembler::SRLW);
    }

    biscuit::GPR result = rec.scratch();
    x86_size_e size = rec.getOperandSize(&operands[0]);
    biscuit::GPR dst = rec.getOperandGPR(&operands[0]);
    biscuit::GPR src = rec.getOperandGPR(&operands[1]);
    biscuit::GPR count = rec.scratch();

    if (instruction.operand_width == 64) {
        as.ANDI(count, src, 0x3F);
    } else {
        as.ANDI(count, src, 0x1F);
    }

    Label zero_source;

    // Gotta load these values as we don't know if the instruction is gonna modify them until runtime
    if (needs_cf)
        rec.flag(X86_REF_CF);

    if (needs_of)
        rec.flag(X86_REF_OF);

    if (needs_zf)
        rec.flag(X86_REF_ZF);

    if (needs_sf)
        rec.flag(X86_REF_SF);

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
        biscuit::GPR cf = rec.flagW(X86_REF_CF);
        as.ADDI(cf, count, -1);
        as.SRL(cf, dst, cf);
        as.ANDI(cf, cf, 1);
    }

    if (needs_of) {
        biscuit::GPR of = rec.flagW(X86_REF_OF);
        as.SRLI(of, dst, rec.getBitSize(size) - 1);
        as.ANDI(of, of, 1);
    }

    as.Bind(&zero_source);

    rec.setOperandGPR(&operands[0], result);
}

FAST_HANDLE(SAR) {
    if (operands[1].type == ZYDIS_OPERAND_TYPE_IMMEDIATE) {
        return fast_SAR_imm(rec, meta, as, instruction, operands);
    }

    biscuit::GPR result = rec.scratch();
    x86_size_e size = rec.getOperandSize(&operands[0]);
    biscuit::GPR dst = rec.getOperandGPR(&operands[0]);
    biscuit::GPR src = rec.getOperandGPR(&operands[1]);
    biscuit::GPR count = rec.scratch();

    if (instruction.operand_width == 64) {
        as.ANDI(count, src, 0x3F);
    } else {
        as.ANDI(count, src, 0x1F);
    }

    Label zero_source;

    // Gotta load these values as we don't know if the instruction is gonna modify them until runtime
    if (rec.shouldEmitFlag(meta.rip, X86_REF_CF))
        rec.flag(X86_REF_CF);

    if (rec.shouldEmitFlag(meta.rip, X86_REF_OF))
        rec.flag(X86_REF_OF);

    if (rec.shouldEmitFlag(meta.rip, X86_REF_ZF))
        rec.flag(X86_REF_ZF);

    if (rec.shouldEmitFlag(meta.rip, X86_REF_SF))
        rec.flag(X86_REF_SF);

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

    if (rec.shouldEmitFlag(meta.rip, X86_REF_PF)) {
        rec.updateParity(result);
    }

    if (rec.shouldEmitFlag(meta.rip, X86_REF_ZF)) {
        rec.updateZero(result, size);
    }

    if (rec.shouldEmitFlag(meta.rip, X86_REF_SF)) {
        rec.updateSign(result, size);
    }

    if (rec.shouldEmitFlag(meta.rip, X86_REF_CF)) {
        biscuit::GPR cf = rec.flagW(X86_REF_CF);
        as.ADDI(cf, count, -1);
        as.SRL(cf, dst, cf);
        as.ANDI(cf, cf, 1);
    }

    if (rec.shouldEmitFlag(meta.rip, X86_REF_OF)) {
        biscuit::GPR of = rec.flagW(X86_REF_OF);
        as.MV(of, x0);
    }

    as.Bind(&zero_source);

    rec.setOperandGPR(&operands[0], result);
}

FAST_HANDLE(MOVQ) {
    if (operands[0].type == ZYDIS_OPERAND_TYPE_MEMORY) {
        ASSERT(operands[0].size == 64);
        biscuit::GPR dst = rec.scratch();
        biscuit::Vec src = rec.getOperandVec(&operands[1]);

        rec.setVectorState(SEW::E64, 2);
        as.VMV_XS(dst, src);

        rec.setOperandGPR(&operands[0], dst);
    } else if (operands[1].type == ZYDIS_OPERAND_TYPE_MEMORY) {
        ASSERT(operands[0].size == 128);
        ASSERT(operands[1].size == 64);
        biscuit::GPR src = rec.getOperandGPR(&operands[1]);
        biscuit::Vec dst = rec.getOperandVec(&operands[0]);

        rec.setVectorState(SEW::E64, 2);
        as.VMV(v0, 0b10);

        // Zero upper 64-bit elements (this will be useful for when we get to AVX)
        as.VXOR(dst, dst, dst, VecMask::Yes);
        as.VMV_SX(dst, src);

        rec.setOperandVec(&operands[0], dst);
    } else if (operands[1].type == ZYDIS_OPERAND_TYPE_REGISTER) {
        ASSERT(operands[0].type == ZYDIS_OPERAND_TYPE_REGISTER && operands[1].type == ZYDIS_OPERAND_TYPE_REGISTER);

        if (rec.isGPR(operands[1].reg.value)) {
            biscuit::GPR src = rec.getOperandGPR(&operands[1]);
            biscuit::Vec dst = rec.getOperandVec(&operands[0]);

            rec.setVectorState(SEW::E64, 2);
            as.VMV(v0, 0b10);

            // Zero upper 64-bit elements (this will be useful for when we get to AVX)
            as.VXOR(dst, dst, dst, VecMask::Yes);
            as.VMV_SX(dst, src);

            rec.setOperandVec(&operands[0], dst);
        } else if (rec.isGPR(operands[0].reg.value)) {
            biscuit::GPR dst = rec.getOperandGPR(&operands[0]);
            biscuit::Vec src = rec.getOperandVec(&operands[1]);

            rec.setVectorState(SEW::E64, 2);
            as.VMV_XS(dst, src);

            rec.setOperandGPR(&operands[0], dst);
        } else {
            biscuit::Vec result = rec.scratchVec();
            biscuit::Vec src = rec.getOperandVec(&operands[1]);

            rec.setVectorState(SEW::E64, 2);
            as.VMV(v0, 0b01);
            as.VMV(result, 0);
            as.VOR(result, src, 0, VecMask::Yes);

            rec.setOperandVec(&operands[0], result);
        }
    }
}

FAST_HANDLE(MOVD) {
    if (operands[0].type == ZYDIS_OPERAND_TYPE_MEMORY) {
        ASSERT(operands[0].size == 32);
        biscuit::GPR dst = rec.scratch();
        biscuit::Vec src = rec.getOperandVec(&operands[1]);

        rec.setVectorState(SEW::E32, 1);
        as.VMV_XS(dst, src);

        rec.setOperandGPR(&operands[0], dst);
    } else if (operands[1].type == ZYDIS_OPERAND_TYPE_MEMORY) {
        ASSERT(operands[0].size == 128);
        ASSERT(operands[1].size == 32);
        biscuit::GPR src = rec.getOperandGPR(&operands[1]);
        biscuit::Vec dst = rec.getOperandVec(&operands[0]);

        rec.setVectorState(SEW::E32, 4);
        as.VMV(v0, 0b1110);

        // Zero upper 32-bit elements (this will be useful for when we get to AVX)
        as.VXOR(dst, dst, dst, VecMask::Yes);
        as.VMV_SX(dst, src);

        rec.setOperandVec(&operands[0], dst);
    } else if (operands[1].type == ZYDIS_OPERAND_TYPE_REGISTER) {
        ASSERT(operands[0].type == ZYDIS_OPERAND_TYPE_REGISTER && operands[1].type == ZYDIS_OPERAND_TYPE_REGISTER);

        if (rec.isGPR(operands[1].reg.value)) {
            biscuit::GPR src = rec.getOperandGPR(&operands[1]);
            biscuit::Vec dst = rec.getOperandVec(&operands[0]);

            rec.setVectorState(SEW::E32, 4);
            as.VMV(v0, 0b1110);

            // Zero upper 32-bit elements (this will be useful for when we get to AVX)
            as.VXOR(dst, dst, dst, VecMask::Yes);
            as.VMV_SX(dst, src);

            rec.setOperandVec(&operands[0], dst);
        } else if (rec.isGPR(operands[0].reg.value)) {
            biscuit::GPR dst = rec.getOperandGPR(&operands[0]);
            biscuit::Vec src = rec.getOperandVec(&operands[1]);

            rec.setVectorState(SEW::E32, 4);
            as.VMV_XS(dst, src);

            rec.setOperandGPR(&operands[0], dst);
        } else {
            biscuit::Vec result = rec.scratchVec();
            biscuit::Vec src = rec.getOperandVec(&operands[1]);

            rec.setVectorState(SEW::E32, 4);
            as.VMV(v0, 0b01);
            as.VMV(result, 0);
            as.VOR(result, src, 0, VecMask::Yes);

            rec.setOperandVec(&operands[0], result);
        }
    }
}

FAST_HANDLE(JMP) {
    switch (operands[0].type) {
    case ZYDIS_OPERAND_TYPE_REGISTER:
    case ZYDIS_OPERAND_TYPE_MEMORY: {
        biscuit::GPR src = rec.getOperandGPR(&operands[0]);
        rec.setRip(src);
        rec.writebackDirtyState();
        rec.invalidStateUntilJump();
        if (!!g_config.link_indirect) {
            rec.linkIndirect();
        } else {
            rec.backToDispatcher();
        }
        rec.stopCompiling();
        break;
    }
    case ZYDIS_OPERAND_TYPE_IMMEDIATE: {
        u64 displacement = rec.sextImmediate(rec.getImmediate(&operands[0]), operands[0].imm.size);
        GuestAddress address = meta.rip.add(instruction.length + displacement).toGuest();
        biscuit::GPR scratch = rec.scratch();
        as.LI(scratch, address.raw());
        rec.setRip(scratch);
        rec.writebackDirtyState();
        rec.invalidStateUntilJump();
        rec.jumpAndLink(meta.rip.add(instruction.length + displacement));
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
    biscuit::GPR address = rec.lea(&operands[1]);
    rec.setOperandGPR(&operands[0], address);
}

FAST_HANDLE(RDFSBASE) {
    biscuit::GPR fs = rec.scratch();
    as.LD(fs, offsetof(ThreadState, fsbase), rec.threadStatePointer());
    rec.setOperandGPR(&operands[0], fs);
}

FAST_HANDLE(RDGSBASE) {
    biscuit::GPR gs = rec.scratch();
    as.LD(gs, offsetof(ThreadState, gsbase), rec.threadStatePointer());
    rec.setOperandGPR(&operands[0], gs);
}

FAST_HANDLE(DIV) {
    x86_size_e size = rec.getOperandSize(&operands[0]);
    // we don't need to move src to scratch because the rdx and rax in all these cases are in scratches
    biscuit::GPR src = rec.getOperandGPR(&operands[0]);

    switch (size) {
    case X86_SIZE_BYTE:
    case X86_SIZE_BYTE_HIGH: {
        biscuit::GPR mod = rec.scratch();
        biscuit::GPR ax = rec.getRefGPR(X86_REF_RAX, X86_SIZE_WORD);

        as.REMUW(mod, ax, src);
        as.DIVUW(ax, ax, src);

        rec.setRefGPR(X86_REF_RAX, X86_SIZE_BYTE, ax); // TODO: word write
        rec.setRefGPR(X86_REF_RAX, X86_SIZE_BYTE_HIGH, mod);
        break;
    }
    case X86_SIZE_WORD: {
        biscuit::GPR ax = rec.getRefGPR(X86_REF_RAX, X86_SIZE_WORD);
        biscuit::GPR dx = rec.getRefGPR(X86_REF_RDX, X86_SIZE_WORD);
        as.SLLIW(dx, dx, 16);
        as.OR(dx, dx, ax);

        as.DIVUW(ax, dx, src);
        as.REMUW(dx, dx, src);

        rec.setRefGPR(X86_REF_RAX, X86_SIZE_WORD, ax);
        rec.setRefGPR(X86_REF_RDX, X86_SIZE_WORD, dx);
        break;
    }
    case X86_SIZE_DWORD: {
        biscuit::GPR eax = rec.getRefGPR(X86_REF_RAX, X86_SIZE_DWORD);
        biscuit::GPR edx = rec.getRefGPR(X86_REF_RDX, X86_SIZE_QWORD);
        as.SLLI(edx, edx, 32);
        as.OR(edx, edx, eax);

        as.DIVU(eax, edx, src);
        as.REMU(edx, edx, src);

        rec.setRefGPR(X86_REF_RAX, X86_SIZE_DWORD, eax);
        rec.setRefGPR(X86_REF_RDX, X86_SIZE_DWORD, edx);
        break;
    }
    case X86_SIZE_QWORD: {
        rec.writebackDirtyState();
        rec.invalidStateUntilJump();

        biscuit::GPR address = rec.scratch();
        as.LI(address, (u64)&felix86_divu128);
        as.MV(a1, src);
        as.MV(a0, rec.threadStatePointer());
        as.JALR(address);
        rec.restoreRoundingMode();
        break;
    }
    default: {
        UNREACHABLE();
        break;
    }
    }

    rec.setFlagUndefined(X86_REF_CF);
    rec.setFlagUndefined(X86_REF_ZF);
    rec.setFlagUndefined(X86_REF_SF);
    rec.setFlagUndefined(X86_REF_OF);
}

FAST_HANDLE(IDIV) {
    x86_size_e size = rec.getOperandSize(&operands[0]);
    biscuit::GPR src = rec.getOperandGPR(&operands[0]);

    switch (size) {
    case X86_SIZE_BYTE:
    case X86_SIZE_BYTE_HIGH: {
        biscuit::GPR mod = rec.scratch();
        biscuit::GPR divisor = rec.scratch();
        biscuit::GPR ax_sext = rec.scratch();
        biscuit::GPR ax = rec.getRefGPR(X86_REF_RAX, X86_SIZE_WORD);

        rec.sexth(ax_sext, ax);
        rec.sextb(divisor, src);

        as.REMW(mod, ax_sext, divisor);
        as.DIVW(ax, ax_sext, divisor);

        rec.popScratch();

        rec.setRefGPR(X86_REF_RAX, X86_SIZE_BYTE, ax);
        rec.setRefGPR(X86_REF_RAX, X86_SIZE_BYTE_HIGH, mod);
        break;
    }
    case X86_SIZE_WORD: {
        biscuit::GPR src_sext = rec.scratch();
        biscuit::GPR ax = rec.getRefGPR(X86_REF_RAX, X86_SIZE_WORD);
        biscuit::GPR dx = rec.getRefGPR(X86_REF_RDX, X86_SIZE_WORD);
        as.SLLIW(dx, dx, 16);
        as.OR(dx, dx, ax);

        rec.sexth(src_sext, src);

        as.DIVW(ax, dx, src_sext);
        as.REMW(dx, dx, src_sext);

        rec.setRefGPR(X86_REF_RAX, X86_SIZE_WORD, ax);
        rec.setRefGPR(X86_REF_RDX, X86_SIZE_WORD, dx);
        break;
    }
    case X86_SIZE_DWORD: {
        biscuit::GPR src_sext = rec.scratch();
        biscuit::GPR eax = rec.getRefGPR(X86_REF_RAX, X86_SIZE_DWORD);
        biscuit::GPR edx = rec.getRefGPR(X86_REF_RDX, X86_SIZE_QWORD);
        as.SLLI(edx, edx, 32);
        as.OR(edx, edx, eax);

        as.ADDIW(src_sext, src, 0);

        as.DIV(eax, edx, src_sext);
        as.REM(edx, edx, src_sext);

        rec.setRefGPR(X86_REF_RAX, X86_SIZE_DWORD, eax);
        rec.setRefGPR(X86_REF_RDX, X86_SIZE_DWORD, edx);
        break;
    }
    case X86_SIZE_QWORD: {
        rec.writebackDirtyState();
        rec.invalidStateUntilJump();

        biscuit::GPR address = rec.scratch();
        as.LI(address, (u64)&felix86_div128);
        as.MV(a1, src);
        as.MV(a0, rec.threadStatePointer());
        as.JALR(address);
        rec.restoreRoundingMode();
        break;
    }
    default: {
        UNREACHABLE();
        break;
    }
    }

    rec.setFlagUndefined(X86_REF_CF);
    rec.setFlagUndefined(X86_REF_ZF);
    rec.setFlagUndefined(X86_REF_SF);
    rec.setFlagUndefined(X86_REF_OF);
}

FAST_HANDLE(TEST) {
    biscuit::GPR result = rec.scratch();

    biscuit::GPR src = rec.getOperandGPR(&operands[1]);
    biscuit::GPR dst = rec.getOperandGPR(&operands[0]);

    as.AND(result, dst, src);

    x86_size_e size = rec.getOperandSize(&operands[0]);

    if (rec.shouldEmitFlag(meta.rip, X86_REF_CF)) {
        rec.zeroFlag(X86_REF_CF);
    }

    if (rec.shouldEmitFlag(meta.rip, X86_REF_PF)) {
        rec.updateParity(result);
    }

    if (rec.shouldEmitFlag(meta.rip, X86_REF_ZF)) {
        rec.updateZero(result, size);
    }

    if (rec.shouldEmitFlag(meta.rip, X86_REF_SF)) {
        rec.updateSign(result, size);
    }

    if (rec.shouldEmitFlag(meta.rip, X86_REF_OF)) {
        rec.zeroFlag(X86_REF_OF);
    }
}

FAST_HANDLE(INC) {
    x86_size_e size = rec.getOperandSize(&operands[0]);
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

        dst = rec.getOperandGPR(&operands[0]);
        as.ADDI(res, dst, 1);
    }

    if (rec.shouldEmitFlag(meta.rip, X86_REF_AF)) {
        rec.updateAuxiliaryAdd(dst, res);
    }

    if (rec.shouldEmitFlag(meta.rip, X86_REF_OF)) {
        biscuit::GPR one = rec.scratch();
        as.LI(one, 1);
        rec.updateOverflowAdd(dst, one, res, size);
        rec.popScratch();
    }

    if (rec.shouldEmitFlag(meta.rip, X86_REF_PF)) {
        rec.updateParity(res);
    }

    if (rec.shouldEmitFlag(meta.rip, X86_REF_ZF)) {
        rec.updateZero(res, size);
    }

    if (rec.shouldEmitFlag(meta.rip, X86_REF_SF)) {
        rec.updateSign(res, size);
    }

    if (writeback) {
        rec.setOperandGPR(&operands[0], res);
    }
}

FAST_HANDLE(DEC) {
    x86_size_e size = rec.getOperandSize(&operands[0]);
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

        dst = rec.getOperandGPR(&operands[0]);
        as.ADDI(res, dst, -1);
    }

    if (rec.shouldEmitFlag(meta.rip, X86_REF_AF)) {
        biscuit::GPR one = rec.scratch();
        as.LI(one, 1);
        rec.updateAuxiliarySub(dst, one);
        rec.popScratch();
    }

    if (rec.shouldEmitFlag(meta.rip, X86_REF_OF)) {
        biscuit::GPR one = rec.scratch();
        as.LI(one, 1);
        rec.updateOverflowSub(dst, one, res, size);
        rec.popScratch();
    }

    if (rec.shouldEmitFlag(meta.rip, X86_REF_PF)) {
        rec.updateParity(res);
    }

    if (rec.shouldEmitFlag(meta.rip, X86_REF_ZF)) {
        rec.updateZero(res, size);
    }

    if (rec.shouldEmitFlag(meta.rip, X86_REF_SF)) {
        rec.updateSign(res, size);
    }

    if (writeback) {
        rec.setOperandGPR(&operands[0], res);
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

    rec.setRefGPR(X86_REF_RAX, X86_SIZE_BYTE_HIGH, result);
}

FAST_HANDLE(SAHF) {
    biscuit::GPR cf = rec.flagW(X86_REF_CF);
    biscuit::GPR af = rec.scratch();
    biscuit::GPR zf = rec.flagW(X86_REF_ZF);
    biscuit::GPR sf = rec.flagW(X86_REF_SF);
    biscuit::GPR ah = rec.getRefGPR(X86_REF_RAX, X86_SIZE_BYTE_HIGH);

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
    ASSERT(operands[0].size != 8 && operands[0].size != 16);
    x86_size_e size = rec.getOperandSize(&operands[0]);
    biscuit::GPR address = rec.lea(&operands[0]);
    biscuit::GPR src = rec.getOperandGPR(&operands[1]);
    biscuit::GPR scratch = rec.scratch();
    biscuit::GPR dst = rec.scratch();

    as.MV(scratch, src);

    switch (size) {
    case X86_SIZE_DWORD: {
        as.AMOSWAP_W(Ordering::AQRL, dst, scratch, address);
        break;
    }
    case X86_SIZE_QWORD: {
        as.AMOSWAP_D(Ordering::AQRL, dst, scratch, address);
        break;
    }
    default: {
        UNREACHABLE();
        break;
    }
    }

    rec.setOperandGPR(&operands[1], dst);
}

FAST_HANDLE(XCHG) {
    if (operands[0].type == ZYDIS_OPERAND_TYPE_MEMORY) {
        if (operands[0].size == 8 || operands[0].size == 16) {
            WARN("Atomic XCHG with 8 or 16-bit operands encountered");
        } else {
            return fast_XCHG_lock(rec, meta, as, instruction, operands);
        }
    }

    biscuit::GPR temp = rec.scratch();
    biscuit::GPR src = rec.getOperandGPR(&operands[1]);
    biscuit::GPR dst = rec.getOperandGPR(&operands[0]);

    as.MV(temp, src);

    rec.setOperandGPR(&operands[1], dst);
    rec.setOperandGPR(&operands[0], temp);
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
    biscuit::GPR cf = rec.flagW(X86_REF_CF);
    as.MV(cf, x0);
}

FAST_HANDLE(STC) {
    biscuit::GPR cf = rec.flagW(X86_REF_CF);
    as.LI(cf, 1);
}

FAST_HANDLE(CBW) {
    biscuit::GPR al = rec.getRefGPR(X86_REF_RAX, X86_SIZE_BYTE);
    rec.sextb(al, al); // al is a scratch already
    rec.setRefGPR(X86_REF_RAX, X86_SIZE_WORD, al);
}

FAST_HANDLE(CWDE) {
    biscuit::GPR ax = rec.getRefGPR(X86_REF_RAX, X86_SIZE_WORD);
    rec.sexth(ax, ax);
    rec.setRefGPR(X86_REF_RAX, X86_SIZE_DWORD, ax);
}

FAST_HANDLE(CDQE) {
    biscuit::GPR eax = rec.getRefGPR(X86_REF_RAX, X86_SIZE_QWORD);
    as.ADDIW(eax, eax, 0);
    rec.setRefGPR(X86_REF_RAX, X86_SIZE_QWORD, eax);
}

FAST_HANDLE(CWD) {
    biscuit::GPR sext = rec.scratch();
    biscuit::GPR ax = rec.getRefGPR(X86_REF_RAX, X86_SIZE_QWORD);
    rec.sexth(sext, ax);
    as.SRLI(sext, sext, 16);
    rec.setRefGPR(X86_REF_RDX, X86_SIZE_WORD, sext);
}

FAST_HANDLE(CDQ) {
    biscuit::GPR sext = rec.scratch();
    biscuit::GPR eax = rec.getRefGPR(X86_REF_RAX, X86_SIZE_DWORD);
    as.SRAIW(sext, eax, 31);
    rec.setRefGPR(X86_REF_RDX, X86_SIZE_DWORD, sext);
}

FAST_HANDLE(CQO) {
    biscuit::GPR sext = rec.scratch();
    biscuit::GPR rax = rec.getRefGPR(X86_REF_RAX, X86_SIZE_QWORD);
    as.SRAI(sext, rax, 63);
    rec.setRefGPR(X86_REF_RDX, X86_SIZE_QWORD, sext);
}

void JCC(Recompiler& rec, const HandlerMetadata& meta, Assembler& as, ZydisDecodedInstruction& instruction, ZydisDecodedOperand* operands,
         biscuit::GPR cond) {
    u64 immediate = rec.sextImmediate(rec.getImmediate(&operands[0]), operands[0].imm.size);
    HostAddress address_false = meta.rip.add(instruction.length);
    HostAddress address_true = address_false.add(immediate);

    biscuit::GPR rip_true = rec.scratch();
    biscuit::GPR rip_false = rec.scratch();

    as.LI(rip_false, address_false.toGuest().raw());
    as.LI(rip_true, address_true.toGuest().raw());

    rec.writebackDirtyState();
    rec.invalidStateUntilJump();
    rec.jumpAndLinkConditional(cond, rip_true, rip_false, address_true, address_false);
    rec.stopCompiling();
}

FAST_HANDLE(JO) {
    JCC(rec, meta, as, instruction, operands, rec.getCond(instruction.opcode & 0xF));
}

FAST_HANDLE(JNO) {
    JCC(rec, meta, as, instruction, operands, rec.getCond(instruction.opcode & 0xF));
}

FAST_HANDLE(JB) {
    JCC(rec, meta, as, instruction, operands, rec.getCond(instruction.opcode & 0xF));
}

FAST_HANDLE(JNB) {
    JCC(rec, meta, as, instruction, operands, rec.getCond(instruction.opcode & 0xF));
}

FAST_HANDLE(JZ) {
    JCC(rec, meta, as, instruction, operands, rec.getCond(instruction.opcode & 0xF));
}

FAST_HANDLE(JNZ) {
    JCC(rec, meta, as, instruction, operands, rec.getCond(instruction.opcode & 0xF));
}

FAST_HANDLE(JBE) {
    JCC(rec, meta, as, instruction, operands, rec.getCond(instruction.opcode & 0xF));
}

FAST_HANDLE(JNBE) {
    JCC(rec, meta, as, instruction, operands, rec.getCond(instruction.opcode & 0xF));
}

FAST_HANDLE(JP) {
    JCC(rec, meta, as, instruction, operands, rec.getCond(instruction.opcode & 0xF));
}

FAST_HANDLE(JNP) {
    JCC(rec, meta, as, instruction, operands, rec.getCond(instruction.opcode & 0xF));
}

FAST_HANDLE(JS) {
    JCC(rec, meta, as, instruction, operands, rec.getCond(instruction.opcode & 0xF));
}

FAST_HANDLE(JNS) {
    JCC(rec, meta, as, instruction, operands, rec.getCond(instruction.opcode & 0xF));
}

FAST_HANDLE(JL) {
    JCC(rec, meta, as, instruction, operands, rec.getCond(instruction.opcode & 0xF));
}

FAST_HANDLE(JNL) {
    JCC(rec, meta, as, instruction, operands, rec.getCond(instruction.opcode & 0xF));
}

FAST_HANDLE(JLE) {
    JCC(rec, meta, as, instruction, operands, rec.getCond(instruction.opcode & 0xF));
}

FAST_HANDLE(JNLE) {
    JCC(rec, meta, as, instruction, operands, rec.getCond(instruction.opcode & 0xF));
}

FAST_HANDLE(JRCXZ) {
    biscuit::GPR is_zero = rec.scratch();
    biscuit::GPR rcx = rec.getRefGPR(X86_REF_RCX, X86_SIZE_QWORD);
    as.SEQZ(is_zero, rcx);
    JCC(rec, meta, as, instruction, operands, is_zero);
}

FAST_HANDLE(JECXZ) {
    biscuit::GPR is_zero = rec.scratch();
    biscuit::GPR rcx = rec.getRefGPR(X86_REF_RCX, X86_SIZE_DWORD);
    as.SEQZ(is_zero, rcx);
    JCC(rec, meta, as, instruction, operands, is_zero);
}

FAST_HANDLE(JCXZ) {
    biscuit::GPR is_zero = rec.scratch();
    biscuit::GPR rcx = rec.getRefGPR(X86_REF_RCX, X86_SIZE_WORD);
    as.SEQZ(is_zero, rcx);
    JCC(rec, meta, as, instruction, operands, is_zero);
}

FAST_HANDLE(LOOP) {
    x86_size_e address_size = rec.zydisToSize(instruction.address_width);
    biscuit::GPR is_not_zero = rec.scratch();
    biscuit::GPR rcx = rec.getRefGPR(X86_REF_RCX, address_size);
    as.ADDI(rcx, rcx, -1);
    as.SNEZ(is_not_zero, rcx);
    rec.setRefGPR(X86_REF_RCX, address_size, rcx);
    JCC(rec, meta, as, instruction, operands, is_not_zero);
}

FAST_HANDLE(LOOPE) {
    biscuit::GPR zf = rec.flag(X86_REF_ZF);
    x86_size_e address_size = rec.zydisToSize(instruction.address_width);
    biscuit::GPR is_not_zero = rec.scratch();
    biscuit::GPR rcx = rec.getRefGPR(X86_REF_RCX, address_size);
    as.ADDI(rcx, rcx, -1);
    as.SNEZ(is_not_zero, rcx);
    as.AND(is_not_zero, is_not_zero, zf);
    rec.setRefGPR(X86_REF_RCX, address_size, rcx);
    JCC(rec, meta, as, instruction, operands, is_not_zero);
}

FAST_HANDLE(LOOPNE) {
    biscuit::GPR zf = rec.flag(X86_REF_ZF);
    biscuit::GPR not_zf = rec.scratch();
    x86_size_e address_size = rec.zydisToSize(instruction.address_width);
    biscuit::GPR is_not_zero = rec.scratch();
    biscuit::GPR rcx = rec.getRefGPR(X86_REF_RCX, address_size);
    as.ADDI(rcx, rcx, -1);
    as.SNEZ(is_not_zero, rcx);
    as.XORI(not_zf, zf, 1);
    as.AND(is_not_zero, is_not_zero, not_zf);
    rec.setRefGPR(X86_REF_RCX, address_size, rcx);
    JCC(rec, meta, as, instruction, operands, is_not_zero);
}

void CMOV(Recompiler& rec, const HandlerMetadata& meta, Assembler& as, ZydisDecodedInstruction& instruction, ZydisDecodedOperand* operands,
          biscuit::GPR cond) {
    biscuit::GPR dst = rec.getOperandGPR(&operands[0]);
    biscuit::GPR src = rec.getOperandGPR(&operands[1]);
    biscuit::GPR result = rec.scratch();

    as.MV(result, dst);
    if (Extensions::Xtheadcondmov) {
        as.TH_MVNEZ(result, src, cond);
    } else if (Extensions::Zicond) {
        biscuit::GPR tmp = rec.scratch();
        as.CZERO_NEZ(tmp, result, cond);
        as.CZERO_EQZ(result, src, cond);
        as.OR(result, result, tmp);
    } else {
        Label false_label;
        as.BEQZ(cond, &false_label);
        as.MV(result, src);
        as.Bind(&false_label);
    }

    rec.setOperandGPR(&operands[0], result);
}

FAST_HANDLE(CMOVO) {
    CMOV(rec, meta, as, instruction, operands, rec.getCond(instruction.opcode & 0xF));
}

FAST_HANDLE(CMOVNO) {
    CMOV(rec, meta, as, instruction, operands, rec.getCond(instruction.opcode & 0xF));
}

FAST_HANDLE(CMOVB) {
    CMOV(rec, meta, as, instruction, operands, rec.getCond(instruction.opcode & 0xF));
}

FAST_HANDLE(CMOVNB) {
    CMOV(rec, meta, as, instruction, operands, rec.getCond(instruction.opcode & 0xF));
}

FAST_HANDLE(CMOVZ) {
    CMOV(rec, meta, as, instruction, operands, rec.getCond(instruction.opcode & 0xF));
}

FAST_HANDLE(CMOVNZ) {
    CMOV(rec, meta, as, instruction, operands, rec.getCond(instruction.opcode & 0xF));
}

FAST_HANDLE(CMOVBE) {
    CMOV(rec, meta, as, instruction, operands, rec.getCond(instruction.opcode & 0xF));
}

FAST_HANDLE(CMOVNBE) {
    CMOV(rec, meta, as, instruction, operands, rec.getCond(instruction.opcode & 0xF));
}

FAST_HANDLE(CMOVP) {
    CMOV(rec, meta, as, instruction, operands, rec.getCond(instruction.opcode & 0xF));
}

FAST_HANDLE(CMOVNP) {
    CMOV(rec, meta, as, instruction, operands, rec.getCond(instruction.opcode & 0xF));
}

FAST_HANDLE(CMOVS) {
    CMOV(rec, meta, as, instruction, operands, rec.getCond(instruction.opcode & 0xF));
}

FAST_HANDLE(CMOVNS) {
    CMOV(rec, meta, as, instruction, operands, rec.getCond(instruction.opcode & 0xF));
}

FAST_HANDLE(CMOVL) {
    CMOV(rec, meta, as, instruction, operands, rec.getCond(instruction.opcode & 0xF));
}

FAST_HANDLE(CMOVNL) {
    CMOV(rec, meta, as, instruction, operands, rec.getCond(instruction.opcode & 0xF));
}

FAST_HANDLE(CMOVLE) {
    CMOV(rec, meta, as, instruction, operands, rec.getCond(instruction.opcode & 0xF));
}

FAST_HANDLE(CMOVNLE) {
    CMOV(rec, meta, as, instruction, operands, rec.getCond(instruction.opcode & 0xF));
}

FAST_HANDLE(MOVSXD) {
    x86_size_e size = rec.getOperandSize(&operands[1]);
    biscuit::GPR src = rec.getOperandGPR(&operands[1]);

    if (size == X86_SIZE_DWORD) {
        biscuit::GPR dst = rec.allocatedGPR(rec.zydisToRef(operands[0].reg.value));
        as.ADDIW(dst, src, 0);
        rec.setOperandGPR(&operands[0], dst);
    } else {
        UNREACHABLE(); // possible but why?
    }
}

FAST_HANDLE(IMUL) {
    x86_size_e size = rec.getOperandSize(&operands[0]);
    u8 opcount = instruction.operand_count_visible;
    if (opcount == 1) {
        biscuit::GPR src = rec.getOperandGPR(&operands[0]);
        switch (size) {
        case X86_SIZE_BYTE:
        case X86_SIZE_BYTE_HIGH: {
            biscuit::GPR result = rec.scratch();
            biscuit::GPR sext = rec.scratch();
            biscuit::GPR al = rec.getRefGPR(X86_REF_RAX, X86_SIZE_BYTE);
            rec.sextb(sext, al);
            rec.sextb(result, al);
            as.MULW(result, sext, src);
            rec.setRefGPR(X86_REF_RAX, X86_SIZE_WORD, result);

            if (rec.shouldEmitFlag(meta.rip, X86_REF_CF) || rec.shouldEmitFlag(meta.rip, X86_REF_OF)) {
                biscuit::GPR cf = rec.flagW(X86_REF_CF);
                biscuit::GPR of = rec.flagW(X86_REF_OF);
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
            biscuit::GPR ax = rec.getRefGPR(X86_REF_RAX, X86_SIZE_WORD);
            rec.sexth(sext, ax);
            rec.sexth(result, src);
            as.MULW(result, sext, result);
            rec.setRefGPR(X86_REF_RAX, X86_SIZE_WORD, result);

            if (rec.shouldEmitFlag(meta.rip, X86_REF_CF) || rec.shouldEmitFlag(meta.rip, X86_REF_OF)) {
                biscuit::GPR cf = rec.flagW(X86_REF_CF);
                biscuit::GPR of = rec.flagW(X86_REF_OF);

                rec.sexth(cf, result);
                as.XOR(of, cf, result);
                as.SNEZ(of, of);
                as.MV(cf, of);
            }

            as.SRAIW(result, result, 16);
            rec.setRefGPR(X86_REF_RDX, X86_SIZE_WORD, result);
            break;
        }
        case X86_SIZE_DWORD: {
            biscuit::GPR result = rec.scratch();
            biscuit::GPR sext = rec.scratch();
            biscuit::GPR eax = rec.getRefGPR(X86_REF_RAX, X86_SIZE_DWORD);
            as.ADDIW(sext, eax, 0);
            as.ADDIW(result, src, 0);
            as.MUL(result, sext, result);
            rec.setRefGPR(X86_REF_RAX, X86_SIZE_DWORD, result);

            if (rec.shouldEmitFlag(meta.rip, X86_REF_CF) || rec.shouldEmitFlag(meta.rip, X86_REF_OF)) {
                biscuit::GPR cf = rec.flagW(X86_REF_CF);
                biscuit::GPR of = rec.flagW(X86_REF_OF);

                as.ADDIW(cf, result, 0);
                as.XOR(of, cf, result);
                as.SNEZ(of, of);
                as.MV(cf, of);
            }

            as.SRLI(result, result, 32);
            rec.setRefGPR(X86_REF_RDX, X86_SIZE_DWORD, result);
            break;
        }
        case X86_SIZE_QWORD: {
            biscuit::GPR result = rec.scratch();
            biscuit::GPR rax = rec.getRefGPR(X86_REF_RAX, X86_SIZE_QWORD);
            as.MULH(result, rax, src);
            as.MUL(rax, rax, src);
            rec.setRefGPR(X86_REF_RAX, X86_SIZE_QWORD, rax);
            rec.setRefGPR(X86_REF_RDX, X86_SIZE_QWORD, result);

            if (rec.shouldEmitFlag(meta.rip, X86_REF_CF) || rec.shouldEmitFlag(meta.rip, X86_REF_OF)) {
                biscuit::GPR cf = rec.flagW(X86_REF_CF);
                biscuit::GPR of = rec.flagW(X86_REF_OF);

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

        rec.setFlagUndefined(X86_REF_ZF);
        rec.setFlagUndefined(X86_REF_SF);
    } else if (opcount == 2 || opcount == 3) {
        biscuit::GPR dst, src1, src2;

        if (opcount == 2) {
            dst = rec.getOperandGPR(&operands[0]);
            src1 = dst;
            src2 = rec.getOperandGPR(&operands[1]);
        } else {
            dst = rec.getOperandGPR(&operands[0]);
            src1 = rec.getOperandGPR(&operands[1]);
            src2 = rec.getOperandGPR(&operands[2]);
        }

        switch (size) {
        case X86_SIZE_WORD: {
            biscuit::GPR result = rec.scratch();
            biscuit::GPR dst_sext = rec.scratch();
            rec.sexth(dst_sext, src1);
            rec.sexth(result, src2);
            as.MULW(result, result, dst_sext);
            rec.setOperandGPR(&operands[0], result);

            if (rec.shouldEmitFlag(meta.rip, X86_REF_CF) || rec.shouldEmitFlag(meta.rip, X86_REF_OF)) {
                biscuit::GPR cf = rec.flagW(X86_REF_CF);
                biscuit::GPR of = rec.flagW(X86_REF_OF);
                rec.sexth(cf, result);
                as.XOR(of, cf, result);
                as.SNEZ(of, of);
                as.MV(cf, of);
            }
            break;
        }
        case X86_SIZE_DWORD: {
            biscuit::GPR result = rec.scratch();
            biscuit::GPR dst_sext = rec.scratch();
            as.ADDIW(dst_sext, src1, 0);
            as.ADDIW(result, src2, 0);
            as.MUL(result, result, dst_sext);
            rec.setOperandGPR(&operands[0], result);

            if (rec.shouldEmitFlag(meta.rip, X86_REF_CF) || rec.shouldEmitFlag(meta.rip, X86_REF_OF)) {
                biscuit::GPR cf = rec.flagW(X86_REF_CF);
                biscuit::GPR of = rec.flagW(X86_REF_OF);
                as.ADDIW(cf, result, 0);
                as.XOR(of, cf, result);
                as.SNEZ(of, of);
                as.MV(cf, of);
            }
            break;
        }
        case X86_SIZE_QWORD: {
            biscuit::GPR result = rec.scratch();
            biscuit::GPR result_low = rec.scratch();
            as.MULH(result, src1, src2);
            as.MUL(result_low, src1, src2);
            rec.setOperandGPR(&operands[0], result_low);

            if (rec.shouldEmitFlag(meta.rip, X86_REF_CF) || rec.shouldEmitFlag(meta.rip, X86_REF_OF)) {
                biscuit::GPR cf = rec.flagW(X86_REF_CF);
                biscuit::GPR of = rec.flagW(X86_REF_OF);
                as.SRAI(cf, result_low, 63);
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

        rec.setFlagUndefined(X86_REF_ZF);
        rec.setFlagUndefined(X86_REF_SF);
    } else {
        UNREACHABLE();
    }
}

FAST_HANDLE(MUL) {
    x86_size_e size = rec.getOperandSize(&operands[0]);
    biscuit::GPR src = rec.getOperandGPR(&operands[0]);
    switch (size) {
    case X86_SIZE_BYTE:
    case X86_SIZE_BYTE_HIGH: {
        biscuit::GPR result = rec.scratch();
        biscuit::GPR al = rec.getRefGPR(X86_REF_RAX, X86_SIZE_BYTE);
        as.MULW(result, al, src);
        rec.setRefGPR(X86_REF_RAX, X86_SIZE_WORD, result);

        if (rec.shouldEmitFlag(meta.rip, X86_REF_CF) || rec.shouldEmitFlag(meta.rip, X86_REF_OF)) {
            biscuit::GPR cf = rec.flagW(X86_REF_CF);
            biscuit::GPR of = rec.flagW(X86_REF_OF);
            // 8 * 8 bit can only be 16 bit so we don't need to zero extend
            as.SRLI(cf, result, 8);
            as.SNEZ(cf, cf);
            as.MV(of, cf);
        }
        break;
    }
    case X86_SIZE_WORD: {
        biscuit::GPR result = rec.scratch();
        biscuit::GPR ax = rec.getRefGPR(X86_REF_RAX, X86_SIZE_WORD);
        as.MULW(result, ax, src);
        rec.setRefGPR(X86_REF_RAX, X86_SIZE_WORD, result);

        as.SRLIW(result, result, 16);

        if (rec.shouldEmitFlag(meta.rip, X86_REF_CF) || rec.shouldEmitFlag(meta.rip, X86_REF_OF)) {
            biscuit::GPR cf = rec.flagW(X86_REF_CF);
            biscuit::GPR of = rec.flagW(X86_REF_OF);
            // Should be already zexted due to srliw
            as.SNEZ(cf, result);
            as.MV(of, cf);
        }

        rec.setRefGPR(X86_REF_RDX, X86_SIZE_WORD, result);
        break;
    }
    case X86_SIZE_DWORD: {
        biscuit::GPR result = rec.scratch();
        biscuit::GPR eax = rec.getRefGPR(X86_REF_RAX, X86_SIZE_DWORD);
        as.MUL(result, eax, src);
        rec.setRefGPR(X86_REF_RAX, X86_SIZE_DWORD, result);
        as.SRLI(result, result, 32);

        if (rec.shouldEmitFlag(meta.rip, X86_REF_CF) || rec.shouldEmitFlag(meta.rip, X86_REF_OF)) {
            biscuit::GPR cf = rec.flagW(X86_REF_CF);
            biscuit::GPR of = rec.flagW(X86_REF_OF);

            as.SNEZ(cf, result);
            as.MV(of, cf);
        }

        rec.setRefGPR(X86_REF_RDX, X86_SIZE_DWORD, result);
        break;
    }
    case X86_SIZE_QWORD: {
        biscuit::GPR result = rec.scratch();
        biscuit::GPR rax = rec.getRefGPR(X86_REF_RAX, X86_SIZE_QWORD);
        as.MULHU(result, rax, src);
        as.MUL(rax, rax, src);
        rec.setRefGPR(X86_REF_RAX, X86_SIZE_QWORD, rax);
        rec.setRefGPR(X86_REF_RDX, X86_SIZE_QWORD, result);

        if (rec.shouldEmitFlag(meta.rip, X86_REF_CF) || rec.shouldEmitFlag(meta.rip, X86_REF_OF)) {
            biscuit::GPR cf = rec.flagW(X86_REF_CF);
            biscuit::GPR of = rec.flagW(X86_REF_OF);

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

    rec.setFlagUndefined(X86_REF_ZF);
    rec.setFlagUndefined(X86_REF_SF);
}

void PUNPCKH(Recompiler& rec, const HandlerMetadata& meta, Assembler& as, ZydisDecodedInstruction& instruction, ZydisDecodedOperand* operands,
             SEW sew, u8 vlen) {
    // Like PUNPCKL but we add a number to iota to pick the high elements
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

    biscuit::Vec dst = rec.getOperandVec(&operands[0]);
    biscuit::Vec src = rec.getOperandVec(&operands[1]);

    // Pick even scratch registers for the widening add (can't use MF2, ruins 128 VLEN)
    biscuit::Vec temp1 = v22;
    biscuit::Vec temp2 = v24;
    biscuit::Vec dst_down = v26;
    biscuit::Vec src_down = v27;

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

    rec.setOperandVec(&operands[0], dst);
}

FAST_HANDLE(PUNPCKLBW) {
    biscuit::Vec dst = rec.getOperandVec(&operands[0]);
    biscuit::Vec src = rec.getOperandVec(&operands[1]);
    biscuit::Vec temp1 = rec.scratchVec();
    biscuit::Vec temp2 = rec.scratchVec();

    rec.setVectorState(SEW::E8, 16, LMUL::MF2);
    as.VWADDU(temp1, dst, x0);
    as.VWADDU(temp2, src, x0);
    rec.setVectorState(SEW::E64, 2);
    as.VSLL(temp2, temp2, 8);
    as.VOR(dst, temp1, temp2);

    rec.setOperandVec(&operands[0], dst);
}

FAST_HANDLE(PUNPCKLWD) {
    biscuit::Vec dst = rec.getOperandVec(&operands[0]);
    biscuit::Vec src = rec.getOperandVec(&operands[1]);
    biscuit::Vec temp1 = rec.scratchVec();
    biscuit::Vec temp2 = rec.scratchVec();

    rec.setVectorState(SEW::E16, 8, LMUL::MF2);
    as.VWADDU(temp1, dst, x0);
    as.VWADDU(temp2, src, x0);
    rec.setVectorState(SEW::E64, 2);
    as.VSLL(temp2, temp2, 16);
    as.VOR(dst, temp1, temp2);

    rec.setOperandVec(&operands[0], dst);
}

FAST_HANDLE(PUNPCKLDQ) {
    biscuit::GPR shift = rec.scratch();
    biscuit::Vec dst = rec.getOperandVec(&operands[0]);
    biscuit::Vec src = rec.getOperandVec(&operands[1]);
    biscuit::Vec temp1 = rec.scratchVec();
    biscuit::Vec temp2 = rec.scratchVec();

    as.LI(shift, 32);
    rec.setVectorState(SEW::E32, 4, LMUL::MF2);
    as.VWADDU(temp1, dst, x0);
    as.VWADDU(temp2, src, x0);
    rec.setVectorState(SEW::E64, 2);
    as.VSLL(temp2, temp2, shift);
    as.VOR(dst, temp1, temp2);

    rec.setOperandVec(&operands[0], dst);
}

FAST_HANDLE(PUNPCKLQDQ) {
    biscuit::Vec dst = rec.getOperandVec(&operands[0]);
    biscuit::Vec src = rec.getOperandVec(&operands[1]);

    rec.setVectorState(SEW::E64, 2);
    if (dst == src) { // VSLIDEUP dst/src overlap limitations
        src = rec.scratchVec();
        as.VMV(src, dst);
    }

    as.VSLIDEUP(dst, src, 1);

    rec.setOperandVec(operands, dst);
}

FAST_HANDLE(PUNPCKHBW) {
    PUNPCKH(rec, meta, as, instruction, operands, SEW::E8, 16);
}

FAST_HANDLE(PUNPCKHWD) {
    PUNPCKH(rec, meta, as, instruction, operands, SEW::E16, 8);
}

FAST_HANDLE(PUNPCKHDQ) {
    PUNPCKH(rec, meta, as, instruction, operands, SEW::E32, 4);
}

FAST_HANDLE(PUNPCKHQDQ) {
    biscuit::Vec temp = rec.scratchVec();
    biscuit::Vec dst = rec.getOperandVec(&operands[0]);
    biscuit::Vec src = rec.getOperandVec(&operands[1]);

    rec.setVectorState(SEW::E64, 2);
    as.VMV(v0, 0b10);
    as.VSLIDE1DOWN(temp, dst, x0);
    as.VMERGE(dst, temp, src);

    rec.setOperandVec(&operands[0], dst);
}

FAST_HANDLE(UNPCKLPS) { // Fuzzed
    biscuit::Vec scratch = rec.scratchVec();
    biscuit::Vec iota = rec.scratchVec();
    biscuit::Vec src1 = rec.getOperandVec(&operands[0]);
    biscuit::Vec src2 = rec.getOperandVec(&operands[1]);

    rec.setVectorState(SEW::E32, 4);
    as.VMV(scratch, 0);
    as.VMV(v0, 0b0101);
    as.VIOTA(iota, v0);
    as.VRGATHER(scratch, src1, iota, VecMask::Yes);
    as.VMV(v0, 0b1010);
    as.VIOTA(iota, v0);
    as.VRGATHER(scratch, src2, iota, VecMask::Yes);

    rec.setOperandVec(&operands[0], scratch);
}

FAST_HANDLE(UNPCKHPS) { // Fuzzed
    biscuit::Vec scratch = rec.scratchVec();
    biscuit::Vec iota = rec.scratchVec();
    biscuit::Vec src1 = rec.getOperandVec(&operands[0]);
    biscuit::Vec src2 = rec.getOperandVec(&operands[1]);

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

    rec.setOperandVec(&operands[0], scratch);
}

FAST_HANDLE(UNPCKLPD) { // Fuzzed
    biscuit::Vec scratch = rec.scratchVec();
    biscuit::Vec result = rec.scratchVec();
    biscuit::Vec src1 = rec.getOperandVec(&operands[0]);
    biscuit::Vec src2 = rec.getOperandVec(&operands[1]);

    rec.setVectorState(SEW::E64, 2);
    as.VSLIDEUP(scratch, src2, 1);
    as.VMV(v0, 0b10);
    as.VMERGE(result, src1, scratch);

    rec.setOperandVec(&operands[0], result);
}

FAST_HANDLE(UNPCKHPD) { // Fuzzed
    biscuit::Vec scratch = rec.scratchVec();
    biscuit::Vec result = rec.scratchVec();
    biscuit::Vec src1 = rec.getOperandVec(&operands[0]);
    biscuit::Vec src2 = rec.getOperandVec(&operands[1]);

    rec.setVectorState(SEW::E64, 2);
    as.VSLIDEDOWN(scratch, src1, 1);
    as.VMV(v0, 0b10);
    as.VMERGE(result, scratch, src2);

    rec.setOperandVec(&operands[0], result);
}

FAST_HANDLE(VECTOR_MOV) {
    if (operands[1].type == ZYDIS_OPERAND_TYPE_REGISTER) {
        if (operands[0].type == ZYDIS_OPERAND_TYPE_REGISTER && operands[0].reg.value == operands[1].reg.value) {
            WARN("vmov from and to same reg?");
        }

        biscuit::Vec src = rec.getOperandVec(&operands[1]);
        rec.setOperandVec(&operands[0], src);
    } else {
        // Operand 1 is memory, so operand 0 must be register
        ASSERT(operands[0].type == ZYDIS_OPERAND_TYPE_REGISTER);
        // Load directly to register to avoid a vmv1r
        biscuit::Vec dst = rec.allocatedVec(rec.zydisToRef(operands[0].reg.value));
        int size = operands[0].size;
        ASSERT(operands[0].size == operands[1].size);
        ASSERT(operands[0].size > 64);
        biscuit::GPR address = rec.lea(&operands[1]);
        rec.readMemory(dst, address, size);
        rec.setOperandVec(&operands[0], dst);
    }
}

FAST_HANDLE(MOVAPD) {
    fast_VECTOR_MOV(rec, meta, as, instruction, operands);
}

FAST_HANDLE(MOVAPS) {
    fast_VECTOR_MOV(rec, meta, as, instruction, operands);
}

FAST_HANDLE(MOVUPD) {
    fast_VECTOR_MOV(rec, meta, as, instruction, operands);
}

FAST_HANDLE(MOVUPS) {
    fast_VECTOR_MOV(rec, meta, as, instruction, operands);
}

FAST_HANDLE(MOVDQA) {
    fast_VECTOR_MOV(rec, meta, as, instruction, operands);
}

FAST_HANDLE(MOVDQU) {
    fast_VECTOR_MOV(rec, meta, as, instruction, operands);
}

FAST_HANDLE(RDTSC) {
    biscuit::GPR tsc = rec.scratch();
    as.RDTIME(tsc);
    rec.setRefGPR(X86_REF_RAX, X86_SIZE_DWORD, tsc);
    as.SRLI(tsc, tsc, 32);
    rec.setRefGPR(X86_REF_RDX, X86_SIZE_QWORD, tsc);
}

FAST_HANDLE(CPUID) {
    rec.writebackDirtyState();
    rec.invalidStateUntilJump();

    biscuit::GPR address = rec.scratch();
    as.LI(address, (u64)&felix86_cpuid);
    as.MV(a0, rec.threadStatePointer());
    as.JALR(address);
    rec.restoreRoundingMode();
}

FAST_HANDLE(SYSCALL) {
    if (!g_config.strace && g_config.inline_syscalls) {
        bool inlined = rec.tryInlineSyscall();
        if (inlined) {
            return;
        }
    }

    biscuit::GPR rcx = rec.allocatedGPR(X86_REF_RCX);
    as.LI(rcx, meta.rip.add(instruction.length).toGuest().raw());
    rec.setRefGPR(X86_REF_RCX, X86_SIZE_QWORD, rcx);

    // Normally the syscall instruction also writes the flags to R11 but we don't need them in our syscall handler

    rec.writebackDirtyState();
    rec.invalidStateUntilJump();
    rec.call((u64)rec.getSyscallThunk());
    rec.restoreRoundingMode();
}

FAST_HANDLE(INT) {
    ASSERT(operands[0].type == ZYDIS_OPERAND_TYPE_IMMEDIATE);
    ASSERT(operands[0].imm.value.u == 0x80);

    rec.writebackDirtyState();
    rec.invalidStateUntilJump();
    rec.call((u64)rec.getSyscallThunk());
    rec.restoreRoundingMode();
}

FAST_HANDLE(MOVZX) {
    biscuit::GPR result = rec.scratch();
    biscuit::GPR src = rec.getOperandGPR(&operands[1]);
    x86_size_e size = rec.getOperandSize(&operands[1]);
    rec.zext(result, src, size);
    rec.setOperandGPR(&operands[0], result);
}

FAST_HANDLE(PXOR) {
    biscuit::Vec dst = rec.getOperandVec(&operands[0]);
    biscuit::Vec src = rec.getOperandVec(&operands[1]);
    rec.setVectorState(SEW::E64, 2);
    as.VXOR(dst, dst, src);
    rec.setOperandVec(&operands[0], dst);
}

FAST_HANDLE(MOVNTDQ) {
    fast_VECTOR_MOV(rec, meta, as, instruction, operands);
}

FAST_HANDLE(MOVNTDQA) {
    fast_VECTOR_MOV(rec, meta, as, instruction, operands);
}

FAST_HANDLE(MOVNTI) {
    biscuit::GPR src = rec.getOperandGPR(&operands[1]);
    rec.setOperandGPR(&operands[0], src);
}

FAST_HANDLE(MOVNTPD) {
    fast_VECTOR_MOV(rec, meta, as, instruction, operands);
}

FAST_HANDLE(MOVNTPS) {
    fast_VECTOR_MOV(rec, meta, as, instruction, operands);
}

FAST_HANDLE(PAND) {
    biscuit::Vec dst = rec.getOperandVec(&operands[0]);
    biscuit::Vec src = rec.getOperandVec(&operands[1]);
    rec.setVectorState(SEW::E64, 2);
    as.VAND(dst, dst, src);
    rec.setOperandVec(&operands[0], dst);
}

FAST_HANDLE(POR) {
    biscuit::Vec dst = rec.getOperandVec(&operands[0]);
    biscuit::Vec src = rec.getOperandVec(&operands[1]);
    rec.setVectorState(SEW::E64, 2);
    as.VOR(dst, dst, src);
    rec.setOperandVec(&operands[0], dst);
}

FAST_HANDLE(PANDN) { // Fuzzed
    biscuit::Vec dst_not = rec.scratchVec();
    biscuit::Vec dst = rec.getOperandVec(&operands[0]);
    biscuit::Vec src = rec.getOperandVec(&operands[1]);
    rec.setVectorState(SEW::E64, 2);
    if (Extensions::Zvbb) {
        WARN_ONCE("PANDN + Zvbb is untested, please run tests and report results");
        as.VANDN(dst, src, dst);
    } else {
        as.VXOR(dst_not, dst, -1);
        as.VAND(dst, dst_not, src);
    }
    rec.setOperandVec(&operands[0], dst);
}

FAST_HANDLE(ANDPS) {
    fast_PAND(rec, meta, as, instruction, operands);
}

FAST_HANDLE(ANDPD) {
    fast_PAND(rec, meta, as, instruction, operands);
}

FAST_HANDLE(ORPS) {
    fast_POR(rec, meta, as, instruction, operands);
}

FAST_HANDLE(ORPD) {
    fast_POR(rec, meta, as, instruction, operands);
}

FAST_HANDLE(XORPS) {
    fast_PXOR(rec, meta, as, instruction, operands);
}

FAST_HANDLE(XORPD) {
    fast_PXOR(rec, meta, as, instruction, operands);
}

FAST_HANDLE(ANDNPS) { // Fuzzed
    fast_PANDN(rec, meta, as, instruction, operands);
}

FAST_HANDLE(ANDNPD) { // Fuzzed
    fast_PANDN(rec, meta, as, instruction, operands);
}

void PADD(Recompiler& rec, const HandlerMetadata& meta, Assembler& as, ZydisDecodedInstruction& instruction, ZydisDecodedOperand* operands, SEW sew,
          u8 vlen) {
    biscuit::Vec dst = rec.getOperandVec(&operands[0]);
    biscuit::Vec src = rec.getOperandVec(&operands[1]);
    rec.setVectorState(sew, vlen);
    as.VADD(dst, dst, src);
    rec.setOperandVec(&operands[0], dst);
}

void PADDS(Recompiler& rec, const HandlerMetadata& meta, Assembler& as, ZydisDecodedInstruction& instruction, ZydisDecodedOperand* operands, SEW sew,
           u8 vlen) {
    biscuit::Vec dst = rec.getOperandVec(&operands[0]);
    biscuit::Vec src = rec.getOperandVec(&operands[1]);
    rec.setVectorState(sew, vlen);
    as.VSADD(dst, dst, src);
    rec.setOperandVec(&operands[0], dst);
}

void PADDSU(Recompiler& rec, const HandlerMetadata& meta, Assembler& as, ZydisDecodedInstruction& instruction, ZydisDecodedOperand* operands, SEW sew,
            u8 vlen) {
    biscuit::Vec dst = rec.getOperandVec(&operands[0]);
    biscuit::Vec src = rec.getOperandVec(&operands[1]);
    rec.setVectorState(sew, vlen);
    as.VSADDU(dst, dst, src);
    rec.setOperandVec(&operands[0], dst);
}

void PSUBS(Recompiler& rec, const HandlerMetadata& meta, Assembler& as, ZydisDecodedInstruction& instruction, ZydisDecodedOperand* operands, SEW sew,
           u8 vlen) {
    biscuit::Vec dst = rec.getOperandVec(&operands[0]);
    biscuit::Vec src = rec.getOperandVec(&operands[1]);
    rec.setVectorState(sew, vlen);
    as.VSSUB(dst, dst, src);
    rec.setOperandVec(&operands[0], dst);
}

void PSUBSU(Recompiler& rec, const HandlerMetadata& meta, Assembler& as, ZydisDecodedInstruction& instruction, ZydisDecodedOperand* operands, SEW sew,
            u8 vlen) {
    biscuit::Vec dst = rec.getOperandVec(&operands[0]);
    biscuit::Vec src = rec.getOperandVec(&operands[1]);
    rec.setVectorState(sew, vlen);
    as.VSSUBU(dst, dst, src);
    rec.setOperandVec(&operands[0], dst);
}

void PSUB(Recompiler& rec, const HandlerMetadata& meta, Assembler& as, ZydisDecodedInstruction& instruction, ZydisDecodedOperand* operands, SEW sew,
          u8 vlen) {
    biscuit::Vec dst = rec.getOperandVec(&operands[0]);
    biscuit::Vec src = rec.getOperandVec(&operands[1]);
    rec.setVectorState(sew, vlen);
    as.VSUB(dst, dst, src);
    rec.setOperandVec(&operands[0], dst);
}

FAST_HANDLE(PADDB) {
    PADD(rec, meta, as, instruction, operands, SEW::E8, 16);
}

FAST_HANDLE(PADDW) {
    PADD(rec, meta, as, instruction, operands, SEW::E16, 8);
}

FAST_HANDLE(PADDD) {
    PADD(rec, meta, as, instruction, operands, SEW::E32, 4);
}

FAST_HANDLE(PADDQ) {
    PADD(rec, meta, as, instruction, operands, SEW::E64, 2);
}

FAST_HANDLE(PADDSB) {
    PADDS(rec, meta, as, instruction, operands, SEW::E8, 16);
}

FAST_HANDLE(PADDSW) {
    PADDS(rec, meta, as, instruction, operands, SEW::E16, 8);
}

FAST_HANDLE(PSUBSB) {
    PSUBS(rec, meta, as, instruction, operands, SEW::E8, 16);
}

FAST_HANDLE(PSUBSW) {
    PSUBS(rec, meta, as, instruction, operands, SEW::E16, 8);
}

FAST_HANDLE(PADDUSB) {
    PADDSU(rec, meta, as, instruction, operands, SEW::E8, 16);
}

FAST_HANDLE(PADDUSW) { // Fuzzed
    PADDSU(rec, meta, as, instruction, operands, SEW::E16, 8);
}

FAST_HANDLE(PSUBUSB) {
    PSUBSU(rec, meta, as, instruction, operands, SEW::E8, 16);
}

FAST_HANDLE(PSUBUSW) {
    PSUBSU(rec, meta, as, instruction, operands, SEW::E16, 8);
}

FAST_HANDLE(PSUBB) {
    PSUB(rec, meta, as, instruction, operands, SEW::E8, 16);
}

FAST_HANDLE(PSUBW) {
    PSUB(rec, meta, as, instruction, operands, SEW::E16, 8);
}

FAST_HANDLE(PSUBD) {
    PSUB(rec, meta, as, instruction, operands, SEW::E32, 4);
}

FAST_HANDLE(PSUBQ) {
    PSUB(rec, meta, as, instruction, operands, SEW::E64, 2);
}

FAST_HANDLE(ADDPS) {
    biscuit::Vec dst = rec.getOperandVec(&operands[0]);
    biscuit::Vec src = rec.getOperandVec(&operands[1]);
    rec.setVectorState(SEW::E32, 4);
    as.VFADD(dst, dst, src);
    rec.setOperandVec(&operands[0], dst);
}

FAST_HANDLE(ADDPD) {
    biscuit::Vec dst = rec.getOperandVec(&operands[0]);
    biscuit::Vec src = rec.getOperandVec(&operands[1]);
    rec.setVectorState(SEW::E64, 2);
    as.VFADD(dst, dst, src);
    rec.setOperandVec(&operands[0], dst);
}

FAST_HANDLE(SUBPS) {
    biscuit::Vec dst = rec.getOperandVec(&operands[0]);
    biscuit::Vec src = rec.getOperandVec(&operands[1]);
    rec.setVectorState(SEW::E32, 4);
    as.VFSUB(dst, dst, src);
    rec.setOperandVec(&operands[0], dst);
}

FAST_HANDLE(SUBPD) {
    biscuit::Vec dst = rec.getOperandVec(&operands[0]);
    biscuit::Vec src = rec.getOperandVec(&operands[1]);
    rec.setVectorState(SEW::E64, 2);
    as.VFSUB(dst, dst, src);
    rec.setOperandVec(&operands[0], dst);
}

FAST_HANDLE(MINPS) {
    if (g_config.inaccurate_minmax && !g_paranoid) {
        biscuit::Vec dst = rec.getOperandVec(&operands[0]);
        biscuit::Vec src = rec.getOperandVec(&operands[1]);
        rec.setVectorState(SEW::E32, 4);
        as.VFMIN(dst, dst, src);
        rec.setOperandVec(&operands[0], dst);
        return;
    }

    biscuit::Vec dst = rec.getOperandVec(&operands[0]);
    biscuit::Vec src = rec.getOperandVec(&operands[1]);
    biscuit::Vec nan_mask_1 = rec.scratchVec();
    biscuit::Vec nan_mask_2 = rec.scratchVec();
    biscuit::Vec equal_mask = rec.scratchVec();
    biscuit::Vec zero_mask = rec.scratchVec();
    biscuit::Vec neg_zero_mask = rec.scratchVec();
    rec.setVectorState(SEW::E32, 4);

    // When either operand is NaN, or they are both 0.0 or both are -0.0, the result is the source
    as.VMFNE(nan_mask_1, dst, dst); // When a register isn't equal to itself, that element must be NaN
    as.VMFNE(nan_mask_2, src, src);
    as.VMOR(nan_mask_1, nan_mask_1, nan_mask_2);
    as.FMV_W_X(ft8, x0);                          // 0.0
    as.FSGNJN_S(ft9, ft8, ft8);                   // -0.0
    as.VMFEQ(equal_mask, dst, src);               // Check where they are equal
    as.VMFEQ(zero_mask, dst, ft8);                // Check where dst is 0.0
    as.VMFEQ(neg_zero_mask, dst, ft9);            // Check where dst is -0.0
    as.VMOR(zero_mask, zero_mask, neg_zero_mask); // Either 0.0 or -0.0
    as.VMAND(equal_mask, equal_mask, zero_mask);  // Check where they are both zeroes
    as.VMOR(v0, nan_mask_1, equal_mask);          // Combine the masks

    as.VFMIN(nan_mask_2, dst, src);        // actual max result calculation
    as.VMERGE(zero_mask, nan_mask_2, src); // Where v0 is 1's, use src, otherwise use result of vfmax
    rec.setOperandVec(&operands[0], zero_mask);
}

FAST_HANDLE(MINPD) {
    if (g_config.inaccurate_minmax && !g_paranoid) {
        biscuit::Vec dst = rec.getOperandVec(&operands[0]);
        biscuit::Vec src = rec.getOperandVec(&operands[1]);
        rec.setVectorState(SEW::E64, 2);
        as.VFMIN(dst, dst, src);
        rec.setOperandVec(&operands[0], dst);
        return;
    }

    biscuit::Vec dst = rec.getOperandVec(&operands[0]);
    biscuit::Vec src = rec.getOperandVec(&operands[1]);
    biscuit::Vec nan_mask_1 = rec.scratchVec();
    biscuit::Vec nan_mask_2 = rec.scratchVec();
    biscuit::Vec equal_mask = rec.scratchVec();
    biscuit::Vec zero_mask = rec.scratchVec();
    biscuit::Vec neg_zero_mask = rec.scratchVec();
    rec.setVectorState(SEW::E64, 2);

    // When either operand is NaN, or they are both 0.0 or both are -0.0, the result is the source
    as.VMFNE(nan_mask_1, dst, dst); // When a register isn't equal to itself, that element must be NaN
    as.VMFNE(nan_mask_2, src, src);
    as.VMOR(nan_mask_1, nan_mask_1, nan_mask_2);
    as.FMV_D_X(ft8, x0);                          // 0.0
    as.FSGNJN_D(ft9, ft8, ft8);                   // -0.0
    as.VMFEQ(equal_mask, dst, src);               // Check where they are equal
    as.VMFEQ(zero_mask, dst, ft8);                // Check where dst is 0.0
    as.VMFEQ(neg_zero_mask, dst, ft9);            // Check where dst is -0.0
    as.VMOR(zero_mask, zero_mask, neg_zero_mask); // Either 0.0 or -0.0
    as.VMAND(equal_mask, equal_mask, zero_mask);  // They are both zeroes
    as.VMOR(v0, nan_mask_1, equal_mask);          // Combine the masks

    as.VFMIN(nan_mask_2, dst, src);        // actual max result calculation
    as.VMERGE(zero_mask, nan_mask_2, src); // Where v0 is 1's, use src, otherwise use result of vfmax
    rec.setOperandVec(&operands[0], zero_mask);
}

FAST_HANDLE(PMINUB) {
    biscuit::Vec dst = rec.getOperandVec(&operands[0]);
    biscuit::Vec src = rec.getOperandVec(&operands[1]);
    rec.setVectorState(SEW::E8, 16);
    as.VMINU(dst, dst, src);
    rec.setOperandVec(&operands[0], dst);
}

FAST_HANDLE(PMINUW) {
    biscuit::Vec dst = rec.getOperandVec(&operands[0]);
    biscuit::Vec src = rec.getOperandVec(&operands[1]);
    rec.setVectorState(SEW::E16, 8);
    as.VMINU(dst, dst, src);
    rec.setOperandVec(&operands[0], dst);
}

FAST_HANDLE(PMINUD) {
    biscuit::Vec dst = rec.getOperandVec(&operands[0]);
    biscuit::Vec src = rec.getOperandVec(&operands[1]);
    rec.setVectorState(SEW::E32, 4);
    as.VMINU(dst, dst, src);
    rec.setOperandVec(&operands[0], dst);
}

FAST_HANDLE(PMAXUB) {
    biscuit::Vec dst = rec.getOperandVec(&operands[0]);
    biscuit::Vec src = rec.getOperandVec(&operands[1]);
    rec.setVectorState(SEW::E8, 16);
    as.VMAXU(dst, dst, src);
    rec.setOperandVec(&operands[0], dst);
}

FAST_HANDLE(PMAXUW) {
    biscuit::Vec dst = rec.getOperandVec(&operands[0]);
    biscuit::Vec src = rec.getOperandVec(&operands[1]);
    rec.setVectorState(SEW::E16, 8);
    as.VMAXU(dst, dst, src);
    rec.setOperandVec(&operands[0], dst);
}

FAST_HANDLE(PMAXUD) {
    biscuit::Vec dst = rec.getOperandVec(&operands[0]);
    biscuit::Vec src = rec.getOperandVec(&operands[1]);
    rec.setVectorState(SEW::E32, 4);
    as.VMAXU(dst, dst, src);
    rec.setOperandVec(&operands[0], dst);
}

FAST_HANDLE(PMINSB) {
    biscuit::Vec dst = rec.getOperandVec(&operands[0]);
    biscuit::Vec src = rec.getOperandVec(&operands[1]);
    rec.setVectorState(SEW::E8, 16);
    as.VMIN(dst, dst, src);
    rec.setOperandVec(&operands[0], dst);
}

FAST_HANDLE(PMINSW) {
    biscuit::Vec dst = rec.getOperandVec(&operands[0]);
    biscuit::Vec src = rec.getOperandVec(&operands[1]);
    rec.setVectorState(SEW::E16, 8);
    as.VMIN(dst, dst, src);
    rec.setOperandVec(&operands[0], dst);
}

FAST_HANDLE(PMINSD) {
    biscuit::Vec dst = rec.getOperandVec(&operands[0]);
    biscuit::Vec src = rec.getOperandVec(&operands[1]);
    rec.setVectorState(SEW::E32, 4);
    as.VMIN(dst, dst, src);
    rec.setOperandVec(&operands[0], dst);
}

FAST_HANDLE(PMAXSB) {
    biscuit::Vec dst = rec.getOperandVec(&operands[0]);
    biscuit::Vec src = rec.getOperandVec(&operands[1]);
    rec.setVectorState(SEW::E8, 16);
    as.VMAX(dst, dst, src);
    rec.setOperandVec(&operands[0], dst);
}

FAST_HANDLE(PMAXSW) {
    biscuit::Vec dst = rec.getOperandVec(&operands[0]);
    biscuit::Vec src = rec.getOperandVec(&operands[1]);
    rec.setVectorState(SEW::E16, 8);
    as.VMAX(dst, dst, src);
    rec.setOperandVec(&operands[0], dst);
}

FAST_HANDLE(PMAXSD) {
    biscuit::Vec dst = rec.getOperandVec(&operands[0]);
    biscuit::Vec src = rec.getOperandVec(&operands[1]);
    rec.setVectorState(SEW::E32, 4);
    as.VMAX(dst, dst, src);
    rec.setOperandVec(&operands[0], dst);
}

FAST_HANDLE(PMULHW) { // Fuzzed
    biscuit::Vec dst = rec.getOperandVec(&operands[0]);
    biscuit::Vec src = rec.getOperandVec(&operands[1]);
    rec.setVectorState(SEW::E16, 8);
    as.VMULH(dst, dst, src);
    rec.setOperandVec(&operands[0], dst);
}

FAST_HANDLE(PMULHUW) { // Fuzzed
    biscuit::Vec dst = rec.getOperandVec(&operands[0]);
    biscuit::Vec src = rec.getOperandVec(&operands[1]);
    rec.setVectorState(SEW::E16, 8);
    as.VMULHU(dst, dst, src);
    rec.setOperandVec(&operands[0], dst);
}

FAST_HANDLE(PMULLW) { // Fuzzed
    biscuit::Vec dst = rec.getOperandVec(&operands[0]);
    biscuit::Vec src = rec.getOperandVec(&operands[1]);
    rec.setVectorState(SEW::E16, 8);
    as.VMUL(dst, dst, src);
    rec.setOperandVec(&operands[0], dst);
}

FAST_HANDLE(PMULLD) { // Fuzzed
    biscuit::Vec dst = rec.getOperandVec(&operands[0]);
    biscuit::Vec src = rec.getOperandVec(&operands[1]);
    rec.setVectorState(SEW::E32, 4);
    as.VMUL(dst, dst, src);
    rec.setOperandVec(&operands[0], dst);
}

FAST_HANDLE(PMULUDQ) { // Fuzzed
    biscuit::GPR shift = rec.scratch();
    biscuit::Vec dst = rec.getOperandVec(&operands[0]);
    biscuit::Vec src = rec.getOperandVec(&operands[1]);
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

    rec.setOperandVec(&operands[0], result);
}

FAST_HANDLE(PMULDQ) { // Fuzzed
    biscuit::GPR shift = rec.scratch();
    biscuit::Vec dst = rec.getOperandVec(&operands[0]);
    biscuit::Vec src = rec.getOperandVec(&operands[1]);
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

    rec.setOperandVec(&operands[0], result);
}

FAST_HANDLE(PMADDWD) {
    biscuit::Vec result = rec.scratchVec();
    biscuit::Vec dst_compress = rec.scratchVec();
    biscuit::Vec src_compress = rec.scratchVec();
    biscuit::Vec dst_compress2 = rec.scratchVec();
    biscuit::Vec src_compress2 = rec.scratchVec();
    biscuit::Vec vec_mask = rec.scratchVec();
    biscuit::GPR mask = rec.scratch();
    biscuit::Vec dst = rec.getOperandVec(&operands[0]);
    biscuit::Vec src = rec.getOperandVec(&operands[1]);

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

    rec.setOperandVec(&operands[0], result);
}

FAST_HANDLE(MAXPS) {
    if (g_config.inaccurate_minmax && !g_paranoid) {
        biscuit::Vec dst = rec.getOperandVec(&operands[0]);
        biscuit::Vec src = rec.getOperandVec(&operands[1]);
        rec.setVectorState(SEW::E32, 4);
        as.VFMAX(dst, dst, src);
        rec.setOperandVec(&operands[0], dst);
        return;
    }

    biscuit::Vec dst = rec.getOperandVec(&operands[0]);
    biscuit::Vec src = rec.getOperandVec(&operands[1]);
    biscuit::Vec nan_mask_1 = rec.scratchVec();
    biscuit::Vec nan_mask_2 = rec.scratchVec();
    biscuit::Vec equal_mask = rec.scratchVec();
    biscuit::Vec zero_mask = rec.scratchVec();
    biscuit::Vec neg_zero_mask = rec.scratchVec();
    rec.setVectorState(SEW::E32, 4);

    // When either operand is NaN, or they are both 0.0 or both are -0.0, the result is the source
    as.VMFNE(nan_mask_1, dst, dst); // When a register isn't equal to itself, that element must be NaN
    as.VMFNE(nan_mask_2, src, src);
    as.VMOR(nan_mask_1, nan_mask_1, nan_mask_2);
    as.FMV_W_X(ft8, x0);                          // 0.0
    as.FSGNJN_S(ft9, ft8, ft8);                   // -0.0
    as.VMFEQ(equal_mask, dst, src);               // Check where they are equal
    as.VMFEQ(zero_mask, dst, ft8);                // Check where dst is 0.0
    as.VMFEQ(neg_zero_mask, dst, ft9);            // Check where dst is -0.0
    as.VMOR(zero_mask, zero_mask, neg_zero_mask); // Either 0.0 or -0.0
    as.VMAND(equal_mask, equal_mask, zero_mask);  // Check where they are both zeroes
    as.VMOR(v0, nan_mask_1, equal_mask);          // Combine the masks

    as.VFMAX(nan_mask_2, dst, src);        // actual max result calculation
    as.VMERGE(zero_mask, nan_mask_2, src); // Where v0 is 1's, use src, otherwise use result of vfmax
    rec.setOperandVec(&operands[0], zero_mask);
}

FAST_HANDLE(MAXPD) {
    if (g_config.inaccurate_minmax && !g_paranoid) {
        biscuit::Vec dst = rec.getOperandVec(&operands[0]);
        biscuit::Vec src = rec.getOperandVec(&operands[1]);
        rec.setVectorState(SEW::E64, 2);
        as.VFMAX(dst, dst, src);
        rec.setOperandVec(&operands[0], dst);
        return;
    }

    biscuit::Vec dst = rec.getOperandVec(&operands[0]);
    biscuit::Vec src = rec.getOperandVec(&operands[1]);
    biscuit::Vec nan_mask_1 = rec.scratchVec();
    biscuit::Vec nan_mask_2 = rec.scratchVec();
    biscuit::Vec equal_mask = rec.scratchVec();
    biscuit::Vec zero_mask = rec.scratchVec();
    biscuit::Vec neg_zero_mask = rec.scratchVec();
    rec.setVectorState(SEW::E64, 2);

    // When either operand is NaN, or they are both 0.0 or both are -0.0, the result is the source
    as.VMFNE(nan_mask_1, dst, dst); // When a register isn't equal to itself, that element must be NaN
    as.VMFNE(nan_mask_2, src, src);
    as.VMOR(nan_mask_1, nan_mask_1, nan_mask_2);
    as.FMV_D_X(ft8, x0);                          // 0.0
    as.FSGNJN_D(ft9, ft8, ft8);                   // -0.0
    as.VMFEQ(equal_mask, dst, src);               // Check where they are equal
    as.VMFEQ(zero_mask, dst, ft8);                // Check where dst is 0.0
    as.VMFEQ(neg_zero_mask, dst, ft9);            // Check where dst is -0.0
    as.VMOR(zero_mask, zero_mask, neg_zero_mask); // Either 0.0 or -0.0
    as.VMAND(equal_mask, equal_mask, zero_mask);  // They are both zeroes
    as.VMOR(v0, nan_mask_1, equal_mask);          // Combine the masks

    as.VFMAX(nan_mask_2, dst, src);        // actual max result calculation
    as.VMERGE(zero_mask, nan_mask_2, src); // Where v0 is 1's, use src, otherwise use result of vfmax
    rec.setOperandVec(&operands[0], zero_mask);
}

FAST_HANDLE(MULPS) { // Fuzzed, TODO: needs NaN handling
    biscuit::Vec dst = rec.getOperandVec(&operands[0]);
    biscuit::Vec src = rec.getOperandVec(&operands[1]);
    rec.setVectorState(SEW::E32, 4);
    as.VFMUL(dst, dst, src);
    rec.setOperandVec(&operands[0], dst);
}

FAST_HANDLE(MULPD) {
    biscuit::Vec dst = rec.getOperandVec(&operands[0]);
    biscuit::Vec src = rec.getOperandVec(&operands[1]);
    rec.setVectorState(SEW::E64, 2);
    as.VFMUL(dst, dst, src);
    rec.setOperandVec(&operands[0], dst);
}

FAST_HANDLE(SQRTPS) { // Fuzzed, TODO: needs NaN handling
    biscuit::Vec dst = rec.allocatedVec(rec.zydisToRef(operands[0].reg.value));
    biscuit::Vec src = rec.getOperandVec(&operands[1]);
    rec.setVectorState(SEW::E32, 4);
    as.VFSQRT(dst, src);
    rec.setOperandVec(&operands[0], dst);
}

FAST_HANDLE(SQRTPD) {
    biscuit::Vec dst = rec.allocatedVec(rec.zydisToRef(operands[0].reg.value));
    biscuit::Vec src = rec.getOperandVec(&operands[1]);
    rec.setVectorState(SEW::E64, 2);
    as.VFSQRT(dst, src);
    rec.setOperandVec(&operands[0], dst);
}

FAST_HANDLE(DIVPS) {
    biscuit::Vec dst = rec.getOperandVec(&operands[0]);
    biscuit::Vec src = rec.getOperandVec(&operands[1]);
    rec.setVectorState(SEW::E32, 4);
    as.VFDIV(dst, dst, src);
    rec.setOperandVec(&operands[0], dst);
}

FAST_HANDLE(DIVPD) {
    biscuit::Vec dst = rec.getOperandVec(&operands[0]);
    biscuit::Vec src = rec.getOperandVec(&operands[1]);
    rec.setVectorState(SEW::E64, 2);
    as.VFDIV(dst, dst, src);
    rec.setOperandVec(&operands[0], dst);
}

FAST_HANDLE(RCPPS) {
    biscuit::Vec dst = rec.allocatedVec(rec.zydisToRef(operands[0].reg.value));
    biscuit::Vec src = rec.getOperandVec(&operands[1]);
    biscuit::Vec ones = rec.scratchVec();
    rec.setVectorState(SEW::E32, 4);
    biscuit::GPR scratch = rec.scratch();
    as.LI(scratch, 0x3f800000);
    as.VMV(ones, scratch);
    as.VFDIV(dst, ones, src);
    rec.setOperandVec(&operands[0], dst);
}

FAST_HANDLE(RSQRTPS) {
    biscuit::Vec temp = rec.scratchVec();
    biscuit::Vec ones = rec.scratchVec();
    biscuit::Vec dst = rec.allocatedVec(rec.zydisToRef(operands[0].reg.value));
    biscuit::Vec src = rec.getOperandVec(&operands[1]);
    rec.setVectorState(SEW::E32, 4);
    biscuit::GPR scratch = rec.scratch();
    as.LI(scratch, 0x3f800000);
    as.VMV(ones, scratch);
    as.VFSQRT(temp, src);
    as.VFDIV(dst, ones, temp);
    rec.setOperandVec(&operands[0], dst);
}

FAST_HANDLE(MOVSB) {
    ASSERT(instruction.address_width > 16);
    u8 width = instruction.operand_width;
    x86_size_e address_width = rec.zydisToSize(instruction.address_width);
    biscuit::GPR rdi = rec.getRefGPR(X86_REF_RDI, address_width);
    biscuit::GPR rsi = rec.getRefGPR(X86_REF_RSI, address_width);
    biscuit::GPR rcx = rec.getRefGPR(X86_REF_RCX, address_width); // TODO: technically wrong, should use ecx/cx sometimes
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

    rec.setRefGPR(X86_REF_RDI, address_width, rdi);
    rec.setRefGPR(X86_REF_RSI, address_width, rsi);
    rec.setRefGPR(X86_REF_RCX, address_width, rcx);
}

FAST_HANDLE(MOVSW) {
    fast_MOVSB(rec, meta, as, instruction, operands);
}

// The rep movsd and sse movsd have the same mnemonic, so we differentiate it like this
FAST_HANDLE(MOVSD_sse) {
    if (operands[0].type == ZYDIS_OPERAND_TYPE_MEMORY) {
        biscuit::Vec src = rec.getOperandVec(&operands[1]);
        rec.setOperandVec(&operands[0], src);
    } else {
        biscuit::Vec dst = rec.getOperandVec(&operands[0]);
        biscuit::Vec src = rec.getOperandVec(&operands[1]);
        if (operands[1].type == ZYDIS_OPERAND_TYPE_MEMORY) {
            rec.setVectorState(SEW::E64, 2);
            as.VMV(dst, 0);
        }
        rec.setVectorState(SEW::E64, 1);
        as.VMV(dst, src);
        rec.setOperandVec(&operands[0], dst);
    }
}

FAST_HANDLE(MOVSD) {
    if (instruction.meta.isa_set == ZYDIS_ISA_SET_SSE2) {
        fast_MOVSD_sse(rec, meta, as, instruction, operands);
    } else if (instruction.meta.isa_set == ZYDIS_ISA_SET_I386) {
        fast_MOVSB(rec, meta, as, instruction, operands);
    } else {
        UNREACHABLE();
    }
}

FAST_HANDLE(MOVSQ) {
    fast_MOVSB(rec, meta, as, instruction, operands);
}

FAST_HANDLE(CMPSB) {
    ASSERT(instruction.address_width > 16);
    u8 width = instruction.operand_width;
    x86_size_e address_width = rec.zydisToSize(instruction.address_width);
    biscuit::GPR rdi = rec.getRefGPR(X86_REF_RDI, address_width);
    biscuit::GPR rsi = rec.getRefGPR(X86_REF_RSI, address_width);
    biscuit::GPR rcx = rec.getRefGPR(X86_REF_RCX, address_width);
    biscuit::GPR temp = rec.scratch();
    biscuit::GPR src1 = rec.scratch();
    biscuit::GPR src2 = rec.scratch();
    biscuit::GPR result = rec.scratch();
    biscuit::GPR df = rec.scratch();
    as.LBU(df, offsetof(ThreadState, df), rec.threadStatePointer());
    x86_size_e size = rec.zydisToSize(width);

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

    rec.readMemory(src1, rsi, 0, size);
    rec.readMemory(src2, rdi, 0, size);

    as.SUB(result, src1, src2);

    SetCmpFlags(meta, rec, as, src1, src2, result, size, false, HAS_REP /* always emit flags for rep */);

    as.ADD(rdi, rdi, temp);
    as.ADD(rsi, rsi, temp);

    if (HAS_REP) {
        rec.repzEpilogue(&loop_body, &loop_end, rcx, instruction.attributes & ZYDIS_ATTRIB_HAS_REPZ);
        as.Bind(&loop_end);
    }

    rec.setRefGPR(X86_REF_RDI, address_width, rdi);
    rec.setRefGPR(X86_REF_RSI, address_width, rsi);
    rec.setRefGPR(X86_REF_RCX, address_width, rcx);
}

FAST_HANDLE(CMPSW) {
    fast_CMPSB(rec, meta, as, instruction, operands);
}

FAST_HANDLE(CMPSD_string) {
    fast_CMPSB(rec, meta, as, instruction, operands);
}

FAST_HANDLE(CMPSQ) {
    fast_CMPSB(rec, meta, as, instruction, operands);
}

FAST_HANDLE(SCASB) {
    ASSERT(instruction.address_width > 16);
    u8 width = instruction.operand_width;
    x86_size_e size = rec.zydisToSize(width);
    x86_size_e address_width = rec.zydisToSize(instruction.address_width);
    biscuit::GPR rax = rec.getRefGPR(X86_REF_RAX, size);
    biscuit::GPR rdi = rec.getRefGPR(X86_REF_RDI, address_width);
    biscuit::GPR rcx = rec.getRefGPR(X86_REF_RCX, address_width);
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

    Label loop_end, loop_body;
    if (HAS_REP) {
        rec.repPrologue(&loop_end, rcx);
        as.Bind(&loop_body);
    }

    rec.readMemory(src2, rdi, 0, size);

    as.SUB(result, rax, src2);

    SetCmpFlags(meta, rec, as, rax, src2, result, size, false, HAS_REP /* always emit flags for rep */);

    as.ADD(rdi, rdi, temp);

    if (HAS_REP) {
        rec.repzEpilogue(&loop_body, &loop_end, rcx, instruction.attributes & ZYDIS_ATTRIB_HAS_REPZ);
        as.Bind(&loop_end);
    }

    rec.setRefGPR(X86_REF_RDI, address_width, rdi);
    rec.setRefGPR(X86_REF_RCX, address_width, rcx);
}

FAST_HANDLE(SCASW) {
    fast_SCASB(rec, meta, as, instruction, operands);
}

FAST_HANDLE(SCASD) {
    fast_SCASB(rec, meta, as, instruction, operands);
}

FAST_HANDLE(SCASQ) {
    fast_SCASB(rec, meta, as, instruction, operands);
}

FAST_HANDLE(LODSB) {
    ASSERT(!HAS_REP); // it can have rep, but it would be too silly
    ASSERT(instruction.address_width > 16);
    int width = instruction.operand_width;
    x86_size_e address_width = rec.zydisToSize(instruction.address_width);
    x86_size_e size = rec.zydisToSize(width);
    biscuit::GPR rsi = rec.getRefGPR(X86_REF_RSI, address_width);
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

    rec.setRefGPR(X86_REF_RAX, size, loaded);
    rec.setRefGPR(X86_REF_RSI, address_width, rsi);
}

FAST_HANDLE(LODSW) {
    fast_LODSB(rec, meta, as, instruction, operands);
}

FAST_HANDLE(LODSD) {
    fast_LODSB(rec, meta, as, instruction, operands);
}

FAST_HANDLE(LODSQ) {
    fast_LODSB(rec, meta, as, instruction, operands);
}

FAST_HANDLE(STOSB) {
    ASSERT(instruction.address_width > 16);
    Label loop_end, loop_body;
    u8 width = instruction.operand_width;
    x86_size_e address_width = rec.zydisToSize(instruction.address_width);
    biscuit::GPR rdi = rec.getRefGPR(X86_REF_RDI, address_width);
    biscuit::GPR rcx = rec.getRefGPR(X86_REF_RCX, address_width);
    biscuit::GPR rax = rec.getRefGPR(X86_REF_RAX, rec.zydisToSize(width));
    biscuit::GPR temp = rec.scratch();
    biscuit::GPR df = rec.scratch();
    as.LBU(df, offsetof(ThreadState, df), rec.threadStatePointer());

    Label end;
    as.LI(temp, -width / 8);
    as.BNEZ(df, &end);
    as.LI(temp, width / 8);
    as.Bind(&end);

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

    rec.setRefGPR(X86_REF_RDI, address_width, rdi);
    rec.setRefGPR(X86_REF_RCX, address_width, rcx);
}

FAST_HANDLE(STOSW) {
    fast_STOSB(rec, meta, as, instruction, operands);
}

FAST_HANDLE(STOSD) {
    fast_STOSB(rec, meta, as, instruction, operands);
}

FAST_HANDLE(STOSQ) {
    fast_STOSB(rec, meta, as, instruction, operands);
}

FAST_HANDLE(MOVHPS) {
    if (operands[0].type == ZYDIS_OPERAND_TYPE_MEMORY) {
        biscuit::Vec temp = rec.scratchVec();
        biscuit::Vec src = rec.getOperandVec(&operands[1]);
        rec.setVectorState(SEW::E64, 2);
        as.VSLIDEDOWN(temp, src, 1);
        rec.setOperandVec(&operands[0], temp);
    } else if (operands[1].type == ZYDIS_OPERAND_TYPE_MEMORY) {
        biscuit::Vec temp = rec.scratchVec();
        biscuit::Vec dst = rec.getOperandVec(&operands[0]);
        biscuit::Vec src = rec.getOperandVec(&operands[1]);
        rec.setVectorState(SEW::E64, 2);
        as.VSLIDEUP(temp, src, 1);
        as.VMV(v0, 0b10);
        as.VMERGE(dst, dst, temp);
        rec.setOperandVec(&operands[0], dst);
    } else {
        UNREACHABLE();
    }
}

FAST_HANDLE(MOVHPD) {
    fast_MOVHPS(rec, meta, as, instruction, operands);
}

FAST_HANDLE(SHUFPD) {
    u8 imm = rec.getImmediate(&operands[2]);
    biscuit::GPR temp = rec.scratch();
    biscuit::Vec vtemp = rec.scratchVec();
    biscuit::Vec vsrc = rec.scratchVec();
    biscuit::Vec dst = rec.getOperandVec(&operands[0]);
    biscuit::Vec src = rec.getOperandVec(&operands[1]);

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

    rec.setOperandVec(&operands[0], vtemp);
}

FAST_HANDLE(LEAVE) {
    x86_size_e size = rec.zydisToSize(instruction.operand_width);
    biscuit::GPR rbp = rec.getRefGPR(X86_REF_RBP, size);
    as.ADDI(rbp, rbp, rec.stackPointerSize());
    rec.setRefGPR(X86_REF_RSP, size, rbp);
    rec.readMemory(rbp, rbp, -rec.stackPointerSize(), size);
    rec.setRefGPR(X86_REF_RBP, size, rbp);
}

FAST_HANDLE(ENTER) {
    ERROR("ENTER instruction is broken");
    x86_size_e size = rec.zydisToSize(instruction.operand_width);
    int alloc_size = rec.getImmediate(&operands[0]);
    u8 nesting_level = rec.getImmediate(&operands[1]) & 0x1F;
    biscuit::GPR frame_temp = rec.scratch();
    biscuit::GPR rsp = rec.getRefGPR(X86_REF_RSP, X86_SIZE_QWORD);
    biscuit::GPR rbp = rec.getRefGPR(X86_REF_RBP, X86_SIZE_QWORD);
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

    rec.setRefGPR(X86_REF_RBP, size, frame_temp);
    biscuit::GPR new_rsp = rec.scratch();
    rec.addi(new_rsp, frame_temp, -alloc_size);
    rec.setRefGPR(X86_REF_RSP, size, new_rsp);
}

void SETCC(Recompiler& rec, const HandlerMetadata& meta, ZydisDecodedInstruction& instruction, ZydisDecodedOperand* operands, biscuit::GPR cond) {
    rec.setOperandGPR(&operands[0], cond);
}

FAST_HANDLE(SETO) {
    SETCC(rec, meta, instruction, operands, rec.getCond(instruction.opcode & 0xF));
}

FAST_HANDLE(SETNO) {
    SETCC(rec, meta, instruction, operands, rec.getCond(instruction.opcode & 0xF));
}

FAST_HANDLE(SETB) {
    SETCC(rec, meta, instruction, operands, rec.getCond(instruction.opcode & 0xF));
}

FAST_HANDLE(SETNB) {
    SETCC(rec, meta, instruction, operands, rec.getCond(instruction.opcode & 0xF));
}

FAST_HANDLE(SETZ) {
    SETCC(rec, meta, instruction, operands, rec.getCond(instruction.opcode & 0xF));
}

FAST_HANDLE(SETNZ) {
    SETCC(rec, meta, instruction, operands, rec.getCond(instruction.opcode & 0xF));
}

FAST_HANDLE(SETBE) {
    SETCC(rec, meta, instruction, operands, rec.getCond(instruction.opcode & 0xF));
}

FAST_HANDLE(SETNBE) {
    SETCC(rec, meta, instruction, operands, rec.getCond(instruction.opcode & 0xF));
}

FAST_HANDLE(SETP) {
    SETCC(rec, meta, instruction, operands, rec.getCond(instruction.opcode & 0xF));
}

FAST_HANDLE(SETNP) {
    SETCC(rec, meta, instruction, operands, rec.getCond(instruction.opcode & 0xF));
}

FAST_HANDLE(SETS) {
    SETCC(rec, meta, instruction, operands, rec.getCond(instruction.opcode & 0xF));
}

FAST_HANDLE(SETNS) {
    SETCC(rec, meta, instruction, operands, rec.getCond(instruction.opcode & 0xF));
}

FAST_HANDLE(SETL) {
    SETCC(rec, meta, instruction, operands, rec.getCond(instruction.opcode & 0xF));
}

FAST_HANDLE(SETNL) {
    SETCC(rec, meta, instruction, operands, rec.getCond(instruction.opcode & 0xF));
}

FAST_HANDLE(SETLE) {
    SETCC(rec, meta, instruction, operands, rec.getCond(instruction.opcode & 0xF));
}

FAST_HANDLE(SETNLE) {
    SETCC(rec, meta, instruction, operands, rec.getCond(instruction.opcode & 0xF));
}

FAST_HANDLE(NOT) {
    biscuit::GPR result = rec.scratch();
    biscuit::GPR dst = rec.getOperandGPR(&operands[0]);
    as.NOT(result, dst);
    rec.setOperandGPR(&operands[0], result);
}

FAST_HANDLE(NEG) {
    x86_size_e size = rec.getOperandSize(&operands[0]);
    biscuit::GPR result = rec.scratch();
    biscuit::GPR dst = rec.getOperandGPR(&operands[0]);
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

    if (rec.shouldEmitFlag(meta.rip, X86_REF_CF)) {
        biscuit::GPR cf = rec.flagW(X86_REF_CF);
        as.SNEZ(cf, dst);
    }

    if (rec.shouldEmitFlag(meta.rip, X86_REF_ZF)) {
        rec.updateZero(result, size);
    }

    if (rec.shouldEmitFlag(meta.rip, X86_REF_OF)) {
        rec.updateOverflowSub(x0, dst, result, size);
    }

    if (rec.shouldEmitFlag(meta.rip, X86_REF_SF)) {
        rec.updateSign(result, size);
    }

    if (rec.shouldEmitFlag(meta.rip, X86_REF_AF)) {
        biscuit::GPR af = rec.scratch();
        as.ANDI(af, dst, 0xF);
        as.SNEZ(af, af);
        as.SB(af, offsetof(ThreadState, af), rec.threadStatePointer());
        rec.popScratch();
    }

    if (rec.shouldEmitFlag(meta.rip, X86_REF_PF)) {
        rec.updateParity(result);
    }

    rec.setOperandGPR(&operands[0], result);
}

// There is no single instruction that can saturate a signed value into an unsigned destination. A sequence of two vector instructions that
// rst removes negative numbers by performing a max against 0 using vmax then clips the resulting unsigned value into the destination
// using vnclipu can be used if setting vxsat value for negative numbers is not required. A vsetvli is required inbetween these two
// instructions to change SEW.
FAST_HANDLE(PACKUSWB) {
    biscuit::Vec result1 = rec.scratchVec();
    biscuit::Vec result2 = rec.scratchVec();
    biscuit::Vec result3 = rec.scratchVec();
    biscuit::Vec result4 = rec.scratchVec();
    biscuit::Vec result_up = rec.scratchVec();
    biscuit::Vec result = rec.scratchVec();
    biscuit::Vec dst = rec.getOperandVec(&operands[0]);
    biscuit::Vec src = rec.getOperandVec(&operands[1]);

    rec.setVectorState(SEW::E16, 8);
    as.VMAX(result1, dst, x0);
    as.VMAX(result2, src, x0);
    rec.setVectorState(SEW::E8, 8, LMUL::MF2);
    as.VNCLIPU(result3, result1, 0);
    as.VNCLIPU(result4, result2, 0);
    rec.setVectorState(SEW::E64, 2);
    as.VMV(v0, 0b10);
    as.VSLIDEUP(result_up, result4, 1);
    as.VMERGE(result, result3, result_up);
    rec.setOperandVec(&operands[0], result);
}

FAST_HANDLE(PACKUSDW) {
    biscuit::Vec result1 = rec.scratchVec();
    biscuit::Vec result2 = rec.scratchVec();
    biscuit::Vec result3 = rec.scratchVec();
    biscuit::Vec result4 = rec.scratchVec();
    biscuit::Vec result_up = rec.scratchVec();
    biscuit::Vec result = rec.scratchVec();
    biscuit::Vec dst = rec.getOperandVec(&operands[0]);
    biscuit::Vec src = rec.getOperandVec(&operands[1]);

    rec.setVectorState(SEW::E32, 4);
    as.VMAX(result1, dst, x0);
    as.VMAX(result2, src, x0);
    rec.setVectorState(SEW::E16, 4, LMUL::MF2);
    as.VNCLIPU(result3, result1, 0);
    as.VNCLIPU(result4, result2, 0);
    rec.setVectorState(SEW::E64, 2);
    as.VMV(v0, 0b10);
    as.VSLIDEUP(result_up, result4, 1);
    as.VMERGE(result, result3, result_up);
    rec.setOperandVec(&operands[0], result);
}

FAST_HANDLE(PACKSSWB) {
    biscuit::Vec result1 = rec.scratchVec();
    biscuit::Vec result2 = rec.scratchVec();
    biscuit::Vec result2_up = rec.scratchVec();
    biscuit::Vec result = rec.scratchVec();
    biscuit::Vec dst = rec.getOperandVec(&operands[0]);
    biscuit::Vec src = rec.getOperandVec(&operands[1]);

    // Use half the register group so we don't run into overlapping problems
    rec.setVectorState(SEW::E8, 8, LMUL::MF2);
    as.VNCLIP(result1, dst, 0);
    as.VNCLIP(result2, src, 0);
    rec.setVectorState(SEW::E64, 2);
    as.VMV(v0, 0b10);
    as.VSLIDEUP(result2_up, result2, 1);
    as.VMERGE(result, result1, result2_up);
    rec.setOperandVec(&operands[0], result);
}

FAST_HANDLE(PACKSSDW) {
    biscuit::Vec result1 = rec.scratchVec();
    biscuit::Vec result2 = rec.scratchVec();
    biscuit::Vec result2_up = rec.scratchVec();
    biscuit::Vec result = rec.scratchVec();
    biscuit::Vec dst = rec.getOperandVec(&operands[0]);
    biscuit::Vec src = rec.getOperandVec(&operands[1]);

    // Use half the register group so we don't run into overlapping problems
    rec.setVectorState(SEW::E16, 4, LMUL::MF2);
    as.VNCLIP(result1, dst, 0);
    as.VNCLIP(result2, src, 0);
    rec.setVectorState(SEW::E64, 2);
    as.VMV(v0, 0b10);
    as.VSLIDEUP(result2_up, result2, 1);
    as.VMERGE(result, result1, result2_up);
    rec.setOperandVec(&operands[0], result);
}

void ROUND(Recompiler& rec, const HandlerMetadata& meta, Assembler& as, ZydisDecodedInstruction& instruction, ZydisDecodedOperand* operands, SEW sew,
           u8 vlen) {
    u8 imm = rec.getImmediate(&operands[2]);
    biscuit::Vec dst = rec.getOperandVec(&operands[0]);
    biscuit::Vec src = rec.getOperandVec(&operands[1]);
    bool dyn_round = imm & 0b100;
    RMode rmode = RMode::DYN;

    if (!dyn_round) {
        rmode = rounding_mode((x86RoundingMode)(imm & 0b11));
    }

    if (!(imm & 0b1000)) {
        WARN("Ignore precision bit not set for roundsd/roundss");
    }

    rec.setVectorState(sew, vlen);
    as.VFMV_FS(ft8, src);

    if (Extensions::Zfa) {
        WARN_ONCE("Zfa extension code, untested");
        if (sew == SEW::E64) {
            as.FROUND_D(ft9, ft8, rmode);
        } else if (sew == SEW::E32) {
            as.FROUND_S(ft9, ft8, rmode);
        } else {
            UNREACHABLE();
        }
    } else {
        biscuit::GPR temp = rec.scratch();
        if (sew == SEW::E64) {
            as.FCVT_L_D(temp, ft8, rmode);
            as.FCVT_D_L(ft9, temp, rmode);
        } else if (sew == SEW::E32) {
            as.FCVT_W_S(temp, ft8, rmode);
            as.FCVT_S_W(ft9, temp, rmode);
        } else {
            UNREACHABLE();
        }
    }

    as.VFMV_SF(dst, ft9);

    rec.setOperandVec(&operands[0], dst);
}

FAST_HANDLE(ROUNDSS) {
    ROUND(rec, meta, as, instruction, operands, SEW::E32, 1);
}

FAST_HANDLE(ROUNDSD) {
    ROUND(rec, meta, as, instruction, operands, SEW::E64, 1);
}

FAST_HANDLE(PMOVMSKB) {
    biscuit::GPR scratch = rec.scratch();
    biscuit::Vec src = rec.getOperandVec(&operands[1]);
    biscuit::Vec temp = rec.scratchVec();

    rec.setVectorState(SEW::E8, 16);
    as.VMSLT(temp, src, x0);

    rec.setVectorState(SEW::E64, 2);
    as.VMV_XS(scratch, temp);

    if (rec.maxVlen() == 128)
        rec.zext(scratch, scratch, X86_SIZE_WORD);
    else if (rec.maxVlen() == 256)
        rec.zext(scratch, scratch, X86_SIZE_DWORD);

    rec.setOperandGPR(&operands[0], scratch);
}

FAST_HANDLE(MOVSHDUP) {
    biscuit::GPR shift = rec.scratch();
    biscuit::Vec dst = rec.getOperandVec(&operands[0]);
    biscuit::Vec src = rec.getOperandVec(&operands[1]);
    biscuit::Vec left = rec.scratchVec();
    biscuit::Vec right = rec.scratchVec();

    as.LI(shift, 32);
    rec.setVectorState(SEW::E64, 2);
    as.VSRL(right, src, shift);
    as.VSLL(left, right, shift);
    as.VOR(dst, left, right);

    rec.setOperandVec(&operands[0], dst);
}

FAST_HANDLE(MOVSLDUP) {
    biscuit::GPR shift = rec.scratch();
    biscuit::Vec dst = rec.getOperandVec(&operands[0]);
    biscuit::Vec src = rec.getOperandVec(&operands[1]);
    biscuit::Vec left = rec.scratchVec();
    biscuit::Vec right = rec.scratchVec();

    as.LI(shift, 32);
    rec.setVectorState(SEW::E64, 2);
    as.VSLL(left, src, shift);
    as.VSRL(right, left, shift);
    as.VOR(dst, left, right);

    rec.setOperandVec(&operands[0], dst);
}

FAST_HANDLE(PTEST) {
    biscuit::Vec zmask = rec.scratchVec();
    biscuit::Vec cmask = rec.scratchVec();
    biscuit::Vec resultz = rec.scratchVec();
    biscuit::Vec resultc = rec.scratchVec();
    biscuit::Vec dst = rec.getOperandVec(&operands[0]);
    biscuit::Vec src = rec.getOperandVec(&operands[1]);

    // PTEST with the same register is common to check if all the elements
    // of a single register are zero. In which case, CF is always 1 because (a & ~a) = 0,
    // and we don't need to perform a VAND
    bool same = dst == src;

    rec.setVectorState(SEW::E64, 2);
    if (!same) {
        as.VAND(resultz, dst, src);
        if (Extensions::Zvbb) {
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
    if (rec.shouldEmitFlag(meta.rip, X86_REF_ZF)) {
        biscuit::GPR zf = rec.flagW(X86_REF_ZF);
        as.VMSNE(zmask, resultz, 0);
        as.VMV_XS(zf, zmask);
        // No need to do a full zext, just shift left
        as.SLLI(zf, zf, 62); // only care about lower 2 bits for the 2 64-bit elements
        as.SEQZ(zf, zf);
    }

    if (rec.shouldEmitFlag(meta.rip, X86_REF_CF)) {
        biscuit::GPR cf = rec.flagW(X86_REF_CF);
        if (!same) {
            as.VMSNE(cmask, resultc, 0);
            as.VMV_XS(cf, cmask);
            as.SLLI(cf, cf, 62);
            as.SEQZ(cf, cf);
        } else {
            as.LI(cf, 1);
        }
    }

    if (rec.shouldEmitFlag(meta.rip, X86_REF_AF)) {
        as.SB(x0, offsetof(ThreadState, af), rec.threadStatePointer());
    }

    if (rec.shouldEmitFlag(meta.rip, X86_REF_OF)) {
        biscuit::GPR of = rec.flagW(X86_REF_OF);
        as.MV(of, x0);
    }

    if (rec.shouldEmitFlag(meta.rip, X86_REF_SF)) {
        biscuit::GPR sf = rec.flagW(X86_REF_SF);
        as.MV(sf, x0);
    }

    if (rec.shouldEmitFlag(meta.rip, X86_REF_PF)) {
        as.SB(x0, offsetof(ThreadState, pf), rec.threadStatePointer());
    }
}

FAST_HANDLE(MOVMSKPS) {
    biscuit::Vec mask = rec.scratchVec();
    biscuit::Vec src = rec.getOperandVec(&operands[1]);
    biscuit::GPR dst = rec.scratch();

    rec.setVectorState(SEW::E32, 4);
    as.VMSLT(mask, src, x0);
    as.VMV_XS(dst, mask);
    as.ANDI(dst, dst, 0b1111);
    rec.setOperandGPR(&operands[0], dst);
}

FAST_HANDLE(MOVMSKPD) {
    biscuit::Vec mask = rec.scratchVec();
    biscuit::Vec src = rec.getOperandVec(&operands[1]);
    biscuit::GPR dst = rec.scratch();

    rec.setVectorState(SEW::E64, 2);
    as.VMSLT(mask, src, x0);
    as.VMV_XS(dst, mask);
    as.ANDI(dst, dst, 0b11);
    rec.setOperandGPR(&operands[0], dst);
}

FAST_HANDLE(PMOVZXBQ) {
    biscuit::Vec dst = rec.getOperandVec(&operands[0]);
    biscuit::Vec src = rec.getOperandVec(&operands[1]);

    rec.setVectorState(SEW::E64, 2);
    as.VZEXTVF8(dst, src);

    rec.setOperandVec(&operands[0], dst);
}

FAST_HANDLE(PMOVZXBD) {
    biscuit::Vec dst = rec.getOperandVec(&operands[0]);
    biscuit::Vec src = rec.getOperandVec(&operands[1]);

    rec.setVectorState(SEW::E32, 4);
    as.VZEXTVF4(dst, src);

    rec.setOperandVec(&operands[0], dst);
}

FAST_HANDLE(PMOVZXBW) {
    biscuit::Vec dst = rec.getOperandVec(&operands[0]);
    biscuit::Vec src = rec.getOperandVec(&operands[1]);

    rec.setVectorState(SEW::E16, 8);
    as.VZEXTVF2(dst, src);

    rec.setOperandVec(&operands[0], dst);
}

FAST_HANDLE(PMOVZXWD) {
    biscuit::Vec dst = rec.getOperandVec(&operands[0]);
    biscuit::Vec src = rec.getOperandVec(&operands[1]);

    rec.setVectorState(SEW::E32, 4);
    as.VZEXTVF2(dst, src);

    rec.setOperandVec(&operands[0], dst);
}

FAST_HANDLE(PMOVZXWQ) {
    biscuit::Vec dst = rec.getOperandVec(&operands[0]);
    biscuit::Vec src = rec.getOperandVec(&operands[1]);

    rec.setVectorState(SEW::E64, 2);
    as.VZEXTVF4(dst, src);

    rec.setOperandVec(&operands[0], dst);
}

FAST_HANDLE(PMOVZXDQ) {
    biscuit::Vec dst = rec.getOperandVec(&operands[0]);
    biscuit::Vec src = rec.getOperandVec(&operands[1]);

    rec.setVectorState(SEW::E64, 2);
    as.VZEXTVF2(dst, src);

    rec.setOperandVec(&operands[0], dst);
}

FAST_HANDLE(PMOVSXBQ) {
    biscuit::Vec dst = rec.getOperandVec(&operands[0]);
    biscuit::Vec src = rec.getOperandVec(&operands[1]);

    rec.setVectorState(SEW::E64, 2);
    as.VSEXTVF8(dst, src);

    rec.setOperandVec(&operands[0], dst);
}

FAST_HANDLE(PMOVSXBD) {
    biscuit::Vec dst = rec.getOperandVec(&operands[0]);
    biscuit::Vec src = rec.getOperandVec(&operands[1]);

    rec.setVectorState(SEW::E32, 4);
    as.VSEXTVF4(dst, src);

    rec.setOperandVec(&operands[0], dst);
}

FAST_HANDLE(PMOVSXBW) {
    biscuit::Vec dst = rec.getOperandVec(&operands[0]);
    biscuit::Vec src = rec.getOperandVec(&operands[1]);

    rec.setVectorState(SEW::E16, 8);
    as.VSEXTVF2(dst, src);

    rec.setOperandVec(&operands[0], dst);
}

FAST_HANDLE(PMOVSXWD) {
    biscuit::Vec dst = rec.getOperandVec(&operands[0]);
    biscuit::Vec src = rec.getOperandVec(&operands[1]);

    rec.setVectorState(SEW::E32, 4);
    as.VSEXTVF2(dst, src);

    rec.setOperandVec(&operands[0], dst);
}

FAST_HANDLE(PMOVSXWQ) {
    biscuit::Vec dst = rec.getOperandVec(&operands[0]);
    biscuit::Vec src = rec.getOperandVec(&operands[1]);

    rec.setVectorState(SEW::E64, 2);
    as.VSEXTVF4(dst, src);

    rec.setOperandVec(&operands[0], dst);
}

FAST_HANDLE(PMOVSXDQ) {
    biscuit::Vec dst = rec.getOperandVec(&operands[0]);
    biscuit::Vec src = rec.getOperandVec(&operands[1]);

    rec.setVectorState(SEW::E64, 2);
    as.VSEXTVF2(dst, src);

    rec.setOperandVec(&operands[0], dst);
}

void PCMPEQ(Recompiler& rec, const HandlerMetadata& meta, Assembler& as, ZydisDecodedInstruction& instruction, ZydisDecodedOperand* operands, SEW sew,
            u8 vlen) {
    biscuit::Vec zero = rec.scratchVec();
    biscuit::Vec dst = rec.getOperandVec(&operands[0]);
    biscuit::Vec src = rec.getOperandVec(&operands[1]);
    rec.setVectorState(sew, vlen);
    as.VMV(zero, 0);
    as.VMSEQ(v0, dst, src);
    as.VMERGE(dst, zero, -1ll);
    rec.setOperandVec(&operands[0], dst);
}

void PCMPGT(Recompiler& rec, const HandlerMetadata& meta, Assembler& as, ZydisDecodedInstruction& instruction, ZydisDecodedOperand* operands, SEW sew,
            u8 vlen) {
    biscuit::Vec zero = rec.scratchVec();
    biscuit::Vec dst = rec.getOperandVec(&operands[0]);
    biscuit::Vec src = rec.getOperandVec(&operands[1]);
    rec.setVectorState(sew, vlen);
    as.VMV(zero, 0);
    as.VMSLT(v0, src, dst);
    as.VMERGE(dst, zero, -1ll);
    rec.setOperandVec(&operands[0], dst);
}

FAST_HANDLE(PCMPEQB) {
    PCMPEQ(rec, meta, as, instruction, operands, SEW::E8, 16);
}

FAST_HANDLE(PCMPEQW) {
    PCMPEQ(rec, meta, as, instruction, operands, SEW::E16, 8);
}

FAST_HANDLE(PCMPEQD) {
    PCMPEQ(rec, meta, as, instruction, operands, SEW::E32, 4);
}

FAST_HANDLE(PCMPEQQ) {
    PCMPEQ(rec, meta, as, instruction, operands, SEW::E64, 2);
}

FAST_HANDLE(PCMPGTB) {
    PCMPGT(rec, meta, as, instruction, operands, SEW::E8, 16);
}

FAST_HANDLE(PCMPGTW) {
    PCMPGT(rec, meta, as, instruction, operands, SEW::E16, 8);
}

FAST_HANDLE(PCMPGTD) {
    PCMPGT(rec, meta, as, instruction, operands, SEW::E32, 4);
}

FAST_HANDLE(PCMPGTQ) {
    PCMPGT(rec, meta, as, instruction, operands, SEW::E64, 2);
}

void CMPP(Recompiler& rec, const HandlerMetadata& meta, Assembler& as, ZydisDecodedInstruction& instruction, ZydisDecodedOperand* operands, SEW sew,
          u8 vlen) {
    u8 imm = rec.getImmediate(&operands[2]);
    biscuit::Vec result = rec.scratchVec();
    biscuit::Vec temp1 = rec.scratchVec();
    biscuit::Vec temp2 = rec.scratchVec();
    biscuit::Vec dst = rec.getOperandVec(&operands[0]);
    biscuit::Vec src = rec.getOperandVec(&operands[1]);
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

    rec.setOperandVec(&operands[0], result);
}

FAST_HANDLE(CMPPS) { // Fuzzed
    CMPP(rec, meta, as, instruction, operands, SEW::E32, 4);
}

FAST_HANDLE(CMPPD) { // Fuzzed
    CMPP(rec, meta, as, instruction, operands, SEW::E64, 2);
}

FAST_HANDLE(PSHUFD) {
    u8 imm = rec.getImmediate(&operands[2]);
    u64 el0 = imm & 0b11;
    u64 el1 = (imm >> 2) & 0b11;
    u64 el2 = (imm >> 4) & 0b11;
    u64 el3 = (imm >> 6) & 0b11;

    biscuit::Vec result = rec.scratchVec();
    biscuit::Vec iota = rec.scratchVec();
    biscuit::Vec src = rec.getOperandVec(&operands[1]);

    rec.setVectorState(SEW::E64, 1);
    biscuit::GPR temp = rec.scratch();
    u64 mask = (el3 << 48) | (el2 << 32) | (el1 << 16) | el0;
    as.LI(temp, mask);
    as.VMV_SX(iota, temp);

    rec.setVectorState(SEW::E32, 4);
    as.VRGATHEREI16(result, src, iota);

    rec.setOperandVec(&operands[0], result);
}

FAST_HANDLE(SHUFPS) {
    u8 imm = rec.getImmediate(&operands[2]);
    u64 el0 = imm & 0b11;
    u64 el1 = (imm >> 2) & 0b11;
    u64 el2 = (imm >> 4) & 0b11;
    u64 el3 = (imm >> 6) & 0b11;

    biscuit::Vec iota = rec.scratchVec();
    biscuit::GPR temp = rec.scratch();
    biscuit::Vec dst = rec.getOperandVec(&operands[0]);
    biscuit::Vec src = rec.getOperandVec(&operands[1]);
    biscuit::Vec result = rec.scratchVec();

    rec.setVectorState(SEW::E64, 1);
    u64 mask = (el3 << 48) | (el2 << 32) | (el1 << 16) | el0;
    as.LI(temp, mask);
    as.VMV_SX(iota, temp);

    as.VMV(v0, 0b11);
    as.VMV(result, 0);
    rec.setVectorState(SEW::E32, 4);
    as.VRGATHEREI16(result, dst, iota, VecMask::Yes);
    as.VMV(v0, 0b1100);
    as.VRGATHEREI16(result, src, iota, VecMask::Yes);

    rec.setOperandVec(&operands[0], result);
}

FAST_HANDLE(PSHUFB) {
    biscuit::GPR bitmask = rec.scratch();
    biscuit::Vec tmp = rec.scratchVec();
    biscuit::Vec mask_masked = rec.scratchVec();
    biscuit::Vec dst = rec.getOperandVec(&operands[0]);
    biscuit::Vec mask = rec.getOperandVec(&operands[1]);

    rec.setVectorState(SEW::E8, 16);
    // Keep 0...3 for regular shifting and bit 7 which indicates resulting element goes to 0, maps well with vrgather this way
    as.LI(bitmask, 0b10001111);
    as.VAND(mask_masked, mask, bitmask);
    as.VRGATHER(tmp, dst, mask_masked);

    rec.setOperandVec(&operands[0], tmp);

    ASSERT_MSG(Extensions::VLEN < 2048, "Woah... How did you get a 2048-bit VLEN device? Our PSHUFB implementation would break");
}

FAST_HANDLE(PBLENDVB) {
    biscuit::Vec dst = rec.getOperandVec(&operands[0]);
    biscuit::Vec src = rec.getOperandVec(&operands[1]);
    biscuit::Vec mask = rec.getRefVec(X86_REF_XMM0);

    rec.setVectorState(SEW::E8, 16);
    as.VMSLT(v0, mask, x0);
    as.VMERGE(dst, dst, src);

    rec.setOperandVec(&operands[0], dst);
}

FAST_HANDLE(PBLENDW) { // Fuzzed
    u8 imm = rec.getImmediate(&operands[2]);
    biscuit::GPR mask = rec.scratch();
    biscuit::Vec dst = rec.getOperandVec(&operands[0]);
    biscuit::Vec src = rec.getOperandVec(&operands[1]);

    rec.setVectorState(SEW::E16, 8);
    as.LI(mask, imm);
    as.VMV(v0, mask);
    as.VMERGE(dst, dst, src);

    rec.setOperandVec(&operands[0], dst);
}

FAST_HANDLE(BLENDPS) {
    u8 imm = rec.getImmediate(&operands[2]) & 0b1111;
    biscuit::Vec dst = rec.getOperandVec(&operands[0]);
    biscuit::Vec src = rec.getOperandVec(&operands[1]);

    rec.setVectorState(SEW::E32, 4);
    as.VMV(v0, imm);
    as.VMERGE(dst, dst, src);

    rec.setOperandVec(&operands[0], dst);
}

FAST_HANDLE(BLENDVPS) {
    biscuit::Vec dst = rec.getOperandVec(&operands[0]);
    biscuit::Vec src = rec.getOperandVec(&operands[1]);
    biscuit::Vec mask = rec.getRefVec(X86_REF_XMM0); // I see where VMERGE took inspiration from /j

    rec.setVectorState(SEW::E32, 4);
    as.VMSLT(v0, mask, x0);
    as.VMERGE(dst, dst, src);

    rec.setOperandVec(&operands[0], dst);
}

FAST_HANDLE(BLENDPD) {
    u8 imm = rec.getImmediate(&operands[2]) & 0b11;
    biscuit::Vec dst = rec.getOperandVec(&operands[0]);
    biscuit::Vec src = rec.getOperandVec(&operands[1]);

    rec.setVectorState(SEW::E64, 2);
    as.VMV(v0, imm);
    as.VMERGE(dst, dst, src);

    rec.setOperandVec(&operands[0], dst);
}

FAST_HANDLE(BLENDVPD) {
    biscuit::Vec dst = rec.getOperandVec(&operands[0]);
    biscuit::Vec src = rec.getOperandVec(&operands[1]);
    biscuit::Vec mask = rec.getRefVec(X86_REF_XMM0);

    rec.setVectorState(SEW::E64, 2);
    as.VMSLT(v0, mask, x0);
    as.VMERGE(dst, dst, src);

    rec.setOperandVec(&operands[0], dst);
}

FAST_HANDLE(DPPS) {
    biscuit::GPR splat = rec.scratch();
    biscuit::Vec mul = rec.scratchVec();
    biscuit::Vec sum = rec.scratchVec();
    biscuit::Vec dst = rec.getOperandVec(&operands[0]);
    biscuit::Vec src = rec.getOperandVec(&operands[1]);
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

    rec.setOperandVec(&operands[0], dst);
}

FAST_HANDLE(PSHUFLW) {
    u8 imm = rec.getImmediate(&operands[2]);
    u8 el0 = imm & 0b11;
    u8 el1 = (imm >> 2) & 0b11;
    u8 el2 = (imm >> 4) & 0b11;
    u8 el3 = (imm >> 6) & 0b11;

    biscuit::Vec iota = rec.scratchVec();
    biscuit::Vec iota2 = rec.scratchVec();
    biscuit::GPR temp = rec.scratch();
    biscuit::Vec src = rec.getOperandVec(&operands[1]);
    biscuit::Vec result = rec.scratchVec();

    rec.setVectorState(SEW::E16, 8);
    as.VMV(iota, 0);
    as.VID(iota2);
    // Slide down 4 words, so then the register looks like 8 7 6 5, then we can slide up the other 4 elements
    // TODO: VRGATHEREI16
    as.VSLIDEDOWN(iota2, iota2, 4);
    as.LI(temp, el3);
    as.VSLIDE1UP(iota, iota2, temp);
    as.LI(temp, el2);
    as.VSLIDE1UP(iota2, iota, temp);
    as.LI(temp, el1);
    as.VSLIDE1UP(iota, iota2, temp);
    as.LI(temp, el0);
    as.VSLIDE1UP(iota2, iota, temp);

    as.VMV(result, 0);
    as.VRGATHER(result, src, iota2);

    rec.setOperandVec(&operands[0], result);
}

FAST_HANDLE(PSHUFHW) {
    u8 imm = rec.getImmediate(&operands[2]);
    biscuit::Vec result = rec.scratchVec();
    biscuit::Vec src = rec.getOperandVec(&operands[1]);
    biscuit::GPR tmp = rec.scratch();
    biscuit::Vec iota = rec.scratchVec();
    biscuit::Vec iota2 = rec.scratchVec();

    rec.setVectorState(SEW::E16, 8);
    as.VMV(result, src); // to move the low words

    // TODO: VRGATHEREI16
    u8 el0 = 4 + (imm & 0b11);
    u8 el1 = 4 + ((imm >> 2) & 0b11);
    u8 el2 = 4 + ((imm >> 4) & 0b11);
    u8 el3 = 4 + ((imm >> 6) & 0b11);
    as.VMV(iota2, el3);
    as.LI(tmp, el2);
    as.VSLIDE1UP(iota, iota2, tmp);
    as.LI(tmp, el1);
    as.VSLIDE1UP(iota2, iota, tmp);
    as.LI(tmp, el0);
    as.VSLIDE1UP(iota, iota2, tmp);
    as.VSLIDEUP(iota2, iota, 4);

    as.LI(tmp, 0b11110000); // operate on top words only
    as.VMV(v0, tmp);

    rec.vrgather(result, src, iota2, VecMask::Yes);

    rec.setOperandVec(&operands[0], result);
}

FAST_HANDLE(PALIGNR) {
    u8 imm = rec.getImmediate(&operands[2]);
    biscuit::GPR temp = rec.scratch();
    biscuit::Vec result = rec.scratchVec();
    biscuit::Vec slide_up = rec.scratchVec();
    biscuit::Vec dst = rec.getOperandVec(&operands[0]);
    biscuit::Vec src = rec.getOperandVec(&operands[1]);

    rec.setVectorState(SEW::E64, 2);

    if (imm > 31) {
        as.VMV(dst, 0);
        rec.setOperandVec(&operands[0], dst);
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

    rec.setOperandVec(&operands[0], result);
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
    biscuit::GPR src = rec.getOperandGPR(&operands[1]);
    biscuit::GPR dst = rec.getOperandGPR(&operands[0]);
    (void)dst; // must be loaded since conditional code follows
    biscuit::GPR zf = rec.flagW(X86_REF_ZF);

    Label end;
    as.SEQZ(zf, src);
    as.BEQZ(src, &end);

    CTZ(rec, as, result, src);

    rec.setOperandGPR(&operands[0], result);

    as.Bind(&end);

    rec.setFlagUndefined(X86_REF_CF);
    rec.setFlagUndefined(X86_REF_OF);
    rec.setFlagUndefined(X86_REF_SF);
}

FAST_HANDLE(TZCNT) {
    biscuit::GPR result = rec.scratch();
    biscuit::GPR src = rec.getOperandGPR(&operands[1]);
    biscuit::GPR dst = rec.getOperandGPR(&operands[0]);
    (void)dst; // must be loaded since conditional code follows
    biscuit::GPR zf = rec.flagW(X86_REF_ZF);
    biscuit::GPR cf = rec.flagW(X86_REF_CF);

    Label end;
    as.LI(result, instruction.operand_width);
    as.SEQZ(cf, src);
    as.BEQZ(src, &end);
    CTZ(rec, as, result, src);
    as.J(&end);

    as.Bind(&end);
    rec.setOperandGPR(&operands[0], result);
    as.SEQZ(zf, result);

    rec.setFlagUndefined(X86_REF_OF);
    rec.setFlagUndefined(X86_REF_SF);
}

void BITSTRING_func(Recompiler& rec, const HandlerMetadata& meta, Assembler& as, ZydisDecodedInstruction& instruction, ZydisDecodedOperand* operands,
                    u64 func) {
    // Special case where the memory may index past the effective address, only when offset is a register
    biscuit::GPR base = rec.lea(&operands[0]);
    biscuit::GPR bit = rec.getOperandGPR(&operands[1]);
    rec.writebackDirtyState();
    rec.invalidStateUntilJump();
    rec.sext(a1, bit, rec.zydisToSize(operands[1].size));
    as.MV(a0, base);
    as.LI(t0, func);
    as.JALR(t0);

    biscuit::GPR cf = rec.flagW(X86_REF_CF);
    as.MV(cf, a0); // Write result to cf
    rec.setFlagUndefined(X86_REF_OF);
    rec.setFlagUndefined(X86_REF_SF);
}

FAST_HANDLE(BTC) {
    if (operands[0].type == ZYDIS_OPERAND_TYPE_MEMORY && operands[1].type == ZYDIS_OPERAND_TYPE_REGISTER) {
        BITSTRING_func(rec, meta, as, instruction, operands, (u64)&felix86_btc);
        return;
    }

    biscuit::GPR bit = rec.getOperandGPR(&operands[1]);
    biscuit::GPR dst = rec.getOperandGPR(&operands[0]);
    biscuit::GPR cf = rec.flagW(X86_REF_CF);

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

    rec.setOperandGPR(&operands[0], result);

    rec.setFlagUndefined(X86_REF_OF);
    rec.setFlagUndefined(X86_REF_SF);
}

FAST_HANDLE(BT) {
    if (operands[0].type == ZYDIS_OPERAND_TYPE_MEMORY && operands[1].type == ZYDIS_OPERAND_TYPE_REGISTER) {
        BITSTRING_func(rec, meta, as, instruction, operands, (u64)&felix86_bt);
        return;
    }

    biscuit::GPR shift = rec.scratch();
    biscuit::GPR bit = rec.getOperandGPR(&operands[1]);
    biscuit::GPR cf = rec.flagW(X86_REF_CF);
    biscuit::GPR dst = rec.getOperandGPR(&operands[0]);

    u8 bit_size = operands[0].size;
    as.ANDI(shift, bit, bit_size - 1);

    as.SRL(cf, dst, shift);
    as.ANDI(cf, cf, 1);

    rec.setFlagUndefined(X86_REF_OF);
    rec.setFlagUndefined(X86_REF_SF);
}

FAST_HANDLE(BTS) {
    if (operands[0].type == ZYDIS_OPERAND_TYPE_MEMORY && operands[1].type == ZYDIS_OPERAND_TYPE_REGISTER) {
        BITSTRING_func(rec, meta, as, instruction, operands, (u64)&felix86_bts);
        return;
    }

    biscuit::GPR result = rec.scratch();
    biscuit::GPR bit = rec.getOperandGPR(&operands[1]);
    biscuit::GPR dst = rec.getOperandGPR(&operands[0]);
    biscuit::GPR shift = rec.scratch();

    u8 bit_size = operands[0].size;
    as.ANDI(shift, bit, bit_size - 1);
    if (rec.shouldEmitFlag(meta.rip, X86_REF_CF)) {
        biscuit::GPR cf = rec.flagW(X86_REF_CF);
        as.SRL(cf, dst, shift);
        as.ANDI(cf, cf, 1);
    }

    biscuit::GPR one = rec.scratch();
    as.LI(one, 1);
    as.SLL(one, one, shift);
    as.OR(result, dst, one);

    rec.setOperandGPR(&operands[0], result);

    rec.setFlagUndefined(X86_REF_OF);
    rec.setFlagUndefined(X86_REF_SF);
}

FAST_HANDLE(BTR) {
    if (operands[0].type == ZYDIS_OPERAND_TYPE_MEMORY && operands[1].type == ZYDIS_OPERAND_TYPE_REGISTER) {
        BITSTRING_func(rec, meta, as, instruction, operands, (u64)&felix86_btr);
        return;
    }

    biscuit::GPR result = rec.scratch();
    biscuit::GPR bit = rec.getOperandGPR(&operands[1]);
    biscuit::GPR dst = rec.getOperandGPR(&operands[0]);
    biscuit::GPR cf = rec.flagW(X86_REF_CF);
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

    rec.setOperandGPR(&operands[0], result);

    rec.setFlagUndefined(X86_REF_OF);
    rec.setFlagUndefined(X86_REF_SF);
}

FAST_HANDLE(BLSR) {
    WARN("BLSR is broken, check BLSR_flags");
    biscuit::GPR src = rec.getOperandGPR(&operands[1]);
    biscuit::GPR result = rec.scratch();

    as.ADDI(result, src, -1);
    as.AND(result, src, result);

    if (rec.shouldEmitFlag(meta.rip, X86_REF_CF)) {
        biscuit::GPR cf = rec.flagW(X86_REF_CF);
        as.SEQZ(cf, src);
    }

    if (rec.shouldEmitFlag(meta.rip, X86_REF_ZF)) {
        rec.updateZero(result, rec.zydisToSize(operands[0].size));
    }

    if (rec.shouldEmitFlag(meta.rip, X86_REF_SF)) {
        rec.updateSign(result, rec.zydisToSize(operands[0].size));
    }

    rec.setOperandGPR(&operands[0], result);
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
    biscuit::GPR src = rec.getOperandGPR(&operands[1]);
    biscuit::GPR dst = rec.getOperandGPR(&operands[0]);
    (void)dst; // must be loaded since conditional code follows
    biscuit::GPR zf = rec.flagW(X86_REF_ZF);

    Label end;
    as.SEQZ(zf, src);
    as.BEQZ(src, &end);
    BSR(rec, as, result, src, instruction.operand_width);
    rec.setOperandGPR(&operands[0], result);

    as.Bind(&end);

    rec.setFlagUndefined(X86_REF_CF);
    rec.setFlagUndefined(X86_REF_OF);
    rec.setFlagUndefined(X86_REF_SF);
}

void REV8(Recompiler& rec, Assembler& as, biscuit::GPR result, biscuit::GPR src) {
    if (Extensions::B) {
        as.REV8(result, src);
    } else {
        biscuit::GPR scratch = rec.scratch();
        // TODO: make this bswap implementation better
        as.SRLI(scratch, src, 8);
        as.ANDI(result, scratch, 0xFF);
        as.SLLI(result, result, 8);
        as.SRLI(scratch, src, 16);
        as.ANDI(scratch, scratch, 0xFF);
        as.OR(result, result, scratch);
        as.SLLI(result, result, 8);
        as.SRLI(scratch, src, 24);
        as.ANDI(scratch, scratch, 0xFF);
        as.OR(result, result, scratch);
        as.SLLI(result, result, 8);
        as.SRLI(scratch, src, 32);
        as.ANDI(scratch, scratch, 0xFF);
        as.OR(result, result, scratch);
        as.SLLI(result, result, 8);
        as.SRLI(scratch, src, 40);
        as.ANDI(scratch, scratch, 0xFF);
        as.OR(result, result, scratch);
        as.SLLI(result, result, 8);
        as.SRLI(scratch, src, 48);
        as.ANDI(scratch, scratch, 0xFF);
        as.OR(result, result, scratch);
        as.SLLI(result, result, 8);
        as.SRLI(scratch, src, 56);
        as.OR(result, result, scratch);
    }
}

FAST_HANDLE(BSWAP) {
    x86_size_e size = rec.getOperandSize(&operands[0]);
    biscuit::GPR dst = rec.getOperandGPR(&operands[0]);
    biscuit::GPR result = rec.scratch();

    if (size == X86_SIZE_DWORD) {
        REV8(rec, as, result, dst);
        as.SRLI(result, result, 32);
    } else if (size == X86_SIZE_QWORD) {
        REV8(rec, as, result, dst);
    } else {
        UNREACHABLE();
    }

    rec.setOperandGPR(&operands[0], result);
}

FAST_HANDLE(MOVLPS) {
    if (operands[0].type == ZYDIS_OPERAND_TYPE_REGISTER) {
        ASSERT(operands[1].type == ZYDIS_OPERAND_TYPE_MEMORY);
        biscuit::Vec dst = rec.getOperandVec(&operands[0]);
        biscuit::Vec src = rec.getOperandVec(&operands[1]);

        rec.setVectorState(SEW::E64, 1);
        as.VMV(dst, src);

        rec.setOperandVec(&operands[0], dst);
    } else if (operands[0].type == ZYDIS_OPERAND_TYPE_MEMORY) {
        biscuit::Vec src = rec.getOperandVec(&operands[1]);
        rec.setOperandVec(&operands[0], src);
    } else {
        UNREACHABLE();
    }
}

FAST_HANDLE(MOVLPD) {
    fast_MOVLPS(rec, meta, as, instruction, operands);
}

FAST_HANDLE(MOVHLPS) {
    ASSERT(operands[0].type == ZYDIS_OPERAND_TYPE_REGISTER);
    biscuit::Vec temp = rec.scratchVec();
    biscuit::Vec dst = rec.getOperandVec(&operands[0]);
    biscuit::Vec src = rec.getOperandVec(&operands[1]);

    rec.setVectorState(SEW::E64, 2);
    as.VSLIDEDOWN(temp, src, 1);
    as.VMV(v0, 0b10);
    as.VMERGE(dst, temp, dst);
    rec.setOperandVec(&operands[0], dst);
}

FAST_HANDLE(ROL) {
    x86_size_e size = rec.getOperandSize(&operands[0]);
    biscuit::GPR dst = rec.getOperandGPR(&operands[0]);
    biscuit::GPR src = rec.getOperandGPR(&operands[1]);
    biscuit::GPR count = rec.scratch();

    Label zero_count;

    biscuit::GPR cf = rec.flagWR(X86_REF_CF);
    biscuit::GPR of = rec.flagWR(X86_REF_OF);
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

    rec.setOperandGPR(&operands[0], dst);

    as.Bind(&zero_count);
}

FAST_HANDLE(ROR) {
    x86_size_e size = rec.getOperandSize(&operands[0]);
    biscuit::GPR dst = rec.getOperandGPR(&operands[0]);
    biscuit::GPR src = rec.getOperandGPR(&operands[1]);
    biscuit::GPR count = rec.scratch();

    Label zero_count;

    biscuit::GPR cf = rec.flagWR(X86_REF_CF);
    biscuit::GPR of = rec.flagWR(X86_REF_OF);
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

    rec.setOperandGPR(&operands[0], dst);

    as.Bind(&zero_count);
}

FAST_HANDLE(PSLLDQ) {
    u8 imm = rec.getImmediate(&operands[1]);
    biscuit::Vec dst = rec.getOperandVec(&operands[0]);
    biscuit::Vec temp = rec.scratchVec();
    rec.setVectorState(SEW::E8, 16);
    if (imm > 15) {
        as.VMV(temp, 0);
    } else {
        as.VMV(temp, 0);
        as.VSLIDEUP(temp, dst, imm);
    }
    rec.setOperandVec(&operands[0], temp);
}

FAST_HANDLE(PSRLDQ) {
    u8 imm = rec.getImmediate(&operands[1]);
    biscuit::Vec dst = rec.getOperandVec(&operands[0]);
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
    rec.setOperandVec(&operands[0], temp);
}

FAST_HANDLE(PSLLW) {
    biscuit::GPR shift = rec.scratch();
    biscuit::Vec dst = rec.getOperandVec(&operands[0]);
    if (operands[1].type == ZYDIS_OPERAND_TYPE_IMMEDIATE) {
        u8 val = rec.getImmediate(&operands[1]);
        as.LI(shift, val);
    } else {
        biscuit::Vec src = rec.getOperandVec(&operands[1]);
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
    rec.setOperandVec(&operands[0], dst);
}

FAST_HANDLE(PSLLQ) {
    biscuit::GPR shift = rec.scratch();
    if (operands[1].type == ZYDIS_OPERAND_TYPE_IMMEDIATE) {
        u8 val = rec.getImmediate(&operands[1]);
        as.LI(shift, val);
    } else {
        biscuit::Vec src = rec.getOperandVec(&operands[1]);
        rec.setVectorState(SEW::E64, 2);
        as.VMV_XS(shift, src);
    }
    biscuit::Vec dst = rec.getOperandVec(&operands[0]);
    biscuit::GPR max = rec.scratch();
    biscuit::Label dont_zero;
    rec.setVectorState(SEW::E64, 2);
    as.LI(max, 64);
    as.BLTU(shift, max, &dont_zero);
    as.VMV(dst, 0);
    as.Bind(&dont_zero);
    as.VSLL(dst, dst, shift);
    rec.setOperandVec(&operands[0], dst);
}

FAST_HANDLE(PSLLD) { // Fuzzed
    biscuit::GPR shift = rec.scratch();
    biscuit::Vec dst = rec.getOperandVec(&operands[0]);
    if (operands[1].type == ZYDIS_OPERAND_TYPE_IMMEDIATE) {
        u8 val = rec.getImmediate(&operands[1]);
        as.LI(shift, val);
    } else {
        biscuit::Vec src = rec.getOperandVec(&operands[1]);
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
    rec.setOperandVec(&operands[0], dst);
}

FAST_HANDLE(PSRLD) {
    biscuit::GPR shift = rec.scratch();
    biscuit::Vec dst = rec.getOperandVec(&operands[0]);
    if (operands[1].type == ZYDIS_OPERAND_TYPE_IMMEDIATE) {
        u8 val = rec.getImmediate(&operands[1]);
        as.LI(shift, val);
    } else {
        biscuit::Vec src = rec.getOperandVec(&operands[1]);
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
    rec.setOperandVec(&operands[0], dst);
}

FAST_HANDLE(PSRLW) {
    biscuit::GPR shift = rec.scratch();
    biscuit::Vec dst = rec.getOperandVec(&operands[0]);
    if (operands[1].type == ZYDIS_OPERAND_TYPE_IMMEDIATE) {
        u8 val = rec.getImmediate(&operands[1]);
        as.LI(shift, val);
    } else {
        biscuit::Vec src = rec.getOperandVec(&operands[1]);
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
    rec.setOperandVec(&operands[0], dst);
}

FAST_HANDLE(PSRLQ) {
    biscuit::GPR shift = rec.scratch();
    if (operands[1].type == ZYDIS_OPERAND_TYPE_IMMEDIATE) {
        u8 val = rec.getImmediate(&operands[1]);
        as.LI(shift, val);
    } else {
        biscuit::Vec src = rec.getOperandVec(&operands[1]);
        rec.setVectorState(SEW::E64, 2);
        as.VMV_XS(shift, src);
    }
    biscuit::Vec dst = rec.getOperandVec(&operands[0]);
    rec.setVectorState(SEW::E64, 2);
    biscuit::GPR max = rec.scratch();
    biscuit::Label dont_zero;
    as.LI(max, 64);
    as.BLTU(shift, max, &dont_zero);
    as.VMV(dst, 0);
    as.Bind(&dont_zero);
    as.VSRL(dst, dst, shift);
    rec.setOperandVec(&operands[0], dst);
}

FAST_HANDLE(PSRAW) {
    biscuit::GPR shift = rec.scratch();
    biscuit::Vec dst = rec.getOperandVec(&operands[0]);
    if (operands[1].type == ZYDIS_OPERAND_TYPE_IMMEDIATE) {
        u8 val = rec.getImmediate(&operands[1]);
        if (val > 15)
            val = 15;
        as.LI(shift, val);
    } else {
        biscuit::Vec src = rec.getOperandVec(&operands[1]);
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
    rec.setOperandVec(&operands[0], dst);
}

FAST_HANDLE(PSRAD) {
    biscuit::GPR shift = rec.scratch();
    biscuit::Vec dst = rec.getOperandVec(&operands[0]);
    if (operands[1].type == ZYDIS_OPERAND_TYPE_IMMEDIATE) {
        u8 val = rec.getImmediate(&operands[1]);
        if (val > 31)
            val = 31;
        as.LI(shift, val);
    } else {
        biscuit::Vec src = rec.getOperandVec(&operands[1]);
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
    rec.setOperandVec(&operands[0], dst);
}

FAST_HANDLE(SFENCE) {
    as.FENCETSO(); // just make a full fence for now, TODO: we can optimize this some day
}

FAST_HANDLE(LFENCE) {
    as.FENCETSO(); // just make a full fence for now, TODO: we can optimize this some day
}

FAST_HANDLE(MFENCE) {
    as.FENCETSO(); // just make a full fence for now, TODO: we can optimize this some day
}

FAST_HANDLE(MOVSX) {
    biscuit::GPR result = rec.scratch();
    biscuit::GPR src = rec.getOperandGPR(&operands[1]);
    x86_size_e size = rec.getOperandSize(&operands[1]);

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

    rec.setOperandGPR(&operands[0], result);
}

void COMIS(Recompiler& rec, const HandlerMetadata& meta, Assembler& as, ZydisDecodedInstruction& instruction, ZydisDecodedOperand* operands,
           SEW sew) {
    biscuit::GPR nan_1 = rec.scratch();
    biscuit::GPR nan_2 = rec.scratch();
    biscuit::Vec temp = rec.scratchVec();
    biscuit::Vec temp2 = rec.scratchVec();
    biscuit::Vec src = rec.getOperandVec(&operands[1]);
    biscuit::Vec dst = rec.getOperandVec(&operands[0]);

    biscuit::GPR cf = rec.flagW(X86_REF_CF);
    biscuit::GPR zf = rec.flagW(X86_REF_ZF);
    biscuit::GPR sf = rec.flagW(X86_REF_SF);
    biscuit::GPR of = rec.flagW(X86_REF_OF);

    if (rec.shouldEmitFlag(meta.rip, X86_REF_AF)) {
        as.SB(x0, offsetof(ThreadState, af), rec.threadStatePointer());
    }

    if (rec.shouldEmitFlag(meta.rip, X86_REF_OF)) {
        as.LI(of, 0);
    }

    if (rec.shouldEmitFlag(meta.rip, X86_REF_SF)) {
        as.LI(sf, 0);
    }

    Label end, nan, equal, less_than;

    rec.setVectorState(sew, 1);

    as.LI(nan_1, 0);
    as.LI(nan_2, 0);

    as.VMFNE(temp, dst, dst);
    as.VMV_XS(nan_1, temp);

    as.VMFNE(temp2, src, src);
    as.VMV_XS(nan_2, temp2);
    as.OR(nan_1, nan_1, nan_2);
    as.ANDI(nan_1, nan_1, 1);

    as.BNEZ(nan_1, &nan);

    // Check for equality
    as.VMFEQ(temp, dst, src);
    as.VMV_XS(nan_1, temp);
    as.ANDI(nan_1, nan_1, 1);

    as.BNEZ(nan_1, &equal);

    // Check for less than
    as.VMFLT(temp, dst, src);
    as.VMV_XS(nan_1, temp);
    as.ANDI(nan_1, nan_1, 1);

    as.BNEZ(nan_1, &less_than);

    // Greater than
    // ZF: 0, PF: 0, CF: 0
    as.LI(zf, 0);
    as.LI(cf, 0);
    as.SB(x0, offsetof(ThreadState, pf), rec.threadStatePointer());

    as.J(&end);

    as.Bind(&less_than);

    // Less than
    // ZF: 0, PF: 0, CF: 1
    as.LI(zf, 0);
    as.LI(cf, 1);
    as.SB(x0, offsetof(ThreadState, pf), rec.threadStatePointer());

    as.J(&end);

    as.Bind(&equal);

    // Equal
    // ZF: 1, PF: 0, CF: 0
    as.LI(zf, 1);
    as.LI(cf, 0);
    as.SB(x0, offsetof(ThreadState, pf), rec.threadStatePointer());

    as.J(&end);

    as.Bind(&nan);

    // Unordered
    // ZF: 1, PF: 1, CF: 1
    as.LI(zf, 1);
    as.LI(cf, 1);
    as.SB(cf, offsetof(ThreadState, pf), rec.threadStatePointer());

    as.Bind(&end);
}

FAST_HANDLE(COMISD) { // Fuzzed
    COMIS(rec, meta, as, instruction, operands, SEW::E64);
}

FAST_HANDLE(UCOMISD) {
    COMIS(rec, meta, as, instruction, operands, SEW::E64);
}

FAST_HANDLE(COMISS) {
    COMIS(rec, meta, as, instruction, operands, SEW::E32);
}

FAST_HANDLE(UCOMISS) {
    COMIS(rec, meta, as, instruction, operands, SEW::E32);
}

FAST_HANDLE(PINSRB) {
    u8 imm = rec.getImmediate(&operands[2]) & 0b1111;
    biscuit::Vec dst = rec.getOperandVec(&operands[0]);
    biscuit::GPR src = rec.getOperandGPR(&operands[1]);
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

    rec.setOperandVec(&operands[0], result);
}

FAST_HANDLE(PINSRW) {
    u8 imm = rec.getImmediate(&operands[2]) & 0b111;
    biscuit::Vec dst = rec.getOperandVec(&operands[0]);
    biscuit::GPR src = rec.getOperandGPR(&operands[1]);
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

    rec.setOperandVec(&operands[0], result);
}

FAST_HANDLE(PINSRD) {
    u8 imm = rec.getImmediate(&operands[2]) & 0b11;
    biscuit::Vec dst = rec.getOperandVec(&operands[0]);
    biscuit::GPR src = rec.getOperandGPR(&operands[1]);
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

    rec.setOperandVec(&operands[0], result);
}

FAST_HANDLE(PINSRQ) {
    u8 imm = rec.getImmediate(&operands[2]) & 0b1;
    biscuit::Vec dst = rec.getOperandVec(&operands[0]);
    biscuit::GPR src = rec.getOperandGPR(&operands[1]);
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

    rec.setOperandVec(&operands[0], result);
}

FAST_HANDLE(PEXTRB) {
    biscuit::Vec temp = rec.scratchVec();
    biscuit::GPR result = rec.scratch();
    biscuit::Vec src = rec.getOperandVec(&operands[1]);
    u8 imm = rec.getImmediate(&operands[2]) & 0b1111;

    rec.setVectorState(SEW::E8, 16);
    as.VSLIDEDOWN(temp, src, imm);
    as.VMV_XS(result, temp);
    rec.zext(result, result, X86_SIZE_BYTE);

    rec.setOperandGPR(&operands[0], result);
}

FAST_HANDLE(PEXTRW) {
    biscuit::Vec temp = rec.scratchVec();
    biscuit::GPR result = rec.scratch();
    biscuit::Vec src = rec.getOperandVec(&operands[1]);
    u8 imm = rec.getImmediate(&operands[2]) & 0b111;

    rec.setVectorState(SEW::E16, 8);
    as.VSLIDEDOWN(temp, src, imm);
    as.VMV_XS(result, temp);
    rec.zext(result, result, X86_SIZE_WORD);

    rec.setOperandGPR(&operands[0], result);
}

FAST_HANDLE(PEXTRD) {
    biscuit::Vec temp = rec.scratchVec();
    biscuit::Vec src = rec.getOperandVec(&operands[1]);
    biscuit::GPR result = rec.scratch();
    u8 imm = rec.getImmediate(&operands[2]) & 0b11;

    rec.setVectorState(SEW::E32, 4);
    as.VSLIDEDOWN(temp, src, imm);
    as.VMV_XS(result, temp);

    rec.setOperandGPR(&operands[0], result);
}

FAST_HANDLE(PEXTRQ) {
    biscuit::Vec temp = rec.scratchVec();
    biscuit::Vec src = rec.getOperandVec(&operands[1]);
    biscuit::GPR result = rec.scratch();
    u8 imm = rec.getImmediate(&operands[2]) & 0b1;

    rec.setVectorState(SEW::E64, 2);
    as.VSLIDEDOWN(temp, src, imm);
    as.VMV_XS(result, temp);

    rec.setOperandGPR(&operands[0], result);
}

FAST_HANDLE(CMPXCHG_lock) {
    ASSERT(operands[0].size != 8 && operands[0].size != 16);

    x86_size_e size = rec.zydisToSize(instruction.operand_width);
    biscuit::GPR address = rec.lea(&operands[0]);
    biscuit::GPR src = rec.getOperandGPR(&operands[1]);
    biscuit::GPR rax = rec.getRefGPR(X86_REF_RAX, size);
    biscuit::GPR dst = rec.scratch();

    switch (size) {
    case X86_SIZE_WORD: {
        if (Extensions::Zacas && Extensions::Zabha) {
            WARN_ONCE("You have Zacas and Zabha, CMPXCHG is untested with this combo");
            dst = rax; // write to the AX scratch to save a move instruction
            as.FENCE();
            as.AMOCAS_H(biscuit::Ordering::AQRL, dst, src, address);
        } else {
            // TODO: runs out of scratch space, also untested
            ASSERT(false);

            // This sequence of instructions was taken from clang RISC-V compiler when using
            // __atomic_compare_exchange with 16-bit operands.
            biscuit::Label not_equal;
            biscuit::Label start;
            biscuit::GPR address_aligned = rec.scratch();
            biscuit::GPR mask = rec.scratch();
            // Save AX we need it for flag calculation later
            as.SH(rax, -2, sp);

            // Align the address so that we can use LR_W/SC_W
            as.ANDI(address_aligned, address, -4);
            // Create a shift amount by shifting the original address left by 3
            // This multiplies it by 8, and the resulting shift count is (based on the low 2 bits of orig address)
            // either 0, 8, 16, 24 (0b00000, 0b01000, 0b10000, 0b11000). We need to shift our operands and a mask
            // to that position, because that position is where the word is inside the dword (the word may also be
            // misaligned inside the dword, however if it is between two dwords then this isn't possible, so this would
            // fail. We assume that this isn't the case)
            as.SLLI(address, address, 3);
            as.LI(mask, 0xFFFF);
            // SLLW ignores the top bits of "address" and it only takes into account the bottom 5 bits
            as.SLLW(rax, rax, address);
            as.SLLW(mask, mask, address);
            as.SLLW(src, src, address);

            biscuit::GPR tmp = rec.scratch();
            as.Bind(&start);
            as.LR_W(Ordering::AQRL, dst, address_aligned);
            as.AND(tmp, dst, mask);
            as.BNE(tmp, rax, &not_equal);
            as.XOR(tmp, dst, src);
            as.AND(tmp, tmp, mask);
            as.XOR(tmp, tmp, dst);
            as.SC_W(Ordering::AQRL, tmp, tmp, address_aligned);
            as.BNEZ(tmp, &start);
            as.Bind(&not_equal);
            rec.popScratch();
            rec.popScratch();

            // Load back the unshifted value of AX
            as.LHU(rax, -2, sp);
            // Shift it back down for the flag calculation
            as.SRLW(dst, dst, address);
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
            as.AMOCAS_D(Ordering::AQRL, dst, src, address);
            WARN_ONCE("Zacas & CMPXCHG, untested");
        } else {
            biscuit::Label not_equal;
            biscuit::Label start;
            biscuit::GPR scratch = rec.scratch();
            as.Bind(&start);
            as.LR_D(Ordering::AQRL, dst, address);
            as.BNE(dst, rax, &not_equal);
            as.SC_D(Ordering::AQRL, scratch, src, address);
            as.BNEZ(scratch, &start);
            as.Bind(&not_equal);
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

    SetCmpFlags(meta, rec, as, rax, dst, result, size);

    Label dont_set;
    as.BEQZ(result, &dont_set);

    rec.setRefGPR(X86_REF_RAX, size, dst);

    as.Bind(&dont_set);
}

FAST_HANDLE(CMPXCHG) {
    if (operands[0].type == ZYDIS_OPERAND_TYPE_MEMORY) {
        if (operands[0].size == 8 || operands[0].size == 16) {
            WARN("Atomic CMPXCHG with 8 or 16 bit operands encountered");
        } else {
            return fast_CMPXCHG_lock(rec, meta, as, instruction, operands);
        }
    }

    x86_size_e size = rec.zydisToSize(instruction.operand_width);
    biscuit::GPR dst = rec.getOperandGPR(&operands[0]);
    biscuit::GPR src = rec.getOperandGPR(&operands[1]);
    biscuit::GPR rax = rec.getRefGPR(X86_REF_RAX, size);

    Label end, equal;

    biscuit::GPR result = rec.scratch();

    as.SUB(result, rax, dst);

    SetCmpFlags(meta, rec, as, rax, dst, result, size);

    as.BEQ(dst, rax, &equal);

    // Not equal
    rec.setRefGPR(X86_REF_RAX, size, dst);
    as.J(&end);

    as.Bind(&equal);
    rec.setOperandGPR(&operands[0], src);

    as.Bind(&end);
}

void SCALAR(Recompiler& rec, const HandlerMetadata& meta, Assembler& as, ZydisDecodedInstruction& instruction, ZydisDecodedOperand* operands, SEW sew,
            u8 vlen, void (Assembler::*func)(Vec, Vec, Vec, VecMask)) {
    biscuit::Vec dst = rec.getOperandVec(&operands[0]);
    biscuit::Vec src = rec.getOperandVec(&operands[1]);
    rec.setVectorState(sew, vlen);
    (as.*func)(dst, dst, src, VecMask::No);
    rec.setOperandVec(&operands[0], dst);
}

void SCALAR(Recompiler& rec, const HandlerMetadata& meta, Assembler& as, ZydisDecodedInstruction& instruction, ZydisDecodedOperand* operands, SEW sew,
            u8 vlen, void (Assembler::*func)(Vec, Vec, VecMask)) {
    biscuit::Vec dst = rec.getOperandVec(&operands[0]);
    biscuit::Vec src = rec.getOperandVec(&operands[1]);
    rec.setVectorState(sew, vlen);
    (as.*func)(dst, src, VecMask::No);
    rec.setOperandVec(&operands[0], dst);
}

FAST_HANDLE(DIVSS) {
    SCALAR(rec, meta, as, instruction, operands, SEW::E32, 1, &Assembler::VFDIV);
}

FAST_HANDLE(DIVSD) {
    SCALAR(rec, meta, as, instruction, operands, SEW::E64, 1, &Assembler::VFDIV);
}

FAST_HANDLE(ADDSS) {
    SCALAR(rec, meta, as, instruction, operands, SEW::E32, 1, &Assembler::VFADD);
}

FAST_HANDLE(ADDSD) { // Fuzzed
    SCALAR(rec, meta, as, instruction, operands, SEW::E64, 1, &Assembler::VFADD);
}

FAST_HANDLE(SUBSS) {
    SCALAR(rec, meta, as, instruction, operands, SEW::E32, 1, &Assembler::VFSUB);
}

FAST_HANDLE(SUBSD) {
    SCALAR(rec, meta, as, instruction, operands, SEW::E64, 1, &Assembler::VFSUB);
}

FAST_HANDLE(MULSS) {
    SCALAR(rec, meta, as, instruction, operands, SEW::E32, 1, &Assembler::VFMUL);
}

FAST_HANDLE(MULSD) {
    SCALAR(rec, meta, as, instruction, operands, SEW::E64, 1, &Assembler::VFMUL);
}

FAST_HANDLE(MINSS) { // TODO: NaN handling
    SCALAR(rec, meta, as, instruction, operands, SEW::E32, 1, &Assembler::VFMIN);
}

FAST_HANDLE(MINSD) {
    SCALAR(rec, meta, as, instruction, operands, SEW::E64, 1, &Assembler::VFMIN);
}

FAST_HANDLE(MAXSS) {
    SCALAR(rec, meta, as, instruction, operands, SEW::E32, 1, &Assembler::VFMAX);
}

FAST_HANDLE(MAXSD) {
    SCALAR(rec, meta, as, instruction, operands, SEW::E64, 1, &Assembler::VFMAX);
}

FAST_HANDLE(CVTSI2SD) {
    x86_size_e gpr_size = rec.getOperandSize(&operands[1]);
    biscuit::Vec dst = rec.getOperandVec(&operands[0]);
    biscuit::GPR src = rec.getOperandGPR(&operands[1]);

    if (gpr_size == X86_SIZE_DWORD) {
        as.FCVT_D_W(ft8, src);
        rec.setVectorState(SEW::E64, 1);
        as.VFMV_SF(dst, ft8);
    } else if (gpr_size == X86_SIZE_QWORD) {
        as.FCVT_D_L(ft8, src);
        rec.setVectorState(SEW::E64, 1);
        as.VFMV_SF(dst, ft8);
    } else {
        UNREACHABLE();
    }

    rec.setOperandVec(&operands[0], dst);
}

FAST_HANDLE(CVTSI2SS) {
    x86_size_e gpr_size = rec.getOperandSize(&operands[1]);
    biscuit::Vec dst = rec.getOperandVec(&operands[0]);
    biscuit::GPR src = rec.getOperandGPR(&operands[1]);

    if (gpr_size == X86_SIZE_DWORD) {
        as.FCVT_S_W(ft8, src);
        rec.setVectorState(SEW::E32, 1);
        as.VFMV_SF(dst, ft8);
    } else if (gpr_size == X86_SIZE_QWORD) {
        as.FCVT_S_L(ft8, src);
        rec.setVectorState(SEW::E32, 1);
        as.VFMV_SF(dst, ft8);
    } else {
        UNREACHABLE();
    }

    rec.setOperandVec(&operands[0], dst);
}

FAST_HANDLE(CVTTSS2SI) {
    x86_size_e gpr_size = rec.getOperandSize(&operands[0]);
    biscuit::Vec src = rec.getOperandVec(&operands[1]);
    biscuit::GPR dst = rec.getOperandGPR(&operands[0]);

    if (gpr_size == X86_SIZE_DWORD) {
        rec.setVectorState(SEW::E32, 1);
        as.VFMV_FS(ft8, src);
        as.FCVT_W_S(dst, ft8, RMode::RTZ);
    } else if (gpr_size == X86_SIZE_QWORD) {
        rec.setVectorState(SEW::E32, 1);
        as.VFMV_FS(ft8, src);
        as.FCVT_L_S(dst, ft8, RMode::RTZ);
    } else {
        UNREACHABLE();
    }

    rec.setOperandGPR(&operands[0], dst);
}

FAST_HANDLE(CVTTSD2SI) {
    x86_size_e gpr_size = rec.getOperandSize(&operands[0]);
    biscuit::Vec src = rec.getOperandVec(&operands[1]);
    biscuit::GPR dst = rec.getOperandGPR(&operands[0]);

    if (gpr_size == X86_SIZE_DWORD) {
        rec.setVectorState(SEW::E64, 1);
        as.VFMV_FS(ft8, src);
        as.FCVT_W_D(dst, ft8, RMode::RTZ);
    } else if (gpr_size == X86_SIZE_QWORD) {
        rec.setVectorState(SEW::E64, 1);
        as.VFMV_FS(ft8, src);
        as.FCVT_L_D(dst, ft8, RMode::RTZ);
    } else {
        UNREACHABLE();
    }

    rec.setOperandGPR(&operands[0], dst);
}
FAST_HANDLE(CVTPD2PS) {
    biscuit::Vec result = rec.scratchVec();
    biscuit::Vec src = rec.getOperandVec(&operands[1]);

    rec.setVectorState(SEW::E32, 4, LMUL::MF2);
    as.VFNCVT_F_F(result, src);
    rec.setVectorState(SEW::E32, 4);
    as.VMV(v0, 0b1100);
    as.VAND(result, result, 0, VecMask::Yes);
    rec.setOperandVec(&operands[0], result);
}

FAST_HANDLE(CVTPS2PD) { // Fuzzed, inaccuracies with NaNs
    biscuit::Vec result = rec.scratchVec();
    biscuit::Vec src = rec.getOperandVec(&operands[1]);

    rec.setVectorState(SEW::E32, 4, LMUL::MF2);
    as.VFWCVT_F_F(result, src);
    rec.setOperandVec(&operands[0], result);
}

FAST_HANDLE(CVTTPS2DQ) { // Fuzzed, returns 0x7FFF'FFFF instead of 0x8000'0000
    biscuit::Vec dst = rec.getOperandVec(&operands[0]);
    biscuit::Vec src = rec.getOperandVec(&operands[1]);

    rec.setVectorState(SEW::E32, 4);
    as.VFCVT_RTZ_X_F(dst, src);

    rec.setOperandVec(&operands[0], dst);
}

FAST_HANDLE(CVTPS2DQ) {
    biscuit::Vec dst = rec.getOperandVec(&operands[0]);
    biscuit::Vec src = rec.getOperandVec(&operands[1]);

    rec.setVectorState(SEW::E32, 4);
    as.VFCVT_X_F(dst, src);

    rec.setOperandVec(&operands[0], dst);
}

FAST_HANDLE(CVTTPD2DQ) { // Fuzzed, same problem as cvttps2dq
    biscuit::Vec dst = rec.getOperandVec(&operands[0]);
    biscuit::Vec src = rec.getOperandVec(&operands[1]);

    rec.setVectorState(SEW::E32, 4, LMUL::MF2);
    as.VFNCVT_RTZ_X_F(dst, src);
    rec.setVectorState(SEW::E32, 4);
    as.VMV(v0, 0b1100);
    as.VAND(dst, dst, 0, VecMask::Yes);

    rec.setOperandVec(&operands[0], dst);
}

FAST_HANDLE(CVTPD2DQ) {
    biscuit::Vec result = rec.scratchVec();
    biscuit::Vec src = rec.getOperandVec(&operands[1]);

    rec.setVectorState(SEW::E32, 4, LMUL::MF2);
    as.VFNCVT_X_F(result, src);

    rec.setVectorState(SEW::E32, 4);
    as.VMV(v0, 0b1100);
    as.VAND(result, result, 0, VecMask::Yes);

    rec.setOperandVec(&operands[0], result);
}

FAST_HANDLE(XGETBV) {
    biscuit::GPR scratch = rec.scratch();
    as.LI(scratch, 0b11);
    rec.setRefGPR(X86_REF_RAX, X86_SIZE_QWORD, scratch);
    rec.setRefGPR(X86_REF_RDX, X86_SIZE_QWORD, x0);
}

FAST_HANDLE(MOVSS) {
    if (operands[0].type == ZYDIS_OPERAND_TYPE_MEMORY) {
        biscuit::Vec src = rec.getOperandVec(&operands[1]);
        rec.setOperandVec(&operands[0], src);
    } else if (operands[0].type == ZYDIS_OPERAND_TYPE_REGISTER) {
        biscuit::Vec dst = rec.getOperandVec(&operands[0]);
        biscuit::Vec src = rec.getOperandVec(&operands[1]);
        if (operands[1].type == ZYDIS_OPERAND_TYPE_MEMORY) {
            rec.setVectorState(SEW::E64, 2);
            as.VMV(dst, 0);
        }
        rec.setVectorState(SEW::E32, 1);
        as.VMV(dst, src);
        rec.setOperandVec(&operands[0], dst);
    } else {
        UNREACHABLE();
    }
}

FAST_HANDLE(CVTSS2SI) {
    x86_size_e gpr_size = rec.getOperandSize(&operands[0]);
    biscuit::Vec src = rec.getOperandVec(&operands[1]);
    biscuit::GPR dst = rec.getOperandGPR(&operands[0]);

    if (gpr_size == X86_SIZE_DWORD) {
        rec.setVectorState(SEW::E32, 1);
        as.VFMV_FS(ft8, src);
        as.FCVT_W_S(dst, ft8);
    } else if (gpr_size == X86_SIZE_QWORD) {
        rec.setVectorState(SEW::E32, 1);
        as.VFMV_FS(ft8, src);
        as.FCVT_L_S(dst, ft8);
    } else {
        UNREACHABLE();
    }

    rec.setOperandGPR(&operands[0], dst);
}

FAST_HANDLE(CVTSD2SI) {
    x86_size_e gpr_size = rec.getOperandSize(&operands[0]);
    biscuit::Vec src = rec.getOperandVec(&operands[1]);
    biscuit::GPR dst = rec.getOperandGPR(&operands[0]);

    rec.setVectorState(SEW::E64, 1);
    if (gpr_size == X86_SIZE_DWORD) {
        as.VFMV_FS(ft8, src);
        as.FCVT_W_D(dst, ft8);
    } else if (gpr_size == X86_SIZE_QWORD) {
        as.VFMV_FS(ft8, src);
        as.FCVT_L_D(dst, ft8);
    } else {
        UNREACHABLE();
    }

    rec.setOperandGPR(&operands[0], dst);
}

FAST_HANDLE(CVTSS2SD) {
    biscuit::Vec dst = rec.getOperandVec(&operands[0]);
    biscuit::Vec src = rec.getOperandVec(&operands[1]);

    rec.setVectorState(SEW::E32, 1);
    as.VFMV_FS(ft8, src);
    as.FCVT_D_S(ft9, ft8);
    rec.setVectorState(SEW::E64, 1);
    as.VFMV_SF(dst, ft9);

    rec.setOperandVec(&operands[0], dst);
}

FAST_HANDLE(CVTSD2SS) {
    biscuit::Vec dst = rec.getOperandVec(&operands[0]);
    biscuit::Vec src = rec.getOperandVec(&operands[1]);

    rec.setVectorState(SEW::E64, 1);
    as.VFMV_FS(ft8, src);
    as.FCVT_S_D(ft9, ft8);
    rec.setVectorState(SEW::E32, 1);
    as.VFMV_SF(dst, ft9);

    rec.setOperandVec(&operands[0], dst);
}

FAST_HANDLE(SQRTSS) {
    SCALAR(rec, meta, as, instruction, operands, SEW::E32, 1, &Assembler::VFSQRT);
}

FAST_HANDLE(SQRTSD) {
    SCALAR(rec, meta, as, instruction, operands, SEW::E64, 1, &Assembler::VFSQRT);
}

FAST_HANDLE(RCPSS) {
    biscuit::Vec temp = rec.scratchVec();
    biscuit::Vec dst = rec.getOperandVec(&operands[0]);
    biscuit::Vec src = rec.getOperandVec(&operands[1]);
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
    rec.setOperandVec(&operands[0], dst);
}

FAST_HANDLE(RSQRTSS) {
    biscuit::Vec temp = rec.scratchVec();
    biscuit::Vec temp2 = rec.scratchVec();
    biscuit::Vec dst = rec.getOperandVec(&operands[0]);
    biscuit::Vec src = rec.getOperandVec(&operands[1]);
    rec.setVectorState(SEW::E32, 1);
    biscuit::GPR ones = rec.scratch();
    as.LI(ones, 0x3F800000);
    as.VMV(temp, ones);
    as.VFSQRT(temp2, src);
    as.VFDIV(dst, temp, temp2);
    rec.setOperandVec(&operands[0], dst);
}

FAST_HANDLE(MOVLHPS) {
    biscuit::Vec dst = rec.getOperandVec(&operands[0]);
    biscuit::Vec src = rec.getOperandVec(&operands[1]);
    rec.setVectorState(SEW::E64, 2);
    if (dst == src) { // VSLIDEUP dst/src overlap limitations
        src = rec.scratchVec();
        as.VMV(src, dst);
    }

    as.VSLIDEUP(dst, src, 1);
    rec.setOperandVec(&operands[0], dst);
}

FAST_HANDLE(ADDSUBPS) {
    // NOTE: using dst directly saves a move but causes potentially
    // torn state if signal happens during vmnand
    biscuit::Vec result = rec.scratchVec();
    biscuit::Vec dst = rec.getOperandVec(&operands[0]);
    biscuit::Vec src = rec.getOperandVec(&operands[1]);
    rec.setVectorState(SEW::E32, 4);
    as.VMV(v0, 0b1010);
    as.VFADD(result, dst, src, VecMask::Yes);
    as.VMNAND(v0, v0, v0);
    as.VFSUB(result, dst, src, VecMask::Yes);
    rec.setOperandVec(&operands[0], result);
}

FAST_HANDLE(ADDSUBPD) {
    // NOTE: using dst directly saves a move but causes potentially
    // torn state if signal happens during vmnand
    biscuit::Vec result = rec.scratchVec();
    biscuit::Vec dst = rec.getOperandVec(&operands[0]);
    biscuit::Vec src = rec.getOperandVec(&operands[1]);
    rec.setVectorState(SEW::E64, 2);
    as.VMV(v0, 0b10);
    as.VFADD(result, dst, src, VecMask::Yes);
    as.VMNAND(v0, v0, v0);
    as.VFSUB(result, dst, src, VecMask::Yes);
    rec.setOperandVec(&operands[0], result);
}

FAST_HANDLE(HADDPD) {
    biscuit::Vec result1 = rec.scratchVec();
    biscuit::Vec result2 = rec.scratchVec();
    biscuit::Vec dst = rec.getOperandVec(&operands[0]);
    biscuit::Vec src = rec.getOperandVec(&operands[1]);

    rec.setVectorState(SEW::E64, 2);
    as.VMV(result1, 0);
    as.VMV(result2, 0);
    as.VFREDUSUM(result1, src, result1);
    as.VFREDUSUM(result2, dst, result2);
    as.VSLIDEUP(result2, result1, 1);

    rec.setOperandVec(&operands[0], result2);
}

FAST_HANDLE(HSUBPD) {
    biscuit::Vec result1 = rec.scratchVec();
    biscuit::Vec result2 = rec.scratchVec();
    biscuit::Vec src_neg = rec.scratchVec();
    biscuit::Vec dst_neg = rec.scratchVec();
    biscuit::Vec dst = rec.getOperandVec(&operands[0]);
    biscuit::Vec src = rec.getOperandVec(&operands[1]);

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

    rec.setOperandVec(&operands[0], result2);
}

void PSIGN(Recompiler& rec, const HandlerMetadata& meta, Assembler& as, ZydisDecodedInstruction& instruction, ZydisDecodedOperand* operands, SEW sew,
           u8 vl) {
    biscuit::Vec result = rec.scratchVec();
    biscuit::Vec dst = rec.getOperandVec(&operands[0]);
    biscuit::Vec src = rec.getOperandVec(&operands[1]);
    rec.setVectorState(sew, vl);

    as.VMSLT(v0, src, x0);
    as.VMV(result, dst);
    as.VRSUB(result, dst, x0, VecMask::Yes);
    as.VMSEQ(v0, src, x0);
    as.VXOR(result, result, result, VecMask::Yes);

    rec.setOperandVec(&operands[0], result);
}

FAST_HANDLE(PSIGND) {
    PSIGN(rec, meta, as, instruction, operands, SEW::E32, 4);
}

FAST_HANDLE(PSIGNW) {
    PSIGN(rec, meta, as, instruction, operands, SEW::E16, 8);
}

FAST_HANDLE(PSIGNB) {
    PSIGN(rec, meta, as, instruction, operands, SEW::E8, 16);
}

FAST_HANDLE(FXSAVE) {
    biscuit::GPR address = rec.lea(&operands[0]);
    rec.writebackDirtyState();
    rec.invalidStateUntilJump();

    as.LI(t0, (u64)&felix86_fxsave);

    as.MV(a0, rec.threadStatePointer());
    as.MV(a1, address);
    as.LI(a2, 0);
    as.JALR(t0);
    rec.restoreRoundingMode();
}

FAST_HANDLE(FXSAVE64) {
    biscuit::GPR address = rec.lea(&operands[0]);
    rec.writebackDirtyState();
    rec.invalidStateUntilJump();

    as.LI(t0, (u64)&felix86_fxsave);

    as.MV(a0, rec.threadStatePointer());
    as.MV(a1, address);
    as.LI(a2, 1);
    as.JALR(t0);
    rec.restoreRoundingMode();
}

FAST_HANDLE(FXRSTOR) {
    biscuit::GPR address = rec.lea(&operands[0]);
    rec.writebackDirtyState();
    rec.invalidStateUntilJump();

    Literal literal((u64)&felix86_fxrstor);
    as.LD(t0, &literal);

    as.MV(a0, rec.threadStatePointer());
    as.MV(a1, address);
    as.LI(a2, 0);
    as.JALR(t0);

    Label end;
    as.J(&end);
    as.Place(&literal);
    as.Bind(&end);
    rec.restoreRoundingMode();
}

FAST_HANDLE(FXRSTOR64) {
    biscuit::GPR address = rec.lea(&operands[0]);
    rec.writebackDirtyState();
    rec.invalidStateUntilJump();

    Literal literal((u64)&felix86_fxrstor);
    as.LD(t0, &literal);

    as.MV(a0, rec.threadStatePointer());
    as.MV(a1, address);
    as.LI(a2, 1);
    as.JALR(t0);

    Label end;
    as.J(&end);
    as.Place(&literal);
    as.Bind(&end);
    rec.restoreRoundingMode();
}

FAST_HANDLE(WRFSBASE) {
    biscuit::GPR reg = rec.getOperandGPR(&operands[0]);

    if (instruction.operand_width == 32) {
        as.SW(reg, offsetof(ThreadState, fsbase), rec.threadStatePointer());
    } else if (instruction.operand_width == 64) {
        as.SD(reg, offsetof(ThreadState, fsbase), rec.threadStatePointer());
    } else {
        UNREACHABLE();
    }
}

FAST_HANDLE(WRGSBASE) {
    biscuit::GPR reg = rec.getOperandGPR(&operands[0]);

    if (instruction.operand_width == 32) {
        as.SW(reg, offsetof(ThreadState, gsbase), rec.threadStatePointer());
    } else if (instruction.operand_width == 64) {
        as.SD(reg, offsetof(ThreadState, gsbase), rec.threadStatePointer());
    } else {
        UNREACHABLE();
    }
}

FAST_HANDLE(XADD_lock_32) {
    bool update_cf = rec.shouldEmitFlag(meta.rip, X86_REF_CF);
    bool update_zf = rec.shouldEmitFlag(meta.rip, X86_REF_ZF);
    bool update_af = rec.shouldEmitFlag(meta.rip, X86_REF_AF);
    bool update_pf = rec.shouldEmitFlag(meta.rip, X86_REF_PF);
    bool update_of = rec.shouldEmitFlag(meta.rip, X86_REF_OF);
    bool update_sf = rec.shouldEmitFlag(meta.rip, X86_REF_SF);
    bool update_any = update_af | update_cf | update_zf | update_pf | update_of | update_sf;

    biscuit::GPR dst = rec.scratch();
    biscuit::GPR src = rec.getOperandGPR(&operands[1]);
    biscuit::GPR address = rec.lea(&operands[0]);
    as.AMOADD_W(Ordering::AQRL, dst, src, address);
    rec.zext(dst, dst, X86_SIZE_DWORD); // amoadd sign extends

    if (update_any) {
        biscuit::GPR result = rec.scratch();
        as.ADD(result, dst, src);

        x86_size_e size = rec.getOperandSize(&operands[0]);

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

    rec.setRefGPR(operands[1].reg.value, X86_SIZE_QWORD, dst);
}

FAST_HANDLE(XADD_lock_64) {
    bool update_cf = rec.shouldEmitFlag(meta.rip, X86_REF_CF);
    bool update_zf = rec.shouldEmitFlag(meta.rip, X86_REF_ZF);
    bool update_af = rec.shouldEmitFlag(meta.rip, X86_REF_AF);
    bool update_pf = rec.shouldEmitFlag(meta.rip, X86_REF_PF);
    bool update_of = rec.shouldEmitFlag(meta.rip, X86_REF_OF);
    bool update_sf = rec.shouldEmitFlag(meta.rip, X86_REF_SF);
    bool update_any = update_af | update_cf | update_zf | update_pf | update_of | update_sf;

    biscuit::GPR dst = rec.scratch();
    biscuit::GPR src = rec.getOperandGPR(&operands[1]);
    biscuit::GPR address = rec.lea(&operands[0]);
    as.AMOADD_D(Ordering::AQRL, dst, src, address);

    if (update_any) {
        biscuit::GPR result = rec.scratch();
        as.ADD(result, dst, src);

        x86_size_e size = rec.getOperandSize(&operands[0]);

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

    rec.setRefGPR(operands[1].reg.value, X86_SIZE_QWORD, dst);
}

FAST_HANDLE(XADD) {
    bool needs_atomic = operands[0].type == ZYDIS_OPERAND_TYPE_MEMORY && (instruction.attributes & ZYDIS_ATTRIB_HAS_LOCK);
    if (needs_atomic) {
        switch (instruction.operand_width) {
        case 32: {
            return fast_XADD_lock_32(rec, meta, as, instruction, operands);
        }
        case 64: {
            return fast_XADD_lock_64(rec, meta, as, instruction, operands);
        }
        default: {
            WARN("Unhandled atomic width: %d for XADD", instruction.operand_width);
        }
        }
    }

    bool update_cf = rec.shouldEmitFlag(meta.rip, X86_REF_CF);
    bool update_zf = rec.shouldEmitFlag(meta.rip, X86_REF_ZF);
    bool update_af = rec.shouldEmitFlag(meta.rip, X86_REF_AF);
    bool update_pf = rec.shouldEmitFlag(meta.rip, X86_REF_PF);
    bool update_of = rec.shouldEmitFlag(meta.rip, X86_REF_OF);
    bool update_sf = rec.shouldEmitFlag(meta.rip, X86_REF_SF);
    // bool update_any = update_af | update_cf | update_zf | update_pf | update_of | update_sf;

    biscuit::GPR result = rec.scratch();
    biscuit::GPR dst = rec.getOperandGPR(&operands[0]);
    biscuit::GPR src = rec.getOperandGPR(&operands[1]);

    as.ADD(result, dst, src);

    x86_size_e size = rec.getOperandSize(&operands[0]);

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
    rec.setOperandGPR(&operands[1], dst);
    rec.setOperandGPR(&operands[0], result);
}

FAST_HANDLE(CMPSD_sse) { // Fuzzed
    u8 imm = rec.getImmediate(&operands[2]) & 0b111;
    biscuit::Vec dst = rec.getOperandVec(&operands[0]);
    biscuit::Vec src = rec.getOperandVec(&operands[1]);

    rec.setVectorState(SEW::E64, 1);
    as.VFMV_FS(ft8, dst);
    as.VFMV_FS(ft9, src);

    biscuit::GPR result = rec.scratch();
    switch ((CmpPredicate)imm) {
    case EQ_OQ: {
        as.FEQ_D(result, ft8, ft9);
        break;
    }
    case LT_OS: {
        as.FLT_D(result, ft8, ft9);
        break;
    }
    case LE_OS: {
        as.FLE_D(result, ft8, ft9);
        break;
    }
    case UNORD_Q: {
        // Check if it's a qNan or sNan, check bit 8 and 9
        biscuit::GPR nan = rec.scratch();
        biscuit::GPR mask = rec.scratch();
        as.FCLASS_D(result, ft8);
        as.FCLASS_D(nan, ft9);
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
        as.FCLASS_D(result, ft8);
        as.FCLASS_D(nan, ft9);
        as.OR(result, result, nan);
        as.LI(mask, 0b11 << 8);
        as.AND(result, result, mask);
        as.SNEZ(result, result);
        rec.popScratch();
        rec.popScratch();

        // After checking if either are nan, also check if they are equal
        as.FEQ_D(nan, ft8, ft9);
        as.XORI(nan, nan, 1);
        as.OR(result, result, nan);
        break;
    }
    case NLT_US: {
        biscuit::GPR nan = rec.scratch();
        biscuit::GPR mask = rec.scratch();
        as.FCLASS_D(result, ft8);
        as.FCLASS_D(nan, ft9);
        as.OR(result, result, nan);
        as.LI(mask, 0b11 << 8);
        as.AND(result, result, mask);
        as.SNEZ(result, result);
        rec.popScratch();
        rec.popScratch();

        // After checking if either are nan, also check if they are equal
        as.FLT_D(nan, ft8, ft9);
        as.XORI(nan, nan, 1);
        as.OR(result, result, nan);
        break;
    }
    case NLE_US: {
        biscuit::GPR nan = rec.scratch();
        biscuit::GPR mask = rec.scratch();
        as.FCLASS_D(result, ft8);
        as.FCLASS_D(nan, ft9);
        as.OR(result, result, nan);
        as.LI(mask, 0b11 << 8);
        as.AND(result, result, mask);
        as.SNEZ(result, result);
        rec.popScratch();
        rec.popScratch();

        // After checking if either are nan, also check if they are equal
        as.FLE_D(nan, ft8, ft9);
        as.XORI(nan, nan, 1);
        as.OR(result, result, nan);
        break;
    }
    case ORD_Q: {
        // Check if neither are NaN
        biscuit::GPR nan = rec.scratch();
        biscuit::GPR mask = rec.scratch();
        as.FCLASS_D(result, ft8);
        as.FCLASS_D(nan, ft9);
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

    rec.setOperandVec(&operands[0], dst);
}

FAST_HANDLE(CMPSS) { // Fuzzed
    u8 imm = rec.getImmediate(&operands[2]) & 0b111;
    biscuit::Vec dst = rec.getOperandVec(&operands[0]);
    biscuit::Vec src = rec.getOperandVec(&operands[1]);

    rec.setVectorState(SEW::E32, 1);
    as.VFMV_FS(ft8, dst);
    as.VFMV_FS(ft9, src);

    biscuit::GPR result = rec.scratch();
    switch ((CmpPredicate)imm) {
    case EQ_OQ: {
        as.FEQ_S(result, ft8, ft9);
        break;
    }
    case LT_OS: {
        as.FLT_S(result, ft8, ft9);
        break;
    }
    case LE_OS: {
        as.FLE_S(result, ft8, ft9);
        break;
    }
    case UNORD_Q: {
        // Check if it's a qNan or sNan, check bit 8 and 9
        biscuit::GPR nan = rec.scratch();
        biscuit::GPR mask = rec.scratch();
        as.FCLASS_S(result, ft8);
        as.FCLASS_S(nan, ft9);
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
        as.FCLASS_S(result, ft8);
        as.FCLASS_S(nan, ft9);
        as.OR(result, result, nan);
        as.LI(mask, 0b11 << 8);
        as.AND(result, result, mask);
        as.SNEZ(result, result);
        rec.popScratch();
        rec.popScratch();

        // After checking if either are nan, also check if they are equal
        as.FEQ_S(nan, ft8, ft9);
        as.XORI(nan, nan, 1);
        as.OR(result, result, nan);
        break;
    }
    case NLT_US: {
        biscuit::GPR nan = rec.scratch();
        biscuit::GPR mask = rec.scratch();
        as.FCLASS_S(result, ft8);
        as.FCLASS_S(nan, ft9);
        as.OR(result, result, nan);
        as.LI(mask, 0b11 << 8);
        as.AND(result, result, mask);
        as.SNEZ(result, result);
        rec.popScratch();
        rec.popScratch();

        as.FLE_S(nan, ft9, ft8);
        as.OR(result, result, nan);
        break;
    }
    case NLE_US: {
        biscuit::GPR nan = rec.scratch();
        biscuit::GPR mask = rec.scratch();
        as.FCLASS_S(result, ft8);
        as.FCLASS_S(nan, ft9);
        as.OR(result, result, nan);
        as.LI(mask, 0b11 << 8);
        as.AND(result, result, mask);
        as.SNEZ(result, result);
        rec.popScratch();
        rec.popScratch();

        // After checking if either are nan, also check if they are equal
        as.FLT_S(nan, ft9, ft8);
        as.OR(result, result, nan);
        break;
    }
    case ORD_Q: {
        // Check if neither are NaN
        biscuit::GPR nan = rec.scratch();
        biscuit::GPR mask = rec.scratch();
        as.FCLASS_S(result, ft8);
        as.FCLASS_S(nan, ft9);
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

    rec.setOperandVec(&operands[0], dst);
}

FAST_HANDLE(CMPSD) {
    if (instruction.meta.isa_set == ZYDIS_ISA_SET_SSE2) {
        fast_CMPSD_sse(rec, meta, as, instruction, operands);
    } else if (instruction.meta.isa_set == ZYDIS_ISA_SET_I386) {
        fast_CMPSD_string(rec, meta, as, instruction, operands);
    } else {
        UNREACHABLE();
    }
}

FAST_HANDLE(CMC) {
    biscuit::GPR cf = rec.flagW(X86_REF_CF);
    as.XORI(cf, cf, 1);
}

FAST_HANDLE(RCL) {
    biscuit::GPR temp_count = rec.scratch();
    biscuit::GPR dst = rec.getOperandGPR(&operands[0]);
    biscuit::GPR dst_temp = rec.scratch();
    biscuit::GPR shift = rec.getOperandGPR(&operands[1]);
    biscuit::GPR cf = rec.flagW(X86_REF_CF);
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

    rec.disableSignals();

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

    rec.enableSignals();

    if (rec.shouldEmitFlag(meta.rip, X86_REF_OF)) {
        biscuit::GPR of = rec.flagW(X86_REF_OF);
        as.SRLI(of, dst_temp, instruction.operand_width - 1);
        as.ANDI(of, of, 1);
        as.XOR(of, of, cf);
    }

    rec.setOperandGPR(&operands[0], dst_temp);
}

FAST_HANDLE(RCR) {
    biscuit::GPR dst = rec.getOperandGPR(&operands[0]);
    biscuit::GPR dst_temp = rec.scratch();
    biscuit::GPR shift = rec.getOperandGPR(&operands[1]);
    biscuit::GPR cf = rec.flagW(X86_REF_CF);
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

    if (rec.shouldEmitFlag(meta.rip, X86_REF_OF)) {
        biscuit::GPR of = rec.flagW(X86_REF_OF);
        as.SRLI(of, dst_temp, instruction.operand_width - 1);
        as.ANDI(of, of, 1);
        as.XOR(of, of, cf);
    }

    rec.disableSignals();

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

    rec.enableSignals();

    rec.setOperandGPR(&operands[0], dst_temp);
}

FAST_HANDLE(SHLD) {
    u8 operand_size = instruction.operand_width;
    u8 mask = operand_size == 64 ? 63 : 31;
    biscuit::GPR dst = rec.getOperandGPR(&operands[0]);
    biscuit::GPR src = rec.getOperandGPR(&operands[1]);
    biscuit::GPR shift = rec.getOperandGPR(&operands[2]); // it's ok to modify if reg, since it can only be cl and that comes as scratch
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

    if (rec.shouldEmitFlag(meta.rip, X86_REF_CF)) {
        biscuit::GPR cf = rec.flagW(X86_REF_CF);
        as.SRL(cf, dst, shift_sub);
        as.ANDI(cf, cf, 1);
    }

    if (rec.shouldEmitFlag(meta.rip, X86_REF_OF)) {
        biscuit::GPR of = rec.flagW(X86_REF_OF);
        as.XOR(of, result, dst);
        as.SRLI(of, of, operand_size - 1);
        as.ANDI(of, of, 1);
    }

    if (rec.shouldEmitFlag(meta.rip, X86_REF_ZF)) {
        rec.updateZero(result, rec.zydisToSize(operand_size));
    }

    if (rec.shouldEmitFlag(meta.rip, X86_REF_SF)) {
        rec.updateSign(result, rec.zydisToSize(operand_size));
    }

    if (rec.shouldEmitFlag(meta.rip, X86_REF_PF)) {
        rec.updateParity(result);
    }

    as.Bind(&end);
    rec.setOperandGPR(&operands[0], result);
}

FAST_HANDLE(SHRD) {
    u8 operand_size = instruction.operand_width;
    u8 mask = operand_size == 64 ? 63 : 31;
    biscuit::GPR dst = rec.getOperandGPR(&operands[0]);
    biscuit::GPR src = rec.getOperandGPR(&operands[1]);
    biscuit::GPR shift = rec.getOperandGPR(&operands[2]); // it's ok to modify if reg, since it can only be cl and that comes as scratch
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

    if (rec.shouldEmitFlag(meta.rip, X86_REF_CF)) {
        biscuit::GPR cf = rec.flagW(X86_REF_CF);
        as.ADDI(shift, shift, -1);
        as.SRL(cf, dst, shift);
        as.ANDI(cf, cf, 1);
    }

    if (rec.shouldEmitFlag(meta.rip, X86_REF_OF)) {
        biscuit::GPR of = rec.flagW(X86_REF_OF);
        as.XOR(of, result, dst);
        as.SRLI(of, of, operand_size - 1);
        as.ANDI(of, of, 1);
    }

    if (rec.shouldEmitFlag(meta.rip, X86_REF_ZF)) {
        rec.updateZero(result, rec.zydisToSize(operand_size));
    }

    if (rec.shouldEmitFlag(meta.rip, X86_REF_SF)) {
        rec.updateSign(result, rec.zydisToSize(operand_size));
    }

    if (rec.shouldEmitFlag(meta.rip, X86_REF_PF)) {
        rec.updateParity(result);
    }

    as.Bind(&end);
    rec.setOperandGPR(&operands[0], result);
}

void PCMPXSTRX(Recompiler& rec, const HandlerMetadata& meta, Assembler& as, ZydisDecodedInstruction& instruction, ZydisDecodedOperand* operands,
               pcmpxstrx type) {
    rec.writebackDirtyState();
    rec.invalidStateUntilJump();

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

    rec.call((u64)felix86_pcmpxstrx);
    rec.restoreRoundingMode();
}

FAST_HANDLE(PCMPISTRI) {
    PCMPXSTRX(rec, meta, as, instruction, operands, pcmpxstrx::ImplicitIndex);
}

FAST_HANDLE(PCMPESTRI) {
    PCMPXSTRX(rec, meta, as, instruction, operands, pcmpxstrx::ExplicitIndex);
}

FAST_HANDLE(PCMPISTRM) {
    PCMPXSTRX(rec, meta, as, instruction, operands, pcmpxstrx::ImplicitMask);
}

FAST_HANDLE(PCMPESTRM) {
    PCMPXSTRX(rec, meta, as, instruction, operands, pcmpxstrx::ExplicitMask);
}

FAST_HANDLE(STMXCSR) {
    biscuit::GPR mxcsr = rec.scratch();
    biscuit::GPR address = rec.scratch();
    // TODO: are overflow/inexact/underflow etc flags set in fcsr? if then we need to copy them over
    as.ADDI(address, rec.threadStatePointer(), offsetof(ThreadState, mxcsr));
    as.LWU(mxcsr, 0, address);
    rec.setOperandGPR(&operands[0], mxcsr);
}

FAST_HANDLE(LDMXCSR) {
    biscuit::GPR src = rec.getOperandGPR(&operands[0]);
    biscuit::GPR rc = rec.scratch(); // rounding control
    biscuit::GPR temp = rec.scratch();
    biscuit::GPR address = rec.scratch();

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
    as.ADDI(address, rec.threadStatePointer(), offsetof(ThreadState, rmode));
    as.SB(temp, 0, address);

    as.ADDI(address, rec.threadStatePointer(), offsetof(ThreadState, mxcsr));
    as.SW(src, 0, address);
}

FAST_HANDLE(CVTDQ2PD) {
    biscuit::Vec scratch = rec.scratchVec();
    biscuit::Vec src = rec.getOperandVec(&operands[1]);

    rec.setVectorState(SEW::E32, 4, LMUL::MF2);
    as.VFWCVT_F_X(scratch, src);

    rec.setOperandVec(&operands[0], scratch);
}

FAST_HANDLE(CVTDQ2PS) {
    biscuit::Vec scratch = rec.scratchVec();
    biscuit::Vec src = rec.getOperandVec(&operands[1]);

    rec.setVectorState(SEW::E32, 4);
    as.VFCVT_F_X(scratch, src);

    rec.setOperandVec(&operands[0], scratch);
}

FAST_HANDLE(EXTRACTPS) {
    u8 imm = rec.getImmediate(&operands[2]) & 0b11;
    biscuit::GPR dst = rec.scratch();
    biscuit::Vec src = rec.getOperandVec(&operands[1]);
    biscuit::Vec tmp = rec.scratchVec();

    rec.setVectorState(SEW::E32, 4);
    as.VSLIDEDOWN(tmp, src, imm);
    as.VMV_XS(dst, tmp);

    rec.setOperandGPR(&operands[0], dst);
}

FAST_HANDLE(INSERTPS) {
    u8 immediate = rec.getImmediate(&operands[2]);
    biscuit::Vec dst = rec.getOperandVec(&operands[0]);
    biscuit::Vec src = rec.getOperandVec(&operands[1]);
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

    rec.setOperandVec(&operands[0], dst);
}

FAST_HANDLE(PREFETCHT0) {
    // NOP
}

FAST_HANDLE(PREFETCHT1) {
    // NOP
}

FAST_HANDLE(PREFETCHT2) {
    // NOP
}

FAST_HANDLE(PREFETCHNTA) {
    // NOP
}

FAST_HANDLE(PUSHFQ) {
    biscuit::GPR src = rec.getFlags();
    biscuit::GPR rsp = rec.getRefGPR(X86_REF_RSP, rec.stackWidth());
    as.ADDI(rsp, rsp, -rec.stackPointerSize());
    rec.setRefGPR(X86_REF_RSP, rec.stackWidth(), rsp);
    rec.writeMemory(src, rsp, 0, rec.stackWidth());
}

FAST_HANDLE(POPFQ) {
    biscuit::GPR flags = rec.scratch();
    biscuit::GPR rsp = rec.getRefGPR(X86_REF_RSP, rec.stackWidth());
    as.LD(flags, 0, rsp);
    as.ADDI(rsp, rsp, rec.stackPointerSize());
    rec.setRefGPR(X86_REF_RSP, rec.stackWidth(), rsp);

    biscuit::GPR cf = rec.flagW(X86_REF_CF);
    biscuit::GPR zf = rec.flagW(X86_REF_ZF);
    biscuit::GPR sf = rec.flagW(X86_REF_SF);
    biscuit::GPR of = rec.flagW(X86_REF_OF);
    biscuit::GPR temp = rec.scratch();

    as.ANDI(cf, flags, 1);

    biscuit::GPR pf = rec.scratch();
    as.SRLI(pf, flags, 2);
    as.ANDI(pf, pf, 1);
    as.SB(pf, offsetof(ThreadState, pf), rec.threadStatePointer());

    biscuit::GPR af = rec.scratch();
    as.SRLI(af, flags, 4);
    as.ANDI(af, af, 1);
    as.SB(af, offsetof(ThreadState, af), rec.threadStatePointer());

    as.SRLI(zf, flags, 6);
    as.ANDI(zf, zf, 1);

    as.SRLI(sf, flags, 7);
    as.ANDI(sf, sf, 1);

    as.SRLI(temp, flags, 10);
    as.ANDI(temp, temp, 1);
    as.SB(temp, offsetof(ThreadState, df), rec.threadStatePointer());

    as.SRLI(of, flags, 11);
    as.ANDI(of, of, 1);

    // CPUID bit may have been modified, which we need to emulate because this is how some programs detect CPUID support
    as.SRLI(temp, flags, 21);
    as.ANDI(temp, temp, 1);
    as.SB(temp, offsetof(ThreadState, cpuid_bit), rec.threadStatePointer());
}

FAST_HANDLE(PUSHFD) {
    fast_PUSHFQ(rec, meta, as, instruction, operands);
}

FAST_HANDLE(POPFD) {
    fast_POPFQ(rec, meta, as, instruction, operands);
}

FAST_HANDLE(MOVDDUP) {
    biscuit::Vec result = rec.scratchVec();
    biscuit::Vec iota = rec.scratchVec();
    biscuit::Vec src = rec.getOperandVec(&operands[1]);

    rec.setVectorState(SEW::E64, 2);
    as.VMV(iota, 0);
    as.VRGATHER(result, src, iota);

    rec.setOperandVec(&operands[0], result);
}

FAST_HANDLE(PSADBW) {
    biscuit::Vec min = rec.scratchVec();
    biscuit::Vec max = rec.scratchVec();
    biscuit::Vec sub = rec.scratchVec();
    biscuit::Vec sub_upper = rec.scratchVec();
    biscuit::Vec result = rec.scratchVec();
    biscuit::Vec result2 = rec.scratchVec();
    biscuit::Vec result2_up = rec.scratchVec();
    biscuit::Vec dst = rec.getOperandVec(&operands[0]);
    biscuit::Vec src = rec.getOperandVec(&operands[1]);

    rec.setVectorState(SEW::E8, 16);
    as.VMV(result, 0);
    as.VMV(result2, 0);
    as.VMIN(min, dst, src);
    as.VMAX(max, dst, src);
    as.VSUB(sub, max, min);
    as.VSLIDEDOWN(sub_upper, sub, 8);

    rec.setVectorState(SEW::E8, 8, LMUL::MF2);
    as.VWREDSUMU(result, sub, result);
    as.VWREDSUMU(result2, sub_upper, result2);

    rec.setVectorState(SEW::E64, 2);
    as.VSLIDE1UP(result2_up, result2, x0);
    as.VOR(dst, result2_up, result);

    rec.setOperandVec(&operands[0], dst);
}

FAST_HANDLE(PAVGB) {
    biscuit::Vec dst = rec.getOperandVec(&operands[0]);
    biscuit::Vec src = rec.getOperandVec(&operands[1]);
    rec.setVectorState(SEW::E8, 16);
    as.VAADDU(dst, dst, src);
    rec.setOperandVec(&operands[0], dst);
}

FAST_HANDLE(PAVGW) {
    biscuit::Vec dst = rec.getOperandVec(&operands[0]);
    biscuit::Vec src = rec.getOperandVec(&operands[1]);
    rec.setVectorState(SEW::E16, 8);
    as.VAADDU(dst, dst, src);
    rec.setOperandVec(&operands[0], dst);
}

FAST_HANDLE(CMPXCHG16B) {
    biscuit::GPR address = rec.lea(&operands[0]);
    if (Extensions::Zacas) {
        WARN_ONCE("cmpxchg16b with zacas, untested, please report results");
        // We are the luckiest emulator alive!
        // AMOCAS.Q needs a register group (meaning, 2 registers side by side like t0, t1) to work
        (void)rec.scratch(); // waste a scratch so we pick 28-29 and 30-31
        biscuit::GPR rax = rec.getRefGPR(X86_REF_RAX, X86_SIZE_QWORD);
        biscuit::GPR rdx = rec.getRefGPR(X86_REF_RDX, X86_SIZE_QWORD);
        biscuit::GPR rbx = rec.getRefGPR(X86_REF_RBX, X86_SIZE_QWORD);
        biscuit::GPR rcx = rec.getRefGPR(X86_REF_RCX, X86_SIZE_QWORD);
        biscuit::GPR rax_t = rec.scratch();
        biscuit::GPR rdx_t = rec.scratch();
        biscuit::GPR rbx_t = rec.scratch();
        biscuit::GPR rcx_t = rec.scratch();
        ASSERT(rax_t == x28 && rdx_t == x29 && rbx_t == x30 && rcx_t == x31); // in case we change the order
        as.MV(rax_t, rax);
        as.MV(rdx_t, rdx);
        as.MV(rbx_t, rbx);
        as.MV(rcx_t, rcx);
        as.AMOCAS_Q(Ordering::AQRL, rax_t, rbx_t, address);

        // Real value is now loaded into rdx_t:rax_t. Compare with rdx:rax to set the zero flag
        // We can overwrite the rbx_t/rcx_t scratches now
        biscuit::GPR zf = rec.flagW(X86_REF_ZF);
        as.XOR(rbx_t, rax_t, rax);
        as.XOR(rcx_t, rdx_t, rdx);
        as.OR(rbx_t, rbx_t, rcx_t);
        as.SEQZ(zf, rbx_t);

        rec.setRefGPR(X86_REF_RAX, X86_SIZE_QWORD, rax_t);
        rec.setRefGPR(X86_REF_RDX, X86_SIZE_QWORD, rdx_t);
    } else {
        // TODO: using a lock would at least make this atomic with respect to other cmpxchg16b instructions
        WARN_ONCE("This program uses CMPXCHG16B and your chip doesn't have the Zacas extension, execution may be unstable");
        biscuit::GPR rax = rec.getRefGPR(X86_REF_RAX, X86_SIZE_QWORD);
        biscuit::GPR rdx = rec.getRefGPR(X86_REF_RDX, X86_SIZE_QWORD);
        biscuit::GPR rbx = rec.getRefGPR(X86_REF_RBX, X86_SIZE_QWORD);
        biscuit::GPR rcx = rec.getRefGPR(X86_REF_RCX, X86_SIZE_QWORD);
        biscuit::GPR mem0 = rec.scratch();
        biscuit::GPR mem1 = rec.scratch();

        rec.readMemory(mem0, address, 0, X86_SIZE_QWORD);
        rec.readMemory(mem1, address, 8, X86_SIZE_QWORD);

        Label not_equal;
        biscuit::GPR zf = rec.flagW(X86_REF_ZF);
        as.MV(zf, x0); // assume not equal
        as.BNE(mem0, rax, &not_equal);
        as.BNE(mem1, rdx, &not_equal);

        as.LI(zf, 1);
        rec.writeMemory(rbx, address, 0, X86_SIZE_QWORD);
        rec.writeMemory(rcx, address, 8, X86_SIZE_QWORD);

        as.Bind(&not_equal);

        as.MV(rax, mem0);
        as.MV(rdx, mem1);

        rec.setRefGPR(X86_REF_RAX, X86_SIZE_QWORD, rax);
        rec.setRefGPR(X86_REF_RDX, X86_SIZE_QWORD, rdx);
    }
}

FAST_HANDLE(PAUSE) {
    if (Extensions::Zihintpause) {
        as.PAUSE();
    }
}

// This is a pseudo-instruction that we generate in our thunked guest libraries to basically
// notify the recompiler that whatever follows here is thunked code and it should call the equivalent
// host function.
// After this instruction (which must be 3 bytes as it always is INVLPG[RAX], see generator.cpp) and a RET follows
// a null terminated string with the name of the host function we want to call. We pass this name to
// Thunks::generateTrampoline to generate us a trampoline to go boing.
// After this INVLPG there will always be a RET, to simulate what a normal function would do
FAST_HANDLE(INVLPG) {
    if (!g_thunking) {
        ERROR("INVLPG while not thunking?");
    }

    enum {
        INVLPG_GENERATE_TRAMPOLINE = ZYDIS_REGISTER_RAX,
        INVLPG_THUNK_CONSTRUCTOR = ZYDIS_REGISTER_RBX,
    };

    ASSERT_MSG(instruction.length == 3, "Hit INVLPG instruction but it's not 3 bytes?");
    ASSERT(operands[0].type == ZYDIS_OPERAND_TYPE_MEMORY);

    switch (operands[0].mem.base) {
    case INVLPG_GENERATE_TRAMPOLINE: {
        const char* address = (const char*)(meta.rip.raw() + instruction.length + 1); // also skip a RET -> 1 byte
        VERBOSE("Generating trampoline for %s", address);
        void* trampoline = Thunks::generateTrampoline(rec, as, address);
        ASSERT_MSG(trampoline != nullptr, "Failed to install trampoline for \"%s\" (%lx)", address, (u64)address);
        break;
    }
    case INVLPG_THUNK_CONSTRUCTOR: {
        u8* signature = (u8*)(meta.rip.raw() + instruction.length + 1);
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
    default: {
        ERROR("Unknown INVLPG instruction base operand?");
        break;
    }
    }
}