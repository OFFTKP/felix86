#include <Zydis/Zydis.h>
#include "felix86/v2/recompiler.hpp"

void felix86_syscall(ThreadState* state);

void felix86_cpuid(ThreadState* state);

#define FAST_HANDLE(name)                                                                                                                            \
    void fast_##name(Recompiler& rec, const HandlerMetadata& meta, ZydisDecodedInstruction& instruction, ZydisDecodedOperand* operands)

#define AS (rec.getAssembler())

#define IS_MMX (instruction.attributes & (ZYDIS_ATTRIB_FPU_STATE_CR | ZYDIS_ATTRIB_FPU_STATE_CW))

#define HAS_REP (instruction.attributes & (ZYDIS_ATTRIB_HAS_REP | ZYDIS_ATTRIB_HAS_REPZ | ZYDIS_ATTRIB_HAS_REPNZ))

void SetCmpFlags(const HandlerMetadata& meta, Recompiler& rec, biscuit::GPR dst, biscuit::GPR src, biscuit::GPR result, x86_size_e size,
                 bool zext_src) {
    u64 sign_mask = rec.getSignMask(size);

    if (rec.shouldEmitFlag(meta.rip, X86_REF_CF)) {
        biscuit::GPR cf = rec.flagW(X86_REF_CF);
        if (zext_src) {
            rec.zext(cf, src, size);
            AS.SLTU(cf, dst, cf);
        } else {
            AS.SLTU(cf, dst, src);
        }
    }

    if (rec.shouldEmitFlag(meta.rip, X86_REF_PF)) {
        rec.updateParity(result);
    }

    if (rec.shouldEmitFlag(meta.rip, X86_REF_AF)) {
        biscuit::GPR af = rec.flagW(X86_REF_AF);
        biscuit::GPR scratch = rec.scratch();
        AS.ANDI(af, src, 0xF);
        AS.ANDI(scratch, dst, 0xF);
        AS.SLTU(af, scratch, af);
        rec.popScratch();
    }

    if (rec.shouldEmitFlag(meta.rip, X86_REF_ZF)) {
        rec.updateZero(result, size);
    }

    if (rec.shouldEmitFlag(meta.rip, X86_REF_SF)) {
        rec.updateSign(result, size);
    }

    if (rec.shouldEmitFlag(meta.rip, X86_REF_OF)) {
        biscuit::GPR scratch = rec.scratch();
        biscuit::GPR of = rec.flagW(X86_REF_OF);
        AS.XOR(scratch, dst, src);
        AS.XOR(of, dst, result);
        AS.AND(of, of, scratch);
        AS.LI(scratch, sign_mask);
        AS.AND(of, of, scratch);
        AS.SNEZ(of, of);
        rec.popScratch();
    }
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

void VEC_function(Recompiler& rec, const HandlerMetadata& meta, ZydisDecodedInstruction& instruction, ZydisDecodedOperand* operands, u64 func) {
    x86_ref_e dst_ref = rec.zydisToRef(operands[0].reg.value);
    ASSERT(dst_ref >= X86_REF_XMM0 && dst_ref <= X86_REF_XMM15);

    biscuit::GPR temp;
    if (operands[1].type == ZYDIS_OPERAND_TYPE_MEMORY) {
        temp = rec.lea(&operands[1]);
    }
    rec.writebackDirtyState();

    AS.LI(t0, func);

    AS.ADDI(a0, rec.threadStatePointer(), offsetof(ThreadState, xmm) + (dst_ref - X86_REF_XMM0) * 16);

    if (operands[1].type == ZYDIS_OPERAND_TYPE_REGISTER) {
        x86_ref_e src_ref = rec.zydisToRef(operands[1].reg.value);
        ASSERT(src_ref >= X86_REF_XMM0 && src_ref <= X86_REF_XMM15);
        AS.ADDI(a1, rec.threadStatePointer(), offsetof(ThreadState, xmm) + (src_ref - X86_REF_XMM0) * 16);
    } else {
        AS.MV(a1, temp);
    }

    AS.JALR(t0);
    rec.restoreRoundingMode();
}

void is_overflow_add(Recompiler& rec, biscuit::GPR of, biscuit::GPR lhs, biscuit::GPR rhs, biscuit::GPR result, u64 sign_mask) {
    // TODO: replace with is_overflow_adc, I think it works in this case too
    biscuit::GPR scratch = rec.scratch();
    AS.XOR(scratch, result, lhs);
    AS.XOR(of, result, rhs);
    AS.AND(of, of, scratch);
    AS.LI(scratch, sign_mask);
    AS.AND(of, of, scratch);
    AS.SNEZ(of, of);
    rec.popScratch();
}

// ((s & d) | ((~res) & (s | d))), xor top 2 bits
void is_overflow_adc(Recompiler& rec, biscuit::GPR of, biscuit::GPR lhs, biscuit::GPR rhs, biscuit::GPR result, int size) {
    biscuit::GPR scratch = rec.scratch();
    AS.OR(of, lhs, rhs);
    AS.NOT(scratch, result);
    AS.AND(of, scratch, of);
    AS.AND(scratch, lhs, rhs);
    AS.OR(of, of, scratch);
    AS.SRLI(scratch, of, size - 2);
    AS.SRLI(of, of, size - 1);
    AS.XOR(of, of, scratch);
    AS.ANDI(of, of, 1);
    rec.popScratch();
}

// (res & (~d | s)) | (~d & s), xor top 2 bits
void is_overflow_sub(Recompiler& rec, biscuit::GPR of, biscuit::GPR lhs, biscuit::GPR rhs, biscuit::GPR result, int size) {
    biscuit::GPR scratch = rec.scratch();
    AS.NOT(scratch, lhs);
    AS.OR(of, scratch, rhs);
    AS.AND(of, of, result);
    AS.AND(scratch, scratch, rhs);
    AS.OR(of, of, scratch);
    AS.SRLI(scratch, of, size - 2);
    AS.SRLI(of, of, size - 1);
    AS.XOR(of, of, scratch);
    AS.ANDI(of, of, 1);
    rec.popScratch();
}

FAST_HANDLE(MOV) {
    biscuit::GPR src = rec.getOperandGPR(&operands[1]);
    rec.setOperandGPR(&operands[0], src);
}

FAST_HANDLE(ADD) {
    biscuit::GPR result = rec.scratch();
    biscuit::GPR src = rec.getOperandGPR(&operands[1]);
    biscuit::GPR dst = rec.getOperandGPR(&operands[0]);

    AS.ADD(result, dst, src);

    x86_size_e size = rec.getOperandSize(&operands[0]);
    u64 sign_mask = rec.getSignMask(size);

    if (rec.shouldEmitFlag(meta.rip, X86_REF_CF)) {
        biscuit::GPR cf = rec.flagW(X86_REF_CF);
        rec.zext(cf, result, size);
        AS.SLTU(cf, cf, dst);
    }

    if (rec.shouldEmitFlag(meta.rip, X86_REF_PF)) {
        rec.updateParity(result);
    }

    if (rec.shouldEmitFlag(meta.rip, X86_REF_AF)) {
        biscuit::GPR af = rec.flagW(X86_REF_AF);
        biscuit::GPR scratch = rec.scratch();
        AS.ANDI(af, result, 0xF);
        AS.ANDI(scratch, dst, 0xF);
        AS.SLTU(af, af, scratch);
        rec.popScratch();
    }

    if (rec.shouldEmitFlag(meta.rip, X86_REF_ZF)) {
        rec.updateZero(result, size);
    }

    if (rec.shouldEmitFlag(meta.rip, X86_REF_SF)) {
        rec.updateSign(result, size);
    }

    if (rec.shouldEmitFlag(meta.rip, X86_REF_OF)) {
        biscuit::GPR of = rec.flagW(X86_REF_OF);
        is_overflow_add(rec, of, dst, src, result, sign_mask);
    }

    rec.setOperandGPR(&operands[0], result);
}

FAST_HANDLE(SUB) {
    biscuit::GPR result = rec.scratch();
    biscuit::GPR src = rec.getOperandGPR(&operands[1]);
    biscuit::GPR dst = rec.getOperandGPR(&operands[0]);

    AS.SUB(result, dst, src);

    x86_size_e size = rec.getOperandSize(&operands[0]);

    if (rec.shouldEmitFlag(meta.rip, X86_REF_CF)) {
        biscuit::GPR cf = rec.flagW(X86_REF_CF);
        if (operands[1].type == ZYDIS_OPERAND_TYPE_IMMEDIATE && size != X86_SIZE_QWORD) {
            rec.zext(cf, src, size);
            AS.SLTU(cf, dst, cf);
        } else {
            AS.SLTU(cf, dst, src);
        }
    }

    if (rec.shouldEmitFlag(meta.rip, X86_REF_PF)) {
        rec.updateParity(result);
    }

    if (rec.shouldEmitFlag(meta.rip, X86_REF_AF)) {
        biscuit::GPR af = rec.flagW(X86_REF_AF);
        biscuit::GPR scratch = rec.scratch();
        AS.ANDI(af, src, 0xF);
        AS.ANDI(scratch, dst, 0xF);
        AS.SLTU(af, scratch, af);
        rec.popScratch();
    }

    if (rec.shouldEmitFlag(meta.rip, X86_REF_ZF)) {
        rec.updateZero(result, size);
    }

    if (rec.shouldEmitFlag(meta.rip, X86_REF_SF)) {
        rec.updateSign(result, size);
    }

    if (rec.shouldEmitFlag(meta.rip, X86_REF_OF)) {
        biscuit::GPR of = rec.flagW(X86_REF_OF);
        is_overflow_sub(rec, of, dst, src, result, rec.getBitSize(size));
    }

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

    AS.SUB(result, dst, src);
    AS.SUB(result_2, result, cf);

    if (rec.shouldEmitFlag(meta.rip, X86_REF_PF)) {
        rec.updateParity(result_2);
    }

    if (rec.shouldEmitFlag(meta.rip, X86_REF_AF)) {
        biscuit::GPR af = rec.flagW(X86_REF_AF);
        biscuit::GPR scratch = rec.scratch();
        AS.ANDI(af, src, 0xF);
        AS.ANDI(scratch, dst, 0xF);
        AS.SLTU(af, scratch, af);
        AS.ANDI(scratch, result, 0xF);
        AS.SLTU(scratch, scratch, cf);
        AS.OR(af, af, scratch);
        rec.popScratch();
    }

    if (rec.shouldEmitFlag(meta.rip, X86_REF_OF)) {
        biscuit::GPR scratch = rec.scratch();
        biscuit::GPR scratch2 = rec.scratch();
        biscuit::GPR of = rec.flagW(X86_REF_OF);
        AS.LI(scratch2, sign_mask);
        AS.XOR(scratch, dst, src);
        AS.XOR(of, dst, result);
        AS.AND(of, of, scratch);
        AS.AND(of, of, scratch2);
        AS.SNEZ(of, of);
        AS.XOR(scratch, result, cf);
        AS.XOR(scratch2, result, result_2);
        AS.AND(scratch, scratch, scratch2);
        AS.LI(scratch2, sign_mask);
        AS.AND(scratch, scratch, scratch2);
        AS.SNEZ(scratch, scratch);
        AS.OR(of, of, scratch);
        rec.popScratch();
        rec.popScratch();
    }

    if (rec.shouldEmitFlag(meta.rip, X86_REF_CF)) {
        biscuit::GPR scratch = rec.scratch();
        biscuit::GPR cf = rec.flagWR(X86_REF_CF);
        rec.zext(scratch, result, size);
        AS.SLTU(scratch, scratch, cf);
        if (operands[1].type == ZYDIS_OPERAND_TYPE_IMMEDIATE && size != X86_SIZE_QWORD) {
            rec.zext(cf, src, size);
            AS.SLTU(cf, dst, cf);
        } else {
            AS.SLTU(cf, dst, src);
        }
        AS.OR(cf, cf, scratch);
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

    AS.ADD(result, dst, src);
    AS.ADD(result_2, result, cf);

    if (rec.shouldEmitFlag(meta.rip, X86_REF_PF)) {
        rec.updateParity(result_2);
    }

    if (rec.shouldEmitFlag(meta.rip, X86_REF_AF)) {
        biscuit::GPR af = rec.flagW(X86_REF_AF);
        biscuit::GPR scratch = rec.scratch();
        AS.ANDI(af, result, 0xF);
        AS.ANDI(scratch, dst, 0xF);
        AS.SLTU(af, af, scratch);
        AS.ANDI(scratch, result_2, 0xF);
        AS.SLTU(scratch, scratch, cf);
        AS.OR(af, af, scratch);
        rec.popScratch();
    }

    if (rec.shouldEmitFlag(meta.rip, X86_REF_OF)) {
        biscuit::GPR of = rec.flagW(X86_REF_OF);
        is_overflow_adc(rec, of, dst, src, result_2, rec.getBitSize(size));
    }

    if (rec.shouldEmitFlag(meta.rip, X86_REF_CF)) {
        biscuit::GPR scratch = rec.scratch();
        biscuit::GPR scratch2 = rec.scratch();
        biscuit::GPR cf = rec.flagWR(X86_REF_CF);
        rec.zext(scratch, result, size);
        rec.zext(scratch2, result_2, size);
        AS.SLTU(scratch, scratch, dst);
        AS.SLTU(scratch2, scratch2, cf);
        AS.OR(cf, scratch, scratch2);
        rec.popScratch();
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

FAST_HANDLE(CMP) {
    biscuit::GPR result = rec.scratch();
    biscuit::GPR src = rec.getOperandGPR(&operands[1]);
    biscuit::GPR dst = rec.getOperandGPR(&operands[0]);

    AS.SUB(result, dst, src);

    x86_size_e size = rec.getOperandSize(&operands[0]);

    SetCmpFlags(meta, rec, dst, src, result, size, operands[1].type == ZYDIS_OPERAND_TYPE_IMMEDIATE && size != X86_SIZE_QWORD);
}

FAST_HANDLE(OR) {
    biscuit::GPR result = rec.scratch();
    biscuit::GPR src = rec.getOperandGPR(&operands[1]);
    biscuit::GPR dst = rec.getOperandGPR(&operands[0]);

    AS.OR(result, dst, src);

    x86_size_e size = rec.getOperandSize(&operands[0]);
    rec.zext(result, result, size);

    if (rec.shouldEmitFlag(meta.rip, X86_REF_CF)) {
        biscuit::GPR cf = rec.flagW(X86_REF_CF);
        AS.MV(cf, x0);
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
        biscuit::GPR of = rec.flagW(X86_REF_OF);
        AS.MV(of, x0);
    }

    rec.setOperandGPR(&operands[0], result);

    rec.setFlagUndefined(X86_REF_AF);
}

FAST_HANDLE(XOR) {
    x86_size_e size = rec.getOperandSize(&operands[0]);

    // Optimize this common case since xor is used to zero out a register frequently
    if ((size == X86_SIZE_DWORD || size == X86_SIZE_QWORD) && operands[0].type == ZYDIS_OPERAND_TYPE_REGISTER &&
        operands[1].type == ZYDIS_OPERAND_TYPE_REGISTER && operands[0].reg.value == operands[1].reg.value) {
        rec.setRefGPR(rec.zydisToRef(operands[0].reg.value), X86_SIZE_QWORD, x0);

        if (rec.shouldEmitFlag(meta.rip, X86_REF_CF)) {
            biscuit::GPR cf = rec.flagW(X86_REF_CF);
            AS.MV(cf, x0);
        }

        if (rec.shouldEmitFlag(meta.rip, X86_REF_PF)) {
            rec.updateParity(x0);
        }

        if (rec.shouldEmitFlag(meta.rip, X86_REF_ZF)) {
            biscuit::GPR zf = rec.flagW(X86_REF_ZF);
            AS.LI(zf, 1);
        }

        if (rec.shouldEmitFlag(meta.rip, X86_REF_SF)) {
            rec.updateSign(x0, size);
        }

        if (rec.shouldEmitFlag(meta.rip, X86_REF_OF)) {
            biscuit::GPR of = rec.flagW(X86_REF_OF);
            AS.MV(of, x0);
        }

        rec.setFlagUndefined(X86_REF_AF);
        return;
    }

    biscuit::GPR result = rec.scratch();
    biscuit::GPR src = rec.getOperandGPR(&operands[1]);
    biscuit::GPR dst = rec.getOperandGPR(&operands[0]);

    AS.XOR(result, dst, src);
    rec.zext(result, result, size);

    if (rec.shouldEmitFlag(meta.rip, X86_REF_CF)) {
        biscuit::GPR cf = rec.flagW(X86_REF_CF);
        AS.MV(cf, x0);
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
        biscuit::GPR of = rec.flagW(X86_REF_OF);
        AS.MV(of, x0);
    }

    rec.setOperandGPR(&operands[0], result);

    rec.setFlagUndefined(X86_REF_AF);
}

FAST_HANDLE(AND) {
    biscuit::GPR result = rec.scratch();
    biscuit::GPR src = rec.getOperandGPR(&operands[1]);
    biscuit::GPR dst = rec.getOperandGPR(&operands[0]);

    AS.AND(result, dst, src);

    x86_size_e size = rec.getOperandSize(&operands[0]);
    rec.zext(result, result, size);

    if (rec.shouldEmitFlag(meta.rip, X86_REF_CF)) {
        biscuit::GPR cf = rec.flagW(X86_REF_CF);
        AS.MV(cf, x0);
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
        biscuit::GPR of = rec.flagW(X86_REF_OF);
        AS.MV(of, x0);
    }

    rec.setOperandGPR(&operands[0], result);

    rec.setFlagUndefined(X86_REF_AF);
}

FAST_HANDLE(HLT) {
    rec.setExitReason(ExitReason::EXIT_REASON_HLT);
    rec.writebackDirtyState();
    rec.backToDispatcher();
    rec.stopCompiling();
}

FAST_HANDLE(CALL) {
    switch (operands[0].type) {
    case ZYDIS_OPERAND_TYPE_REGISTER:
    case ZYDIS_OPERAND_TYPE_MEMORY: {
        biscuit::GPR scratch = rec.getRip();
        biscuit::GPR src = rec.getOperandGPR(&operands[0]);
        rec.setRip(src);
        biscuit::GPR rsp = rec.getRefGPR(X86_REF_RSP, X86_SIZE_QWORD);
        AS.ADDI(rsp, rsp, -8);
        rec.setRefGPR(X86_REF_RSP, X86_SIZE_QWORD, rsp);

        u64 return_offset = meta.rip - meta.block_start + instruction.length;
        rec.addi(scratch, scratch, return_offset);

        AS.SD(scratch, 0, rsp);

        rec.writebackDirtyState();
        rec.pushCalltrace();
        rec.backToDispatcher();
        rec.stopCompiling();
        break;
    }
    case ZYDIS_OPERAND_TYPE_IMMEDIATE: {
        u64 displacement = rec.sextImmediate(rec.getImmediate(&operands[0]), operands[0].imm.size);
        u64 return_offset = meta.rip - meta.block_start + instruction.length;

        biscuit::GPR scratch = rec.getRip();
        biscuit::GPR rsp = rec.getRefGPR(X86_REF_RSP, X86_SIZE_QWORD);
        AS.ADDI(rsp, rsp, -8);
        rec.setRefGPR(X86_REF_RSP, X86_SIZE_QWORD, rsp);

        rec.addi(scratch, scratch, return_offset);

        AS.SD(scratch, 0, rsp);

        rec.addi(scratch, scratch, displacement);

        rec.setRip(scratch);
        rec.writebackDirtyState();
        rec.pushCalltrace();
        rec.jumpAndLink(meta.rip + instruction.length + displacement);
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
    biscuit::GPR rsp = rec.getRefGPR(X86_REF_RSP, X86_SIZE_QWORD);
    biscuit::GPR scratch = rec.scratch();
    AS.LD(scratch, 0, rsp);

    u64 imm = 8;
    if (operands[0].type == ZYDIS_OPERAND_TYPE_IMMEDIATE) {
        imm += rec.getImmediate(&operands[0]);
    }

    rec.addi(rsp, rsp, imm);

    rec.setRefGPR(X86_REF_RSP, X86_SIZE_QWORD, rsp);
    rec.setRip(scratch);
    rec.writebackDirtyState();
    rec.popCalltrace();
    rec.backToDispatcher();
    rec.stopCompiling();
}

FAST_HANDLE(PUSH) {
    biscuit::GPR src = rec.getOperandGPR(&operands[0]);
    biscuit::GPR rsp = rec.getRefGPR(X86_REF_RSP, X86_SIZE_QWORD);
    int imm = instruction.operand_width == 16 ? -2 : -8;

    if (instruction.operand_width == 16) {
        AS.SH(src, imm, rsp);
    } else {
        AS.SD(src, imm, rsp);
    }

    AS.ADDI(rsp, rsp, imm);
    rec.setRefGPR(X86_REF_RSP, X86_SIZE_QWORD, rsp);
}

FAST_HANDLE(POP) {
    biscuit::GPR result = rec.scratch();
    biscuit::GPR rsp = rec.getRefGPR(X86_REF_RSP, X86_SIZE_QWORD);

    if (instruction.operand_width == 16) {
        AS.LHU(result, 0, rsp);
    } else {
        AS.LD(result, 0, rsp);
    }

    int imm = instruction.operand_width == 16 ? 2 : 8;
    rec.setOperandGPR(&operands[0], result);

    x86_ref_e ref = rec.zydisToRef(operands[0].reg.value);
    if (ref == X86_REF_RSP) {
        // pop rsp special case
        rec.setRefGPR(X86_REF_RSP, X86_SIZE_QWORD, result);
    } else {
        AS.ADDI(rsp, rsp, imm);
        rec.setRefGPR(X86_REF_RSP, X86_SIZE_QWORD, rsp);
    }
}

FAST_HANDLE(NOP) {}

FAST_HANDLE(ENDBR64) {}

FAST_HANDLE(RDSSPD) {}

FAST_HANDLE(RDSSPQ) {}

FAST_HANDLE(RSTORSSP) {}

FAST_HANDLE(SAVEPREVSSP) {}

FAST_HANDLE(SHL) {
    biscuit::GPR result = rec.scratch();
    x86_size_e size = rec.getOperandSize(&operands[0]);
    biscuit::GPR dst = rec.getOperandGPR(&operands[0]);
    biscuit::GPR src = rec.getOperandGPR(&operands[1]);
    biscuit::GPR count = rec.scratch();

    if (instruction.operand_width == 64) {
        AS.ANDI(count, src, 0x3F);
    } else {
        AS.ANDI(count, src, 0x1F);
    }

    Label zero_source;

    if (rec.shouldEmitFlag(meta.rip, X86_REF_CF))
        rec.flag(X86_REF_CF);

    if (rec.shouldEmitFlag(meta.rip, X86_REF_OF))
        rec.flag(X86_REF_OF);

    if (rec.shouldEmitFlag(meta.rip, X86_REF_ZF))
        rec.flag(X86_REF_ZF);

    if (rec.shouldEmitFlag(meta.rip, X86_REF_SF))
        rec.flag(X86_REF_SF);

    AS.SLL(result, dst, count);

    AS.BEQZ(count, &zero_source);

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
        AS.LI(cf, rec.getBitSize(size));
        AS.SUB(cf, cf, count);
        AS.SRL(cf, dst, cf);
        AS.ANDI(cf, cf, 1);
    }

    if (rec.shouldEmitFlag(meta.rip, X86_REF_OF)) {
        biscuit::GPR of = rec.flagW(X86_REF_OF);
        AS.SRLI(of, result, rec.getBitSize(size) - 1);
        AS.ANDI(of, of, 1);
        AS.XOR(of, of, rec.flag(X86_REF_CF));
    }

    AS.Bind(&zero_source);

    rec.setOperandGPR(&operands[0], result);
}

FAST_HANDLE(SHR) {
    biscuit::GPR result = rec.scratch();
    x86_size_e size = rec.getOperandSize(&operands[0]);
    biscuit::GPR dst = rec.getOperandGPR(&operands[0]);
    biscuit::GPR src = rec.getOperandGPR(&operands[1]);
    biscuit::GPR count = rec.scratch();

    if (instruction.operand_width == 64) {
        AS.ANDI(count, src, 0x3F);
    } else {
        AS.ANDI(count, src, 0x1F);
    }

    Label zero_source;

    if (rec.shouldEmitFlag(meta.rip, X86_REF_CF))
        rec.flag(X86_REF_CF);

    if (rec.shouldEmitFlag(meta.rip, X86_REF_OF))
        rec.flag(X86_REF_OF);

    if (rec.shouldEmitFlag(meta.rip, X86_REF_ZF))
        rec.flag(X86_REF_ZF);

    if (rec.shouldEmitFlag(meta.rip, X86_REF_SF))
        rec.flag(X86_REF_SF);

    AS.SRL(result, dst, count);

    AS.BEQZ(count, &zero_source);

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
        AS.ADDI(cf, count, -1);
        AS.SRL(cf, dst, cf);
        AS.ANDI(cf, cf, 1);
    }

    if (rec.shouldEmitFlag(meta.rip, X86_REF_OF)) {
        biscuit::GPR of = rec.flagW(X86_REF_OF);
        AS.SRLI(of, dst, rec.getBitSize(size) - 1);
        AS.ANDI(of, of, 1);
    }

    AS.Bind(&zero_source);

    rec.setOperandGPR(&operands[0], result);
}

FAST_HANDLE(SAR) {
    biscuit::GPR result = rec.scratch();
    x86_size_e size = rec.getOperandSize(&operands[0]);
    biscuit::GPR dst = rec.getOperandGPR(&operands[0]);
    biscuit::GPR src = rec.getOperandGPR(&operands[1]);
    biscuit::GPR count = rec.scratch();

    if (instruction.operand_width == 64) {
        AS.ANDI(count, src, 0x3F);
    } else {
        AS.ANDI(count, src, 0x1F);
    }

    Label zero_source;

    if (rec.shouldEmitFlag(meta.rip, X86_REF_CF))
        rec.flag(X86_REF_CF);

    if (rec.shouldEmitFlag(meta.rip, X86_REF_OF))
        rec.flag(X86_REF_OF);

    if (rec.shouldEmitFlag(meta.rip, X86_REF_ZF))
        rec.flag(X86_REF_ZF);

    if (rec.shouldEmitFlag(meta.rip, X86_REF_SF))
        rec.flag(X86_REF_SF);

    switch (size) {
    case X86_SIZE_BYTE: {
        AS.SLLI(result, dst, 56);
        AS.SRAI(result, result, 56);
        AS.SRA(result, result, count);
        break;
    }
    case X86_SIZE_WORD: {
        AS.SLLI(result, dst, 48);
        AS.SRAI(result, result, 48);
        AS.SRA(result, result, count);
        break;
    }
    case X86_SIZE_DWORD: {
        AS.SRAW(result, dst, count);
        break;
    }
    case X86_SIZE_QWORD: {
        AS.SRA(result, dst, count);
        break;
    }
    default: {
        UNREACHABLE();
        break;
    }
    }

    AS.BEQZ(count, &zero_source);

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
        AS.ADDI(cf, count, -1);
        AS.SRL(cf, dst, cf);
        AS.ANDI(cf, cf, 1);
    }

    if (rec.shouldEmitFlag(meta.rip, X86_REF_OF)) {
        biscuit::GPR of = rec.flagW(X86_REF_OF);
        AS.MV(of, x0);
    }

    AS.Bind(&zero_source);

    rec.setOperandGPR(&operands[0], result);
}

FAST_HANDLE(MOVQ) {
    if (operands[0].type == ZYDIS_OPERAND_TYPE_MEMORY) {
        ASSERT(operands[0].size == 64);
        biscuit::GPR dst = rec.scratch();
        biscuit::Vec src = rec.getOperandVec(&operands[1]);

        rec.setVectorState(SEW::E64, rec.maxVlen() / 64);
        AS.VMV_XS(dst, src);

        rec.setOperandGPR(&operands[0], dst);
    } else if (operands[1].type == ZYDIS_OPERAND_TYPE_MEMORY) {
        ASSERT(operands[0].size == 128);
        ASSERT(operands[1].size == 64);
        biscuit::GPR src = rec.getOperandGPR(&operands[1]);
        biscuit::Vec dst = rec.getOperandVec(&operands[0]);

        rec.setVectorState(SEW::E64, rec.maxVlen() / 64);
        AS.VMV(v0, 0b10);

        // Zero upper 64-bit elements (this will be useful for when we get to AVX)
        AS.VXOR(dst, dst, dst, VecMask::Yes);
        AS.VMV_SX(dst, src);

        rec.setOperandVec(&operands[0], dst);
    } else if (operands[1].type == ZYDIS_OPERAND_TYPE_REGISTER) {
        ASSERT(operands[0].type == ZYDIS_OPERAND_TYPE_REGISTER && operands[1].type == ZYDIS_OPERAND_TYPE_REGISTER);

        if (rec.isGPR(operands[1].reg.value)) {
            biscuit::GPR src = rec.getOperandGPR(&operands[1]);
            biscuit::Vec dst = rec.getOperandVec(&operands[0]);

            rec.setVectorState(SEW::E64, rec.maxVlen() / 64);
            AS.VMV(v0, 0b10);

            // Zero upper 64-bit elements (this will be useful for when we get to AVX)
            AS.VXOR(dst, dst, dst, VecMask::Yes);
            AS.VMV_SX(dst, src);

            rec.setOperandVec(&operands[0], dst);
        } else if (rec.isGPR(operands[0].reg.value)) {
            biscuit::GPR dst = rec.getOperandGPR(&operands[0]);
            biscuit::Vec src = rec.getOperandVec(&operands[1]);

            rec.setVectorState(SEW::E64, rec.maxVlen() / 64);
            AS.VMV_XS(dst, src);

            rec.setOperandGPR(&operands[0], dst);
        } else {
            biscuit::Vec result = rec.scratchVec();
            biscuit::Vec src = rec.getOperandVec(&operands[1]);

            rec.setVectorState(SEW::E64, rec.maxVlen() / 64);
            AS.VMV(v0, 0b01);
            AS.VMV(result, 0);
            AS.VOR(result, src, 0, VecMask::Yes);

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
        AS.VMV_XS(dst, src);

        rec.setOperandGPR(&operands[0], dst);
    } else if (operands[1].type == ZYDIS_OPERAND_TYPE_MEMORY) {
        ASSERT(operands[0].size == 128);
        ASSERT(operands[1].size == 32);
        biscuit::GPR src = rec.getOperandGPR(&operands[1]);
        biscuit::Vec dst = rec.getOperandVec(&operands[0]);

        rec.setVectorState(SEW::E32, rec.maxVlen() / 32);
        AS.VMV(v0, 0b1110);

        // Zero upper 32-bit elements (this will be useful for when we get to AVX)
        AS.VXOR(dst, dst, dst, VecMask::Yes);
        AS.VMV_SX(dst, src);

        rec.setOperandVec(&operands[0], dst);
    } else if (operands[1].type == ZYDIS_OPERAND_TYPE_REGISTER) {
        ASSERT(operands[0].type == ZYDIS_OPERAND_TYPE_REGISTER && operands[1].type == ZYDIS_OPERAND_TYPE_REGISTER);

        if (rec.isGPR(operands[1].reg.value)) {
            biscuit::GPR src = rec.getOperandGPR(&operands[1]);
            biscuit::Vec dst = rec.getOperandVec(&operands[0]);

            rec.setVectorState(SEW::E32, rec.maxVlen() / 32);
            AS.VMV(v0, 0b1110);

            // Zero upper 32-bit elements (this will be useful for when we get to AVX)
            AS.VXOR(dst, dst, dst, VecMask::Yes);
            AS.VMV_SX(dst, src);

            rec.setOperandVec(&operands[0], dst);
        } else if (rec.isGPR(operands[0].reg.value)) {
            biscuit::GPR dst = rec.getOperandGPR(&operands[0]);
            biscuit::Vec src = rec.getOperandVec(&operands[1]);

            rec.setVectorState(SEW::E32, rec.maxVlen() / 32);
            AS.VMV_XS(dst, src);

            rec.setOperandGPR(&operands[0], dst);
        } else {
            biscuit::Vec result = rec.scratchVec();
            biscuit::Vec src = rec.getOperandVec(&operands[1]);

            rec.setVectorState(SEW::E32, rec.maxVlen() / 32);
            AS.VMV(v0, 0b01);
            AS.VMV(result, 0);
            AS.VOR(result, src, 0, VecMask::Yes);

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
        rec.backToDispatcher();
        rec.stopCompiling();
        break;
    }
    case ZYDIS_OPERAND_TYPE_IMMEDIATE: {
        u64 displacement = rec.sextImmediate(rec.getImmediate(&operands[0]), operands[0].imm.size);
        u64 offset = meta.rip - meta.block_start + instruction.length;
        biscuit::GPR scratch = rec.getRip();
        rec.addi(scratch, scratch, offset + displacement);
        rec.setRip(scratch);
        rec.writebackDirtyState();
        rec.jumpAndLink(meta.rip + instruction.length + displacement);
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

FAST_HANDLE(DIV) {
    x86_size_e size = rec.getOperandSize(&operands[0]);
    // we don't need to move src to scratch because the rdx and rax in all these cases are in scratches
    biscuit::GPR src = rec.getOperandGPR(&operands[0]);

    switch (size) {
    case X86_SIZE_BYTE: {
        biscuit::GPR mod = rec.scratch();
        biscuit::GPR ax = rec.getRefGPR(X86_REF_RAX, X86_SIZE_WORD);

        AS.REMUW(mod, ax, src);
        AS.DIVUW(ax, ax, src);

        rec.setRefGPR(X86_REF_RAX, X86_SIZE_BYTE, ax); // TODO: word write
        rec.setRefGPR(X86_REF_RAX, X86_SIZE_BYTE_HIGH, mod);
        break;
    }
    case X86_SIZE_WORD: {
        biscuit::GPR ax = rec.getRefGPR(X86_REF_RAX, X86_SIZE_WORD);
        biscuit::GPR dx = rec.getRefGPR(X86_REF_RDX, X86_SIZE_WORD);
        AS.SLLIW(dx, dx, 16);
        AS.OR(dx, dx, ax);

        AS.DIVUW(ax, dx, src);
        AS.REMUW(dx, dx, src);

        rec.setRefGPR(X86_REF_RAX, X86_SIZE_WORD, ax);
        rec.setRefGPR(X86_REF_RDX, X86_SIZE_WORD, dx);
        break;
    }
    case X86_SIZE_DWORD: {
        biscuit::GPR eax = rec.getRefGPR(X86_REF_RAX, X86_SIZE_DWORD);
        biscuit::GPR edx = rec.getRefGPR(X86_REF_RDX, X86_SIZE_QWORD);
        AS.SLLI(edx, edx, 32);
        AS.OR(edx, edx, eax);

        AS.DIVU(eax, edx, src);
        AS.REMU(edx, edx, src);

        rec.setRefGPR(X86_REF_RAX, X86_SIZE_DWORD, eax);
        rec.setRefGPR(X86_REF_RDX, X86_SIZE_DWORD, edx);
        break;
    }
    case X86_SIZE_QWORD: {
        rec.writebackDirtyState();

        biscuit::GPR address = rec.scratch();
        AS.LI(address, (u64)&felix86_divu128);
        AS.MV(a1, src);
        AS.MV(a0, rec.threadStatePointer());
        AS.JALR(address);
        rec.restoreRoundingMode();
        break;
    }
    default: {
        UNREACHABLE();
        break;
    }
    }

    rec.setFlagUndefined(X86_REF_CF);
    rec.setFlagUndefined(X86_REF_AF);
    rec.setFlagUndefined(X86_REF_ZF);
    rec.setFlagUndefined(X86_REF_SF);
    rec.setFlagUndefined(X86_REF_OF);
}

FAST_HANDLE(IDIV) {
    x86_size_e size = rec.getOperandSize(&operands[0]);
    biscuit::GPR src = rec.getOperandGPR(&operands[0]);

    switch (size) {
    case X86_SIZE_BYTE: {
        biscuit::GPR mod = rec.scratch();
        biscuit::GPR divisor = rec.scratch();
        biscuit::GPR ax_sext = rec.scratch();
        biscuit::GPR ax = rec.getRefGPR(X86_REF_RAX, X86_SIZE_WORD);

        rec.sexth(ax_sext, ax);
        rec.sextb(divisor, src);

        AS.REMW(mod, ax_sext, divisor);
        AS.DIVW(ax, ax_sext, divisor);

        rec.popScratch();

        rec.setRefGPR(X86_REF_RAX, X86_SIZE_BYTE, ax);
        rec.setRefGPR(X86_REF_RAX, X86_SIZE_BYTE_HIGH, mod);
        break;
    }
    case X86_SIZE_WORD: {
        biscuit::GPR src_sext = rec.scratch();
        biscuit::GPR ax = rec.getRefGPR(X86_REF_RAX, X86_SIZE_WORD);
        biscuit::GPR dx = rec.getRefGPR(X86_REF_RDX, X86_SIZE_WORD);
        AS.SLLIW(dx, dx, 16);
        AS.OR(dx, dx, ax);

        rec.sexth(src_sext, src);

        AS.DIVW(ax, dx, src_sext);
        AS.REMW(dx, dx, src_sext);

        rec.setRefGPR(X86_REF_RAX, X86_SIZE_WORD, ax);
        rec.setRefGPR(X86_REF_RDX, X86_SIZE_WORD, dx);
        break;
    }
    case X86_SIZE_DWORD: {
        biscuit::GPR src_sext = rec.scratch();
        biscuit::GPR eax = rec.getRefGPR(X86_REF_RAX, X86_SIZE_DWORD);
        biscuit::GPR edx = rec.getRefGPR(X86_REF_RDX, X86_SIZE_QWORD);
        AS.SLLI(edx, edx, 32);
        AS.OR(edx, edx, eax);

        AS.ADDIW(src_sext, src, 0);

        AS.DIV(eax, edx, src_sext);
        AS.REM(edx, edx, src_sext);

        rec.setRefGPR(X86_REF_RAX, X86_SIZE_DWORD, eax);
        rec.setRefGPR(X86_REF_RDX, X86_SIZE_DWORD, edx);
        break;
    }
    case X86_SIZE_QWORD: {
        rec.writebackDirtyState();

        biscuit::GPR address = rec.scratch();
        AS.LI(address, (u64)&felix86_div128);
        AS.MV(a1, src);
        AS.MV(a0, rec.threadStatePointer());
        AS.JALR(address);
        rec.restoreRoundingMode();
        break;
    }
    default: {
        UNREACHABLE();
        break;
    }
    }

    rec.setFlagUndefined(X86_REF_CF);
    rec.setFlagUndefined(X86_REF_AF);
    rec.setFlagUndefined(X86_REF_ZF);
    rec.setFlagUndefined(X86_REF_SF);
    rec.setFlagUndefined(X86_REF_OF);
}

FAST_HANDLE(TEST) {
    biscuit::GPR result = rec.scratch();

    biscuit::GPR src = rec.getOperandGPR(&operands[1]);
    biscuit::GPR dst = rec.getOperandGPR(&operands[0]);

    AS.AND(result, dst, src);

    x86_size_e size = rec.getOperandSize(&operands[0]);

    if (rec.shouldEmitFlag(meta.rip, X86_REF_CF)) {
        biscuit::GPR cf = rec.flagW(X86_REF_CF);
        AS.MV(cf, x0);
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
        biscuit::GPR of = rec.flagW(X86_REF_OF);
        AS.MV(of, x0);
    }

    rec.setFlagUndefined(X86_REF_AF);
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
        AS.LI(one, 1);
        if (operands[0].size == 32) {
            AS.AMOADD_W(Ordering::AQRL, dst, one, address);
        } else if (operands[0].size == 64) {
            AS.AMOADD_D(Ordering::AQRL, dst, one, address);
        } else {
            UNREACHABLE();
        }
        AS.ADDI(res, dst, 1); // Do the operation in the register as well to calculate the flags
        writeback = false;
    } else {
        if (needs_atomic) {
            WARN("Atomic INC with 8 or 16 bit operands encountered");
        }

        dst = rec.getOperandGPR(&operands[0]);
        AS.ADDI(res, dst, 1);
    }

    if (rec.shouldEmitFlag(meta.rip, X86_REF_AF)) {
        biscuit::GPR af = rec.flagW(X86_REF_AF);
        AS.ANDI(af, res, 0xF);
        AS.SEQZ(af, af);
    }

    if (rec.shouldEmitFlag(meta.rip, X86_REF_OF)) {
        biscuit::GPR of = rec.flagW(X86_REF_OF);
        biscuit::GPR one = rec.scratch();
        u64 sign_mask = rec.getSignMask(size);
        AS.LI(one, 1);
        is_overflow_add(rec, of, dst, one, res, sign_mask);
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
        AS.LI(one, -1);
        if (operands[0].size == 32) {
            AS.AMOADD_W(Ordering::AQRL, dst, one, address);
        } else if (operands[0].size == 64) {
            AS.AMOADD_D(Ordering::AQRL, dst, one, address);
        } else {
            UNREACHABLE();
        }
        AS.ADDI(res, dst, -1); // Do the operation in the register as well to calculate the flags
        writeback = false;
    } else {
        if (needs_atomic) {
            WARN("Atomic DEC with 8 or 16 bit operands encountered");
        }

        dst = rec.getOperandGPR(&operands[0]);
        AS.ADDI(res, dst, -1);
    }

    if (rec.shouldEmitFlag(meta.rip, X86_REF_AF)) {
        biscuit::GPR af = rec.flagW(X86_REF_AF);
        AS.ANDI(af, dst, 0xF);
        AS.SEQZ(af, af);
    }

    if (rec.shouldEmitFlag(meta.rip, X86_REF_OF)) {
        biscuit::GPR of = rec.flagW(X86_REF_OF);
        biscuit::GPR one = rec.scratch();
        AS.LI(one, 1);
        is_overflow_sub(rec, of, dst, one, res, rec.getBitSize(size));
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
    AS.SLLI(scratch, pf, 2);
    AS.OR(result, cf, scratch);

    biscuit::GPR af = rec.flag(X86_REF_AF);
    AS.SLLI(scratch, af, 4);
    AS.OR(result, result, scratch);

    biscuit::GPR zf = rec.flag(X86_REF_ZF);
    AS.SLLI(scratch, zf, 6);
    AS.OR(result, result, scratch);

    biscuit::GPR sf = rec.flag(X86_REF_SF);
    AS.SLLI(scratch, sf, 7);
    AS.OR(result, result, scratch);
    AS.ORI(result, result, 0b10); // bit 1 is always set

    rec.setRefGPR(X86_REF_RAX, X86_SIZE_BYTE_HIGH, result);
}

FAST_HANDLE(SAHF) {
    biscuit::GPR cf = rec.flagW(X86_REF_CF);
    biscuit::GPR af = rec.flagW(X86_REF_AF);
    biscuit::GPR zf = rec.flagW(X86_REF_ZF);
    biscuit::GPR sf = rec.flagW(X86_REF_SF);
    biscuit::GPR ah = rec.getRefGPR(X86_REF_RAX, X86_SIZE_BYTE_HIGH);

    AS.ANDI(cf, ah, 1);

    biscuit::GPR pf = rec.scratch();
    AS.SRLI(pf, ah, 2);
    AS.ANDI(pf, pf, 1);
    AS.SB(pf, offsetof(ThreadState, pf), rec.threadStatePointer());

    AS.SRLI(af, ah, 4);
    AS.ANDI(af, af, 1);

    AS.SRLI(zf, ah, 6);
    AS.ANDI(zf, zf, 1);

    AS.SRLI(sf, ah, 7);
    AS.ANDI(sf, sf, 1);
}

FAST_HANDLE(XCHG_lock) {
    ASSERT(operands[0].size != 8 && operands[0].size != 16);
    x86_size_e size = rec.getOperandSize(&operands[0]);
    biscuit::GPR address = rec.lea(&operands[0]);
    biscuit::GPR src = rec.getOperandGPR(&operands[1]);
    biscuit::GPR scratch = rec.scratch();
    biscuit::GPR dst = rec.scratch();

    AS.MV(scratch, src);

    switch (size) {
    case X86_SIZE_DWORD: {
        AS.AMOSWAP_W(Ordering::AQRL, dst, scratch, address);
        break;
    }
    case X86_SIZE_QWORD: {
        AS.AMOSWAP_D(Ordering::AQRL, dst, scratch, address);
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
            return fast_XCHG_lock(rec, meta, instruction, operands);
        }
    }

    biscuit::GPR temp = rec.scratch();
    biscuit::GPR src = rec.getOperandGPR(&operands[1]);
    biscuit::GPR dst = rec.getOperandGPR(&operands[0]);

    AS.MV(temp, src);

    rec.setOperandGPR(&operands[1], dst);
    rec.setOperandGPR(&operands[0], temp);
}

FAST_HANDLE(CLD) {
    AS.SB(x0, offsetof(ThreadState, df), rec.threadStatePointer());
}

FAST_HANDLE(STD) {
    biscuit::GPR df = rec.scratch();
    AS.LI(df, 1);
    AS.SB(df, offsetof(ThreadState, df), rec.threadStatePointer());
}

FAST_HANDLE(CLC) {
    biscuit::GPR cf = rec.flagW(X86_REF_CF);
    AS.MV(cf, x0);
}

FAST_HANDLE(STC) {
    biscuit::GPR cf = rec.flagW(X86_REF_CF);
    AS.LI(cf, 1);
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
    AS.ADDIW(eax, eax, 0);
    rec.setRefGPR(X86_REF_RAX, X86_SIZE_QWORD, eax);
}

FAST_HANDLE(CWD) {
    biscuit::GPR sext = rec.scratch();
    biscuit::GPR ax = rec.getRefGPR(X86_REF_RAX, X86_SIZE_QWORD);
    rec.sexth(sext, ax);
    AS.SRLI(sext, sext, 16);
    rec.setRefGPR(X86_REF_RDX, X86_SIZE_WORD, sext);
}

FAST_HANDLE(CDQ) {
    biscuit::GPR sext = rec.scratch();
    biscuit::GPR eax = rec.getRefGPR(X86_REF_RAX, X86_SIZE_DWORD);
    AS.SRAIW(sext, eax, 31);
    rec.setRefGPR(X86_REF_RDX, X86_SIZE_DWORD, sext);
}

FAST_HANDLE(CQO) {
    biscuit::GPR sext = rec.scratch();
    biscuit::GPR rax = rec.getRefGPR(X86_REF_RAX, X86_SIZE_QWORD);
    AS.SRAI(sext, rax, 63);
    rec.setRefGPR(X86_REF_RDX, X86_SIZE_QWORD, sext);
}

void JCC(Recompiler& rec, const HandlerMetadata& meta, ZydisDecodedInstruction& instruction, ZydisDecodedOperand* operands, biscuit::GPR cond) {
    u64 immediate = rec.sextImmediate(rec.getImmediate(&operands[0]), operands[0].imm.size);
    u64 address_false = meta.rip - meta.block_start + instruction.length;
    u64 address_true = address_false + immediate;

    biscuit::GPR rip_true = rec.getRip();
    biscuit::GPR rip_false = rec.scratch();

    rec.addi(rip_false, rip_true, address_false);
    rec.addi(rip_true, rip_false, immediate);

    address_false += meta.block_start;
    address_true += meta.block_start;

    rec.writebackDirtyState();
    rec.jumpAndLinkConditional(cond, rip_true, rip_false, address_true, address_false);
    rec.stopCompiling();
}

FAST_HANDLE(JO) {
    JCC(rec, meta, instruction, operands, rec.getCond(instruction.opcode & 0xF));
}

FAST_HANDLE(JNO) {
    JCC(rec, meta, instruction, operands, rec.getCond(instruction.opcode & 0xF));
}

FAST_HANDLE(JB) {
    JCC(rec, meta, instruction, operands, rec.getCond(instruction.opcode & 0xF));
}

FAST_HANDLE(JNB) {
    JCC(rec, meta, instruction, operands, rec.getCond(instruction.opcode & 0xF));
}

FAST_HANDLE(JZ) {
    JCC(rec, meta, instruction, operands, rec.getCond(instruction.opcode & 0xF));
}

FAST_HANDLE(JNZ) {
    JCC(rec, meta, instruction, operands, rec.getCond(instruction.opcode & 0xF));
}

FAST_HANDLE(JBE) {
    JCC(rec, meta, instruction, operands, rec.getCond(instruction.opcode & 0xF));
}

FAST_HANDLE(JNBE) {
    JCC(rec, meta, instruction, operands, rec.getCond(instruction.opcode & 0xF));
}

FAST_HANDLE(JP) {
    JCC(rec, meta, instruction, operands, rec.getCond(instruction.opcode & 0xF));
}

FAST_HANDLE(JNP) {
    JCC(rec, meta, instruction, operands, rec.getCond(instruction.opcode & 0xF));
}

FAST_HANDLE(JS) {
    JCC(rec, meta, instruction, operands, rec.getCond(instruction.opcode & 0xF));
}

FAST_HANDLE(JNS) {
    JCC(rec, meta, instruction, operands, rec.getCond(instruction.opcode & 0xF));
}

FAST_HANDLE(JL) {
    JCC(rec, meta, instruction, operands, rec.getCond(instruction.opcode & 0xF));
}

FAST_HANDLE(JNL) {
    JCC(rec, meta, instruction, operands, rec.getCond(instruction.opcode & 0xF));
}

FAST_HANDLE(JLE) {
    JCC(rec, meta, instruction, operands, rec.getCond(instruction.opcode & 0xF));
}

FAST_HANDLE(JNLE) {
    JCC(rec, meta, instruction, operands, rec.getCond(instruction.opcode & 0xF));
}

void CMOV(Recompiler& rec, const HandlerMetadata& meta, ZydisDecodedInstruction& instruction, ZydisDecodedOperand* operands, biscuit::GPR cond) {
    biscuit::GPR dst = rec.getOperandGPR(&operands[0]);
    biscuit::GPR src = rec.getOperandGPR(&operands[1]);
    biscuit::GPR result = rec.scratch();

    AS.MV(result, dst);
    if (Extensions::Xtheadcondmov) {
        AS.TH_MVNEZ(result, src, cond);
    } else if (Extensions::Zicond) {
        biscuit::GPR tmp = rec.scratch();
        AS.CZERO_NEZ(tmp, result, cond);
        AS.CZERO_EQZ(result, src, cond);
        AS.OR(result, result, tmp);
    } else {
        Label false_label;
        AS.BEQZ(cond, &false_label);
        AS.MV(result, src);
        AS.Bind(&false_label);
    }

    rec.setOperandGPR(&operands[0], result);
}

FAST_HANDLE(CMOVO) {
    CMOV(rec, meta, instruction, operands, rec.getCond(instruction.opcode & 0xF));
}

FAST_HANDLE(CMOVNO) {
    CMOV(rec, meta, instruction, operands, rec.getCond(instruction.opcode & 0xF));
}

FAST_HANDLE(CMOVB) {
    CMOV(rec, meta, instruction, operands, rec.getCond(instruction.opcode & 0xF));
}

FAST_HANDLE(CMOVNB) {
    CMOV(rec, meta, instruction, operands, rec.getCond(instruction.opcode & 0xF));
}

FAST_HANDLE(CMOVZ) {
    CMOV(rec, meta, instruction, operands, rec.getCond(instruction.opcode & 0xF));
}

FAST_HANDLE(CMOVNZ) {
    CMOV(rec, meta, instruction, operands, rec.getCond(instruction.opcode & 0xF));
}

FAST_HANDLE(CMOVBE) {
    CMOV(rec, meta, instruction, operands, rec.getCond(instruction.opcode & 0xF));
}

FAST_HANDLE(CMOVNBE) {
    CMOV(rec, meta, instruction, operands, rec.getCond(instruction.opcode & 0xF));
}

FAST_HANDLE(CMOVP) {
    CMOV(rec, meta, instruction, operands, rec.getCond(instruction.opcode & 0xF));
}

FAST_HANDLE(CMOVNP) {
    CMOV(rec, meta, instruction, operands, rec.getCond(instruction.opcode & 0xF));
}

FAST_HANDLE(CMOVS) {
    CMOV(rec, meta, instruction, operands, rec.getCond(instruction.opcode & 0xF));
}

FAST_HANDLE(CMOVNS) {
    CMOV(rec, meta, instruction, operands, rec.getCond(instruction.opcode & 0xF));
}

FAST_HANDLE(CMOVL) {
    CMOV(rec, meta, instruction, operands, rec.getCond(instruction.opcode & 0xF));
}

FAST_HANDLE(CMOVNL) {
    CMOV(rec, meta, instruction, operands, rec.getCond(instruction.opcode & 0xF));
}

FAST_HANDLE(CMOVLE) {
    CMOV(rec, meta, instruction, operands, rec.getCond(instruction.opcode & 0xF));
}

FAST_HANDLE(CMOVNLE) {
    CMOV(rec, meta, instruction, operands, rec.getCond(instruction.opcode & 0xF));
}

FAST_HANDLE(MOVSXD) {
    x86_size_e size = rec.getOperandSize(&operands[1]);
    biscuit::GPR src = rec.getOperandGPR(&operands[1]);

    if (size == X86_SIZE_DWORD) {
        biscuit::GPR dst = rec.allocatedGPR(rec.zydisToRef(operands[0].reg.value));
        AS.ADDIW(dst, src, 0);
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
        case X86_SIZE_BYTE: {
            biscuit::GPR result = rec.scratch();
            biscuit::GPR sext = rec.scratch();
            biscuit::GPR al = rec.getRefGPR(X86_REF_RAX, X86_SIZE_BYTE);
            rec.sextb(sext, al);
            rec.sextb(result, al);
            AS.MULW(result, sext, src);
            rec.setRefGPR(X86_REF_RAX, X86_SIZE_WORD, result);

            if (rec.shouldEmitFlag(meta.rip, X86_REF_CF) || rec.shouldEmitFlag(meta.rip, X86_REF_OF)) {
                biscuit::GPR cf = rec.flagW(X86_REF_CF);
                biscuit::GPR of = rec.flagW(X86_REF_OF);
                rec.sextb(cf, result);
                AS.XOR(of, cf, result);
                AS.SNEZ(of, of);
                AS.MV(cf, of);
            }
            break;
        }
        case X86_SIZE_WORD: {
            biscuit::GPR result = rec.scratch();
            biscuit::GPR sext = rec.scratch();
            biscuit::GPR ax = rec.getRefGPR(X86_REF_RAX, X86_SIZE_WORD);
            rec.sexth(sext, ax);
            rec.sexth(result, src);
            AS.MULW(result, sext, result);
            rec.setRefGPR(X86_REF_RAX, X86_SIZE_WORD, result);

            if (rec.shouldEmitFlag(meta.rip, X86_REF_CF) || rec.shouldEmitFlag(meta.rip, X86_REF_OF)) {
                biscuit::GPR cf = rec.flagW(X86_REF_CF);
                biscuit::GPR of = rec.flagW(X86_REF_OF);

                rec.sexth(cf, result);
                AS.XOR(of, cf, result);
                AS.SNEZ(of, of);
                AS.MV(cf, of);
            }

            AS.SRAIW(result, result, 16);
            rec.setRefGPR(X86_REF_RDX, X86_SIZE_WORD, result);
            break;
        }
        case X86_SIZE_DWORD: {
            biscuit::GPR result = rec.scratch();
            biscuit::GPR sext = rec.scratch();
            biscuit::GPR eax = rec.getRefGPR(X86_REF_RAX, X86_SIZE_DWORD);
            AS.ADDIW(sext, eax, 0);
            AS.ADDIW(result, src, 0);
            AS.MUL(result, sext, result);
            rec.setRefGPR(X86_REF_RAX, X86_SIZE_DWORD, result);

            if (rec.shouldEmitFlag(meta.rip, X86_REF_CF) || rec.shouldEmitFlag(meta.rip, X86_REF_OF)) {
                biscuit::GPR cf = rec.flagW(X86_REF_CF);
                biscuit::GPR of = rec.flagW(X86_REF_OF);

                AS.ADDIW(cf, result, 0);
                AS.XOR(of, cf, result);
                AS.SNEZ(of, of);
                AS.MV(cf, of);
            }

            AS.SRLI(result, result, 32);
            rec.setRefGPR(X86_REF_RDX, X86_SIZE_DWORD, result);
            break;
        }
        case X86_SIZE_QWORD: {
            biscuit::GPR result = rec.scratch();
            biscuit::GPR rax = rec.getRefGPR(X86_REF_RAX, X86_SIZE_QWORD);
            AS.MULH(result, rax, src);
            AS.MUL(rax, rax, src);
            rec.setRefGPR(X86_REF_RAX, X86_SIZE_QWORD, rax);
            rec.setRefGPR(X86_REF_RDX, X86_SIZE_QWORD, result);

            if (rec.shouldEmitFlag(meta.rip, X86_REF_CF) || rec.shouldEmitFlag(meta.rip, X86_REF_OF)) {
                biscuit::GPR cf = rec.flagW(X86_REF_CF);
                biscuit::GPR of = rec.flagW(X86_REF_OF);

                AS.SRAI(cf, rax, 63);
                AS.XOR(of, cf, result);
                AS.SNEZ(of, of);
                AS.MV(cf, of);
            }
            break;
        }
        default: {
            UNREACHABLE();
            break;
        }
        }

        rec.setFlagUndefined(X86_REF_AF);
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
            AS.MULW(result, result, dst_sext);
            rec.setOperandGPR(&operands[0], result);

            if (rec.shouldEmitFlag(meta.rip, X86_REF_CF) || rec.shouldEmitFlag(meta.rip, X86_REF_OF)) {
                biscuit::GPR cf = rec.flagW(X86_REF_CF);
                biscuit::GPR of = rec.flagW(X86_REF_OF);
                rec.sexth(cf, result);
                AS.XOR(of, cf, result);
                AS.SNEZ(of, of);
                AS.MV(cf, of);
            }
            break;
        }
        case X86_SIZE_DWORD: {
            biscuit::GPR result = rec.scratch();
            biscuit::GPR dst_sext = rec.scratch();
            AS.ADDIW(dst_sext, src1, 0);
            AS.ADDIW(result, src2, 0);
            AS.MUL(result, result, dst_sext);
            rec.setOperandGPR(&operands[0], result);

            if (rec.shouldEmitFlag(meta.rip, X86_REF_CF) || rec.shouldEmitFlag(meta.rip, X86_REF_OF)) {
                biscuit::GPR cf = rec.flagW(X86_REF_CF);
                biscuit::GPR of = rec.flagW(X86_REF_OF);
                AS.ADDIW(cf, result, 0);
                AS.XOR(of, cf, result);
                AS.SNEZ(of, of);
                AS.MV(cf, of);
            }
            break;
        }
        case X86_SIZE_QWORD: {
            biscuit::GPR result = rec.scratch();
            biscuit::GPR result_low = rec.scratch();
            AS.MULH(result, src1, src2);
            AS.MUL(result_low, src1, src2);
            rec.setOperandGPR(&operands[0], result_low);

            if (rec.shouldEmitFlag(meta.rip, X86_REF_CF) || rec.shouldEmitFlag(meta.rip, X86_REF_OF)) {
                biscuit::GPR cf = rec.flagW(X86_REF_CF);
                biscuit::GPR of = rec.flagW(X86_REF_OF);
                AS.SRAI(cf, result_low, 63);
                AS.XOR(of, cf, result);
                AS.SNEZ(of, of);
                AS.MV(cf, of);
            }
            break;
        }
        default: {
            UNREACHABLE();
            break;
        }
        }

        rec.setFlagUndefined(X86_REF_AF);
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
    case X86_SIZE_BYTE: {
        biscuit::GPR result = rec.scratch();
        biscuit::GPR al = rec.getRefGPR(X86_REF_RAX, X86_SIZE_BYTE);
        AS.MULW(result, al, src);
        rec.setRefGPR(X86_REF_RAX, X86_SIZE_WORD, result);

        if (rec.shouldEmitFlag(meta.rip, X86_REF_CF) || rec.shouldEmitFlag(meta.rip, X86_REF_OF)) {
            biscuit::GPR cf = rec.flagW(X86_REF_CF);
            biscuit::GPR of = rec.flagW(X86_REF_OF);
            // 8 * 8 bit can only be 16 bit so we don't need to zero extend
            AS.SRLI(cf, result, 8);
            AS.SNEZ(cf, cf);
            AS.MV(of, cf);
        }
        break;
    }
    case X86_SIZE_WORD: {
        biscuit::GPR result = rec.scratch();
        biscuit::GPR ax = rec.getRefGPR(X86_REF_RAX, X86_SIZE_WORD);
        AS.MULW(result, ax, src);
        rec.setRefGPR(X86_REF_RAX, X86_SIZE_WORD, result);

        AS.SRLIW(result, result, 16);

        if (rec.shouldEmitFlag(meta.rip, X86_REF_CF) || rec.shouldEmitFlag(meta.rip, X86_REF_OF)) {
            biscuit::GPR cf = rec.flagW(X86_REF_CF);
            biscuit::GPR of = rec.flagW(X86_REF_OF);
            // Should be already zexted due to srliw
            AS.SNEZ(cf, result);
            AS.MV(of, cf);
        }

        rec.setRefGPR(X86_REF_RDX, X86_SIZE_WORD, result);
        break;
    }
    case X86_SIZE_DWORD: {
        biscuit::GPR result = rec.scratch();
        biscuit::GPR eax = rec.getRefGPR(X86_REF_RAX, X86_SIZE_DWORD);
        AS.MUL(result, eax, src);
        rec.setRefGPR(X86_REF_RAX, X86_SIZE_DWORD, result);
        AS.SRLI(result, result, 32);

        if (rec.shouldEmitFlag(meta.rip, X86_REF_CF) || rec.shouldEmitFlag(meta.rip, X86_REF_OF)) {
            biscuit::GPR cf = rec.flagW(X86_REF_CF);
            biscuit::GPR of = rec.flagW(X86_REF_OF);

            AS.SNEZ(cf, result);
            AS.MV(of, cf);
        }

        rec.setRefGPR(X86_REF_RDX, X86_SIZE_DWORD, result);
        break;
    }
    case X86_SIZE_QWORD: {
        biscuit::GPR result = rec.scratch();
        biscuit::GPR rax = rec.getRefGPR(X86_REF_RAX, X86_SIZE_QWORD);
        AS.MULHU(result, rax, src);
        AS.MUL(rax, rax, src);
        rec.setRefGPR(X86_REF_RAX, X86_SIZE_QWORD, rax);
        rec.setRefGPR(X86_REF_RDX, X86_SIZE_QWORD, result);

        if (rec.shouldEmitFlag(meta.rip, X86_REF_CF) || rec.shouldEmitFlag(meta.rip, X86_REF_OF)) {
            biscuit::GPR cf = rec.flagW(X86_REF_CF);
            biscuit::GPR of = rec.flagW(X86_REF_OF);

            AS.SNEZ(cf, result);
            AS.MV(of, cf);
        }
        break;
    }
    default: {
        UNREACHABLE();
        break;
    }
    }

    rec.setFlagUndefined(X86_REF_AF);
    rec.setFlagUndefined(X86_REF_ZF);
    rec.setFlagUndefined(X86_REF_SF);
}

void PUNPCKL(Recompiler& rec, const HandlerMetadata& meta, ZydisDecodedInstruction& instruction, ZydisDecodedOperand* operands, SEW sew, u8 vlen) {
    // Essentially two "vdecompress" (viota + vrgather) instructions
    // If an element index is out of range ( vs1[i] >= VLMAX ) then zero is returned for the element value.
    // This means we don't care to reduce the splat to only the first two elements
    // Doing iota with these masks essentially creates something like
    // [3 3 2 2 1 1 0 0] and [4 3 3 2 2 1 1 0]
    // And the gather itself is also masked
    // So for the reg it picks:
    // [h g f e d c b a]
    // [4 3 3 2 2 1 1 0]
    // [0 1 0 1 0 1 0 1]
    // [x d x c x b x a]
    // And for the rm it picks:
    // [p o n m l k j i]
    // [3 3 2 2 1 1 0 0]
    // [1 0 1 0 1 0 1 0]
    // [l x k x j x i x]
    // Which is the correct interleaving of the two vectors
    // [h g f e d c b a]
    // [p o n m l k j i]
    // -----------------
    // [l d k c j b i a]
    biscuit::Vec dst = rec.getOperandVec(&operands[0]);
    biscuit::Vec src = rec.getOperandVec(&operands[1]);
    biscuit::GPR mask = rec.scratch();
    biscuit::Vec iota = rec.scratchVec();
    biscuit::Vec result = rec.scratchVec();
    AS.LI(mask, 0b10101010);

    rec.setVectorState(sew, vlen);
    AS.VMV(v0, mask);
    AS.VIOTA(iota, v0);
    AS.VMV(result, 0);
    rec.vrgather(result, src, iota, VecMask::Yes);

    AS.VSRL(v0, v0, 1);
    AS.VIOTA(iota, v0);
    rec.vrgather(result, dst, iota, VecMask::Yes);

    rec.setOperandVec(&operands[0], result);
}

void PUNPCKH(Recompiler& rec, const HandlerMetadata& meta, ZydisDecodedInstruction& instruction, ZydisDecodedOperand* operands, SEW sew, u8 vlen) {
    // Like PUNPCKL but we add a number to iota to pick the high elements
    int num = 0;
    switch (sew) {
    case SEW::E8: {
        num = 8;
        break;
    }
    case SEW::E16: {
        num = 4;
        break;
    }
    case SEW::E32: {
        num = 2;
        break;
    }
    case SEW::E64: {
        num = 1;
        break;
    }
    default: {
        UNREACHABLE();
        break;
    }
    }

    biscuit::Vec dst = rec.getOperandVec(&operands[0]);
    biscuit::Vec src = rec.getOperandVec(&operands[1]);
    biscuit::GPR mask = rec.scratch();
    biscuit::Vec iota = rec.scratchVec();
    biscuit::Vec result = rec.scratchVec();
    AS.LI(mask, 0b10101010);

    rec.setVectorState(sew, vlen);
    AS.VMV(v0, mask);
    AS.VIOTA(iota, v0);
    AS.VMV(result, 0);
    AS.VADD(iota, iota, num);
    rec.vrgather(result, src, iota, VecMask::Yes);

    AS.VSRL(v0, v0, 1);
    AS.VIOTA(iota, v0);
    AS.VADD(iota, iota, num);
    rec.vrgather(result, dst, iota, VecMask::Yes);

    rec.setOperandVec(&operands[0], result);
}

FAST_HANDLE(PUNPCKLBW) {
    PUNPCKL(rec, meta, instruction, operands, SEW::E8, rec.maxVlen() / 8);
}

FAST_HANDLE(PUNPCKLWD) {
    PUNPCKL(rec, meta, instruction, operands, SEW::E16, rec.maxVlen() / 16);
}

FAST_HANDLE(PUNPCKLDQ) {
    PUNPCKL(rec, meta, instruction, operands, SEW::E32, rec.maxVlen() / 32);
}

FAST_HANDLE(PUNPCKLQDQ) {
    PUNPCKL(rec, meta, instruction, operands, SEW::E64, rec.maxVlen() / 64);
}

FAST_HANDLE(PUNPCKHBW) {
    PUNPCKH(rec, meta, instruction, operands, SEW::E8, rec.maxVlen() / 8);
}

FAST_HANDLE(PUNPCKHWD) {
    PUNPCKH(rec, meta, instruction, operands, SEW::E16, rec.maxVlen() / 16);
}

FAST_HANDLE(PUNPCKHDQ) {
    PUNPCKH(rec, meta, instruction, operands, SEW::E32, rec.maxVlen() / 32);
}

FAST_HANDLE(PUNPCKHQDQ) {
    PUNPCKH(rec, meta, instruction, operands, SEW::E64, rec.maxVlen() / 64);
}

FAST_HANDLE(UNPCKLPS) { // Fuzzed
    biscuit::Vec scratch = rec.scratchVec();
    biscuit::Vec iota = rec.scratchVec();
    biscuit::Vec src1 = rec.getOperandVec(&operands[0]);
    biscuit::Vec src2 = rec.getOperandVec(&operands[1]);

    rec.setVectorState(SEW::E32, rec.maxVlen() / 32);
    AS.VMV(scratch, 0);
    AS.VMV(v0, 0b0101);
    AS.VIOTA(iota, v0);
    AS.VRGATHER(scratch, src1, iota, VecMask::Yes);
    AS.VMV(v0, 0b1010);
    AS.VIOTA(iota, v0);
    AS.VRGATHER(scratch, src2, iota, VecMask::Yes);

    rec.setOperandVec(&operands[0], scratch);
}

FAST_HANDLE(UNPCKHPS) { // Fuzzed
    biscuit::Vec scratch = rec.scratchVec();
    biscuit::Vec iota = rec.scratchVec();
    biscuit::Vec src1 = rec.getOperandVec(&operands[0]);
    biscuit::Vec src2 = rec.getOperandVec(&operands[1]);

    rec.setVectorState(SEW::E32, rec.maxVlen() / 32);
    AS.VMV(scratch, 0);
    AS.VMV(v0, 0b0101);
    AS.VIOTA(iota, v0);
    AS.VADD(iota, iota, 2);
    AS.VRGATHER(scratch, src1, iota, VecMask::Yes);
    AS.VMV(v0, 0b1010);
    AS.VIOTA(iota, v0);
    AS.VADD(iota, iota, 2);
    AS.VRGATHER(scratch, src2, iota, VecMask::Yes);

    rec.setOperandVec(&operands[0], scratch);
}

FAST_HANDLE(UNPCKLPD) { // Fuzzed
    biscuit::Vec scratch = rec.scratchVec();
    biscuit::Vec result = rec.scratchVec();
    biscuit::Vec src1 = rec.getOperandVec(&operands[0]);
    biscuit::Vec src2 = rec.getOperandVec(&operands[1]);

    rec.setVectorState(SEW::E64, rec.maxVlen() / 64);
    AS.VSLIDEUP(scratch, src2, 1);
    AS.VMV(v0, 0b10);
    AS.VMERGE(result, src1, scratch);

    rec.setOperandVec(&operands[0], result);
}

FAST_HANDLE(UNPCKHPD) { // Fuzzed
    biscuit::Vec scratch = rec.scratchVec();
    biscuit::Vec result = rec.scratchVec();
    biscuit::Vec src1 = rec.getOperandVec(&operands[0]);
    biscuit::Vec src2 = rec.getOperandVec(&operands[1]);

    rec.setVectorState(SEW::E64, rec.maxVlen() / 64);
    AS.VSLIDEDOWN(scratch, src1, 1);
    AS.VMV(v0, 0b10);
    AS.VMERGE(result, scratch, src2);

    rec.setOperandVec(&operands[0], result);
}

FAST_HANDLE(MOVAPD) {
    biscuit::Vec src = rec.getOperandVec(&operands[1]);
    rec.setOperandVec(&operands[0], src);
}

FAST_HANDLE(MOVAPS) {
    biscuit::Vec src = rec.getOperandVec(&operands[1]);
    rec.setOperandVec(&operands[0], src);
}

FAST_HANDLE(MOVUPD) {
    biscuit::Vec src = rec.getOperandVec(&operands[1]);
    rec.setOperandVec(&operands[0], src);
}

FAST_HANDLE(MOVUPS) {
    biscuit::Vec src = rec.getOperandVec(&operands[1]);
    rec.setOperandVec(&operands[0], src);
}

FAST_HANDLE(MOVDQA) {
    biscuit::Vec src = rec.getOperandVec(&operands[1]);
    rec.setOperandVec(&operands[0], src);
}

FAST_HANDLE(MOVDQU) {
    biscuit::Vec src = rec.getOperandVec(&operands[1]);
    rec.setOperandVec(&operands[0], src);
}

FAST_HANDLE(RDTSC) {
    biscuit::GPR tsc = rec.scratch();
    AS.RDTIME(tsc);
    rec.setRefGPR(X86_REF_RAX, X86_SIZE_DWORD, tsc);
    AS.SRLI(tsc, tsc, 32);
    rec.setRefGPR(X86_REF_RDX, X86_SIZE_DWORD, tsc);
}

FAST_HANDLE(CPUID) {
    rec.writebackDirtyState();

    biscuit::GPR address = rec.scratch();
    AS.LI(address, (u64)&felix86_cpuid);
    AS.MV(a0, rec.threadStatePointer());
    AS.JALR(address);
    rec.restoreRoundingMode();
}

FAST_HANDLE(SYSCALL) {
    if (!g_strace && !g_dont_inline_syscalls) {
        bool inlined = rec.tryInlineSyscall();
        if (inlined) {
            return;
        }
    }

    biscuit::GPR rcx = rec.allocatedGPR(X86_REF_RCX);
    AS.LI(rcx, meta.rip + instruction.length);
    rec.setRefGPR(X86_REF_RCX, X86_SIZE_QWORD, rcx);

    // Normally the syscall instruction also writes the flags to R11 but we don't need them in our syscall handler

    rec.writebackDirtyState();

    biscuit::GPR address = rec.scratch();
    AS.LI(address, (u64)&felix86_syscall);
    AS.MV(a0, rec.threadStatePointer());
    AS.JALR(address);
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
    rec.setVectorState(SEW::E64, rec.maxVlen() / 64);
    AS.VXOR(dst, dst, src);
    rec.setOperandVec(&operands[0], dst);
}

FAST_HANDLE(MOVNTDQ) {
    biscuit::Vec src = rec.getOperandVec(&operands[1]);
    rec.setOperandVec(&operands[0], src);
}

FAST_HANDLE(MOVNTDQA) {
    biscuit::Vec src = rec.getOperandVec(&operands[1]);
    rec.setOperandVec(&operands[0], src);
}

FAST_HANDLE(MOVNTI) {
    biscuit::GPR src = rec.getOperandGPR(&operands[1]);
    rec.setOperandGPR(&operands[0], src);
}

FAST_HANDLE(MOVNTPD) {
    biscuit::Vec src = rec.getOperandVec(&operands[1]);
    rec.setOperandVec(&operands[0], src);
}

FAST_HANDLE(MOVNTPS) {
    biscuit::Vec src = rec.getOperandVec(&operands[1]);
    rec.setOperandVec(&operands[0], src);
}

FAST_HANDLE(PAND) {
    biscuit::Vec dst = rec.getOperandVec(&operands[0]);
    biscuit::Vec src = rec.getOperandVec(&operands[1]);
    rec.setVectorState(SEW::E64, rec.maxVlen() / 64);
    AS.VAND(dst, dst, src);
    rec.setOperandVec(&operands[0], dst);
}

FAST_HANDLE(POR) {
    biscuit::Vec dst = rec.getOperandVec(&operands[0]);
    biscuit::Vec src = rec.getOperandVec(&operands[1]);
    rec.setVectorState(SEW::E64, rec.maxVlen() / 64);
    AS.VOR(dst, dst, src);
    rec.setOperandVec(&operands[0], dst);
}

FAST_HANDLE(PANDN) { // Fuzzed
    biscuit::Vec dst_not = rec.scratchVec();
    biscuit::Vec dst = rec.getOperandVec(&operands[0]);
    biscuit::Vec src = rec.getOperandVec(&operands[1]);
    rec.setVectorState(SEW::E64, rec.maxVlen() / 64);
    if (Extensions::Zvbb) {
        WARN_ONCE("PANDN + Zvbb is untested, please run tests and report results");
        AS.VANDN(dst, src, dst);
    } else {
        AS.VXOR(dst_not, dst, -1);
        AS.VAND(dst, dst_not, src);
    }
    rec.setOperandVec(&operands[0], dst);
}

FAST_HANDLE(ANDPS) {
    fast_PAND(rec, meta, instruction, operands);
}

FAST_HANDLE(ANDPD) {
    fast_PAND(rec, meta, instruction, operands);
}

FAST_HANDLE(ORPS) {
    fast_POR(rec, meta, instruction, operands);
}

FAST_HANDLE(ORPD) {
    fast_POR(rec, meta, instruction, operands);
}

FAST_HANDLE(XORPS) {
    fast_PXOR(rec, meta, instruction, operands);
}

FAST_HANDLE(XORPD) {
    fast_PXOR(rec, meta, instruction, operands);
}

FAST_HANDLE(ANDNPS) { // Fuzzed
    fast_PANDN(rec, meta, instruction, operands);
}

FAST_HANDLE(ANDNPD) { // Fuzzed
    fast_PANDN(rec, meta, instruction, operands);
}

void PADD(Recompiler& rec, const HandlerMetadata& meta, ZydisDecodedInstruction& instruction, ZydisDecodedOperand* operands, SEW sew, u8 vlen) {
    biscuit::Vec dst = rec.getOperandVec(&operands[0]);
    biscuit::Vec src = rec.getOperandVec(&operands[1]);
    rec.setVectorState(sew, vlen);
    AS.VADD(dst, dst, src);
    rec.setOperandVec(&operands[0], dst);
}

void PADDS(Recompiler& rec, const HandlerMetadata& meta, ZydisDecodedInstruction& instruction, ZydisDecodedOperand* operands, SEW sew, u8 vlen) {
    biscuit::Vec dst = rec.getOperandVec(&operands[0]);
    biscuit::Vec src = rec.getOperandVec(&operands[1]);
    rec.setVectorState(sew, vlen);
    AS.VSADD(dst, dst, src);
    rec.setOperandVec(&operands[0], dst);
}

void PADDSU(Recompiler& rec, const HandlerMetadata& meta, ZydisDecodedInstruction& instruction, ZydisDecodedOperand* operands, SEW sew, u8 vlen) {
    biscuit::Vec dst = rec.getOperandVec(&operands[0]);
    biscuit::Vec src = rec.getOperandVec(&operands[1]);
    rec.setVectorState(sew, vlen);
    AS.VSADDU(dst, dst, src);
    rec.setOperandVec(&operands[0], dst);
}

void PSUBS(Recompiler& rec, const HandlerMetadata& meta, ZydisDecodedInstruction& instruction, ZydisDecodedOperand* operands, SEW sew, u8 vlen) {
    biscuit::Vec dst = rec.getOperandVec(&operands[0]);
    biscuit::Vec src = rec.getOperandVec(&operands[1]);
    rec.setVectorState(sew, vlen);
    AS.VSSUB(dst, dst, src);
    rec.setOperandVec(&operands[0], dst);
}

void PSUBSU(Recompiler& rec, const HandlerMetadata& meta, ZydisDecodedInstruction& instruction, ZydisDecodedOperand* operands, SEW sew, u8 vlen) {
    biscuit::Vec dst = rec.getOperandVec(&operands[0]);
    biscuit::Vec src = rec.getOperandVec(&operands[1]);
    rec.setVectorState(sew, vlen);
    AS.VSSUBU(dst, dst, src);
    rec.setOperandVec(&operands[0], dst);
}

void PSUB(Recompiler& rec, const HandlerMetadata& meta, ZydisDecodedInstruction& instruction, ZydisDecodedOperand* operands, SEW sew, u8 vlen) {
    biscuit::Vec dst = rec.getOperandVec(&operands[0]);
    biscuit::Vec src = rec.getOperandVec(&operands[1]);
    rec.setVectorState(sew, vlen);
    AS.VSUB(dst, dst, src);
    rec.setOperandVec(&operands[0], dst);
}

FAST_HANDLE(PADDB) {
    PADD(rec, meta, instruction, operands, SEW::E8, rec.maxVlen() / 8);
}

FAST_HANDLE(PADDW) {
    PADD(rec, meta, instruction, operands, SEW::E16, rec.maxVlen() / 16);
}

FAST_HANDLE(PADDD) {
    PADD(rec, meta, instruction, operands, SEW::E32, rec.maxVlen() / 32);
}

FAST_HANDLE(PADDQ) {
    PADD(rec, meta, instruction, operands, SEW::E64, rec.maxVlen() / 64);
}

FAST_HANDLE(PADDSB) {
    PADDS(rec, meta, instruction, operands, SEW::E8, rec.maxVlen() / 8);
}

FAST_HANDLE(PADDSW) {
    PADDS(rec, meta, instruction, operands, SEW::E16, rec.maxVlen() / 16);
}

FAST_HANDLE(PSUBSB) {
    PSUBS(rec, meta, instruction, operands, SEW::E8, rec.maxVlen() / 8);
}

FAST_HANDLE(PSUBSW) {
    PSUBS(rec, meta, instruction, operands, SEW::E16, rec.maxVlen() / 16);
}

FAST_HANDLE(PADDUSB) {
    PADDSU(rec, meta, instruction, operands, SEW::E8, rec.maxVlen() / 8);
}

FAST_HANDLE(PADDUSW) { // Fuzzed
    PADDSU(rec, meta, instruction, operands, SEW::E16, rec.maxVlen() / 16);
}

FAST_HANDLE(PSUBUSB) {
    PSUBSU(rec, meta, instruction, operands, SEW::E8, rec.maxVlen() / 8);
}

FAST_HANDLE(PSUBUSW) {
    PSUBSU(rec, meta, instruction, operands, SEW::E16, rec.maxVlen() / 16);
}

FAST_HANDLE(PSUBB) {
    PSUB(rec, meta, instruction, operands, SEW::E8, rec.maxVlen() / 8);
}

FAST_HANDLE(PSUBW) {
    PSUB(rec, meta, instruction, operands, SEW::E16, rec.maxVlen() / 16);
}

FAST_HANDLE(PSUBD) {
    PSUB(rec, meta, instruction, operands, SEW::E32, rec.maxVlen() / 32);
}

FAST_HANDLE(PSUBQ) {
    PSUB(rec, meta, instruction, operands, SEW::E64, rec.maxVlen() / 64);
}

FAST_HANDLE(ADDPS) {
    biscuit::Vec dst = rec.getOperandVec(&operands[0]);
    biscuit::Vec src = rec.getOperandVec(&operands[1]);
    rec.setVectorState(SEW::E32, rec.maxVlen() / 32);
    AS.VFADD(dst, dst, src);
    rec.setOperandVec(&operands[0], dst);
}

FAST_HANDLE(ADDPD) {
    biscuit::Vec dst = rec.getOperandVec(&operands[0]);
    biscuit::Vec src = rec.getOperandVec(&operands[1]);
    rec.setVectorState(SEW::E64, rec.maxVlen() / 64);
    AS.VFADD(dst, dst, src);
    rec.setOperandVec(&operands[0], dst);
}

FAST_HANDLE(SUBPS) {
    biscuit::Vec dst = rec.getOperandVec(&operands[0]);
    biscuit::Vec src = rec.getOperandVec(&operands[1]);
    rec.setVectorState(SEW::E32, rec.maxVlen() / 32);
    AS.VFSUB(dst, dst, src);
    rec.setOperandVec(&operands[0], dst);
}

FAST_HANDLE(SUBPD) {
    biscuit::Vec dst = rec.getOperandVec(&operands[0]);
    biscuit::Vec src = rec.getOperandVec(&operands[1]);
    rec.setVectorState(SEW::E64, rec.maxVlen() / 64);
    AS.VFSUB(dst, dst, src);
    rec.setOperandVec(&operands[0], dst);
}

FAST_HANDLE(MINPS) {
    biscuit::Vec dst = rec.getOperandVec(&operands[0]);
    biscuit::Vec src = rec.getOperandVec(&operands[1]);
    biscuit::Vec nan_mask_1 = rec.scratchVec();
    biscuit::Vec nan_mask_2 = rec.scratchVec();
    biscuit::Vec equal_mask = rec.scratchVec();
    biscuit::Vec zero_mask = rec.scratchVec();
    biscuit::Vec neg_zero_mask = rec.scratchVec();
    rec.setVectorState(SEW::E32, rec.maxVlen() / 32);

    // When either operand is NaN, or they are both 0.0 or both are -0.0, the result is the source
    AS.VMFNE(nan_mask_1, dst, dst); // When a register isn't equal to itself, that element must be NaN
    AS.VMFNE(nan_mask_2, src, src);
    AS.VMOR(nan_mask_1, nan_mask_1, nan_mask_2);
    AS.FMV_W_X(ft8, x0);                          // 0.0
    AS.FSGNJN_S(ft9, ft8, ft8);                   // -0.0
    AS.VMFEQ(equal_mask, dst, src);               // Check where they are equal
    AS.VMFEQ(zero_mask, dst, ft8);                // Check where dst is 0.0
    AS.VMFEQ(neg_zero_mask, dst, ft9);            // Check where dst is -0.0
    AS.VMOR(zero_mask, zero_mask, neg_zero_mask); // Either 0.0 or -0.0
    AS.VMAND(equal_mask, equal_mask, zero_mask);  // Check where they are both zeroes
    AS.VMOR(v0, nan_mask_1, equal_mask);          // Combine the masks

    AS.VFMIN(nan_mask_2, dst, src);        // actual max result calculation
    AS.VMERGE(zero_mask, nan_mask_2, src); // Where v0 is 1's, use src, otherwise use result of vfmax
    rec.setOperandVec(&operands[0], zero_mask);
}

FAST_HANDLE(MINPD) {
    biscuit::Vec dst = rec.getOperandVec(&operands[0]);
    biscuit::Vec src = rec.getOperandVec(&operands[1]);
    biscuit::Vec nan_mask_1 = rec.scratchVec();
    biscuit::Vec nan_mask_2 = rec.scratchVec();
    biscuit::Vec equal_mask = rec.scratchVec();
    biscuit::Vec zero_mask = rec.scratchVec();
    biscuit::Vec neg_zero_mask = rec.scratchVec();
    rec.setVectorState(SEW::E64, rec.maxVlen() / 64);

    // When either operand is NaN, or they are both 0.0 or both are -0.0, the result is the source
    AS.VMFNE(nan_mask_1, dst, dst); // When a register isn't equal to itself, that element must be NaN
    AS.VMFNE(nan_mask_2, src, src);
    AS.VMOR(nan_mask_1, nan_mask_1, nan_mask_2);
    AS.FMV_D_X(ft8, x0);                          // 0.0
    AS.FSGNJN_D(ft9, ft8, ft8);                   // -0.0
    AS.VMFEQ(equal_mask, dst, src);               // Check where they are equal
    AS.VMFEQ(zero_mask, dst, ft8);                // Check where dst is 0.0
    AS.VMFEQ(neg_zero_mask, dst, ft9);            // Check where dst is -0.0
    AS.VMOR(zero_mask, zero_mask, neg_zero_mask); // Either 0.0 or -0.0
    AS.VMAND(equal_mask, equal_mask, zero_mask);  // They are both zeroes
    AS.VMOR(v0, nan_mask_1, equal_mask);          // Combine the masks

    AS.VFMIN(nan_mask_2, dst, src);        // actual max result calculation
    AS.VMERGE(zero_mask, nan_mask_2, src); // Where v0 is 1's, use src, otherwise use result of vfmax
    rec.setOperandVec(&operands[0], zero_mask);
}

FAST_HANDLE(PMINUB) {
    biscuit::Vec dst = rec.getOperandVec(&operands[0]);
    biscuit::Vec src = rec.getOperandVec(&operands[1]);
    rec.setVectorState(SEW::E8, rec.maxVlen() / 8);
    AS.VMINU(dst, dst, src);
    rec.setOperandVec(&operands[0], dst);
}

FAST_HANDLE(PMINUW) {
    biscuit::Vec dst = rec.getOperandVec(&operands[0]);
    biscuit::Vec src = rec.getOperandVec(&operands[1]);
    rec.setVectorState(SEW::E16, rec.maxVlen() / 16);
    AS.VMINU(dst, dst, src);
    rec.setOperandVec(&operands[0], dst);
}

FAST_HANDLE(PMINUD) {
    biscuit::Vec dst = rec.getOperandVec(&operands[0]);
    biscuit::Vec src = rec.getOperandVec(&operands[1]);
    rec.setVectorState(SEW::E32, rec.maxVlen() / 32);
    AS.VMINU(dst, dst, src);
    rec.setOperandVec(&operands[0], dst);
}

FAST_HANDLE(PMAXUB) {
    biscuit::Vec dst = rec.getOperandVec(&operands[0]);
    biscuit::Vec src = rec.getOperandVec(&operands[1]);
    rec.setVectorState(SEW::E8, rec.maxVlen() / 8);
    AS.VMAXU(dst, dst, src);
    rec.setOperandVec(&operands[0], dst);
}

FAST_HANDLE(PMAXUW) {
    biscuit::Vec dst = rec.getOperandVec(&operands[0]);
    biscuit::Vec src = rec.getOperandVec(&operands[1]);
    rec.setVectorState(SEW::E16, rec.maxVlen() / 16);
    AS.VMAXU(dst, dst, src);
    rec.setOperandVec(&operands[0], dst);
}

FAST_HANDLE(PMAXUD) {
    biscuit::Vec dst = rec.getOperandVec(&operands[0]);
    biscuit::Vec src = rec.getOperandVec(&operands[1]);
    rec.setVectorState(SEW::E32, rec.maxVlen() / 32);
    AS.VMAXU(dst, dst, src);
    rec.setOperandVec(&operands[0], dst);
}

FAST_HANDLE(PMINSB) {
    biscuit::Vec dst = rec.getOperandVec(&operands[0]);
    biscuit::Vec src = rec.getOperandVec(&operands[1]);
    rec.setVectorState(SEW::E8, rec.maxVlen() / 8);
    AS.VMIN(dst, dst, src);
    rec.setOperandVec(&operands[0], dst);
}

FAST_HANDLE(PMINSW) {
    biscuit::Vec dst = rec.getOperandVec(&operands[0]);
    biscuit::Vec src = rec.getOperandVec(&operands[1]);
    rec.setVectorState(SEW::E16, rec.maxVlen() / 16);
    AS.VMIN(dst, dst, src);
    rec.setOperandVec(&operands[0], dst);
}

FAST_HANDLE(PMINSD) {
    biscuit::Vec dst = rec.getOperandVec(&operands[0]);
    biscuit::Vec src = rec.getOperandVec(&operands[1]);
    rec.setVectorState(SEW::E32, rec.maxVlen() / 32);
    AS.VMIN(dst, dst, src);
    rec.setOperandVec(&operands[0], dst);
}

FAST_HANDLE(PMAXSB) {
    biscuit::Vec dst = rec.getOperandVec(&operands[0]);
    biscuit::Vec src = rec.getOperandVec(&operands[1]);
    rec.setVectorState(SEW::E8, rec.maxVlen() / 8);
    AS.VMAX(dst, dst, src);
    rec.setOperandVec(&operands[0], dst);
}

FAST_HANDLE(PMAXSW) {
    biscuit::Vec dst = rec.getOperandVec(&operands[0]);
    biscuit::Vec src = rec.getOperandVec(&operands[1]);
    rec.setVectorState(SEW::E16, rec.maxVlen() / 16);
    AS.VMAX(dst, dst, src);
    rec.setOperandVec(&operands[0], dst);
}

FAST_HANDLE(PMAXSD) {
    biscuit::Vec dst = rec.getOperandVec(&operands[0]);
    biscuit::Vec src = rec.getOperandVec(&operands[1]);
    rec.setVectorState(SEW::E32, rec.maxVlen() / 32);
    AS.VMAX(dst, dst, src);
    rec.setOperandVec(&operands[0], dst);
}

FAST_HANDLE(PMULHW) { // Fuzzed
    biscuit::Vec dst = rec.getOperandVec(&operands[0]);
    biscuit::Vec src = rec.getOperandVec(&operands[1]);
    rec.setVectorState(SEW::E16, rec.maxVlen() / 16);
    AS.VMULH(dst, dst, src);
    rec.setOperandVec(&operands[0], dst);
}

FAST_HANDLE(PMULHUW) { // Fuzzed
    biscuit::Vec dst = rec.getOperandVec(&operands[0]);
    biscuit::Vec src = rec.getOperandVec(&operands[1]);
    rec.setVectorState(SEW::E16, rec.maxVlen() / 16);
    AS.VMULHU(dst, dst, src);
    rec.setOperandVec(&operands[0], dst);
}

FAST_HANDLE(PMULLW) { // Fuzzed
    biscuit::Vec dst = rec.getOperandVec(&operands[0]);
    biscuit::Vec src = rec.getOperandVec(&operands[1]);
    rec.setVectorState(SEW::E16, rec.maxVlen() / 16);
    AS.VMUL(dst, dst, src);
    rec.setOperandVec(&operands[0], dst);
}

FAST_HANDLE(PMULLD) { // Fuzzed
    biscuit::Vec dst = rec.getOperandVec(&operands[0]);
    biscuit::Vec src = rec.getOperandVec(&operands[1]);
    rec.setVectorState(SEW::E32, rec.maxVlen() / 32);
    AS.VMUL(dst, dst, src);
    rec.setOperandVec(&operands[0], dst);
}

FAST_HANDLE(PMULUDQ) { // Fuzzed
    biscuit::GPR shift = rec.scratch();
    biscuit::Vec dst = rec.getOperandVec(&operands[0]);
    biscuit::Vec src = rec.getOperandVec(&operands[1]);
    biscuit::Vec dst_masked = rec.scratchVec();
    biscuit::Vec src_masked = rec.scratchVec();
    biscuit::Vec result = rec.scratchVec();

    rec.setVectorState(SEW::E64, rec.maxVlen() / 64);
    AS.LI(shift, 32);
    AS.VSLL(dst_masked, dst, shift);
    AS.VSRL(dst_masked, dst_masked, shift);
    AS.VSLL(src_masked, src, shift);
    AS.VSRL(src_masked, src_masked, shift);
    AS.VMUL(result, dst_masked, src_masked);

    rec.setOperandVec(&operands[0], result);
}

FAST_HANDLE(PMULDQ) { // Fuzzed
    biscuit::GPR shift = rec.scratch();
    biscuit::Vec dst = rec.getOperandVec(&operands[0]);
    biscuit::Vec src = rec.getOperandVec(&operands[1]);
    biscuit::Vec dst_masked = rec.scratchVec();
    biscuit::Vec src_masked = rec.scratchVec();
    biscuit::Vec result = rec.scratchVec();

    rec.setVectorState(SEW::E64, rec.maxVlen() / 64);
    AS.LI(shift, 32);
    AS.VSLL(dst_masked, dst, shift);
    AS.VSRA(dst_masked, dst_masked, shift);
    AS.VSLL(src_masked, src, shift);
    AS.VSRA(src_masked, src_masked, shift);
    AS.VMUL(result, dst_masked, src_masked);

    rec.setOperandVec(&operands[0], result);
}

FAST_HANDLE(PMADDWD) { // Fuzzed
    VEC_function(rec, meta, instruction, operands, (u64)&felix86_pmaddwd);
}

FAST_HANDLE(MAXPS) {
    biscuit::Vec dst = rec.getOperandVec(&operands[0]);
    biscuit::Vec src = rec.getOperandVec(&operands[1]);
    biscuit::Vec nan_mask_1 = rec.scratchVec();
    biscuit::Vec nan_mask_2 = rec.scratchVec();
    biscuit::Vec equal_mask = rec.scratchVec();
    biscuit::Vec zero_mask = rec.scratchVec();
    biscuit::Vec neg_zero_mask = rec.scratchVec();
    rec.setVectorState(SEW::E32, rec.maxVlen() / 32);

    // When either operand is NaN, or they are both 0.0 or both are -0.0, the result is the source
    AS.VMFNE(nan_mask_1, dst, dst); // When a register isn't equal to itself, that element must be NaN
    AS.VMFNE(nan_mask_2, src, src);
    AS.VMOR(nan_mask_1, nan_mask_1, nan_mask_2);
    AS.FMV_W_X(ft8, x0);                          // 0.0
    AS.FSGNJN_S(ft9, ft8, ft8);                   // -0.0
    AS.VMFEQ(equal_mask, dst, src);               // Check where they are equal
    AS.VMFEQ(zero_mask, dst, ft8);                // Check where dst is 0.0
    AS.VMFEQ(neg_zero_mask, dst, ft9);            // Check where dst is -0.0
    AS.VMOR(zero_mask, zero_mask, neg_zero_mask); // Either 0.0 or -0.0
    AS.VMAND(equal_mask, equal_mask, zero_mask);  // Check where they are both zeroes
    AS.VMOR(v0, nan_mask_1, equal_mask);          // Combine the masks

    AS.VFMAX(nan_mask_2, dst, src);        // actual max result calculation
    AS.VMERGE(zero_mask, nan_mask_2, src); // Where v0 is 1's, use src, otherwise use result of vfmax
    rec.setOperandVec(&operands[0], zero_mask);
}

FAST_HANDLE(MAXPD) {
    biscuit::Vec dst = rec.getOperandVec(&operands[0]);
    biscuit::Vec src = rec.getOperandVec(&operands[1]);
    biscuit::Vec nan_mask_1 = rec.scratchVec();
    biscuit::Vec nan_mask_2 = rec.scratchVec();
    biscuit::Vec equal_mask = rec.scratchVec();
    biscuit::Vec zero_mask = rec.scratchVec();
    biscuit::Vec neg_zero_mask = rec.scratchVec();
    rec.setVectorState(SEW::E64, rec.maxVlen() / 64);

    // When either operand is NaN, or they are both 0.0 or both are -0.0, the result is the source
    AS.VMFNE(nan_mask_1, dst, dst); // When a register isn't equal to itself, that element must be NaN
    AS.VMFNE(nan_mask_2, src, src);
    AS.VMOR(nan_mask_1, nan_mask_1, nan_mask_2);
    AS.FMV_D_X(ft8, x0);                          // 0.0
    AS.FSGNJN_D(ft9, ft8, ft8);                   // -0.0
    AS.VMFEQ(equal_mask, dst, src);               // Check where they are equal
    AS.VMFEQ(zero_mask, dst, ft8);                // Check where dst is 0.0
    AS.VMFEQ(neg_zero_mask, dst, ft9);            // Check where dst is -0.0
    AS.VMOR(zero_mask, zero_mask, neg_zero_mask); // Either 0.0 or -0.0
    AS.VMAND(equal_mask, equal_mask, zero_mask);  // They are both zeroes
    AS.VMOR(v0, nan_mask_1, equal_mask);          // Combine the masks

    AS.VFMAX(nan_mask_2, dst, src);        // actual max result calculation
    AS.VMERGE(zero_mask, nan_mask_2, src); // Where v0 is 1's, use src, otherwise use result of vfmax
    rec.setOperandVec(&operands[0], zero_mask);
}

FAST_HANDLE(MULPS) { // Fuzzed, TODO: needs NaN handling
    biscuit::Vec dst = rec.getOperandVec(&operands[0]);
    biscuit::Vec src = rec.getOperandVec(&operands[1]);
    rec.setVectorState(SEW::E32, rec.maxVlen() / 32);
    AS.VFMUL(dst, dst, src);
    rec.setOperandVec(&operands[0], dst);
}

FAST_HANDLE(MULPD) {
    biscuit::Vec dst = rec.getOperandVec(&operands[0]);
    biscuit::Vec src = rec.getOperandVec(&operands[1]);
    rec.setVectorState(SEW::E64, rec.maxVlen() / 64);
    AS.VFMUL(dst, dst, src);
    rec.setOperandVec(&operands[0], dst);
}

FAST_HANDLE(SQRTPS) { // Fuzzed, TODO: needs NaN handling
    biscuit::Vec dst = rec.allocatedVec(rec.zydisToRef(operands[0].reg.value));
    biscuit::Vec src = rec.getOperandVec(&operands[1]);
    rec.setVectorState(SEW::E32, rec.maxVlen() / 32);
    AS.VFSQRT(dst, src);
    rec.setOperandVec(&operands[0], dst);
}

FAST_HANDLE(SQRTPD) {
    biscuit::Vec dst = rec.allocatedVec(rec.zydisToRef(operands[0].reg.value));
    biscuit::Vec src = rec.getOperandVec(&operands[1]);
    rec.setVectorState(SEW::E64, rec.maxVlen() / 64);
    AS.VFSQRT(dst, src);
    rec.setOperandVec(&operands[0], dst);
}

FAST_HANDLE(DIVPS) {
    biscuit::Vec dst = rec.getOperandVec(&operands[0]);
    biscuit::Vec src = rec.getOperandVec(&operands[1]);
    rec.setVectorState(SEW::E32, rec.maxVlen() / 32);
    AS.VFDIV(dst, dst, src);
    rec.setOperandVec(&operands[0], dst);
}

FAST_HANDLE(DIVPD) {
    biscuit::Vec dst = rec.getOperandVec(&operands[0]);
    biscuit::Vec src = rec.getOperandVec(&operands[1]);
    rec.setVectorState(SEW::E64, rec.maxVlen() / 64);
    AS.VFDIV(dst, dst, src);
    rec.setOperandVec(&operands[0], dst);
}

FAST_HANDLE(RCPPS) {
    biscuit::Vec dst = rec.allocatedVec(rec.zydisToRef(operands[0].reg.value));
    biscuit::Vec src = rec.getOperandVec(&operands[1]);
    biscuit::Vec ones = rec.scratchVec();
    rec.setVectorState(SEW::E32, rec.maxVlen() / 32);
    biscuit::GPR scratch = rec.scratch();
    AS.LI(scratch, 0x3f800000);
    AS.VMV(ones, scratch);
    AS.VFDIV(dst, ones, src);
    rec.setOperandVec(&operands[0], dst);
}

FAST_HANDLE(RSQRTPS) {
    biscuit::Vec temp = rec.scratchVec();
    biscuit::Vec ones = rec.scratchVec();
    biscuit::Vec dst = rec.allocatedVec(rec.zydisToRef(operands[0].reg.value));
    biscuit::Vec src = rec.getOperandVec(&operands[1]);
    rec.setVectorState(SEW::E32, rec.maxVlen() / 32);
    biscuit::GPR scratch = rec.scratch();
    AS.LI(scratch, 0x3f800000);
    AS.VMV(ones, scratch);
    AS.VFSQRT(temp, src);
    AS.VFDIV(dst, ones, temp);
    rec.setOperandVec(&operands[0], dst);
}

FAST_HANDLE(MOVSB) {
    u8 width = instruction.operand_width;
    biscuit::GPR rdi = rec.getRefGPR(X86_REF_RDI, X86_SIZE_QWORD);
    biscuit::GPR rsi = rec.getRefGPR(X86_REF_RSI, X86_SIZE_QWORD);
    biscuit::GPR rcx = rec.getRefGPR(X86_REF_RCX, X86_SIZE_QWORD);
    biscuit::GPR temp = rec.scratch();
    biscuit::GPR data = rec.scratch();
    biscuit::GPR df = rec.scratch();
    AS.LBU(df, offsetof(ThreadState, df), rec.threadStatePointer());

    Label end;
    AS.LI(temp, -width / 8);
    AS.BNEZ(df, &end);
    AS.LI(temp, width / 8);
    AS.Bind(&end);

    Label loop_end, loop_body;
    if (HAS_REP) {
        rec.repPrologue(&loop_end, rcx);
        AS.Bind(&loop_body);
    }

    rec.readMemory(data, rsi, 0, rec.zydisToSize(width));
    rec.writeMemory(data, rdi, 0, rec.zydisToSize(width));

    AS.ADD(rdi, rdi, temp);
    AS.ADD(rsi, rsi, temp);

    if (HAS_REP) {
        rec.repEpilogue(&loop_body, rcx);
        AS.Bind(&loop_end);
    }

    rec.setRefGPR(X86_REF_RDI, X86_SIZE_QWORD, rdi);
    rec.setRefGPR(X86_REF_RSI, X86_SIZE_QWORD, rsi);
    rec.setRefGPR(X86_REF_RCX, X86_SIZE_QWORD, rcx);
}

FAST_HANDLE(MOVSW) {
    fast_MOVSB(rec, meta, instruction, operands);
}

// The rep movsd and sse movsd have the same mnemonic, so we differentiate it like this
FAST_HANDLE(MOVSD_sse) {
    if (operands[0].type == ZYDIS_OPERAND_TYPE_MEMORY) {
        biscuit::Vec src = rec.getOperandVec(&operands[1]);
        rec.setOperandVec(&operands[0], src);
    } else {
        biscuit::Vec result = rec.scratchVec();
        biscuit::Vec dst = rec.getOperandVec(&operands[0]);
        biscuit::Vec src = rec.getOperandVec(&operands[1]);
        rec.setVectorState(SEW::E64, rec.maxVlen() / 64);
        AS.VMV(v0, 1);
        if (operands[1].type == ZYDIS_OPERAND_TYPE_MEMORY) {
            // Only when src is memory are the upper bits zeroed
            AS.VMV(result, 0);
            AS.VOR(result, src, 0, VecMask::Yes);
        } else {
            AS.VMERGE(result, dst, src);
        }
        rec.setOperandVec(&operands[0], result);
    }
}

FAST_HANDLE(MOVSD) {
    if (instruction.meta.isa_set == ZYDIS_ISA_SET_SSE2) {
        fast_MOVSD_sse(rec, meta, instruction, operands);
    } else if (instruction.meta.isa_set == ZYDIS_ISA_SET_I386) {
        fast_MOVSB(rec, meta, instruction, operands);
    } else {
        UNREACHABLE();
    }
}

FAST_HANDLE(MOVSQ) {
    fast_MOVSB(rec, meta, instruction, operands);
}

FAST_HANDLE(CMPSB) {
    u8 width = instruction.operand_width;
    biscuit::GPR rdi = rec.getRefGPR(X86_REF_RDI, X86_SIZE_QWORD);
    biscuit::GPR rsi = rec.getRefGPR(X86_REF_RSI, X86_SIZE_QWORD);
    biscuit::GPR rcx = rec.getRefGPR(X86_REF_RCX, X86_SIZE_QWORD);
    biscuit::GPR temp = rec.scratch();
    biscuit::GPR src1 = rec.scratch();
    biscuit::GPR src2 = rec.scratch();
    biscuit::GPR result = rec.scratch();
    biscuit::GPR df = rec.scratch();
    AS.LBU(df, offsetof(ThreadState, df), rec.threadStatePointer());
    x86_size_e size = rec.zydisToSize(width);

    Label end;
    AS.LI(temp, -width / 8);
    AS.BNEZ(df, &end);
    AS.LI(temp, width / 8);
    AS.Bind(&end);

    Label loop_end, loop_body;
    if (HAS_REP) {
        rec.repPrologue(&loop_end, rcx);
        AS.Bind(&loop_body);
    }

    rec.readMemory(src1, rsi, 0, size);
    rec.readMemory(src2, rdi, 0, size);

    AS.SUB(result, src1, src2);

    SetCmpFlags(meta, rec, src1, src2, result, size, false);

    AS.ADD(rdi, rdi, temp);
    AS.ADD(rsi, rsi, temp);

    if (HAS_REP) {
        rec.repzEpilogue(&loop_body, &loop_end, rcx, instruction.attributes & ZYDIS_ATTRIB_HAS_REPZ);
        AS.Bind(&loop_end);
    }

    rec.setRefGPR(X86_REF_RDI, X86_SIZE_QWORD, rdi);
    rec.setRefGPR(X86_REF_RSI, X86_SIZE_QWORD, rsi);
    rec.setRefGPR(X86_REF_RCX, X86_SIZE_QWORD, rcx);
}

FAST_HANDLE(CMPSW) {
    fast_CMPSB(rec, meta, instruction, operands);
}

FAST_HANDLE(CMPSD_string) {
    fast_CMPSB(rec, meta, instruction, operands);
}

FAST_HANDLE(CMPSQ) {
    fast_CMPSB(rec, meta, instruction, operands);
}

FAST_HANDLE(SCASB) {
    u8 width = instruction.operand_width;
    x86_size_e size = rec.zydisToSize(width);
    biscuit::GPR rax = rec.getRefGPR(X86_REF_RAX, size);
    biscuit::GPR rdi = rec.getRefGPR(X86_REF_RDI, X86_SIZE_QWORD);
    biscuit::GPR rcx = rec.getRefGPR(X86_REF_RCX, X86_SIZE_QWORD);
    biscuit::GPR temp = rec.scratch();
    biscuit::GPR src2 = rec.scratch();
    biscuit::GPR result = rec.scratch();
    biscuit::GPR df = rec.scratch();
    AS.LBU(df, offsetof(ThreadState, df), rec.threadStatePointer());

    Label end;
    AS.LI(temp, -width / 8);
    AS.BNEZ(df, &end);
    AS.LI(temp, width / 8);
    AS.Bind(&end);

    Label loop_end, loop_body;
    if (HAS_REP) {
        rec.repPrologue(&loop_end, rcx);
        AS.Bind(&loop_body);
    }

    rec.readMemory(src2, rdi, 0, size);

    AS.SUB(result, rax, src2);

    SetCmpFlags(meta, rec, rax, src2, result, size, false);

    AS.ADD(rdi, rdi, temp);

    if (HAS_REP) {
        rec.repzEpilogue(&loop_body, &loop_end, rcx, instruction.attributes & ZYDIS_ATTRIB_HAS_REPZ);
        AS.Bind(&loop_end);
    }

    rec.setRefGPR(X86_REF_RDI, X86_SIZE_QWORD, rdi);
    rec.setRefGPR(X86_REF_RCX, X86_SIZE_QWORD, rcx);
}

FAST_HANDLE(SCASW) {
    fast_SCASB(rec, meta, instruction, operands);
}

FAST_HANDLE(SCASD) {
    fast_SCASB(rec, meta, instruction, operands);
}

FAST_HANDLE(SCASQ) {
    fast_SCASB(rec, meta, instruction, operands);
}

FAST_HANDLE(STOSB) {
#if 0
    rec.writebackDirtyState();
    AS.LI(t0, (u64)print_args);
    AS.MV(a0, rec.threadStatePointer());
    AS.JALR(t0);
#endif

    Label loop_end, loop_body;
    u8 width = instruction.operand_width;
    biscuit::GPR rdi = rec.getRefGPR(X86_REF_RDI, X86_SIZE_QWORD);
    biscuit::GPR rcx = rec.getRefGPR(X86_REF_RCX, X86_SIZE_QWORD);
    biscuit::GPR rax = rec.getRefGPR(X86_REF_RAX, rec.zydisToSize(width));
    biscuit::GPR temp = rec.scratch();
    biscuit::GPR df = rec.scratch();
    AS.LBU(df, offsetof(ThreadState, df), rec.threadStatePointer());

    Label end;
    AS.LI(temp, -width / 8);
    AS.BNEZ(df, &end);
    AS.LI(temp, width / 8);
    AS.Bind(&end);

    if (HAS_REP) {
        rec.repPrologue(&loop_end, rcx);
        AS.Bind(&loop_body);
    }

    rec.writeMemory(rax, rdi, 0, rec.zydisToSize(width));
    AS.ADD(rdi, rdi, temp);

    if (HAS_REP) {
        rec.repEpilogue(&loop_body, rcx);
        AS.Bind(&loop_end);
    }

    rec.setRefGPR(X86_REF_RDI, X86_SIZE_QWORD, rdi);
    rec.setRefGPR(X86_REF_RCX, X86_SIZE_QWORD, rcx);
}

FAST_HANDLE(STOSW) {
    fast_STOSB(rec, meta, instruction, operands);
}

FAST_HANDLE(STOSD) {
    fast_STOSB(rec, meta, instruction, operands);
}

FAST_HANDLE(STOSQ) {
    fast_STOSB(rec, meta, instruction, operands);
}

FAST_HANDLE(MOVHPS) {
    if (operands[0].type == ZYDIS_OPERAND_TYPE_MEMORY) {
        biscuit::Vec temp = rec.scratchVec();
        biscuit::Vec src = rec.getOperandVec(&operands[1]);
        rec.setVectorState(SEW::E64, rec.maxVlen() / 64);
        AS.VSLIDEDOWN(temp, src, 1);
        rec.setOperandVec(&operands[0], temp);
    } else if (operands[1].type == ZYDIS_OPERAND_TYPE_MEMORY) {
        biscuit::Vec temp = rec.scratchVec();
        biscuit::Vec dst = rec.getOperandVec(&operands[0]);
        biscuit::Vec src = rec.getOperandVec(&operands[1]);
        rec.setVectorState(SEW::E64, rec.maxVlen() / 64);
        AS.VSLIDEUP(temp, src, 1);
        AS.VMV(v0, 0b10);
        AS.VMERGE(dst, dst, temp);
        rec.setOperandVec(&operands[0], dst);
    } else {
        UNREACHABLE();
    }
}

FAST_HANDLE(MOVHPD) {
    fast_MOVHPS(rec, meta, instruction, operands);
}

FAST_HANDLE(SHUFPD) {
    u8 imm = rec.getImmediate(&operands[2]);
    biscuit::GPR temp = rec.scratch();
    biscuit::Vec vtemp = rec.scratchVec();
    biscuit::Vec vsrc = rec.scratchVec();
    biscuit::Vec dst = rec.getOperandVec(&operands[0]);
    biscuit::Vec src = rec.getOperandVec(&operands[1]);

    rec.setVectorState(SEW::E64, rec.maxVlen() / 64);

    if ((imm & 0b1) == 0) {
        AS.VMV_XS(temp, dst);
    } else {
        AS.VSLIDEDOWN(vtemp, dst, 1);
        AS.VMV_XS(temp, vtemp);
    }

    if ((imm & 0b10) != 0) {
        AS.VSLIDEDOWN(vsrc, src, 1);
    } else {
        vsrc = src;
    }

    AS.VSLIDE1UP(vtemp, vsrc, temp);

    rec.setOperandVec(&operands[0], vtemp);
}

FAST_HANDLE(LEAVE) {
    x86_size_e size = rec.zydisToSize(instruction.operand_width);
    ASSERT(size == X86_SIZE_QWORD);
    biscuit::GPR rbp = rec.getRefGPR(X86_REF_RBP, X86_SIZE_QWORD);
    AS.ADDI(rbp, rbp, 8);
    rec.setRefGPR(X86_REF_RSP, X86_SIZE_QWORD, rbp);
    rec.readMemory(rbp, rbp, -8, size);
    rec.setRefGPR(X86_REF_RBP, X86_SIZE_QWORD, rbp);
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
    AS.NOT(result, dst);
    rec.setOperandGPR(&operands[0], result);
}

FAST_HANDLE(NEG) {
    x86_size_e size = rec.getOperandSize(&operands[0]);
    biscuit::GPR result = rec.scratch();
    biscuit::GPR dst = rec.getOperandGPR(&operands[0]);
    if (size == X86_SIZE_BYTE || size == X86_SIZE_BYTE_HIGH) {
        rec.sextb(result, dst);
        AS.NEG(result, result);
    } else if (size == X86_SIZE_WORD) {
        rec.sexth(result, dst);
        AS.NEG(result, result);
    } else if (size == X86_SIZE_DWORD) {
        AS.SUBW(result, x0, dst);
    } else if (size == X86_SIZE_QWORD) {
        AS.NEG(result, dst);
    } else {
        UNREACHABLE();
    }

    if (rec.shouldEmitFlag(meta.rip, X86_REF_CF)) {
        biscuit::GPR cf = rec.flagW(X86_REF_CF);
        AS.SNEZ(cf, dst);
    }

    if (rec.shouldEmitFlag(meta.rip, X86_REF_ZF)) {
        rec.updateZero(result, size);
    }

    if (rec.shouldEmitFlag(meta.rip, X86_REF_OF)) {
        biscuit::GPR of = rec.flagW(X86_REF_OF);
        is_overflow_sub(rec, of, x0, dst, result, rec.getBitSize(size));
    }

    if (rec.shouldEmitFlag(meta.rip, X86_REF_SF)) {
        rec.updateSign(result, size);
    }

    if (rec.shouldEmitFlag(meta.rip, X86_REF_AF)) {
        biscuit::GPR af = rec.flagW(X86_REF_AF);
        AS.ANDI(af, dst, 0xF);
        AS.SNEZ(af, af);
    }

    if (rec.shouldEmitFlag(meta.rip, X86_REF_PF)) {
        rec.updateParity(result);
    }

    rec.setOperandGPR(&operands[0], result);
}

FAST_HANDLE(PACKUSWB) { // TODO: vectorize
    // There is no single instruction that can saturate a signed value into an unsigned destination. A sequence of two vector instructions that
    // rst removes negative numbers by performing a max against 0 using vmax then clips the resulting unsigned value into the destination
    // using vnclipu can be used if setting vxsat value for negative numbers is not required. A vsetvli is required inbetween these two
    // instructions to change SEW.
    VEC_function(rec, meta, instruction, operands, (u64)&felix86_packuswb);
}

FAST_HANDLE(PACKUSDW) {
    VEC_function(rec, meta, instruction, operands, (u64)&felix86_packusdw);
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
    AS.VNCLIP(result1, dst, 0);
    AS.VNCLIP(result2, src, 0);
    rec.setVectorState(SEW::E64, rec.maxVlen() / 64);
    AS.VMV(v0, 0b10);
    AS.VSLIDEUP(result2_up, result2, 1);
    AS.VMERGE(result, result1, result2_up);
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
    AS.VNCLIP(result1, dst, 0);
    AS.VNCLIP(result2, src, 0);
    rec.setVectorState(SEW::E64, rec.maxVlen() / 64);
    AS.VMV(v0, 0b10);
    AS.VSLIDEUP(result2_up, result2, 1);
    AS.VMERGE(result, result1, result2_up);
    rec.setOperandVec(&operands[0], result);
}

void ROUND(Recompiler& rec, const HandlerMetadata& meta, ZydisDecodedInstruction& instruction, ZydisDecodedOperand* operands, SEW sew, u8 vlen) {
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
    AS.VFMV_FS(ft8, src);

    if (Extensions::Zfa) {
        if (sew == SEW::E64) {
            AS.FROUND_D(ft9, ft8, rmode);
        } else if (sew == SEW::E32) {
            AS.FROUND_S(ft9, ft8, rmode);
        } else {
            UNREACHABLE();
        }
    } else {
        biscuit::GPR temp = rec.scratch();
        if (sew == SEW::E64) {
            AS.FCVT_L_D(temp, ft8, rmode);
            AS.FCVT_D_L(ft9, temp, rmode);
        } else if (sew == SEW::E32) {
            AS.FCVT_W_S(temp, ft8, rmode);
            AS.FCVT_S_W(ft9, temp, rmode);
        } else {
            UNREACHABLE();
        }
    }

    AS.VFMV_SF(dst, ft9);

    rec.setOperandVec(&operands[0], dst);
}

FAST_HANDLE(ROUNDSS) {
    ROUND(rec, meta, instruction, operands, SEW::E32, 1);
}

FAST_HANDLE(ROUNDSD) {
    ROUND(rec, meta, instruction, operands, SEW::E64, 1);
}

FAST_HANDLE(PMOVMSKB) {
    biscuit::GPR scratch = rec.scratch();
    biscuit::Vec src = rec.getOperandVec(&operands[1]);
    biscuit::Vec temp = rec.scratchVec();

    rec.setVectorState(SEW::E8, rec.maxVlen() / 8);
    AS.VMSLT(temp, src, x0);

    rec.setVectorState(SEW::E64, rec.maxVlen() / 64);
    AS.VMV_XS(scratch, temp);

    if (rec.maxVlen() == 128)
        rec.zext(scratch, scratch, X86_SIZE_WORD);
    else if (rec.maxVlen() == 256)
        rec.zext(scratch, scratch, X86_SIZE_DWORD);

    rec.setOperandGPR(&operands[0], scratch);
}

FAST_HANDLE(MOVMSKPS) {
    biscuit::Vec mask = rec.scratchVec();
    biscuit::Vec src = rec.getOperandVec(&operands[1]);
    biscuit::GPR dst = rec.scratch();

    rec.setVectorState(SEW::E32, rec.maxVlen() / 32);
    AS.VMSLT(mask, src, x0);
    AS.VMV_XS(dst, mask);
    AS.ANDI(dst, dst, 0b1111);
    rec.setOperandGPR(&operands[0], dst);
}

FAST_HANDLE(MOVMSKPD) {
    biscuit::Vec mask = rec.scratchVec();
    biscuit::Vec src = rec.getOperandVec(&operands[1]);
    biscuit::GPR dst = rec.scratch();

    rec.setVectorState(SEW::E64, rec.maxVlen() / 64);
    AS.VMSLT(mask, src, x0);
    AS.VMV_XS(dst, mask);
    AS.ANDI(dst, dst, 0b11);
    rec.setOperandGPR(&operands[0], dst);
}

FAST_HANDLE(PMOVZXBQ) {
    biscuit::GPR mask = rec.scratch();
    biscuit::Vec iota = rec.scratchVec();
    biscuit::Vec result = rec.scratchVec();
    biscuit::Vec src = rec.getOperandVec(&operands[1]);

    rec.setVectorState(SEW::E64, rec.maxVlen() / 64);
    AS.VID(iota); // iota with 64-bit elements will place the indices at the right locations
    rec.setVectorState(SEW::E8, rec.maxVlen() / 8);
    AS.LI(mask, 0b00000001'00000001'00000001'00000001);
    AS.VMV(result, 0);
    AS.VMV(v0, mask);
    AS.VRGATHER(result, src, iota, VecMask::Yes);

    rec.setOperandVec(&operands[0], result);
}

void PCMPEQ(Recompiler& rec, const HandlerMetadata& meta, ZydisDecodedInstruction& instruction, ZydisDecodedOperand* operands, SEW sew, u8 vlen) {
    biscuit::Vec zero = rec.scratchVec();
    biscuit::Vec dst = rec.getOperandVec(&operands[0]);
    biscuit::Vec src = rec.getOperandVec(&operands[1]);
    rec.setVectorState(sew, vlen);
    AS.VMV(zero, 0);
    AS.VMSEQ(v0, dst, src);
    AS.VMERGE(dst, zero, -1ll);
    rec.setOperandVec(&operands[0], dst);
}

void PCMPGT(Recompiler& rec, const HandlerMetadata& meta, ZydisDecodedInstruction& instruction, ZydisDecodedOperand* operands, SEW sew, u8 vlen) {
    biscuit::Vec zero = rec.scratchVec();
    biscuit::Vec dst = rec.getOperandVec(&operands[0]);
    biscuit::Vec src = rec.getOperandVec(&operands[1]);
    rec.setVectorState(sew, vlen);
    AS.VMV(zero, 0);
    AS.VMSLT(v0, src, dst);
    AS.VMERGE(dst, zero, -1ll);
    rec.setOperandVec(&operands[0], dst);
}

FAST_HANDLE(PCMPEQB) {
    PCMPEQ(rec, meta, instruction, operands, SEW::E8, rec.maxVlen() / 8);
}

FAST_HANDLE(PCMPEQW) {
    PCMPEQ(rec, meta, instruction, operands, SEW::E16, rec.maxVlen() / 16);
}

FAST_HANDLE(PCMPEQD) {
    PCMPEQ(rec, meta, instruction, operands, SEW::E32, rec.maxVlen() / 32);
}

FAST_HANDLE(PCMPEQQ) {
    PCMPEQ(rec, meta, instruction, operands, SEW::E64, rec.maxVlen() / 64);
}

FAST_HANDLE(PCMPGTB) {
    PCMPGT(rec, meta, instruction, operands, SEW::E8, rec.maxVlen() / 8);
}

FAST_HANDLE(PCMPGTW) {
    PCMPGT(rec, meta, instruction, operands, SEW::E16, rec.maxVlen() / 16);
}

FAST_HANDLE(PCMPGTD) {
    PCMPGT(rec, meta, instruction, operands, SEW::E32, rec.maxVlen() / 32);
}

FAST_HANDLE(PCMPGTQ) {
    PCMPGT(rec, meta, instruction, operands, SEW::E64, rec.maxVlen() / 64);
}

void CMPP(Recompiler& rec, const HandlerMetadata& meta, ZydisDecodedInstruction& instruction, ZydisDecodedOperand* operands, SEW sew, u8 vlen) {
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
        AS.VMFEQ(v0, dst, src);
        break;
    }
    case LT_OS: {
        AS.VMFLT(v0, dst, src);
        break;
    }
    case LE_OS: {
        AS.VMFLE(v0, dst, src);
        break;
    }
    case UNORD_Q: {
        // Set if either are NaN
        AS.VMFNE(temp1, dst, dst);
        AS.VMFNE(temp2, src, src);
        AS.VMOR(v0, temp1, temp2);
        break;
    }
    case NEQ_UQ: {
        AS.VMFNE(temp1, dst, dst);
        AS.VMFNE(temp2, src, src);
        AS.VMFNE(v0, dst, src);
        AS.VMOR(v0, v0, temp1);
        AS.VMOR(v0, v0, temp2);
        break;
    }
    case NLT_US: {
        AS.VMFNE(temp1, dst, dst);
        AS.VMFNE(temp2, src, src);
        AS.VMFLE(v0, src, dst);
        AS.VMOR(v0, v0, temp1);
        AS.VMOR(v0, v0, temp2);
        break;
    }
    case NLE_US: {
        AS.VMFNE(temp1, dst, dst);
        AS.VMFNE(temp2, src, src);
        AS.VMFLT(v0, src, dst);
        AS.VMOR(v0, v0, temp1);
        AS.VMOR(v0, v0, temp2);
        break;
    }
    case ORD_Q: {
        // Set if neither are NaN
        AS.VMFEQ(temp1, dst, dst);
        AS.VMFEQ(temp2, src, src);
        AS.VMAND(v0, temp1, temp2);
        break;
    }
    default: {
        UNREACHABLE();
        break;
    }
    }

    // Set to 1s where the mask is set
    AS.VMV(result, 0);
    AS.VOR(result, result, -1, VecMask::Yes);

    rec.setOperandVec(&operands[0], result);
}

FAST_HANDLE(CMPPS) { // Fuzzed
    CMPP(rec, meta, instruction, operands, SEW::E32, rec.maxVlen() / 32);
}

FAST_HANDLE(CMPPD) { // Fuzzed
    CMPP(rec, meta, instruction, operands, SEW::E64, rec.maxVlen() / 64);
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
    AS.LI(temp, mask);
    AS.VMV_SX(iota, temp);

    rec.setVectorState(SEW::E32, rec.maxVlen() / 32);
    AS.VRGATHEREI16(result, src, iota);

    rec.setOperandVec(&operands[0], result);
}

FAST_HANDLE(SHUFPS) {
    u8 imm = rec.getImmediate(&operands[2]);
    u8 el0 = imm & 0b11;
    u8 el1 = (imm >> 2) & 0b11;
    u8 el2 = (imm >> 4) & 0b11;
    u8 el3 = (imm >> 6) & 0b11;

    biscuit::Vec iota = rec.scratchVec();
    biscuit::Vec iota2 = rec.scratchVec();
    biscuit::GPR temp = rec.scratch();
    biscuit::Vec dst = rec.getOperandVec(&operands[0]);
    biscuit::Vec src = rec.getOperandVec(&operands[1]);
    biscuit::Vec result = rec.scratchVec();

    rec.setVectorState(SEW::E32, rec.maxVlen() / 32);
    AS.VMV(iota2, el3);
    AS.LI(temp, el2);
    AS.VSLIDE1UP(iota, iota2, temp);
    AS.LI(temp, el1);
    AS.VSLIDE1UP(iota2, iota, temp);
    AS.LI(temp, el0);
    AS.VSLIDE1UP(iota, iota2, temp);

    AS.VMV(v0, 0b11);
    AS.VMV(result, 0);
    AS.VRGATHER(result, dst, iota, VecMask::Yes);
    AS.VMV(v0, 0b1100);
    AS.VRGATHER(result, src, iota, VecMask::Yes);

    rec.setOperandVec(&operands[0], result);
}

FAST_HANDLE(PSHUFB) {
    biscuit::GPR bitmask = rec.scratch();
    biscuit::Vec tmp = rec.scratchVec();
    biscuit::Vec mask_masked = rec.scratchVec();
    biscuit::Vec dst = rec.getOperandVec(&operands[0]);
    biscuit::Vec mask = rec.getOperandVec(&operands[1]);

    rec.setVectorState(SEW::E8, rec.maxVlen() / 8);
    // Keep 0...3 for regular shifting and bit 7 which indicates resulting element goes to 0, maps well with vrgather this way
    AS.LI(bitmask, 0b10001111);
    AS.VAND(mask_masked, mask, bitmask);
    AS.VRGATHER(tmp, dst, mask_masked);

    rec.setOperandVec(&operands[0], tmp);
}

FAST_HANDLE(PBLENDW) { // Fuzzed
    u8 imm = rec.getImmediate(&operands[2]);
    biscuit::GPR mask = rec.scratch();
    biscuit::Vec result = rec.scratchVec();
    biscuit::Vec dst = rec.getOperandVec(&operands[0]);
    biscuit::Vec src = rec.getOperandVec(&operands[1]);

    rec.setVectorState(SEW::E16, rec.maxVlen() / 16);
    AS.LI(mask, imm);
    AS.VMV(v0, mask);
    AS.VMERGE(result, dst, src);

    rec.setOperandVec(&operands[0], result);
}

FAST_HANDLE(BLENDPS) {
    u8 imm = rec.getImmediate(&operands[2]) & 0b1111;
    biscuit::Vec result = rec.scratchVec();
    biscuit::Vec dst = rec.getOperandVec(&operands[0]);
    biscuit::Vec src = rec.getOperandVec(&operands[1]);

    rec.setVectorState(SEW::E32, rec.maxVlen() / 32);
    AS.VMV(v0, imm);
    AS.VMERGE(result, dst, src);

    rec.setOperandVec(&operands[0], result);
}

FAST_HANDLE(BLENDVPS) {
    biscuit::Vec result = rec.scratchVec();
    biscuit::Vec dst = rec.getOperandVec(&operands[0]);
    biscuit::Vec src = rec.getOperandVec(&operands[1]);
    biscuit::Vec mask = rec.getRefVec(X86_REF_XMM0); // I see where VMERGE took inspiration from

    rec.setVectorState(SEW::E32, rec.maxVlen() / 32);
    AS.VMSLT(v0, mask, x0);
    AS.VMERGE(result, dst, src);

    rec.setOperandVec(&operands[0], result);
}

FAST_HANDLE(BLENDPD) {
    u8 imm = rec.getImmediate(&operands[2]) & 0b11;
    biscuit::Vec result = rec.scratchVec();
    biscuit::Vec dst = rec.getOperandVec(&operands[0]);
    biscuit::Vec src = rec.getOperandVec(&operands[1]);

    rec.setVectorState(SEW::E64, rec.maxVlen() / 64);
    AS.VMV(v0, imm);
    AS.VMERGE(result, dst, src);

    rec.setOperandVec(&operands[0], result);
}

FAST_HANDLE(BLENDVPD) {
    biscuit::Vec result = rec.scratchVec();
    biscuit::Vec dst = rec.getOperandVec(&operands[0]);
    biscuit::Vec src = rec.getOperandVec(&operands[1]);
    biscuit::Vec mask = rec.getRefVec(X86_REF_XMM0);

    rec.setVectorState(SEW::E64, rec.maxVlen() / 64);
    AS.VMSLT(v0, mask, x0);
    AS.VMERGE(result, dst, src);

    rec.setOperandVec(&operands[0], result);
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

    rec.setVectorState(SEW::E16, rec.maxVlen() / 16);
    AS.VMV(iota, 0);
    AS.VID(iota2);
    // Slide down 4 words, so then the register looks like 8 7 6 5, then we can slide up the other 4 elements
    AS.VSLIDEDOWN(iota2, iota2, 4);
    AS.LI(temp, el3);
    AS.VSLIDE1UP(iota, iota2, temp);
    AS.LI(temp, el2);
    AS.VSLIDE1UP(iota2, iota, temp);
    AS.LI(temp, el1);
    AS.VSLIDE1UP(iota, iota2, temp);
    AS.LI(temp, el0);
    AS.VSLIDE1UP(iota2, iota, temp);

    AS.VMV(result, 0);
    AS.VRGATHER(result, src, iota2);

    rec.setOperandVec(&operands[0], result);
}

FAST_HANDLE(PSHUFHW) {
    u8 imm = rec.getImmediate(&operands[2]);
    biscuit::Vec result = rec.scratchVec();
    biscuit::Vec src = rec.getOperandVec(&operands[1]);
    biscuit::GPR tmp = rec.scratch();
    biscuit::Vec iota = rec.scratchVec();
    biscuit::Vec iota2 = rec.scratchVec();

    rec.setVectorState(SEW::E16, rec.maxVlen() / 16);
    AS.VMV(result, src); // to move the low words

    u8 el0 = 4 + (imm & 0b11);
    u8 el1 = 4 + ((imm >> 2) & 0b11);
    u8 el2 = 4 + ((imm >> 4) & 0b11);
    u8 el3 = 4 + ((imm >> 6) & 0b11);
    AS.VMV(iota2, el3);
    AS.LI(tmp, el2);
    AS.VSLIDE1UP(iota, iota2, tmp);
    AS.LI(tmp, el1);
    AS.VSLIDE1UP(iota2, iota, tmp);
    AS.LI(tmp, el0);
    AS.VSLIDE1UP(iota, iota2, tmp);
    AS.VSLIDEUP(iota2, iota, 4);

    AS.LI(tmp, 0b11110000); // operate on top words only
    AS.VMV(v0, tmp);

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

    rec.setVectorState(SEW::E64, rec.maxVlen() / 64);

    if (imm > 31) {
        AS.VMV(dst, 0);
        rec.setOperandVec(&operands[0], dst);
        return;
    }

    if (16 - imm > 0) {
        AS.LI(temp, ~((1ull << (16 - imm)) - 1));
        AS.VMV_SX(v0, temp);
        rec.setVectorState(SEW::E8, rec.maxVlen() / 8);
        AS.VMV(result, 0);
        AS.VSLIDEDOWN(result, src, imm);
        AS.VAND(result, result, 0, VecMask::Yes);
        AS.VMV(slide_up, 0);
        AS.VSLIDEUP(slide_up, dst, 16 - imm);
        AS.VOR(result, result, slide_up);
    } else {
        AS.LI(temp, ~((1ull << (32 - imm)) - 1));
        AS.VMV_SX(v0, temp);
        rec.setVectorState(SEW::E8, rec.maxVlen() / 8);
        AS.VMV(result, 0);
        AS.VSLIDEDOWN(result, dst, imm - 16);
        AS.VAND(result, result, 0, VecMask::Yes);
    }

    rec.setOperandVec(&operands[0], result);
}

FAST_HANDLE(BSF) {
    ASSERT(Extensions::B);
    biscuit::GPR result = rec.scratch();
    biscuit::GPR src = rec.getOperandGPR(&operands[1]);
    biscuit::GPR dst = rec.getOperandGPR(&operands[0]);
    (void)dst; // must be loaded since conditional code follows
    biscuit::GPR zf = rec.flagW(X86_REF_ZF);

    Label end;
    AS.SEQZ(zf, src);
    AS.BEQZ(src, &end);
    AS.CTZ(result, src);
    rec.setOperandGPR(&operands[0], result);

    AS.Bind(&end);

    rec.setFlagUndefined(X86_REF_CF);
    rec.setFlagUndefined(X86_REF_OF);
    rec.setFlagUndefined(X86_REF_SF);
    rec.setFlagUndefined(X86_REF_AF);
}

FAST_HANDLE(TZCNT) {
    ASSERT(Extensions::B);
    biscuit::GPR result = rec.scratch();
    biscuit::GPR src = rec.getOperandGPR(&operands[1]);
    biscuit::GPR dst = rec.getOperandGPR(&operands[0]);
    (void)dst; // must be loaded since conditional code follows
    biscuit::GPR zf = rec.flagW(X86_REF_ZF);
    biscuit::GPR cf = rec.flagW(X86_REF_CF);

    Label end;
    AS.LI(result, instruction.operand_width);
    AS.LI(cf, 1);
    AS.BEQZ(src, &end);
    AS.LI(cf, 0);
    AS.CTZ(result, src);
    AS.J(&end);

    AS.Bind(&end);
    rec.setOperandGPR(&operands[0], result);
    AS.SEQZ(zf, result);

    rec.setFlagUndefined(X86_REF_OF);
    rec.setFlagUndefined(X86_REF_SF);
    rec.setFlagUndefined(X86_REF_AF);
}

void BITSTRING_func(Recompiler& rec, const HandlerMetadata& meta, ZydisDecodedInstruction& instruction, ZydisDecodedOperand* operands, u64 func) {
    // Special case where the memory may index past the effective address, only when offset is a register
    biscuit::GPR base = rec.lea(&operands[0]);
    biscuit::GPR bit = rec.getOperandGPR(&operands[1]);
    rec.writebackDirtyState();
    rec.sext(a1, bit, rec.zydisToSize(operands[1].size));
    AS.MV(a0, base);
    AS.LI(t0, func);
    AS.JALR(t0);

    biscuit::GPR cf = rec.flagW(X86_REF_CF);
    AS.MV(cf, a0); // Write result to cf
    rec.setFlagUndefined(X86_REF_OF);
    rec.setFlagUndefined(X86_REF_SF);
    rec.setFlagUndefined(X86_REF_AF);
}

FAST_HANDLE(BTC) {
    if (operands[0].type == ZYDIS_OPERAND_TYPE_MEMORY && operands[1].type == ZYDIS_OPERAND_TYPE_REGISTER) {
        BITSTRING_func(rec, meta, instruction, operands, (u64)&felix86_btc);
        return;
    }

    biscuit::GPR bit = rec.getOperandGPR(&operands[1]);
    biscuit::GPR dst = rec.getOperandGPR(&operands[0]);
    biscuit::GPR cf = rec.flagW(X86_REF_CF);

    biscuit::GPR shift = rec.scratch();
    biscuit::GPR mask = rec.scratch();
    biscuit::GPR result = rec.scratch();

    u8 bit_size = operands[0].size;
    AS.ANDI(shift, bit, bit_size - 1);
    AS.SRL(cf, dst, shift);
    AS.ANDI(cf, cf, 1);
    AS.LI(mask, 1);
    AS.SLL(mask, mask, shift);
    AS.XOR(result, dst, mask);

    rec.setOperandGPR(&operands[0], result);

    rec.setFlagUndefined(X86_REF_OF);
    rec.setFlagUndefined(X86_REF_SF);
    rec.setFlagUndefined(X86_REF_AF);
}

FAST_HANDLE(BT) {
    if (operands[0].type == ZYDIS_OPERAND_TYPE_MEMORY && operands[1].type == ZYDIS_OPERAND_TYPE_REGISTER) {
        BITSTRING_func(rec, meta, instruction, operands, (u64)&felix86_bt);
        return;
    }

    biscuit::GPR shift = rec.scratch();
    biscuit::GPR bit = rec.getOperandGPR(&operands[1]);
    biscuit::GPR cf = rec.flagW(X86_REF_CF);
    biscuit::GPR dst = rec.getOperandGPR(&operands[0]);

    u8 bit_size = operands[0].size;
    AS.ANDI(shift, bit, bit_size - 1);

    AS.SRL(cf, dst, shift);
    AS.ANDI(cf, cf, 1);

    rec.setFlagUndefined(X86_REF_OF);
    rec.setFlagUndefined(X86_REF_SF);
    rec.setFlagUndefined(X86_REF_AF);
}

FAST_HANDLE(BTS) {
    if (operands[0].type == ZYDIS_OPERAND_TYPE_MEMORY && operands[1].type == ZYDIS_OPERAND_TYPE_REGISTER) {
        BITSTRING_func(rec, meta, instruction, operands, (u64)&felix86_bts);
        return;
    }

    biscuit::GPR result = rec.scratch();
    biscuit::GPR bit = rec.getOperandGPR(&operands[1]);
    biscuit::GPR dst = rec.getOperandGPR(&operands[0]);
    biscuit::GPR shift = rec.scratch();

    u8 bit_size = operands[0].size;
    AS.ANDI(shift, bit, bit_size - 1);
    if (rec.shouldEmitFlag(meta.rip, X86_REF_CF)) {
        biscuit::GPR cf = rec.flagW(X86_REF_CF);
        AS.SRL(cf, dst, shift);
        AS.ANDI(cf, cf, 1);
    }

    biscuit::GPR one = rec.scratch();
    AS.LI(one, 1);
    AS.SLL(one, one, shift);
    AS.OR(result, dst, one);

    rec.setOperandGPR(&operands[0], result);

    rec.setFlagUndefined(X86_REF_OF);
    rec.setFlagUndefined(X86_REF_SF);
    rec.setFlagUndefined(X86_REF_AF);
}

FAST_HANDLE(BTR) {
    if (operands[0].type == ZYDIS_OPERAND_TYPE_MEMORY && operands[1].type == ZYDIS_OPERAND_TYPE_REGISTER) {
        BITSTRING_func(rec, meta, instruction, operands, (u64)&felix86_btr);
        return;
    }

    biscuit::GPR result = rec.scratch();
    biscuit::GPR bit = rec.getOperandGPR(&operands[1]);
    biscuit::GPR dst = rec.getOperandGPR(&operands[0]);
    biscuit::GPR cf = rec.flagW(X86_REF_CF);
    biscuit::GPR shift = rec.scratch();

    u8 bit_size = operands[0].size;
    AS.ANDI(shift, bit, bit_size - 1);
    AS.SRL(cf, dst, shift);
    AS.ANDI(cf, cf, 1);
    biscuit::GPR one = rec.scratch();
    AS.LI(one, 1);
    AS.SLL(one, one, shift);
    AS.NOT(one, one);
    AS.AND(result, dst, one);

    rec.setOperandGPR(&operands[0], result);

    rec.setFlagUndefined(X86_REF_OF);
    rec.setFlagUndefined(X86_REF_SF);
    rec.setFlagUndefined(X86_REF_AF);
}

FAST_HANDLE(BLSR) {
    WARN("BLSR is broken, check BLSR_flags");
    biscuit::GPR src = rec.getOperandGPR(&operands[1]);
    biscuit::GPR result = rec.scratch();

    AS.ADDI(result, src, -1);
    AS.AND(result, src, result);

    if (rec.shouldEmitFlag(meta.rip, X86_REF_CF)) {
        biscuit::GPR cf = rec.flagW(X86_REF_CF);
        AS.SEQZ(cf, src);
    }

    if (rec.shouldEmitFlag(meta.rip, X86_REF_ZF)) {
        rec.updateZero(result, rec.zydisToSize(operands[0].size));
    }

    if (rec.shouldEmitFlag(meta.rip, X86_REF_SF)) {
        rec.updateSign(result, rec.zydisToSize(operands[0].size));
    }

    rec.setOperandGPR(&operands[0], result);
}

FAST_HANDLE(BSR) {
    ASSERT(Extensions::B);
    biscuit::GPR result = rec.scratch();
    biscuit::GPR src = rec.getOperandGPR(&operands[1]);
    biscuit::GPR dst = rec.getOperandGPR(&operands[0]);
    (void)dst; // must be loaded since conditional code follows
    biscuit::GPR zf = rec.flagW(X86_REF_ZF);

    Label end;
    AS.SEQZ(zf, src);
    AS.BEQZ(src, &end);
    if (instruction.operand_width == 64) {
        AS.CLZ(result, src);
        AS.XORI(result, result, 63);
    } else if (instruction.operand_width == 32) {
        AS.CLZW(result, src);
        AS.XORI(result, result, 31);
    } else if (instruction.operand_width == 16) {
        AS.SLLI(result, src, 16);
        AS.CLZW(result, result);
        AS.XORI(result, result, 15);
    } else {
        UNREACHABLE();
    }
    rec.setOperandGPR(&operands[0], result);

    AS.Bind(&end);

    rec.setFlagUndefined(X86_REF_CF);
    rec.setFlagUndefined(X86_REF_OF);
    rec.setFlagUndefined(X86_REF_SF);
    rec.setFlagUndefined(X86_REF_AF);
}

FAST_HANDLE(BSWAP) {
    ASSERT(Extensions::B);
    x86_size_e size = rec.getOperandSize(&operands[0]);
    biscuit::GPR dst = rec.getOperandGPR(&operands[0]);
    biscuit::GPR result = rec.scratch();

    if (size == X86_SIZE_DWORD) {
        AS.REV8(result, dst);
        AS.SRLI(result, result, 32);
    } else if (size == X86_SIZE_QWORD) {
        AS.REV8(result, dst);
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

        rec.setVectorState(SEW::E64, rec.maxVlen() / 64);
        AS.VMV(v0, 0b10);
        AS.VMERGE(dst, src, dst);

        rec.setOperandVec(&operands[0], dst);
    } else if (operands[0].type == ZYDIS_OPERAND_TYPE_MEMORY) {
        biscuit::Vec src = rec.getOperandVec(&operands[1]);
        rec.setOperandVec(&operands[0], src);
    } else {
        UNREACHABLE();
    }
}

FAST_HANDLE(MOVLPD) {
    fast_MOVLPS(rec, meta, instruction, operands);
}

FAST_HANDLE(MOVHLPS) {
    ASSERT(operands[0].type == ZYDIS_OPERAND_TYPE_REGISTER);
    biscuit::Vec temp = rec.scratchVec();
    biscuit::Vec dst = rec.getOperandVec(&operands[0]);
    biscuit::Vec src = rec.getOperandVec(&operands[1]);

    rec.setVectorState(SEW::E64, rec.maxVlen() / 64);
    AS.VSLIDEDOWN(temp, src, 1);
    AS.VMV(v0, 0b10);
    AS.VMERGE(dst, temp, dst);
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
    AS.ANDI(count, src, rec.getBitSize(size) == 64 ? 63 : 31);
    AS.BEQZ(count, &zero_count);

    biscuit::GPR temp = rec.scratch();
    biscuit::GPR neg_count = rec.scratch();
    AS.NEG(neg_count, count);
    AS.ANDI(neg_count, neg_count, rec.getBitSize(size) - 1);
    AS.SLL(temp, dst, count);
    AS.SRL(neg_count, dst, neg_count);
    AS.OR(dst, temp, neg_count);
    AS.ANDI(cf, dst, 1);
    AS.SRLI(of, dst, rec.getBitSize(size) - 1);
    AS.XOR(of, of, cf);

    rec.setOperandGPR(&operands[0], dst);

    AS.Bind(&zero_count);
}

FAST_HANDLE(ROR) {
    x86_size_e size = rec.getOperandSize(&operands[0]);
    biscuit::GPR dst = rec.getOperandGPR(&operands[0]);
    biscuit::GPR src = rec.getOperandGPR(&operands[1]);
    biscuit::GPR count = rec.scratch();

    Label zero_count;

    biscuit::GPR cf = rec.flagWR(X86_REF_CF);
    biscuit::GPR of = rec.flagWR(X86_REF_OF);
    AS.ANDI(count, src, rec.getBitSize(size) == 64 ? 63 : 31);
    AS.BEQZ(count, &zero_count);

    biscuit::GPR temp = rec.scratch();
    biscuit::GPR neg_count = rec.scratch();
    AS.NEG(neg_count, count);
    AS.ANDI(neg_count, neg_count, rec.getBitSize(size) - 1);
    AS.SRL(temp, dst, count);
    AS.SLL(neg_count, dst, neg_count);
    AS.OR(dst, temp, neg_count);
    AS.SRLI(cf, dst, rec.getBitSize(size) - 1);
    AS.ANDI(cf, cf, 1);
    AS.SRLI(of, dst, rec.getBitSize(size) - 2);
    AS.ANDI(of, of, 1);
    AS.XOR(of, of, cf);

    rec.setOperandGPR(&operands[0], dst);

    AS.Bind(&zero_count);
}

FAST_HANDLE(PSLLDQ) {
    u8 imm = rec.getImmediate(&operands[1]);
    biscuit::Vec dst = rec.getOperandVec(&operands[0]);
    biscuit::Vec temp = rec.scratchVec();
    rec.setVectorState(SEW::E8, rec.maxVlen() / 8);
    if (imm > 15) {
        AS.VMV(temp, 0);
    } else {
        AS.VMV(temp, 0);
        AS.VSLIDEUP(temp, dst, imm);
    }
    rec.setOperandVec(&operands[0], temp);
}

FAST_HANDLE(PSRLDQ) {
    u8 imm = rec.getImmediate(&operands[1]);
    biscuit::Vec dst = rec.getOperandVec(&operands[0]);
    biscuit::Vec temp = rec.scratchVec();
    if (imm > 15) {
        rec.setVectorState(SEW::E64, rec.maxVlen() / 64);
        AS.VMV(temp, 0);
    } else {
        rec.setVectorState(SEW::E64, rec.maxVlen() / 64);
        biscuit::GPR mask = rec.scratch();
        AS.LI(mask, ~((1ull << (16 - imm)) - 1));
        AS.VMV_SX(v0, mask);
        rec.setVectorState(SEW::E8, rec.maxVlen() / 8);
        AS.VSLIDEDOWN(temp, dst, imm);
        AS.VAND(temp, temp, 0, VecMask::Yes);
    }
    rec.setOperandVec(&operands[0], temp);
}

FAST_HANDLE(PSLLW) {
    biscuit::GPR shift = rec.scratch();
    biscuit::Vec dst = rec.getOperandVec(&operands[0]);
    if (operands[1].type == ZYDIS_OPERAND_TYPE_IMMEDIATE) {
        u8 val = rec.getImmediate(&operands[1]);
        AS.LI(shift, val);
    } else {
        biscuit::Vec src = rec.getOperandVec(&operands[1]);
        rec.setVectorState(SEW::E64, rec.maxVlen() / 64);
        AS.VMV_XS(shift, src);
    }
    rec.setVectorState(SEW::E16, rec.maxVlen() / 16);
    biscuit::GPR max = rec.scratch();
    biscuit::Label dont_zero;
    AS.LI(max, 16);
    AS.BLTU(shift, max, &dont_zero);
    AS.VMV(dst, 0);
    AS.Bind(&dont_zero);
    AS.VSLL(dst, dst, shift);
    rec.setOperandVec(&operands[0], dst);
}

FAST_HANDLE(PSLLQ) {
    biscuit::GPR shift = rec.scratch();
    if (operands[1].type == ZYDIS_OPERAND_TYPE_IMMEDIATE) {
        u8 val = rec.getImmediate(&operands[1]);
        AS.LI(shift, val);
    } else {
        biscuit::Vec src = rec.getOperandVec(&operands[1]);
        rec.setVectorState(SEW::E64, rec.maxVlen() / 64);
        AS.VMV_XS(shift, src);
    }
    biscuit::Vec dst = rec.getOperandVec(&operands[0]);
    biscuit::GPR max = rec.scratch();
    biscuit::Label dont_zero;
    rec.setVectorState(SEW::E64, rec.maxVlen() / 64);
    AS.LI(max, 64);
    AS.BLTU(shift, max, &dont_zero);
    AS.VMV(dst, 0);
    AS.Bind(&dont_zero);
    AS.VSLL(dst, dst, shift);
    rec.setOperandVec(&operands[0], dst);
}

FAST_HANDLE(PSLLD) { // Fuzzed
    biscuit::GPR shift = rec.scratch();
    biscuit::Vec dst = rec.getOperandVec(&operands[0]);
    if (operands[1].type == ZYDIS_OPERAND_TYPE_IMMEDIATE) {
        u8 val = rec.getImmediate(&operands[1]);
        AS.LI(shift, val);
    } else {
        biscuit::Vec src = rec.getOperandVec(&operands[1]);
        rec.setVectorState(SEW::E64, rec.maxVlen() / 64);
        AS.VMV_XS(shift, src);
    }
    rec.setVectorState(SEW::E32, rec.maxVlen() / 32);
    biscuit::GPR max = rec.scratch();
    biscuit::Label dont_zero;
    AS.LI(max, 32);
    AS.BLTU(shift, max, &dont_zero);
    AS.VMV(dst, 0);
    AS.Bind(&dont_zero);
    AS.VSLL(dst, dst, shift);
    rec.setOperandVec(&operands[0], dst);
}

FAST_HANDLE(PSRLD) {
    biscuit::GPR shift = rec.scratch();
    biscuit::Vec dst = rec.getOperandVec(&operands[0]);
    if (operands[1].type == ZYDIS_OPERAND_TYPE_IMMEDIATE) {
        u8 val = rec.getImmediate(&operands[1]);
        AS.LI(shift, val);
    } else {
        biscuit::Vec src = rec.getOperandVec(&operands[1]);
        rec.setVectorState(SEW::E64, rec.maxVlen() / 64);
        AS.VMV_XS(shift, src);
    }
    rec.setVectorState(SEW::E32, rec.maxVlen() / 32);
    biscuit::GPR max = rec.scratch();
    biscuit::Label dont_zero;
    AS.LI(max, 32);
    AS.BLTU(shift, max, &dont_zero);
    AS.VMV(dst, 0);
    AS.Bind(&dont_zero);
    AS.VSRL(dst, dst, shift);
    rec.setOperandVec(&operands[0], dst);
}

FAST_HANDLE(PSRLW) {
    biscuit::GPR shift = rec.scratch();
    biscuit::Vec dst = rec.getOperandVec(&operands[0]);
    if (operands[1].type == ZYDIS_OPERAND_TYPE_IMMEDIATE) {
        u8 val = rec.getImmediate(&operands[1]);
        AS.LI(shift, val);
    } else {
        biscuit::Vec src = rec.getOperandVec(&operands[1]);
        rec.setVectorState(SEW::E64, rec.maxVlen() / 64);
        AS.VMV_XS(shift, src);
    }
    rec.setVectorState(SEW::E16, rec.maxVlen() / 16);
    biscuit::GPR max = rec.scratch();
    biscuit::Label dont_zero;
    AS.LI(max, 16);
    AS.BLTU(shift, max, &dont_zero);
    AS.VMV(dst, 0);
    AS.Bind(&dont_zero);
    AS.VSRL(dst, dst, shift);
    rec.setOperandVec(&operands[0], dst);
}

FAST_HANDLE(PSRLQ) {
    biscuit::GPR shift = rec.scratch();
    if (operands[1].type == ZYDIS_OPERAND_TYPE_IMMEDIATE) {
        u8 val = rec.getImmediate(&operands[1]);
        AS.LI(shift, val);
    } else {
        biscuit::Vec src = rec.getOperandVec(&operands[1]);
        rec.setVectorState(SEW::E64, rec.maxVlen() / 64);
        AS.VMV_XS(shift, src);
    }
    biscuit::Vec dst = rec.getOperandVec(&operands[0]);
    rec.setVectorState(SEW::E64, rec.maxVlen() / 64);
    biscuit::GPR max = rec.scratch();
    biscuit::Label dont_zero;
    AS.LI(max, 64);
    AS.BLTU(shift, max, &dont_zero);
    AS.VMV(dst, 0);
    AS.Bind(&dont_zero);
    AS.VSRL(dst, dst, shift);
    rec.setOperandVec(&operands[0], dst);
}

FAST_HANDLE(PSRAW) {
    u8 shift = rec.getImmediate(&operands[1]);
    biscuit::Vec dst = rec.getOperandVec(&operands[0]);
    rec.setVectorState(SEW::E16, rec.maxVlen() / 16);
    if (shift > 15)
        shift = 15;
    AS.VSRA(dst, dst, shift);
    rec.setOperandVec(&operands[0], dst);
}

FAST_HANDLE(PSRAD) {
    u8 shift = rec.getImmediate(&operands[1]);
    biscuit::Vec dst = rec.getOperandVec(&operands[0]);
    rec.setVectorState(SEW::E32, rec.maxVlen() / 32);
    if (shift > 31)
        shift = 31;
    AS.VSRA(dst, dst, shift);
    rec.setOperandVec(&operands[0], dst);
}

FAST_HANDLE(SFENCE) {
    AS.FENCETSO(); // just make a full fence for now, TODO: we can optimize this some day
}

FAST_HANDLE(LFENCE) {
    AS.FENCETSO(); // just make a full fence for now, TODO: we can optimize this some day
}

FAST_HANDLE(MFENCE) {
    AS.FENCETSO(); // just make a full fence for now, TODO: we can optimize this some day
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

void COMIS(Recompiler& rec, const HandlerMetadata& meta, ZydisDecodedInstruction& instruction, ZydisDecodedOperand* operands, SEW sew) {
    biscuit::GPR nan_1 = rec.scratch();
    biscuit::GPR nan_2 = rec.scratch();
    biscuit::Vec temp = rec.scratchVec();
    biscuit::Vec temp2 = rec.scratchVec();
    biscuit::Vec src = rec.getOperandVec(&operands[1]);
    biscuit::Vec dst = rec.getOperandVec(&operands[0]);

    biscuit::GPR cf = rec.flagW(X86_REF_CF);
    biscuit::GPR zf = rec.flagW(X86_REF_ZF);
    biscuit::GPR af = rec.flagW(X86_REF_AF);
    biscuit::GPR sf = rec.flagW(X86_REF_SF);
    biscuit::GPR of = rec.flagW(X86_REF_OF);

    AS.LI(of, 0);
    AS.LI(af, 0);
    AS.LI(sf, 0);

    Label end, nan, equal, less_than;

    rec.setVectorState(sew, 1);

    AS.LI(nan_1, 0);
    AS.LI(nan_2, 0);

    AS.VMFNE(temp, dst, dst);
    AS.VMV_XS(nan_1, temp);

    AS.VMFNE(temp2, src, src);
    AS.VMV_XS(nan_2, temp2);
    AS.OR(nan_1, nan_1, nan_2);
    AS.ANDI(nan_1, nan_1, 1);

    AS.BNEZ(nan_1, &nan);

    // Check for equality
    AS.VMFEQ(temp, dst, src);
    AS.VMV_XS(nan_1, temp);
    AS.ANDI(nan_1, nan_1, 1);

    AS.BNEZ(nan_1, &equal);

    // Check for less than
    AS.VMFLT(temp, dst, src);
    AS.VMV_XS(nan_1, temp);
    AS.ANDI(nan_1, nan_1, 1);

    AS.BNEZ(nan_1, &less_than);

    // Greater than
    // ZF: 0, PF: 0, CF: 0
    AS.LI(zf, 0);
    AS.LI(cf, 0);
    AS.SB(x0, offsetof(ThreadState, pf), rec.threadStatePointer());

    AS.J(&end);

    AS.Bind(&less_than);

    // Less than
    // ZF: 0, PF: 0, CF: 1
    AS.LI(zf, 0);
    AS.LI(cf, 1);
    AS.SB(x0, offsetof(ThreadState, pf), rec.threadStatePointer());

    AS.J(&end);

    AS.Bind(&equal);

    // Equal
    // ZF: 1, PF: 0, CF: 0
    AS.LI(zf, 1);
    AS.LI(cf, 0);
    AS.SB(x0, offsetof(ThreadState, pf), rec.threadStatePointer());

    AS.J(&end);

    AS.Bind(&nan);

    // Unordered
    // ZF: 1, PF: 1, CF: 1
    AS.LI(zf, 1);
    AS.LI(cf, 1);
    AS.SB(cf, offsetof(ThreadState, pf), rec.threadStatePointer());

    AS.Bind(&end);
}

FAST_HANDLE(COMISD) { // Fuzzed
    COMIS(rec, meta, instruction, operands, SEW::E64);
}

FAST_HANDLE(UCOMISD) {
    COMIS(rec, meta, instruction, operands, SEW::E64);
}

FAST_HANDLE(COMISS) {
    COMIS(rec, meta, instruction, operands, SEW::E32);
}

FAST_HANDLE(UCOMISS) {
    COMIS(rec, meta, instruction, operands, SEW::E32);
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
    AS.LI(mask, (1 << imm));
    AS.VMV(v0, mask);

    rec.setVectorState(SEW::E8, rec.maxVlen() / 8);
    AS.VMV_SX(tmp, src);
    AS.VSLIDEUP(tmp2, tmp, imm);
    AS.VMERGE(result, dst, tmp2);

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

    rec.setVectorState(SEW::E16, rec.maxVlen() / 16);
    AS.LI(mask, (1 << imm));
    AS.VMV(v0, mask);
    AS.VMV_SX(tmp, src);
    AS.VSLIDEUP(tmp2, tmp, imm);
    AS.VMERGE(result, dst, tmp2);

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

    rec.setVectorState(SEW::E32, rec.maxVlen() / 32);
    AS.LI(mask, (1 << imm));
    AS.VMV(v0, mask);
    AS.VMV_SX(tmp, src);
    AS.VSLIDEUP(tmp2, tmp, imm);
    AS.VMERGE(result, dst, tmp2);

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

    rec.setVectorState(SEW::E64, rec.maxVlen() / 64);
    AS.LI(mask, (1 << imm));
    AS.VMV(v0, mask);
    AS.VMV_SX(tmp, src);
    AS.VSLIDEUP(tmp2, tmp, imm);
    AS.VMERGE(result, dst, tmp2);

    rec.setOperandVec(&operands[0], result);
}

FAST_HANDLE(PEXTRB) {
    biscuit::Vec temp = rec.scratchVec();
    biscuit::GPR result = rec.scratch();
    biscuit::Vec src = rec.getOperandVec(&operands[1]);
    u8 imm = rec.getImmediate(&operands[2]) & 0b1111;

    rec.setVectorState(SEW::E8, rec.maxVlen() / 8);
    AS.VSLIDEDOWN(temp, src, imm);
    AS.VMV_XS(result, temp);
    rec.zext(result, result, X86_SIZE_BYTE);

    rec.setOperandGPR(&operands[0], result);
}

FAST_HANDLE(PEXTRW) {
    biscuit::Vec temp = rec.scratchVec();
    biscuit::GPR result = rec.scratch();
    biscuit::Vec src = rec.getOperandVec(&operands[1]);
    u8 imm = rec.getImmediate(&operands[2]) & 0b111;

    rec.setVectorState(SEW::E16, rec.maxVlen() / 16);
    AS.VSLIDEDOWN(temp, src, imm);
    AS.VMV_XS(result, temp);
    rec.zext(result, result, X86_SIZE_WORD);

    rec.setOperandGPR(&operands[0], result);
}

FAST_HANDLE(PEXTRD) {
    biscuit::Vec temp = rec.scratchVec();
    biscuit::Vec src = rec.getOperandVec(&operands[1]);
    biscuit::GPR result = rec.scratch();
    u8 imm = rec.getImmediate(&operands[2]) & 0b11;

    rec.setVectorState(SEW::E32, rec.maxVlen() / 32);
    AS.VSLIDEDOWN(temp, src, imm);
    AS.VMV_XS(result, temp);

    rec.setOperandGPR(&operands[0], result);
}

FAST_HANDLE(PEXTRQ) {
    biscuit::Vec temp = rec.scratchVec();
    biscuit::Vec src = rec.getOperandVec(&operands[1]);
    biscuit::GPR result = rec.scratch();
    u8 imm = rec.getImmediate(&operands[2]) & 0b1;

    rec.setVectorState(SEW::E64, rec.maxVlen() / 64);
    AS.VSLIDEDOWN(temp, src, imm);
    AS.VMV_XS(result, temp);

    rec.setOperandGPR(&operands[0], result);
}

FAST_HANDLE(CMPXCHG_lock) {
    ASSERT(operands[0].size != 8 && operands[0].size != 16);

    x86_size_e size = rec.zydisToSize(instruction.operand_width);
    biscuit::GPR address = rec.lea(&operands[0]);
    biscuit::GPR src = rec.getOperandGPR(&operands[1]);
    biscuit::GPR rax = rec.getRefGPR(X86_REF_RAX, size);
    biscuit::GPR zf = rec.flagW(X86_REF_ZF);
    biscuit::GPR cf = rec.flagWR(X86_REF_CF);
    biscuit::GPR of = rec.flagWR(X86_REF_OF);
    rec.flagWR(X86_REF_SF);
    biscuit::GPR af = rec.flagWR(X86_REF_AF);
    biscuit::GPR result = rec.scratch();
    biscuit::GPR dst = rec.scratch();

    switch (size) {
    case X86_SIZE_DWORD: {
        biscuit::Label not_equal;
        biscuit::Label start;
        biscuit::GPR scratch = rec.scratch();
        AS.Bind(&start);
        AS.LR_W(Ordering::AQRL, dst, address);
        AS.ZEXTW(dst, dst); // LR sign extends
        AS.BNE(dst, rax, &not_equal);
        AS.SC_W(Ordering::AQRL, scratch, src, address);
        AS.BNEZ(scratch, &start);
        AS.Bind(&not_equal);
        rec.popScratch();
        break;
    }
    case X86_SIZE_QWORD: {
        biscuit::Label not_equal;
        biscuit::Label start;
        biscuit::GPR scratch = rec.scratch();
        AS.Bind(&start);
        AS.LR_D(Ordering::AQRL, dst, address);
        AS.BNE(dst, rax, &not_equal);
        AS.SC_D(Ordering::AQRL, scratch, src, address);
        AS.BNEZ(scratch, &start);
        AS.Bind(&not_equal);
        rec.popScratch();
        break;
    }
    default: {
        UNREACHABLE();
        break;
    }
    }

    AS.SUB(result, rax, dst);

    if (rec.shouldEmitFlag(meta.rip, X86_REF_CF)) {
        AS.SLTU(cf, rax, dst);
    }

    if (rec.shouldEmitFlag(meta.rip, X86_REF_PF)) {
        rec.updateParity(result);
    }

    if (rec.shouldEmitFlag(meta.rip, X86_REF_AF)) {
        biscuit::GPR scratch = rec.scratch();
        AS.ANDI(af, rax, 0xF);
        AS.ANDI(scratch, dst, 0xF);
        AS.SLTU(af, af, scratch);
        rec.popScratch();
    }

    if (rec.shouldEmitFlag(meta.rip, X86_REF_SF)) {
        rec.updateSign(result, size);
    }

    if (rec.shouldEmitFlag(meta.rip, X86_REF_OF)) {
        is_overflow_sub(rec, of, rax, dst, result, rec.getBitSize(size));
    }

    biscuit::Label end, equal;
    AS.BEQ(dst, rax, &equal);

    // Not equal
    AS.LI(zf, 0);
    rec.setRefGPR(X86_REF_RAX, size, dst);
    AS.J(&end);

    AS.Bind(&equal);
    AS.LI(zf, 1);
    AS.Bind(&end);
}

FAST_HANDLE(CMPXCHG) {
    if (operands[0].type == ZYDIS_OPERAND_TYPE_MEMORY) {
        if (operands[0].size == 8 || operands[0].size == 16) {
            WARN("Atomic CMPXCHG with 8 or 16 bit operands encountered");
        } else {
            return fast_CMPXCHG_lock(rec, meta, instruction, operands);
        }
    }

    x86_size_e size = rec.zydisToSize(instruction.operand_width);
    biscuit::GPR dst = rec.getOperandGPR(&operands[0]);
    biscuit::GPR src = rec.getOperandGPR(&operands[1]);
    biscuit::GPR rax = rec.getRefGPR(X86_REF_RAX, size);
    biscuit::GPR zf = rec.flagW(X86_REF_ZF);
    biscuit::GPR cf = rec.flagWR(X86_REF_CF);
    biscuit::GPR of = rec.flagWR(X86_REF_OF);
    rec.flagWR(X86_REF_SF);
    biscuit::GPR af = rec.flagWR(X86_REF_AF);

    Label end, equal;

    biscuit::GPR result = rec.scratch();

    AS.SUB(result, rax, dst);

    if (rec.shouldEmitFlag(meta.rip, X86_REF_CF)) {
        AS.SLTU(cf, rax, dst);
    }

    if (rec.shouldEmitFlag(meta.rip, X86_REF_PF)) {
        rec.updateParity(result);
    }

    if (rec.shouldEmitFlag(meta.rip, X86_REF_AF)) {
        biscuit::GPR scratch = rec.scratch();
        AS.ANDI(af, rax, 0xF);
        AS.ANDI(scratch, dst, 0xF);
        AS.SLTU(af, af, scratch);
        rec.popScratch();
    }

    if (rec.shouldEmitFlag(meta.rip, X86_REF_SF)) {
        rec.updateSign(result, size);
    }

    if (rec.shouldEmitFlag(meta.rip, X86_REF_OF)) {
        is_overflow_sub(rec, of, rax, dst, result, rec.getBitSize(size));
    }

    AS.BEQ(dst, rax, &equal);

    // Not equal
    AS.LI(zf, 0);
    rec.setRefGPR(X86_REF_RAX, size, dst);
    AS.J(&end);

    AS.Bind(&equal);
    AS.LI(zf, 1);
    rec.setOperandGPR(&operands[0], src);

    AS.Bind(&end);
}

void SCALAR(Recompiler& rec, const HandlerMetadata& meta, ZydisDecodedInstruction& instruction, ZydisDecodedOperand* operands, SEW sew, u8 vlen,
            void (Assembler::*func)(Vec, Vec, Vec, VecMask)) {
    biscuit::Vec temp = rec.scratchVec();
    biscuit::Vec dst = rec.getOperandVec(&operands[0]);
    biscuit::Vec src = rec.getOperandVec(&operands[1]);
    rec.setVectorState(sew, vlen);
    (AS.*func)(temp, dst, src, VecMask::No);

    if (sew == SEW::E32) {
        rec.setVectorState(SEW::E32, rec.maxVlen() / 32);
    } else if (sew == SEW::E64) {
        rec.setVectorState(SEW::E64, rec.maxVlen() / 64);
    } else {
        UNREACHABLE();
    }

    biscuit::Vec result = rec.scratchVec();
    AS.VMV(v0, 1);
    AS.VMERGE(result, dst, temp);

    rec.setOperandVec(&operands[0], result);
}

void SCALAR(Recompiler& rec, const HandlerMetadata& meta, ZydisDecodedInstruction& instruction, ZydisDecodedOperand* operands, SEW sew, u8 vlen,
            void (Assembler::*func)(Vec, Vec, VecMask)) {
    biscuit::Vec temp = rec.scratchVec();
    biscuit::Vec dst = rec.getOperandVec(&operands[0]);
    biscuit::Vec src = rec.getOperandVec(&operands[1]);
    rec.setVectorState(sew, vlen);
    (AS.*func)(temp, src, VecMask::No);

    if (sew == SEW::E32) {
        rec.setVectorState(SEW::E32, rec.maxVlen() / 32);
    } else if (sew == SEW::E64) {
        rec.setVectorState(SEW::E64, rec.maxVlen() / 64);
    } else {
        UNREACHABLE();
    }

    biscuit::Vec result = rec.scratchVec();
    AS.VMV(v0, 1);
    AS.VMERGE(result, dst, temp);

    rec.setOperandVec(&operands[0], result);
}

FAST_HANDLE(DIVSS) {
    SCALAR(rec, meta, instruction, operands, SEW::E32, 1, &Assembler::VFDIV);
}

FAST_HANDLE(DIVSD) {
    SCALAR(rec, meta, instruction, operands, SEW::E64, 1, &Assembler::VFDIV);
}

FAST_HANDLE(ADDSS) {
    SCALAR(rec, meta, instruction, operands, SEW::E32, 1, &Assembler::VFADD);
}

FAST_HANDLE(ADDSD) { // Fuzzed
    SCALAR(rec, meta, instruction, operands, SEW::E64, 1, &Assembler::VFADD);
}

FAST_HANDLE(SUBSS) {
    SCALAR(rec, meta, instruction, operands, SEW::E32, 1, &Assembler::VFSUB);
}

FAST_HANDLE(SUBSD) {
    SCALAR(rec, meta, instruction, operands, SEW::E64, 1, &Assembler::VFSUB);
}

FAST_HANDLE(MULSS) {
    SCALAR(rec, meta, instruction, operands, SEW::E32, 1, &Assembler::VFMUL);
}

FAST_HANDLE(MULSD) {
    SCALAR(rec, meta, instruction, operands, SEW::E64, 1, &Assembler::VFMUL);
}

FAST_HANDLE(MINSS) { // TODO: NaN handling
    SCALAR(rec, meta, instruction, operands, SEW::E32, 1, &Assembler::VFMIN);
}

FAST_HANDLE(MINSD) {
    SCALAR(rec, meta, instruction, operands, SEW::E64, 1, &Assembler::VFMIN);
}

FAST_HANDLE(MAXSS) {
    SCALAR(rec, meta, instruction, operands, SEW::E32, 1, &Assembler::VFMAX);
}

FAST_HANDLE(MAXSD) {
    SCALAR(rec, meta, instruction, operands, SEW::E64, 1, &Assembler::VFMAX);
}

FAST_HANDLE(CVTSI2SD) {
    x86_size_e gpr_size = rec.getOperandSize(&operands[1]);
    biscuit::Vec dst = rec.getOperandVec(&operands[0]);
    biscuit::GPR src = rec.getOperandGPR(&operands[1]);

    if (gpr_size == X86_SIZE_DWORD) {
        AS.FCVT_D_W(ft8, src);
        rec.setVectorState(SEW::E64, 1);
        AS.VFMV_SF(dst, ft8);
    } else if (gpr_size == X86_SIZE_QWORD) {
        AS.FCVT_D_L(ft8, src);
        rec.setVectorState(SEW::E64, 1);
        AS.VFMV_SF(dst, ft8);
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
        AS.FCVT_S_W(ft8, src);
        rec.setVectorState(SEW::E32, 1);
        AS.VFMV_SF(dst, ft8);
    } else if (gpr_size == X86_SIZE_QWORD) {
        AS.FCVT_S_L(ft8, src);
        rec.setVectorState(SEW::E32, 1);
        AS.VFMV_SF(dst, ft8);
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
        AS.VFMV_FS(ft8, src);
        AS.FCVT_W_S(dst, ft8, RMode::RTZ);
    } else if (gpr_size == X86_SIZE_QWORD) {
        rec.setVectorState(SEW::E32, 1);
        AS.VFMV_FS(ft8, src);
        AS.FCVT_L_S(dst, ft8, RMode::RTZ);
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
        AS.VFMV_FS(ft8, src);
        AS.FCVT_W_D(dst, ft8, RMode::RTZ);
    } else if (gpr_size == X86_SIZE_QWORD) {
        rec.setVectorState(SEW::E64, 1);
        AS.VFMV_FS(ft8, src);
        AS.FCVT_L_D(dst, ft8, RMode::RTZ);
    } else {
        UNREACHABLE();
    }

    rec.setOperandGPR(&operands[0], dst);
}

FAST_HANDLE(CVTPD2PS) {
    biscuit::Vec result = rec.scratchVec();
    biscuit::Vec src = rec.getOperandVec(&operands[1]);

    rec.setVectorState(SEW::E32, rec.maxVlen() / 32, LMUL::MF2);
    AS.VFNCVT_F_F(result, src);

    AS.VMV(v0, 0b1100);
    AS.VAND(result, result, 0, VecMask::Yes);

    rec.setOperandVec(&operands[0], result);
}

FAST_HANDLE(CVTPS2PD) { // Fuzzed, inaccuracies with NaNs
    biscuit::Vec result = rec.scratchVec();
    biscuit::Vec src = rec.getOperandVec(&operands[1]);

    rec.setVectorState(SEW::E32, rec.maxVlen() / 32, LMUL::MF2);
    AS.VFWCVT_F_F(result, src);

    rec.setOperandVec(&operands[0], result);
}

FAST_HANDLE(CVTTPS2DQ) { // Fuzzed, returns 0x7FFF'FFFF instead of 0x8000'0000
    biscuit::Vec result = rec.scratchVec();
    biscuit::Vec src = rec.getOperandVec(&operands[1]);

    rec.setVectorState(SEW::E32, rec.maxVlen() / 32);
    AS.VFCVT_RTZ_X_F(result, src);

    rec.setOperandVec(&operands[0], result);
}

FAST_HANDLE(CVTPS2DQ) {
    biscuit::Vec result = rec.scratchVec();
    biscuit::Vec src = rec.getOperandVec(&operands[1]);

    rec.setVectorState(SEW::E32, rec.maxVlen() / 32);
    AS.VFCVT_X_F(result, src);

    rec.setOperandVec(&operands[0], result);
}

FAST_HANDLE(CVTTPD2DQ) { // Fuzzed, same problem as cvttps2dq
    biscuit::Vec result = rec.scratchVec();
    biscuit::Vec src = rec.getOperandVec(&operands[1]);

    rec.setVectorState(SEW::E32, rec.maxVlen() / 32, LMUL::MF2);
    AS.VFNCVT_RTZ_X_F(result, src);

    AS.VMV(v0, 0b1100);
    AS.VAND(result, result, 0, VecMask::Yes);

    rec.setOperandVec(&operands[0], result);
}

FAST_HANDLE(CVTPD2DQ) {
    biscuit::Vec result = rec.scratchVec();
    biscuit::Vec src = rec.getOperandVec(&operands[1]);

    rec.setVectorState(SEW::E32, rec.maxVlen() / 32, LMUL::MF2);
    AS.VFNCVT_X_F(result, src);

    AS.VMV(v0, 0b1100);
    AS.VAND(result, result, 0, VecMask::Yes);

    rec.setOperandVec(&operands[0], result);
}

FAST_HANDLE(XGETBV) {
    biscuit::GPR scratch = rec.scratch();
    AS.LI(scratch, 0b11);
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
        biscuit::Vec result = rec.scratchVec();
        rec.setVectorState(SEW::E32, rec.maxVlen() / 32);
        AS.VMV(v0, 1);
        if (operands[1].type == ZYDIS_OPERAND_TYPE_MEMORY) {
            // Only when src is memory are the upper bits zeroed
            AS.VMV(result, 0);
            AS.VOR(result, src, 0, VecMask::Yes);
        } else if (operands[1].type == ZYDIS_OPERAND_TYPE_REGISTER) {
            AS.VMERGE(result, dst, src);
        } else {
            UNREACHABLE();
        }
        rec.setOperandVec(&operands[0], result);
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
        AS.VFMV_FS(ft8, src);
        AS.FCVT_W_S(dst, ft8);
    } else if (gpr_size == X86_SIZE_QWORD) {
        rec.setVectorState(SEW::E32, 1);
        AS.VFMV_FS(ft8, src);
        AS.FCVT_L_S(dst, ft8);
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
        AS.VFMV_FS(ft8, src);
        AS.FCVT_W_D(dst, ft8);
    } else if (gpr_size == X86_SIZE_QWORD) {
        AS.VFMV_FS(ft8, src);
        AS.FCVT_L_D(dst, ft8);
    } else {
        UNREACHABLE();
    }

    rec.setOperandGPR(&operands[0], dst);
}

FAST_HANDLE(CVTSS2SD) {
    biscuit::Vec dst = rec.getOperandVec(&operands[0]);
    biscuit::Vec src = rec.getOperandVec(&operands[1]);

    rec.setVectorState(SEW::E32, 1);
    AS.VFMV_FS(ft8, src);
    AS.FCVT_D_S(ft9, ft8);
    rec.setVectorState(SEW::E64, 1);
    AS.VFMV_SF(dst, ft9);

    rec.setOperandVec(&operands[0], dst);
}

FAST_HANDLE(CVTSD2SS) {
    biscuit::Vec dst = rec.getOperandVec(&operands[0]);
    biscuit::Vec src = rec.getOperandVec(&operands[1]);

    rec.setVectorState(SEW::E64, 1);
    AS.VFMV_FS(ft8, src);
    AS.FCVT_S_D(ft9, ft8);
    rec.setVectorState(SEW::E32, 1);
    AS.VFMV_SF(dst, ft9);

    rec.setOperandVec(&operands[0], dst);
}

FAST_HANDLE(SQRTSS) {
    SCALAR(rec, meta, instruction, operands, SEW::E32, 1, &Assembler::VFSQRT);
}

FAST_HANDLE(SQRTSD) {
    SCALAR(rec, meta, instruction, operands, SEW::E64, 1, &Assembler::VFSQRT);
}

FAST_HANDLE(RCPSS) {
    biscuit::Vec temp = rec.scratchVec();
    biscuit::Vec dst = rec.getOperandVec(&operands[0]);
    biscuit::Vec src = rec.getOperandVec(&operands[1]);
    rec.setVectorState(SEW::E32, 1);
    biscuit::GPR ones = rec.scratch();
    AS.LI(ones, 0x3F800000);
    AS.VMV(temp, ones);
    AS.VFDIV(temp, temp, src);

    rec.setVectorState(SEW::E32, rec.maxVlen() / 32);

    biscuit::Vec result = rec.scratchVec();
    AS.VMV(v0, 1);
    AS.VMERGE(result, dst, temp);

    rec.setOperandVec(&operands[0], result);
}

FAST_HANDLE(RSQRTSS) {
    biscuit::Vec temp = rec.scratchVec();
    biscuit::Vec temp2 = rec.scratchVec();
    biscuit::Vec dst = rec.getOperandVec(&operands[0]);
    biscuit::Vec src = rec.getOperandVec(&operands[1]);
    rec.setVectorState(SEW::E32, 1);
    biscuit::GPR ones = rec.scratch();
    AS.LI(ones, 0x3F800000);
    AS.VMV(temp, ones);
    AS.VFSQRT(temp2, src);
    AS.VFDIV(temp, temp, temp2);

    rec.setVectorState(SEW::E32, rec.maxVlen() / 32);

    biscuit::Vec result = rec.scratchVec();
    AS.VMV(v0, 1);
    AS.VMERGE(result, dst, temp);

    rec.setOperandVec(&operands[0], result);
}

FAST_HANDLE(MOVLHPS) { // TODO: vmerge
    biscuit::Vec temp = rec.scratchVec();
    biscuit::Vec iota = rec.scratchVec();
    biscuit::Vec dst = rec.getOperandVec(&operands[0]);
    biscuit::Vec src = rec.getOperandVec(&operands[1]);
    rec.setVectorState(SEW::E64, rec.maxVlen() / 64);
    AS.VMV(v0, 0b10);
    AS.VMV(temp, dst);
    AS.VMV(iota, 0);
    AS.VRGATHER(temp, src, iota, VecMask::Yes); // make only high element pick low from src
    rec.setOperandVec(&operands[0], temp);
}

FAST_HANDLE(FXSAVE) {
    biscuit::GPR address = rec.lea(&operands[0]);
    rec.writebackDirtyState();

    AS.LI(t0, (u64)&felix86_fxsave);

    AS.MV(a0, rec.threadStatePointer());
    AS.MV(a1, address);
    AS.LI(a2, 0);
    AS.JALR(t0);
    rec.restoreRoundingMode();
}

FAST_HANDLE(FXSAVE64) {
    biscuit::GPR address = rec.lea(&operands[0]);
    rec.writebackDirtyState();

    AS.LI(t0, (u64)&felix86_fxsave);

    AS.MV(a0, rec.threadStatePointer());
    AS.MV(a1, address);
    AS.LI(a2, 1);
    AS.JALR(t0);
    rec.restoreRoundingMode();
}

FAST_HANDLE(FXRSTOR) {
    biscuit::GPR address = rec.lea(&operands[0]);
    rec.writebackDirtyState();

    Literal literal((u64)&felix86_fxrstor);
    AS.LD(t0, &literal);

    AS.MV(a0, rec.threadStatePointer());
    AS.MV(a1, address);
    AS.LI(a2, 0);
    AS.JALR(t0);

    Label end;
    AS.J(&end);
    AS.Place(&literal);
    AS.Bind(&end);
    rec.restoreRoundingMode();
}

FAST_HANDLE(FXRSTOR64) {
    biscuit::GPR address = rec.lea(&operands[0]);
    rec.writebackDirtyState();

    Literal literal((u64)&felix86_fxrstor);
    AS.LD(t0, &literal);

    AS.MV(a0, rec.threadStatePointer());
    AS.MV(a1, address);
    AS.LI(a2, 1);
    AS.JALR(t0);

    Label end;
    AS.J(&end);
    AS.Place(&literal);
    AS.Bind(&end);
    rec.restoreRoundingMode();
}

FAST_HANDLE(WRFSBASE) {
    biscuit::GPR reg = rec.getOperandGPR(&operands[0]);

    if (instruction.operand_width == 32) {
        AS.SW(reg, offsetof(ThreadState, fsbase), rec.threadStatePointer());
    } else if (instruction.operand_width == 64) {
        AS.SD(reg, offsetof(ThreadState, fsbase), rec.threadStatePointer());
    } else {
        UNREACHABLE();
    }
}

FAST_HANDLE(XADD) {
    biscuit::GPR result = rec.scratch();
    biscuit::GPR dst;
    biscuit::GPR src = rec.getOperandGPR(&operands[1]);
    bool needs_atomic = operands[0].type == ZYDIS_OPERAND_TYPE_MEMORY && (instruction.attributes & ZYDIS_ATTRIB_HAS_LOCK);
    bool too_small_for_atomic = operands[0].size == 8 || operands[0].size == 16; // amoadd.h amoadd.b aren't out yet, TODO: implement with lr/sc
    bool writeback = true;
    if (needs_atomic && !too_small_for_atomic) {
        // In this case the add+writeback needs to happen atomically
        biscuit::GPR address = rec.lea(&operands[0]);

        dst = rec.scratch();
        if (instruction.operand_width == 32) {
            AS.AMOADD_W(Ordering::AQRL, dst, src, address);
        } else if (instruction.operand_width == 64) {
            AS.AMOADD_D(Ordering::AQRL, dst, src, address);
        } else {
            UNREACHABLE();
        }

        // Still perform the addition in registers to calculate the flags
        // AMOADD stores the loaded value in Rd
        AS.ADD(result, dst, src);
        rec.setOperandGPR(&operands[1], dst);
        rec.popScratch(); // pop LEA scratch
        rec.popScratch();
        writeback = false;
    } else {
        if (needs_atomic) {
            WARN("Atomic XADD with 8 or 16 bit operands encountered");
        }

        dst = rec.getOperandGPR(&operands[0]);
        AS.ADD(result, dst, src);
        rec.setOperandGPR(&operands[1], dst);
    }

    x86_size_e size = rec.getOperandSize(&operands[0]);
    u64 sign_mask = rec.getSignMask(size);

    if (rec.shouldEmitFlag(meta.rip, X86_REF_CF)) {
        biscuit::GPR cf = rec.flagW(X86_REF_CF);
        rec.zext(cf, result, size);
        AS.SLTU(cf, cf, dst);
    }

    if (rec.shouldEmitFlag(meta.rip, X86_REF_PF)) {
        rec.updateParity(result);
    }

    if (rec.shouldEmitFlag(meta.rip, X86_REF_AF)) {
        biscuit::GPR af = rec.flagW(X86_REF_AF);
        biscuit::GPR scratch = rec.scratch();
        AS.ANDI(af, result, 0xF);
        AS.ANDI(scratch, dst, 0xF);
        AS.SLTU(af, af, scratch);
        rec.popScratch();
    }

    if (rec.shouldEmitFlag(meta.rip, X86_REF_ZF)) {
        rec.updateZero(result, size);
    }

    if (rec.shouldEmitFlag(meta.rip, X86_REF_SF)) {
        rec.updateSign(result, size);
    }

    if (rec.shouldEmitFlag(meta.rip, X86_REF_OF)) {
        biscuit::GPR of = rec.flagW(X86_REF_OF);
        is_overflow_add(rec, of, dst, src, result, sign_mask);
    }

    // In this case we also need to writeback the result, otherwise amoadd will do it for us
    if (writeback) {
        rec.setOperandGPR(&operands[0], result);
    }
}

FAST_HANDLE(CMPSD_sse) { // Fuzzed
    u8 imm = rec.getImmediate(&operands[2]) & 0b111;
    biscuit::Vec dst = rec.getOperandVec(&operands[0]);
    biscuit::Vec src = rec.getOperandVec(&operands[1]);

    rec.setVectorState(SEW::E64, 1);
    AS.VFMV_FS(ft8, dst);
    AS.VFMV_FS(ft9, src);

    biscuit::GPR result = rec.scratch();
    switch ((CmpPredicate)imm) {
    case EQ_OQ: {
        AS.FEQ_D(result, ft8, ft9);
        break;
    }
    case LT_OS: {
        AS.FLT_D(result, ft8, ft9);
        break;
    }
    case LE_OS: {
        AS.FLE_D(result, ft8, ft9);
        break;
    }
    case UNORD_Q: {
        // Check if it's a qNan or sNan, check bit 8 and 9
        biscuit::GPR nan = rec.scratch();
        biscuit::GPR mask = rec.scratch();
        AS.FCLASS_D(result, ft8);
        AS.FCLASS_D(nan, ft9);
        AS.OR(result, result, nan);
        AS.LI(mask, 0b11 << 8);
        AS.AND(result, result, mask);
        AS.SNEZ(result, result);
        rec.popScratch();
        rec.popScratch();
        break;
    }
    case NEQ_UQ: {
        biscuit::GPR nan = rec.scratch();
        biscuit::GPR mask = rec.scratch();
        AS.FCLASS_D(result, ft8);
        AS.FCLASS_D(nan, ft9);
        AS.OR(result, result, nan);
        AS.LI(mask, 0b11 << 8);
        AS.AND(result, result, mask);
        AS.SNEZ(result, result);
        rec.popScratch();
        rec.popScratch();

        // After checking if either are nan, also check if they are equal
        AS.FEQ_D(nan, ft8, ft9);
        AS.XORI(nan, nan, 1);
        AS.OR(result, result, nan);
        break;
    }
    case NLT_US: {
        biscuit::GPR nan = rec.scratch();
        biscuit::GPR mask = rec.scratch();
        AS.FCLASS_D(result, ft8);
        AS.FCLASS_D(nan, ft9);
        AS.OR(result, result, nan);
        AS.LI(mask, 0b11 << 8);
        AS.AND(result, result, mask);
        AS.SNEZ(result, result);
        rec.popScratch();
        rec.popScratch();

        // After checking if either are nan, also check if they are equal
        AS.FLT_D(nan, ft8, ft9);
        AS.XORI(nan, nan, 1);
        AS.OR(result, result, nan);
        break;
    }
    case NLE_US: {
        biscuit::GPR nan = rec.scratch();
        biscuit::GPR mask = rec.scratch();
        AS.FCLASS_D(result, ft8);
        AS.FCLASS_D(nan, ft9);
        AS.OR(result, result, nan);
        AS.LI(mask, 0b11 << 8);
        AS.AND(result, result, mask);
        AS.SNEZ(result, result);
        rec.popScratch();
        rec.popScratch();

        // After checking if either are nan, also check if they are equal
        AS.FLE_D(nan, ft8, ft9);
        AS.XORI(nan, nan, 1);
        AS.OR(result, result, nan);
        break;
    }
    case ORD_Q: {
        // Check if neither are NaN
        biscuit::GPR nan = rec.scratch();
        biscuit::GPR mask = rec.scratch();
        AS.FCLASS_D(result, ft8);
        AS.FCLASS_D(nan, ft9);
        AS.OR(result, result, nan);
        AS.LI(mask, 0b11 << 8);
        AS.AND(result, result, mask);
        AS.SEQZ(result, result);
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
    AS.SUB(result, x0, result);
    AS.VMV_SX(dst, result);

    rec.setOperandVec(&operands[0], dst);
}

FAST_HANDLE(CMPSS) { // Fuzzed
    u8 imm = rec.getImmediate(&operands[2]) & 0b111;
    biscuit::Vec dst = rec.getOperandVec(&operands[0]);
    biscuit::Vec src = rec.getOperandVec(&operands[1]);

    rec.setVectorState(SEW::E32, 1);
    AS.VFMV_FS(ft8, dst);
    AS.VFMV_FS(ft9, src);

    biscuit::GPR result = rec.scratch();
    switch ((CmpPredicate)imm) {
    case EQ_OQ: {
        AS.FEQ_S(result, ft8, ft9);
        break;
    }
    case LT_OS: {
        AS.FLT_S(result, ft8, ft9);
        break;
    }
    case LE_OS: {
        AS.FLE_S(result, ft8, ft9);
        break;
    }
    case UNORD_Q: {
        // Check if it's a qNan or sNan, check bit 8 and 9
        biscuit::GPR nan = rec.scratch();
        biscuit::GPR mask = rec.scratch();
        AS.FCLASS_S(result, ft8);
        AS.FCLASS_S(nan, ft9);
        AS.OR(result, result, nan);
        AS.LI(mask, 0b11 << 8);
        AS.AND(result, result, mask);
        AS.SNEZ(result, result);
        rec.popScratch();
        rec.popScratch();
        break;
    }
    case NEQ_UQ: {
        biscuit::GPR nan = rec.scratch();
        biscuit::GPR mask = rec.scratch();
        AS.FCLASS_S(result, ft8);
        AS.FCLASS_S(nan, ft9);
        AS.OR(result, result, nan);
        AS.LI(mask, 0b11 << 8);
        AS.AND(result, result, mask);
        AS.SNEZ(result, result);
        rec.popScratch();
        rec.popScratch();

        // After checking if either are nan, also check if they are equal
        AS.FEQ_S(nan, ft8, ft9);
        AS.XORI(nan, nan, 1);
        AS.OR(result, result, nan);
        break;
    }
    case NLT_US: {
        biscuit::GPR nan = rec.scratch();
        biscuit::GPR mask = rec.scratch();
        AS.FCLASS_S(result, ft8);
        AS.FCLASS_S(nan, ft9);
        AS.OR(result, result, nan);
        AS.LI(mask, 0b11 << 8);
        AS.AND(result, result, mask);
        AS.SNEZ(result, result);
        rec.popScratch();
        rec.popScratch();

        AS.FLE_S(nan, ft9, ft8);
        AS.OR(result, result, nan);
        break;
    }
    case NLE_US: {
        biscuit::GPR nan = rec.scratch();
        biscuit::GPR mask = rec.scratch();
        AS.FCLASS_S(result, ft8);
        AS.FCLASS_S(nan, ft9);
        AS.OR(result, result, nan);
        AS.LI(mask, 0b11 << 8);
        AS.AND(result, result, mask);
        AS.SNEZ(result, result);
        rec.popScratch();
        rec.popScratch();

        // After checking if either are nan, also check if they are equal
        AS.FLT_S(nan, ft9, ft8);
        AS.OR(result, result, nan);
        break;
    }
    case ORD_Q: {
        // Check if neither are NaN
        biscuit::GPR nan = rec.scratch();
        biscuit::GPR mask = rec.scratch();
        AS.FCLASS_S(result, ft8);
        AS.FCLASS_S(nan, ft9);
        AS.OR(result, result, nan);
        AS.LI(mask, 0b11 << 8);
        AS.AND(result, result, mask);
        AS.SEQZ(result, result);
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
    AS.SUB(result, x0, result);
    AS.VMV_SX(dst, result);

    rec.setOperandVec(&operands[0], dst);
}

FAST_HANDLE(CMPSD) {
    if (instruction.meta.isa_set == ZYDIS_ISA_SET_SSE2) {
        fast_CMPSD_sse(rec, meta, instruction, operands);
    } else if (instruction.meta.isa_set == ZYDIS_ISA_SET_I386) {
        fast_CMPSD_string(rec, meta, instruction, operands);
    } else {
        UNREACHABLE();
    }
}

FAST_HANDLE(CMC) {
    biscuit::GPR cf = rec.flagW(X86_REF_CF);
    AS.XORI(cf, cf, 1);
}

FAST_HANDLE(RCL) {
    biscuit::GPR temp_count = rec.scratch();
    biscuit::GPR dst = rec.getOperandGPR(&operands[0]);
    biscuit::GPR dst_temp = rec.scratch();
    biscuit::GPR shift = rec.getOperandGPR(&operands[1]);
    biscuit::GPR cf = rec.flagW(X86_REF_CF);
    biscuit::GPR cf_temp = rec.scratch();

    AS.ANDI(temp_count, shift, instruction.operand_width == 64 ? 63 : 31);
    if (instruction.operand_width == 8) {
        AS.LI(cf_temp, 9);
        AS.REMUW(temp_count, temp_count, cf_temp);
    } else if (instruction.operand_width == 16) {
        AS.LI(cf_temp, 17);
        AS.REMUW(temp_count, temp_count, cf_temp);
    }

    AS.MV(dst_temp, dst);

    rec.disableSignals();

    Label loop, end;
    AS.Bind(&loop);
    AS.BEQZ(temp_count, &end);

    AS.SRLI(cf_temp, dst_temp, instruction.operand_width - 1);
    AS.ANDI(cf_temp, cf_temp, 1);
    AS.SLLI(dst_temp, dst_temp, 1);
    AS.OR(dst_temp, dst_temp, cf);
    AS.MV(cf, cf_temp);
    AS.ADDI(temp_count, temp_count, -1);
    AS.J(&loop);

    AS.Bind(&end);

    rec.enableSignals();

    if (rec.shouldEmitFlag(meta.rip, X86_REF_OF)) {
        biscuit::GPR of = rec.flagW(X86_REF_OF);
        AS.SRLI(of, dst_temp, instruction.operand_width - 1);
        AS.ANDI(of, of, 1);
        AS.XOR(of, of, cf);
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

    AS.ANDI(shift, shift, instruction.operand_width == 64 ? 63 : 31); // shift is always a temporary reg
    if (instruction.operand_width == 8) {
        AS.LI(cf_temp, 9);
        AS.REMUW(shift, shift, cf_temp);
    } else if (instruction.operand_width == 16) {
        AS.LI(cf_temp, 17);
        AS.REMUW(shift, shift, cf_temp);
    }

    AS.MV(dst_temp, dst);

    if (rec.shouldEmitFlag(meta.rip, X86_REF_OF)) {
        biscuit::GPR of = rec.flagW(X86_REF_OF);
        AS.SRLI(of, dst_temp, instruction.operand_width - 1);
        AS.ANDI(of, of, 1);
        AS.XOR(of, of, cf);
    }

    rec.disableSignals();

    Label loop, end;
    AS.Bind(&loop);
    AS.BEQZ(shift, &end);

    AS.ANDI(cf_temp, dst_temp, 1);
    AS.SRLI(dst_temp, dst_temp, 1);
    AS.SLLI(cf_shifted, cf, instruction.operand_width - 1);
    AS.OR(dst_temp, dst_temp, cf_shifted);
    AS.MV(cf, cf_temp);
    AS.ADDI(shift, shift, -1);
    AS.J(&loop);

    AS.Bind(&end);

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
    AS.ANDI(shift, shift, mask);
    AS.MV(result, dst);
    AS.BEQZ(shift, &end);
    AS.LI(shift_sub, operand_size);
    AS.SUB(shift_sub, shift_sub, shift);

    if (operand_size == 64) {
        biscuit::GPR temp = rec.scratch();
        AS.SLL(result, dst, shift);
        AS.SRL(temp, src, shift_sub);
        AS.OR(result, result, temp);
        rec.popScratch();
    } else if (operand_size == 32 || operand_size == 16) {
        biscuit::GPR temp = rec.scratch();
        AS.SLLW(result, dst, shift);
        AS.SRLW(temp, src, shift_sub);
        AS.OR(result, result, temp);
        rec.popScratch();
    } else {
        UNREACHABLE();
    }

    if (rec.shouldEmitFlag(meta.rip, X86_REF_CF)) {
        biscuit::GPR cf = rec.flagW(X86_REF_CF);
        AS.SRL(cf, dst, shift_sub);
        AS.ANDI(cf, cf, 1);
    }

    if (rec.shouldEmitFlag(meta.rip, X86_REF_OF)) {
        biscuit::GPR of = rec.flagW(X86_REF_OF);
        AS.XOR(of, result, dst);
        AS.SRLI(of, of, operand_size - 1);
        AS.ANDI(of, of, 1);
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

    AS.Bind(&end);
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
    AS.ANDI(shift, shift, mask);
    AS.MV(result, dst);
    AS.BEQZ(shift, &end);
    AS.LI(shift_sub, operand_size);
    AS.SUB(shift_sub, shift_sub, shift);

    if (operand_size == 64) {
        biscuit::GPR temp = rec.scratch();
        AS.SRL(result, dst, shift);
        AS.SLL(temp, src, shift_sub);
        AS.OR(result, result, temp);
        rec.popScratch();
    } else if (operand_size == 32 || operand_size == 16) {
        biscuit::GPR temp = rec.scratch();
        AS.SRLW(result, dst, shift);
        AS.SLLW(temp, src, shift_sub);
        AS.OR(result, result, temp);
        rec.popScratch();
    } else {
        UNREACHABLE();
    }

    if (rec.shouldEmitFlag(meta.rip, X86_REF_CF)) {
        biscuit::GPR cf = rec.flagW(X86_REF_CF);
        AS.ADDI(shift, shift, -1);
        AS.SRL(cf, dst, shift);
        AS.ANDI(cf, cf, 1);
    }

    if (rec.shouldEmitFlag(meta.rip, X86_REF_OF)) {
        biscuit::GPR of = rec.flagW(X86_REF_OF);
        AS.XOR(of, result, dst);
        AS.SRLI(of, of, operand_size - 1);
        AS.ANDI(of, of, 1);
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

    AS.Bind(&end);
    rec.setOperandGPR(&operands[0], result);
}

FAST_HANDLE(FNSTCW) {
    WARN("FNSTCW is not implemented, ignoring");
}

FAST_HANDLE(FLDCW) {
    WARN("FLDCW is not implemented, ignoring");
}

FAST_HANDLE(STMXCSR) {
    biscuit::GPR mxcsr = rec.scratch();
    biscuit::GPR address = rec.scratch();
    // TODO: are overflow/inexact/underflow etc flags set in fcsr? if then we need to copy them over
    AS.ADDI(address, rec.threadStatePointer(), offsetof(ThreadState, mxcsr));
    AS.LWU(mxcsr, 0, address);
    rec.setOperandGPR(&operands[0], mxcsr);
}

FAST_HANDLE(LDMXCSR) {
    biscuit::GPR src = rec.getOperandGPR(&operands[0]);
    biscuit::GPR rc = rec.scratch(); // rounding control
    biscuit::GPR temp = rec.scratch();
    biscuit::GPR address = rec.scratch();

    // Extract rounding mode from MXCSR
    AS.SRLI(rc, src, 13);
    AS.ANDI(rc, rc, 0b11);

    // Here's how the rounding modes match up
    // 00 - Round to nearest (even) x86 -> 00 RISC-V
    // 01 - Round down (towards -inf) x86 -> 10 RISC-V
    // 10 - Round up (towards +inf) x86 -> 11 RISC-V
    // 11 - Round towards zero x86 -> 01 RISC-V
    // So we can shift the following bit sequence to the right and mask it
    // 01111000, shift by the rc * 2 and we get the RISC-V rounding mode
    AS.SLLI(rc, rc, 1);
    AS.LI(temp, 0b01111000);
    AS.SRL(temp, temp, rc);
    AS.ANDI(temp, temp, 0b11);
    AS.FSRM(x0, temp); // load the equivalent RISC-V rounding mode

    // Also save the converted rounding mode for quick access
    AS.ADDI(address, rec.threadStatePointer(), offsetof(ThreadState, rmode));
    AS.SB(temp, 0, address);

    AS.ADDI(address, rec.threadStatePointer(), offsetof(ThreadState, mxcsr));
    AS.SW(src, 0, address);
}

FAST_HANDLE(CVTDQ2PD) {
    biscuit::Vec scratch = rec.scratchVec();
    biscuit::Vec src = rec.getOperandVec(&operands[1]);

    rec.setVectorState(SEW::E32, rec.maxVlen() / 32, LMUL::MF2);
    AS.VFWCVT_F_X(scratch, src);

    rec.setOperandVec(&operands[0], scratch);
}

FAST_HANDLE(CVTDQ2PS) {
    biscuit::Vec scratch = rec.scratchVec();
    biscuit::Vec src = rec.getOperandVec(&operands[1]);

    rec.setVectorState(SEW::E32, 4);
    AS.VFCVT_F_X(scratch, src);

    rec.setOperandVec(&operands[0], scratch);
}

FAST_HANDLE(EXTRACTPS) {
    u8 imm = rec.getImmediate(&operands[2]) & 0b11;
    biscuit::GPR dst = rec.scratch();
    biscuit::Vec src = rec.getOperandVec(&operands[1]);
    biscuit::Vec tmp = rec.scratchVec();

    rec.setVectorState(SEW::E32, rec.maxVlen() / 32);
    AS.VSLIDEDOWN(tmp, src, imm);
    AS.VMV_XS(dst, tmp);

    rec.setOperandGPR(&operands[0], dst);
}

FAST_HANDLE(INSERTPS) {
    ERROR("INSERTPS is not implemented");
    u8 immediate = rec.getImmediate(&operands[2]);
    biscuit::Vec dst = rec.getOperandVec(&operands[0]);
    biscuit::Vec src = rec.getOperandVec(&operands[1]);
    biscuit::Vec tmp = rec.scratchVec();
    biscuit::Vec tmp2 = rec.scratchVec();
    biscuit::Vec result = rec.scratchVec();
    biscuit::Vec result_masked = rec.scratchVec();

    u8 count_s = 0;
    u8 count_d = (immediate >> 4) & 0b11;
    u8 zmask = immediate & 0b1111;
    if (operands[1].type == ZYDIS_OPERAND_TYPE_REGISTER) {
        count_s = (immediate >> 6) & 0b11;
    }

    rec.setVectorState(SEW::E32, rec.maxVlen() / 32);
    if (count_s != 0) {
        AS.VSLIDEDOWN(tmp, src, count_s);
    } else {
        AS.VMV(tmp, src);
    }

    if (count_d != 0) {
        AS.VSLIDEUP(tmp2, tmp, count_d);
    } else {
        AS.VMV(tmp2, tmp);
    }

    u8 mask = 1 << count_d;
    AS.VMV(v0, mask);
    AS.VMERGE(result, dst, tmp2);

    AS.VMV(v0, zmask);
    AS.VXOR(result_masked, result, result, VecMask::Yes);

    rec.setOperandVec(&operands[0], result_masked);
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
    biscuit::GPR rsp = rec.getRefGPR(X86_REF_RSP, X86_SIZE_QWORD);
    AS.ADDI(rsp, rsp, -8);
    rec.setRefGPR(X86_REF_RSP, X86_SIZE_QWORD, rsp);
    AS.SD(src, 0, rsp);
}

FAST_HANDLE(POPFQ) {
    biscuit::GPR flags = rec.scratch();
    biscuit::GPR rsp = rec.getRefGPR(X86_REF_RSP, X86_SIZE_QWORD);
    AS.LD(flags, 0, rsp);
    AS.ADDI(rsp, rsp, 8);
    rec.setRefGPR(X86_REF_RSP, X86_SIZE_QWORD, rsp);

    biscuit::GPR cf = rec.flagW(X86_REF_CF);
    biscuit::GPR af = rec.flagW(X86_REF_AF);
    biscuit::GPR zf = rec.flagW(X86_REF_ZF);
    biscuit::GPR sf = rec.flagW(X86_REF_SF);
    biscuit::GPR of = rec.flagW(X86_REF_OF);
    biscuit::GPR temp = rec.scratch();

    AS.ANDI(cf, flags, 1);

    biscuit::GPR pf = rec.scratch();
    AS.SRLI(pf, flags, 2);
    AS.ANDI(pf, pf, 1);
    AS.SB(pf, offsetof(ThreadState, pf), rec.threadStatePointer());

    AS.SRLI(af, flags, 4);
    AS.ANDI(af, af, 1);

    AS.SRLI(zf, flags, 6);
    AS.ANDI(zf, zf, 1);

    AS.SRLI(sf, flags, 7);
    AS.ANDI(sf, sf, 1);

    AS.SRLI(temp, flags, 10);
    AS.ANDI(temp, temp, 1);
    AS.SB(temp, offsetof(ThreadState, df), rec.threadStatePointer());

    AS.SRLI(of, flags, 11);
    AS.ANDI(of, of, 1);

    // CPUID bit may have been modified, which we need to emulate because this is how some programs detect CPUID support
    AS.SRLI(temp, flags, 21);
    AS.ANDI(temp, temp, 1);
    AS.SB(temp, offsetof(ThreadState, cpuid_bit), rec.threadStatePointer());
}

FAST_HANDLE(MOVDDUP) {
    biscuit::Vec result = rec.scratchVec();
    biscuit::Vec iota = rec.scratchVec();
    biscuit::Vec src = rec.getOperandVec(&operands[1]);

    rec.setVectorState(SEW::E64, rec.maxVlen() / 64);
    AS.VMV(iota, 0);
    AS.VRGATHER(result, src, iota);

    rec.setOperandVec(&operands[0], result);
}

FAST_HANDLE(PSADBW) {
    VEC_function(rec, meta, instruction, operands, (u64)&felix86_psadbw);
}

FAST_HANDLE(PAVGB) {
    biscuit::Vec dst = rec.getOperandVec(&operands[0]);
    biscuit::Vec src = rec.getOperandVec(&operands[1]);
    rec.setVectorState(SEW::E8, rec.maxVlen() / 8);
    AS.VAADDU(dst, dst, src);
    rec.setOperandVec(&operands[0], dst);
}

FAST_HANDLE(PAVGW) {
    biscuit::Vec dst = rec.getOperandVec(&operands[0]);
    biscuit::Vec src = rec.getOperandVec(&operands[1]);
    rec.setVectorState(SEW::E16, rec.maxVlen() / 16);
    AS.VAADDU(dst, dst, src);
    rec.setOperandVec(&operands[0], dst);
}