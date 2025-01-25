#include <Zydis/Zydis.h>
#include "felix86/v2/recompiler.hpp"

#define FAST_HANDLE(name)                                                                                                                            \
    void fast_##name(Recompiler& rec, const HandlerMetadata& meta, ZydisDecodedInstruction& instruction, ZydisDecodedOperand* operands)

#define AS (rec.getAssembler())

#define IS_MMX (instruction.attributes & (ZYDIS_ATTRIB_FPU_STATE_CR | ZYDIS_ATTRIB_FPU_STATE_CW))

#define HAS_REP (instruction.attributes & (ZYDIS_ATTRIB_HAS_REP | ZYDIS_ATTRIB_HAS_REPZ | ZYDIS_ATTRIB_HAS_REPNZ))

void is_overflow_sub(Recompiler& rec, biscuit::GPR of, biscuit::GPR lhs, biscuit::GPR rhs, biscuit::GPR result, u64 sign_mask) {
    biscuit::GPR scratch = rec.scratch();
    AS.XOR(scratch, lhs, rhs);
    AS.XOR(of, lhs, result);
    AS.AND(of, of, scratch);
    AS.LI(scratch, sign_mask);
    AS.AND(of, of, scratch);
    AS.SNEZ(of, of);
}

void is_overflow_add(Recompiler& rec, biscuit::GPR of, biscuit::GPR lhs, biscuit::GPR rhs, biscuit::GPR result, u64 sign_mask) {
    biscuit::GPR scratch = rec.scratch();
    AS.XOR(scratch, result, lhs);
    AS.XOR(of, result, rhs);
    AS.AND(of, of, scratch);
    AS.LI(scratch, sign_mask);
    AS.AND(of, of, scratch);
    AS.SNEZ(of, of);
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
        rec.popScratch();
    }

    rec.setOperandGPR(&operands[0], result);
}

FAST_HANDLE(SUB) {
    biscuit::GPR result = rec.scratch();
    biscuit::GPR src = rec.getOperandGPR(&operands[1]);
    biscuit::GPR dst = rec.getOperandGPR(&operands[0]);

    AS.SUB(result, dst, src);

    x86_size_e size = rec.getOperandSize(&operands[0]);
    u64 sign_mask = rec.getSignMask(size);

    if (rec.shouldEmitFlag(meta.rip, X86_REF_CF)) {
        biscuit::GPR cf = rec.flagW(X86_REF_CF);
        AS.SLTU(cf, dst, src);
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
        is_overflow_sub(rec, of, dst, src, result, sign_mask);
        rec.popScratch();
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
        AS.SLTU(cf, dst, src);
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
    u64 sign_mask = rec.getSignMask(size);

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
        AS.SEQZ(scratch, scratch);
        AS.OR(af, af, scratch);
        rec.popScratch();
    }

    if (rec.shouldEmitFlag(meta.rip, X86_REF_OF)) {
        biscuit::GPR scratch = rec.scratch();
        biscuit::GPR scratch2 = rec.scratch();
        biscuit::GPR of = rec.flagW(X86_REF_OF);
        AS.XOR(scratch, result, dst);
        AS.XOR(of, result, src);
        AS.AND(of, of, scratch);
        AS.LI(scratch, sign_mask);
        AS.AND(of, of, scratch);
        AS.SNEZ(of, of);
        AS.XOR(scratch2, result_2, cf);
        AS.XOR(scratch, result_2, result);
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

    rec.writebackDirtyState();
}

FAST_HANDLE(CMP) {
    biscuit::GPR result = rec.scratch();
    biscuit::GPR src = rec.getOperandGPR(&operands[1]);
    biscuit::GPR dst = rec.getOperandGPR(&operands[0]);

    AS.SUB(result, dst, src);

    x86_size_e size = rec.getOperandSize(&operands[0]);
    u64 sign_mask = rec.getSignMask(size);

    if (rec.shouldEmitFlag(meta.rip, X86_REF_CF)) {
        biscuit::GPR cf = rec.flagW(X86_REF_CF);
        AS.SLTU(cf, dst, src);
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
        biscuit::GPR rsp = rec.getRefGPR(X86_REF_RSP, X86_SIZE_QWORD);
        AS.ADDI(rsp, rsp, -8);
        rec.setRefGPR(X86_REF_RSP, X86_SIZE_QWORD, rsp);

        u64 return_offset = meta.rip - meta.block_start + instruction.length;
        rec.addi(scratch, scratch, return_offset);

        AS.SD(scratch, 0, rsp);

        biscuit::GPR src = rec.getOperandGPR(&operands[0]);
        rec.setRip(src);
        rec.writebackDirtyState();
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
    rec.backToDispatcher();
    rec.stopCompiling();
}

FAST_HANDLE(PUSH) {
    biscuit::GPR src = rec.getOperandGPR(&operands[0]);
    biscuit::GPR rsp = rec.getRefGPR(X86_REF_RSP, X86_SIZE_QWORD);
    int imm = instruction.operand_width == 16 ? -2 : -8;
    AS.ADDI(rsp, rsp, imm);
    rec.setRefGPR(X86_REF_RSP, X86_SIZE_QWORD, rsp);

    if (instruction.operand_width == 16) {
        AS.SH(src, 0, rsp);
    } else {
        AS.SD(src, 0, rsp);
    }
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
    AS.ADDI(rsp, rsp, imm);
    rec.setRefGPR(X86_REF_RSP, X86_SIZE_QWORD, rsp);
    rec.setOperandGPR(&operands[0], result);
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

    AS.BEQZ(count, &zero_source);

    AS.SLL(result, dst, count);

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

    rec.setOperandGPR(&operands[0], result);

    AS.Bind(&zero_source);
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

    AS.BEQZ(count, &zero_source);

    AS.SRL(result, dst, count);

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

    rec.setOperandGPR(&operands[0], result);

    AS.Bind(&zero_source);
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

    AS.BEQZ(count, &zero_source);

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

    rec.setOperandGPR(&operands[0], result);

    AS.Bind(&zero_source);
}

FAST_HANDLE(MOVQ) {
    if (operands[0].type == ZYDIS_OPERAND_TYPE_MEMORY) {
        biscuit::GPR dst = rec.scratch();
        biscuit::Vec src = rec.getOperandVec(&operands[1]);

        rec.setVectorState(SEW::E64, rec.maxVlen() / 64);
        AS.VMV_XS(dst, src);

        rec.setOperandGPR(&operands[0], dst);
    } else if (operands[1].type == ZYDIS_OPERAND_TYPE_MEMORY) {
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
            biscuit::Vec dst = rec.getOperandVec(&operands[0]);
            biscuit::Vec src = rec.getOperandVec(&operands[1]);

            rec.setVectorState(SEW::E64, rec.maxVlen() / 64);
            AS.VMV(dst, 0);
            AS.VMV(v0, 0b01);
            AS.VOR(dst, dst, src, VecMask::Yes);

            rec.setOperandVec(&operands[0], dst);
        }
    }
}

FAST_HANDLE(MOVD) {
    if (operands[0].type == ZYDIS_OPERAND_TYPE_MEMORY) {
        biscuit::GPR dst = rec.scratch();
        biscuit::Vec src = rec.getOperandVec(&operands[1]);

        rec.setVectorState(SEW::E32, rec.maxVlen() / 64);
        AS.VMV_XS(dst, src);

        rec.setOperandGPR(&operands[0], dst);
    } else if (operands[1].type == ZYDIS_OPERAND_TYPE_MEMORY) {
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

            rec.setVectorState(SEW::E32, rec.maxVlen() / 64);
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
            biscuit::Vec dst = rec.getOperandVec(&operands[0]);
            biscuit::Vec src = rec.getOperandVec(&operands[1]);

            rec.setVectorState(SEW::E32, rec.maxVlen() / 32);
            AS.VMV(dst, 0);
            AS.VMV(v0, 0b01);
            AS.VOR(dst, dst, src, VecMask::Yes);

            rec.setOperandVec(&operands[0], dst);
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
        AS.LD(address, offsetof(ThreadState, divu128_handler), rec.threadStatePointer());
        AS.MV(a0, rec.threadStatePointer());
        AS.MV(a1, src);
        AS.JALR(address);
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
        AS.LD(address, offsetof(ThreadState, div128_handler), rec.threadStatePointer());
        AS.MV(a0, rec.threadStatePointer());
        AS.MV(a1, src);
        AS.JALR(address);
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
    biscuit::GPR dst = rec.getOperandGPR(&operands[0]);
    biscuit::GPR res = rec.scratch();

    AS.ADDI(res, dst, 1);

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

    rec.setOperandGPR(&operands[0], res);
}

FAST_HANDLE(DEC) {
    x86_size_e size = rec.getOperandSize(&operands[0]);
    biscuit::GPR dst = rec.getOperandGPR(&operands[0]);
    biscuit::GPR res = rec.scratch();

    if (rec.shouldEmitFlag(meta.rip, X86_REF_AF)) {
        biscuit::GPR af = rec.flagW(X86_REF_AF);
        AS.ANDI(af, dst, 0xF);
        AS.SEQZ(af, af);
    }

    AS.ADDI(res, dst, -1);

    if (rec.shouldEmitFlag(meta.rip, X86_REF_OF)) {
        biscuit::GPR of = rec.flagW(X86_REF_OF);
        biscuit::GPR one = rec.scratch();
        u64 sign_mask = rec.getSignMask(size);
        AS.LI(one, 1);
        is_overflow_sub(rec, of, dst, one, res, sign_mask);
        rec.popScratch();
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

    rec.setOperandGPR(&operands[0], res);
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

FAST_HANDLE(XCHG) {
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
    rec.sextb(al, al);
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
        biscuit::GPR dst = rec.getOperandGPR(&operands[0]);
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
            AS.MULH(result, src1, src2);
            AS.MUL(dst, src1, src2);
            rec.setOperandGPR(&operands[0], dst);

            if (rec.shouldEmitFlag(meta.rip, X86_REF_CF) || rec.shouldEmitFlag(meta.rip, X86_REF_OF)) {
                biscuit::GPR cf = rec.flagW(X86_REF_CF);
                biscuit::GPR of = rec.flagW(X86_REF_OF);
                AS.SRAI(cf, dst, 63);
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
            AS.SRLI(cf, result, 8);
            AS.ANDI(cf, cf, 8);
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
    AS.RDCYCLE(tsc);
    rec.setRefGPR(X86_REF_RAX, X86_SIZE_DWORD, tsc);
    AS.SRLI(tsc, tsc, 32);
    rec.setRefGPR(X86_REF_RDX, X86_SIZE_DWORD, tsc);
}

FAST_HANDLE(CPUID) {
    rec.writebackDirtyState();

    biscuit::GPR address = rec.scratch();
    AS.LD(address, offsetof(ThreadState, cpuid_handler), rec.threadStatePointer());
    AS.MV(a0, rec.threadStatePointer());
    AS.JALR(address);
}

FAST_HANDLE(SYSCALL) {
    biscuit::GPR rcx = rec.getRefGPR(X86_REF_RCX, X86_SIZE_QWORD);
    AS.LI(rcx, meta.rip + instruction.length);
    rec.setRefGPR(X86_REF_RCX, X86_SIZE_QWORD, rcx);
    biscuit::GPR flags = rec.getFlags();
    rec.setRefGPR(X86_REF_R11, X86_SIZE_QWORD, flags);

    // Normally IA32_FMASK masks the RFLAGS but surely we don't have to do anything here :cluegi:

    rec.writebackDirtyState();

    biscuit::GPR address = rec.scratch();
    AS.LD(address, offsetof(ThreadState, syscall_handler), rec.threadStatePointer());
    AS.MV(a0, rec.threadStatePointer());
    AS.JALR(address);
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

FAST_HANDLE(PANDN) {
    biscuit::Vec dst_not = rec.scratchVec();
    biscuit::Vec dst = rec.getOperandVec(&operands[0]);
    biscuit::Vec src = rec.getOperandVec(&operands[1]);
    rec.setVectorState(SEW::E64, rec.maxVlen() / 64);
    AS.VXOR(dst_not, dst, -1);
    AS.VAND(dst, dst_not, src);
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

FAST_HANDLE(ANDNPS) {
    fast_PANDN(rec, meta, instruction, operands);
}

FAST_HANDLE(ANDNPD) {
    fast_PANDN(rec, meta, instruction, operands);
}

void PADD(Recompiler& rec, const HandlerMetadata& meta, ZydisDecodedInstruction& instruction, ZydisDecodedOperand* operands, SEW sew, u8 vlen) {
    biscuit::Vec dst = rec.getOperandVec(&operands[0]);
    biscuit::Vec src = rec.getOperandVec(&operands[1]);
    rec.setVectorState(sew, vlen);
    AS.VADD(dst, dst, src);
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
    rec.setVectorState(SEW::E32, rec.maxVlen() / 32);
    AS.VFMIN(dst, dst, src);
    rec.setOperandVec(&operands[0], dst);
}

FAST_HANDLE(MINPD) {
    biscuit::Vec dst = rec.getOperandVec(&operands[0]);
    biscuit::Vec src = rec.getOperandVec(&operands[1]);
    rec.setVectorState(SEW::E64, rec.maxVlen() / 64);
    AS.VFMIN(dst, dst, src);
    rec.setOperandVec(&operands[0], dst);
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

FAST_HANDLE(MAXPS) {
    biscuit::Vec dst = rec.getOperandVec(&operands[0]);
    biscuit::Vec src = rec.getOperandVec(&operands[1]);
    rec.setVectorState(SEW::E32, rec.maxVlen() / 32);
    AS.VFMAX(dst, dst, src);
    rec.setOperandVec(&operands[0], dst);
}

FAST_HANDLE(MAXPD) {
    biscuit::Vec dst = rec.getOperandVec(&operands[0]);
    biscuit::Vec src = rec.getOperandVec(&operands[1]);
    rec.setVectorState(SEW::E64, rec.maxVlen() / 64);
    AS.VFMAX(dst, dst, src);
    rec.setOperandVec(&operands[0], dst);
}

FAST_HANDLE(MULPS) {
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

FAST_HANDLE(SQRTPS) {
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
    biscuit::Vec dst = rec.allocatedVec(rec.zydisToRef(operands[0].reg.value));
    biscuit::Vec src = rec.getOperandVec(&operands[1]);
    rec.setVectorState(SEW::E32, rec.maxVlen() / 32);
    AS.VFDIV(dst, dst, src);
    rec.setOperandVec(&operands[0], dst);
}

FAST_HANDLE(DIVPD) {
    biscuit::Vec dst = rec.allocatedVec(rec.zydisToRef(operands[0].reg.value));
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
    Label loop_end, loop_body;
    if (HAS_REP) {
        rec.repPrologue(&loop_end);
        AS.Bind(&loop_body);
    }

    biscuit::GPR rdi = rec.getRefGPR(X86_REF_RDI, X86_SIZE_QWORD);
    biscuit::GPR rsi = rec.getRefGPR(X86_REF_RSI, X86_SIZE_QWORD);
    biscuit::GPR temp = rec.scratch();
    u8 width = instruction.operand_width;
    rec.readMemory(temp, rsi, 0, rec.zydisToSize(width));
    rec.writeMemory(temp, rdi, 0, rec.zydisToSize(width));

    AS.LB(temp, offsetof(ThreadState, df), rec.threadStatePointer());

    // TODO: move this stuff outside the loop like in STOS
    Label end, false_label;

    AS.BEQZ(temp, &false_label);

    AS.ADDI(rdi, rdi, -width / 8);
    AS.ADDI(rsi, rsi, -width / 8);
    AS.J(&end);

    AS.Bind(&false_label);

    AS.ADDI(rdi, rdi, width / 8);
    AS.ADDI(rsi, rsi, width / 8);

    AS.Bind(&end);

    rec.setRefGPR(X86_REF_RDI, X86_SIZE_QWORD, rdi);
    rec.setRefGPR(X86_REF_RSI, X86_SIZE_QWORD, rsi);

    if (HAS_REP) {
        rec.repEpilogue(&loop_body);
        AS.Bind(&loop_end);
    }
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
        biscuit::Vec dst = rec.getOperandVec(&operands[0]);
        biscuit::Vec src = rec.getOperandVec(&operands[1]);
        rec.setVectorState(SEW::E64, rec.maxVlen() / 64);
        AS.VMV(v0, 1);
        if (operands[1].type == ZYDIS_OPERAND_TYPE_MEMORY) {
            // Only when src is memory are the upper bits zeroed
            AS.VMV(dst, 0);
            AS.VOR(dst, dst, src, VecMask::Yes);
        } else {
            AS.VMERGE(dst, dst, src);
        }
        rec.setOperandVec(&operands[0], dst);
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

FAST_HANDLE(STOSB) {
    Label loop_end, loop_body;
    u8 width = instruction.operand_width;
    biscuit::GPR rdi = rec.getRefGPR(X86_REF_RDI, X86_SIZE_QWORD);
    biscuit::GPR rax = rec.getRefGPR(X86_REF_RAX, rec.zydisToSize(width));
    biscuit::GPR temp = rec.scratch();
    biscuit::GPR df = rec.scratch();
    AS.LB(df, offsetof(ThreadState, df), rec.threadStatePointer());

    Label end;
    AS.LI(temp, -width / 8);
    AS.BNEZ(df, &end);
    AS.LI(temp, width / 8);
    AS.Bind(&end);

    if (HAS_REP) {
        rec.repPrologue(&loop_end);
        AS.Bind(&loop_body);
    }

    rec.writeMemory(rax, rdi, 0, rec.zydisToSize(width));
    AS.ADD(rdi, rdi, temp);

    if (HAS_REP) {
        rec.repEpilogue(&loop_body);
        AS.Bind(&loop_end);
    }

    rec.setRefGPR(X86_REF_RDI, X86_SIZE_QWORD, rdi);
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
    if (size == X86_SIZE_BYTE) {
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
        is_overflow_sub(rec, of, x0, dst, result, size);
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

FAST_HANDLE(PACKUSWB) {
    // While this instruction seems like a perfect target for VNCLIPU, my board (?) decides to throw a SIGILL
    // no matter what I do, so I'm just gonna do a function.
    x86_ref_e dst_ref = rec.zydisToRef(operands[0].reg.value);
    ASSERT(dst_ref >= X86_REF_XMM0 && dst_ref <= X86_REF_XMM15);

    biscuit::GPR temp;
    if (operands[1].type == ZYDIS_OPERAND_TYPE_MEMORY) {
        temp = rec.lea(&operands[1]);
    }
    rec.writebackDirtyState();

    AS.LI(t0, (u64)&felix86_packuswb);

    AS.ADDI(a0, rec.threadStatePointer(), offsetof(ThreadState, xmm) + (dst_ref - X86_REF_XMM0) * 16);

    if (operands[1].type == ZYDIS_OPERAND_TYPE_REGISTER) {
        x86_ref_e src_ref = rec.zydisToRef(operands[1].reg.value);
        ASSERT(src_ref >= X86_REF_XMM0 && src_ref <= X86_REF_XMM15);
        AS.ADDI(a1, rec.threadStatePointer(), offsetof(ThreadState, xmm) + (src_ref - X86_REF_XMM0) * 16);
    } else {
        AS.MV(a1, temp);
    }

    AS.JALR(t0);
}

enum class x86RoundingMode { Nearest = 0, Down = 1, Up = 2, Truncate = 3 };

RMode rounding_mode(x86RoundingMode mode) {
    switch (mode) {
    case x86RoundingMode::Nearest:
        return RMode::RNE;
    case x86RoundingMode::Down:
        return RMode::RDN;
    case x86RoundingMode::Up:
        return RMode::RUP;
    case x86RoundingMode::Truncate:
        return RMode::RTZ;
    default:
        UNREACHABLE();
    }
}

void ROUND(Recompiler& rec, const HandlerMetadata& meta, ZydisDecodedInstruction& instruction, ZydisDecodedOperand* operands, SEW sew, u8 vlen) {
    u8 imm = rec.getImmediate(&operands[2]);
    biscuit::Vec dst = rec.getOperandVec(&operands[0]);
    biscuit::Vec src = rec.getOperandVec(&operands[1]);
    if (!(imm & 0b1000)) {
        WARN("Ignore precision bit not set for roundsd/roundss");
    }
    ASSERT(!(imm & 0b100)); // rounding mode not from mxscr

    rec.setVectorState(sew, vlen);
    AS.VFMV_FS(ft0, src);

    if (Extensions::Zfa) {
        if (sew == SEW::E64) {
            AS.FROUND_D(ft1, ft0, rounding_mode((x86RoundingMode)(imm & 0b11)));
        } else {
            AS.FROUND_S(ft1, ft0, rounding_mode((x86RoundingMode)(imm & 0b11)));
        }
    } else {
        biscuit::GPR temp = rec.scratch();
        if (sew == SEW::E64) {
            AS.FCVT_L_D(temp, ft0, rounding_mode((x86RoundingMode)(imm & 0b11)));
            AS.FCVT_D_L(ft1, temp);
        } else {
            AS.FCVT_L_S(temp, ft0, rounding_mode((x86RoundingMode)(imm & 0b11)));
            AS.FCVT_S_L(ft1, temp);
        }
    }

    AS.VFMV_SF(dst, ft1);

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

FAST_HANDLE(PSHUFD) {
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

    rec.setVectorState(SEW::E32, rec.maxVlen() / 32);
    AS.VMV(iota2, el3);
    AS.LI(temp, el2);
    AS.VSLIDE1UP(iota, iota2, temp);
    AS.LI(temp, el1);
    AS.VSLIDE1UP(iota2, iota, temp);
    AS.LI(temp, el0);
    AS.VSLIDE1UP(iota, iota2, temp);

    AS.VMV(result, 0);
    AS.VRGATHER(result, src, iota);

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

    // Use two register grouping

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
    AS.BEQZ(src, &end);
    AS.CTZ(result, src);
    rec.setOperandGPR(&operands[0], result);

    AS.Bind(&end);
    AS.SEQZ(zf, src);

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

    Label end;
    AS.LI(result, instruction.operand_width);
    AS.BEQZ(src, &end);
    AS.CTZ(result, src);
    AS.J(&end);

    AS.Bind(&end);
    rec.setOperandGPR(&operands[0], result);
    AS.SEQZ(zf, src);

    rec.setFlagUndefined(X86_REF_CF);
    rec.setFlagUndefined(X86_REF_OF);
    rec.setFlagUndefined(X86_REF_SF);
    rec.setFlagUndefined(X86_REF_AF);
}

FAST_HANDLE(BTC) {
    ASSERT(operands[0].type != ZYDIS_OPERAND_TYPE_MEMORY);
    biscuit::GPR shift = rec.scratch();
    biscuit::GPR mask = rec.scratch();
    biscuit::GPR result = rec.scratch();
    biscuit::GPR bit = rec.getOperandGPR(&operands[1]);
    biscuit::GPR dst = rec.getOperandGPR(&operands[0]);
    biscuit::GPR cf = rec.flagW(X86_REF_CF);

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
    ASSERT(operands[0].type != ZYDIS_OPERAND_TYPE_MEMORY);
    biscuit::GPR shift = rec.scratch();
    biscuit::GPR bit = rec.getOperandGPR(&operands[1]);
    biscuit::GPR dst = rec.getOperandGPR(&operands[0]);
    biscuit::GPR cf = rec.flagW(X86_REF_CF);

    u8 bit_size = operands[0].size;
    AS.ANDI(shift, bit, bit_size - 1);
    AS.SRL(cf, dst, shift);
    AS.ANDI(cf, cf, 1);

    rec.setFlagUndefined(X86_REF_OF);
    rec.setFlagUndefined(X86_REF_SF);
    rec.setFlagUndefined(X86_REF_AF);
}

FAST_HANDLE(BTS) {
    ASSERT(operands[0].type != ZYDIS_OPERAND_TYPE_MEMORY);
    biscuit::GPR shift = rec.scratch();
    biscuit::GPR result = rec.scratch();
    biscuit::GPR bit = rec.getOperandGPR(&operands[1]);
    biscuit::GPR dst = rec.getOperandGPR(&operands[0]);
    biscuit::GPR cf = rec.flagW(X86_REF_CF);

    u8 bit_size = operands[0].size;
    AS.ANDI(shift, bit, bit_size - 1);
    AS.SRL(cf, dst, shift);
    AS.ANDI(cf, cf, 1);
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
    ASSERT(operands[0].type != ZYDIS_OPERAND_TYPE_MEMORY);
    biscuit::GPR shift = rec.scratch();
    biscuit::GPR result = rec.scratch();
    biscuit::GPR bit = rec.getOperandGPR(&operands[1]);
    biscuit::GPR dst = rec.getOperandGPR(&operands[0]);
    biscuit::GPR cf = rec.flagW(X86_REF_CF);

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

FAST_HANDLE(BSR) {
    ASSERT(Extensions::B);
    biscuit::GPR result = rec.scratch();
    biscuit::GPR src = rec.getOperandGPR(&operands[1]);
    biscuit::GPR dst = rec.getOperandGPR(&operands[0]);
    (void)dst; // must be loaded since conditional code follows
    biscuit::GPR zf = rec.flagW(X86_REF_ZF);

    Label end;
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
    AS.SEQZ(zf, src);

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
    } else {
        AS.REV8(result, dst);
    }

    rec.setOperandGPR(&operands[0], result);
}

FAST_HANDLE(MOVLPS) {
    if (operands[0].type == ZYDIS_OPERAND_TYPE_REGISTER) {
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

FAST_HANDLE(PSLLQ) {
    u8 shift = rec.getImmediate(&operands[1]);
    biscuit::Vec dst = rec.getOperandVec(&operands[0]);
    rec.setVectorState(SEW::E64, rec.maxVlen() / 64);
    if (shift > 63) {
        AS.VMV(dst, 0);
    } else {
        if (shift <= 31) {
            AS.VSLL(dst, dst, shift);
        } else {
            biscuit::GPR sh = rec.scratch();
            AS.LI(sh, shift);
            AS.VSLL(dst, dst, sh);
        }
    }
    rec.setOperandVec(&operands[0], dst);
}

FAST_HANDLE(PSLLD) {
    u8 shift = rec.getImmediate(&operands[1]);
    biscuit::Vec dst = rec.getOperandVec(&operands[0]);
    rec.setVectorState(SEW::E32, rec.maxVlen() / 32);
    if (shift > 31) {
        AS.VMV(dst, 0);
    } else {
        AS.VSLL(dst, dst, shift);
    }
    rec.setOperandVec(&operands[0], dst);
}

FAST_HANDLE(PSRLD) {
    u8 shift = rec.getImmediate(&operands[1]);
    biscuit::Vec dst = rec.getOperandVec(&operands[0]);
    rec.setVectorState(SEW::E32, rec.maxVlen() / 32);
    if (shift > 31) {
        AS.VMV(dst, 0);
    } else {
        AS.VSRL(dst, dst, shift);
    }
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

FAST_HANDLE(PSRLQ) {
    u8 shift = rec.getImmediate(&operands[1]);
    biscuit::Vec dst = rec.getOperandVec(&operands[0]);
    rec.setVectorState(SEW::E64, rec.maxVlen() / 64);
    if (shift > 63) {
        AS.VMV(dst, 0);
    } else {
        if (shift <= 31) {
            AS.VSRL(dst, dst, shift);
        } else {
            biscuit::GPR sh = rec.scratch();
            AS.LI(sh, shift);
            AS.VSRL(dst, dst, sh);
        }
    }
    rec.setOperandVec(&operands[0], dst);
}

FAST_HANDLE(SFENCE) {
    AS.FENCE(); // just make a full fence for now, TODO: we can optimize this some day
}

FAST_HANDLE(LFENCE) {
    AS.FENCE(); // just make a full fence for now, TODO: we can optimize this some day
}

FAST_HANDLE(MFENCE) {
    AS.FENCE(); // just make a full fence for now, TODO: we can optimize this some day
}

FAST_HANDLE(MOVSX) {
    biscuit::GPR dst = rec.getOperandGPR(&operands[0]);
    biscuit::GPR src = rec.getOperandGPR(&operands[1]);
    x86_size_e size = rec.getOperandSize(&operands[1]);

    switch (size) {
    case X86_SIZE_BYTE:
    case X86_SIZE_BYTE_HIGH: {
        rec.sextb(dst, src);
        break;
    }
    case X86_SIZE_WORD: {
        rec.sexth(dst, src);
        break;
    }
    default: {
        UNREACHABLE();
        break;
    }
    }

    rec.setOperandGPR(&operands[0], dst);
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
    AS.VMSLT(temp, dst, src);
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

FAST_HANDLE(COMISD) {
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

FAST_HANDLE(PEXTRW) {
    biscuit::Vec temp = rec.scratchVec();
    biscuit::GPR result = rec.scratch();
    biscuit::Vec src = rec.getOperandVec(&operands[1]);
    biscuit::GPR dst = rec.getOperandGPR(&operands[0]);
    u8 imm = rec.getImmediate(&operands[2]) & 0b111;

    rec.setVectorState(SEW::E16, rec.maxVlen() / 16);
    AS.VSLIDEDOWN(temp, src, imm);
    AS.VMV_XS(dst, temp);
    rec.zext(result, dst, X86_SIZE_WORD);

    rec.setOperandGPR(&operands[0], result);
}

FAST_HANDLE(CMPXCHG) {
    x86_size_e size = rec.zydisToSize(instruction.operand_width);
    biscuit::GPR dst = rec.getOperandGPR(&operands[0]);
    biscuit::GPR src = rec.getOperandGPR(&operands[1]);
    biscuit::GPR rax = rec.getRefGPR(X86_REF_RAX, size);
    biscuit::GPR zf = rec.flagW(X86_REF_ZF);

    Label end, equal;

    biscuit::GPR result = rec.scratch();

    AS.SUB(result, rax, dst);

    if (rec.shouldEmitFlag(meta.rip, X86_REF_CF)) {
        biscuit::GPR cf = rec.flagW(X86_REF_CF);
        AS.SLTU(cf, rax, dst);
    }

    if (rec.shouldEmitFlag(meta.rip, X86_REF_PF)) {
        rec.updateParity(result);
    }

    if (rec.shouldEmitFlag(meta.rip, X86_REF_AF)) {
        biscuit::GPR af = rec.flagW(X86_REF_AF);
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
        biscuit::GPR scratch = rec.scratch();
        biscuit::GPR of = rec.flagW(X86_REF_OF);
        u64 sign_mask = rec.getSignMask(size);
        AS.XOR(scratch, dst, rax);
        AS.XOR(of, dst, result);
        AS.AND(of, of, scratch);
        AS.LI(scratch, sign_mask);
        AS.AND(of, of, scratch);
        AS.SNEZ(of, of);
        rec.popScratch();
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
    } else {
        rec.setVectorState(SEW::E64, rec.maxVlen() / 64);
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
    } else {
        rec.setVectorState(SEW::E64, rec.maxVlen() / 64);
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

FAST_HANDLE(ADDSD) {
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

FAST_HANDLE(MINSS) {
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
        AS.FCVT_D_W(ft0, src);
        rec.setVectorState(SEW::E64, 1);
        AS.VFMV_SF(dst, ft0);
    } else {
        AS.FCVT_D_L(ft0, src);
        rec.setVectorState(SEW::E64, 1);
        AS.VFMV_SF(dst, ft0);
    }

    rec.setOperandVec(&operands[0], dst);
}

FAST_HANDLE(CVTSI2SS) {
    x86_size_e gpr_size = rec.getOperandSize(&operands[1]);
    biscuit::Vec dst = rec.getOperandVec(&operands[0]);
    biscuit::GPR src = rec.getOperandGPR(&operands[1]);

    if (gpr_size == X86_SIZE_DWORD) {
        AS.FCVT_S_W(ft0, src);
        rec.setVectorState(SEW::E32, 1);
        AS.VFMV_SF(dst, ft0);

    } else {
        AS.FCVT_S_L(ft0, src);
        rec.setVectorState(SEW::E32, 1);
        AS.VFMV_SF(dst, ft0);
    }

    rec.setOperandVec(&operands[0], dst);
}

FAST_HANDLE(CVTTSS2SI) {
    x86_size_e gpr_size = rec.getOperandSize(&operands[1]);
    biscuit::Vec src = rec.getOperandVec(&operands[1]);
    biscuit::GPR dst = rec.getOperandGPR(&operands[0]);

    if (gpr_size == X86_SIZE_DWORD) {
        rec.setVectorState(SEW::E32, 1);
        AS.VFMV_FS(ft0, src);
        AS.FCVT_W_S(dst, ft0, RMode::RTZ);
    } else {
        rec.setVectorState(SEW::E32, 1);
        AS.VFMV_FS(ft0, src);
        AS.FCVT_L_S(dst, ft0, RMode::RTZ);
    }

    rec.setOperandGPR(&operands[0], dst);
}

FAST_HANDLE(CVTTSD2SI) {
    x86_size_e gpr_size = rec.getOperandSize(&operands[1]);
    biscuit::Vec src = rec.getOperandVec(&operands[1]);
    biscuit::GPR dst = rec.getOperandGPR(&operands[0]);

    if (gpr_size == X86_SIZE_DWORD) {
        rec.setVectorState(SEW::E64, 1);
        AS.VFMV_FS(ft0, src);
        AS.FCVT_W_D(dst, ft0, RMode::RTZ);
    } else {
        rec.setVectorState(SEW::E64, 1);
        AS.VFMV_FS(ft0, src);
        AS.FCVT_L_D(dst, ft0, RMode::RTZ);
    }

    rec.setOperandGPR(&operands[0], dst);
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
    } else {
        biscuit::Vec dst = rec.getOperandVec(&operands[0]);
        biscuit::Vec src = rec.getOperandVec(&operands[1]);
        rec.setVectorState(SEW::E32, rec.maxVlen() / 32);
        AS.VMV(v0, 1);
        if (operands[1].type == ZYDIS_OPERAND_TYPE_MEMORY) {
            // Only when src is memory are the upper bits zeroed
            AS.VMV(dst, 0);
            AS.VOR(dst, dst, src, VecMask::Yes);
        } else {
            AS.VMERGE(dst, dst, src);
        }
        rec.setOperandVec(&operands[0], dst);
    }
}

FAST_HANDLE(CVTSS2SI) {
    x86_size_e gpr_size = rec.getOperandSize(&operands[1]);
    biscuit::Vec src = rec.getOperandVec(&operands[1]);
    biscuit::GPR dst = rec.getOperandGPR(&operands[0]);

    if (gpr_size == X86_SIZE_DWORD) {
        rec.setVectorState(SEW::E32, 1);
        AS.VFMV_FS(ft0, src);
        AS.FCVT_W_S(dst, ft0);
    } else {
        rec.setVectorState(SEW::E32, 1);
        AS.VFMV_FS(ft0, src);
        AS.FCVT_L_S(dst, ft0);
    }

    rec.setOperandGPR(&operands[0], dst);
}

FAST_HANDLE(CVTSD2SI) {
    x86_size_e gpr_size = rec.getOperandSize(&operands[1]);
    biscuit::Vec src = rec.getOperandVec(&operands[1]);
    biscuit::GPR dst = rec.getOperandGPR(&operands[0]);

    if (gpr_size == X86_SIZE_DWORD) {
        rec.setVectorState(SEW::E64, 1);
        AS.VFMV_FS(ft0, src);
        AS.FCVT_W_D(dst, ft0);
    } else {
        rec.setVectorState(SEW::E64, 1);
        AS.VFMV_FS(ft0, src);
        AS.FCVT_L_D(dst, ft0);
    }

    rec.setOperandGPR(&operands[0], dst);
}

FAST_HANDLE(CVTSS2SD) {
    biscuit::Vec dst = rec.getOperandVec(&operands[0]);
    biscuit::Vec src = rec.getOperandVec(&operands[1]);

    rec.setVectorState(SEW::E32, 1);
    AS.VFMV_FS(ft0, src);
    AS.FCVT_D_S(ft1, ft0);
    rec.setVectorState(SEW::E64, 1);
    AS.VFMV_SF(dst, ft1);

    rec.setOperandVec(&operands[0], dst);
}

FAST_HANDLE(CVTSD2SS) {
    biscuit::Vec dst = rec.getOperandVec(&operands[0]);
    biscuit::Vec src = rec.getOperandVec(&operands[1]);

    rec.setVectorState(SEW::E64, 1);
    AS.VFMV_FS(ft0, src);
    AS.FCVT_S_D(ft1, ft0);
    rec.setVectorState(SEW::E32, 1);
    AS.VFMV_SF(dst, ft1);

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

FAST_HANDLE(MOVLHPS) {
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
}

FAST_HANDLE(FXSAVE64) {
    biscuit::GPR address = rec.lea(&operands[0]);
    rec.writebackDirtyState();

    AS.LI(t0, (u64)&felix86_fxsave);

    AS.MV(a0, rec.threadStatePointer());
    AS.MV(a1, address);
    AS.LI(a2, 1);
    AS.JALR(t0);
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
}

FAST_HANDLE(WRFSBASE) {
    biscuit::GPR reg = rec.getOperandGPR(&operands[0]);

    if (instruction.operand_width == 32) {
        AS.SW(reg, offsetof(ThreadState, fsbase), rec.threadStatePointer());
    } else {
        AS.SD(reg, offsetof(ThreadState, fsbase), rec.threadStatePointer());
    }
}

FAST_HANDLE(XADD) {
    biscuit::GPR result = rec.scratch();
    biscuit::GPR dst;
    biscuit::GPR src = rec.getOperandGPR(&operands[1]);
    bool needs_atomic = operands[0].type == ZYDIS_OPERAND_TYPE_MEMORY && (instruction.attributes & ZYDIS_ATTRIB_HAS_LOCK);
    if (!needs_atomic) {
        dst = rec.getOperandGPR(&operands[0]);
        AS.ADD(result, dst, src);
        rec.setOperandGPR(&operands[1], dst);
    } else {
        // Unlikely a smaller xadd is used, and risc-v doesn't support amoadd.b amoadd.h without a specific extension
        // and we'd have to emulate it with lr/sc so let's postpone it for now
        ASSERT(instruction.operand_width == 32 || instruction.operand_width == 64);

        // In this case the add+writeback needs to happen atomically
        biscuit::GPR address = rec.lea(&operands[0]);

        dst = rec.scratch();
        if (instruction.operand_width == 32) {
            AS.AMOADD_W(Ordering::AQRL, dst, src, address);
        } else {
            AS.AMOADD_D(Ordering::AQRL, dst, src, address);
        }

        // Still perform the addition in registers to calculate the flags
        // AMOADD stores the loaded value in Rd
        AS.ADD(result, dst, src);
        rec.setOperandGPR(&operands[1], dst);
        rec.popScratch(); // pop LEA scratch
        rec.popScratch();
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
        rec.popScratch();
    }

    // In this case we also need to writeback the result, otherwise amoadd will do it for us
    if (!needs_atomic) {
        rec.setOperandGPR(&operands[0], result);
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

FAST_HANDLE(CMPSD_sse) {
    u8 imm = rec.getImmediate(&operands[2]) & 0b111;
    biscuit::Vec dst = rec.getOperandVec(&operands[0]);
    biscuit::Vec src = rec.getOperandVec(&operands[1]);

    rec.setVectorState(SEW::E64, 1);
    AS.VFMV_FS(ft0, dst);
    AS.VFMV_FS(ft1, src);

    biscuit::GPR result = rec.scratch();
    switch ((CmpPredicate)imm) {
    case EQ_OQ: {
        AS.FEQ_D(result, ft0, ft1);
        break;
    }
    case LT_OS: {
        AS.FLT_D(result, ft0, ft1);
        break;
    }
    case LE_OS: {
        AS.FLE_D(result, ft0, ft1);
        break;
    }
    case UNORD_Q: {
        // Check if it's a qNan or sNan, check bit 8 and 9
        biscuit::GPR nan = rec.scratch();
        biscuit::GPR mask = rec.scratch();
        AS.FCLASS_D(result, ft0);
        AS.FCLASS_D(nan, ft1);
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
        AS.FCLASS_D(result, ft0);
        AS.FCLASS_D(nan, ft1);
        AS.OR(result, result, nan);
        AS.LI(mask, 0b11 << 8);
        AS.AND(result, result, mask);
        AS.SNEZ(result, result);
        rec.popScratch();
        rec.popScratch();

        // After checking if either are nan, also check if they are equal
        AS.FEQ_D(nan, ft0, ft1);
        AS.XORI(nan, nan, 1);
        AS.OR(result, result, nan);
        break;
    }
    case NLT_US: {
        biscuit::GPR nan = rec.scratch();
        biscuit::GPR mask = rec.scratch();
        AS.FCLASS_D(result, ft0);
        AS.FCLASS_D(nan, ft1);
        AS.OR(result, result, nan);
        AS.LI(mask, 0b11 << 8);
        AS.AND(result, result, mask);
        AS.SNEZ(result, result);
        rec.popScratch();
        rec.popScratch();

        // After checking if either are nan, also check if they are equal
        AS.FLT_D(nan, ft0, ft1);
        AS.XORI(nan, nan, 1);
        AS.OR(result, result, nan);
        break;
    }
    case NLE_US: {
        biscuit::GPR nan = rec.scratch();
        biscuit::GPR mask = rec.scratch();
        AS.FCLASS_D(result, ft0);
        AS.FCLASS_D(nan, ft1);
        AS.OR(result, result, nan);
        AS.LI(mask, 0b11 << 8);
        AS.AND(result, result, mask);
        AS.SNEZ(result, result);
        rec.popScratch();
        rec.popScratch();

        // After checking if either are nan, also check if they are equal
        AS.FLE_D(nan, ft0, ft1);
        AS.XORI(nan, nan, 1);
        AS.OR(result, result, nan);
        break;
    }
    case ORD_Q: {
        // Check if neither are NaN
        biscuit::GPR nan = rec.scratch();
        biscuit::GPR mask = rec.scratch();
        AS.FCLASS_D(result, ft0);
        AS.FCLASS_D(nan, ft1);
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
    } else {
        ERROR("Unimplemented: cmpsd (the string one)");
    }
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