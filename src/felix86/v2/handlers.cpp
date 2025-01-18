#include <Zydis/Zydis.h>
#include "felix86/v2/fast_recompiler.hpp"

#define FAST_HANDLE(name)                                                                                                                            \
    void fast_##name(FastRecompiler& rec, const HandlerMetadata& meta, ZydisDecodedInstruction& instruction, ZydisDecodedOperand* operands)

#define AS (rec.getAssembler())

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
        rec.updateZero(result);
    }

    if (rec.shouldEmitFlag(meta.rip, X86_REF_SF)) {
        rec.updateSign(result, size);
    }

    if (rec.shouldEmitFlag(meta.rip, X86_REF_OF)) {
        biscuit::GPR scratch = rec.scratch();
        biscuit::GPR of = rec.flagW(X86_REF_OF);
        AS.XOR(scratch, result, dst);
        AS.XOR(of, result, src);
        AS.AND(of, of, scratch);
        AS.LI(scratch, sign_mask);
        AS.AND(of, of, scratch);
        AS.SNEZ(of, of);
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
        rec.updateZero(result);
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

    rec.setOperandGPR(&operands[0], result);
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
        rec.updateZero(result);
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

    if (rec.shouldEmitFlag(meta.rip, X86_REF_CF)) {
        biscuit::GPR cf = rec.flagW(X86_REF_CF);
        AS.LI(cf, 0);
    }

    if (rec.shouldEmitFlag(meta.rip, X86_REF_PF)) {
        rec.updateParity(result);
    }

    if (rec.shouldEmitFlag(meta.rip, X86_REF_ZF)) {
        rec.updateZero(result);
    }

    if (rec.shouldEmitFlag(meta.rip, X86_REF_SF)) {
        rec.updateSign(result, size);
    }

    if (rec.shouldEmitFlag(meta.rip, X86_REF_OF)) {
        biscuit::GPR of = rec.flagW(X86_REF_OF);
        AS.LI(of, 0);
    }

    rec.setOperandGPR(&operands[0], result);
}

FAST_HANDLE(XOR) {
    biscuit::GPR result = rec.scratch();
    biscuit::GPR src = rec.getOperandGPR(&operands[1]);
    biscuit::GPR dst = rec.getOperandGPR(&operands[0]);

    AS.XOR(result, dst, src);

    x86_size_e size = rec.getOperandSize(&operands[0]);

    if (rec.shouldEmitFlag(meta.rip, X86_REF_CF)) {
        biscuit::GPR cf = rec.flagW(X86_REF_CF);
        AS.LI(cf, 0);
    }

    if (rec.shouldEmitFlag(meta.rip, X86_REF_PF)) {
        rec.updateParity(result);
    }

    if (rec.shouldEmitFlag(meta.rip, X86_REF_ZF)) {
        rec.updateZero(result);
    }

    if (rec.shouldEmitFlag(meta.rip, X86_REF_SF)) {
        rec.updateSign(result, size);
    }

    if (rec.shouldEmitFlag(meta.rip, X86_REF_OF)) {
        biscuit::GPR of = rec.flagW(X86_REF_OF);
        AS.LI(of, 0);
    }

    rec.setOperandGPR(&operands[0], result);
}

FAST_HANDLE(AND) {
    biscuit::GPR result = rec.scratch();
    biscuit::GPR src = rec.getOperandGPR(&operands[1]);
    biscuit::GPR dst = rec.getOperandGPR(&operands[0]);

    AS.AND(result, dst, src);

    x86_size_e size = rec.getOperandSize(&operands[0]);

    if (rec.shouldEmitFlag(meta.rip, X86_REF_CF)) {
        biscuit::GPR cf = rec.flagW(X86_REF_CF);
        AS.LI(cf, 0);
    }

    if (rec.shouldEmitFlag(meta.rip, X86_REF_PF)) {
        rec.updateParity(result);
    }

    if (rec.shouldEmitFlag(meta.rip, X86_REF_ZF)) {
        rec.updateZero(result);
    }

    if (rec.shouldEmitFlag(meta.rip, X86_REF_SF)) {
        rec.updateSign(result, size);
    }

    if (rec.shouldEmitFlag(meta.rip, X86_REF_OF)) {
        biscuit::GPR of = rec.flagW(X86_REF_OF);
        AS.LI(of, 0);
    }

    rec.setOperandGPR(&operands[0], result);
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
        u64 displacement = (i64)(i32)operands[0].imm.value.u;
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
        imm += operands[0].imm.value.u;
    }

    if (imm >= 2048) {
        biscuit::GPR scratch2 = rec.scratch();
        AS.LI(scratch2, imm);
        AS.ADD(rsp, rsp, scratch2);
        rec.popScratch();
    } else {
        AS.ADDI(rsp, rsp, imm);
    }

    rec.setRefGPR(X86_REF_RSP, X86_SIZE_QWORD, rsp);
    rec.setRip(scratch);
    rec.writebackDirtyState();
    rec.backToDispatcher();
    rec.stopCompiling();
}

FAST_HANDLE(TEST) {
    biscuit::GPR result = rec.scratch();
    biscuit::GPR src = rec.getOperandGPR(&operands[1]);
    biscuit::GPR dst = rec.getOperandGPR(&operands[0]);

    AS.AND(result, dst, src);

    x86_size_e size = rec.getOperandSize(&operands[0]);

    if (rec.shouldEmitFlag(meta.rip, X86_REF_CF)) {
        biscuit::GPR cf = rec.flagW(X86_REF_CF);
        AS.LI(cf, 0);
    }

    if (rec.shouldEmitFlag(meta.rip, X86_REF_PF)) {
        rec.updateParity(result);
    }

    if (rec.shouldEmitFlag(meta.rip, X86_REF_ZF)) {
        rec.updateZero(result);
    }

    if (rec.shouldEmitFlag(meta.rip, X86_REF_SF)) {
        rec.updateSign(result, size);
    }

    if (rec.shouldEmitFlag(meta.rip, X86_REF_OF)) {
        biscuit::GPR of = rec.flagW(X86_REF_OF);
        AS.LI(of, 0);
    }
}

FAST_HANDLE(PUSH) {
    x86_size_e size = rec.getOperandSize(&operands[0]);
    biscuit::GPR src = rec.getOperandGPR(&operands[0]);
    biscuit::GPR rsp = rec.getRefGPR(X86_REF_RSP, X86_SIZE_QWORD);
    int imm = size == X86_SIZE_WORD ? -2 : -8;
    AS.ADDI(rsp, rsp, imm);
    rec.setRefGPR(X86_REF_RSP, X86_SIZE_QWORD, rsp);

    if (size == X86_SIZE_WORD) {
        AS.SW(src, 0, rsp);
    } else {
        AS.SD(src, 0, rsp);
    }
}

FAST_HANDLE(POP) {
    x86_size_e size = rec.getOperandSize(&operands[0]);
    biscuit::GPR dst = rec.getOperandGPR(&operands[0]);
    biscuit::GPR rsp = rec.getRefGPR(X86_REF_RSP, X86_SIZE_QWORD);

    if (size == X86_SIZE_WORD) {
        AS.LW(dst, 0, rsp);
    } else {
        AS.LD(dst, 0, rsp);
    }

    int imm = size == X86_SIZE_WORD ? 2 : 8;
    AS.ADDI(rsp, rsp, imm);
    rec.setRefGPR(X86_REF_RSP, X86_SIZE_QWORD, rsp);
}

FAST_HANDLE(NOP) {}

FAST_HANDLE(SHR) {
    x86_size_e size = rec.getOperandSize(&operands[0]);
    biscuit::GPR result = rec.scratch();
    biscuit::GPR dst = rec.getOperandGPR(&operands[0]);

    switch (operands[1].type) {
    case ZYDIS_OPERAND_TYPE_IMMEDIATE: {
        u8 imm = operands[1].imm.value.u;
        AS.SRLI(result, dst, imm);

        if (rec.shouldEmitFlag(meta.rip, X86_REF_CF)) {
            biscuit::GPR cf = rec.flagW(X86_REF_CF);
            if (imm == 0) {
                AS.LI(cf, 0);
            } else {
                AS.SRLI(cf, dst, imm - 1);
                AS.ANDI(cf, cf, 1);
            }
        }
        break;
    }
    case ZYDIS_OPERAND_TYPE_REGISTER: {
        biscuit::GPR src = rec.getOperandGPR(&operands[1]);
        AS.SRL(result, dst, src);

        if (rec.shouldEmitFlag(meta.rip, X86_REF_CF)) {
            Label not_zero, end;
            biscuit::GPR cf = rec.flagW(X86_REF_CF);
            AS.SEQZ(cf, src);
            AS.BEQZ(cf, &not_zero);
            AS.LI(cf, 0);
            AS.J(&end);

            AS.Bind(&not_zero);
            AS.ADDI(cf, src, -1);
            AS.SRL(cf, dst, cf);
            AS.ANDI(cf, cf, 1);

            AS.Bind(&end);
        }
        break;
    }
    default: {
        UNREACHABLE();
        break;
    }
    }

    if (rec.shouldEmitFlag(meta.rip, X86_REF_OF)) {
        biscuit::GPR of = rec.flagW(X86_REF_OF);
        AS.SRLI(of, dst, rec.getBitSize(size) - 1);
        AS.ANDI(of, of, 1);
    }

    rec.setOperandGPR(&operands[0], result);
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
        u64 displacement = rec.sextImmediate(operands[0].imm.value.u, operands[0].imm.size);
        biscuit::GPR scratch = rec.getRip();
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

void JCC(FastRecompiler& rec, const HandlerMetadata& meta, ZydisDecodedInstruction& instruction, ZydisDecodedOperand* operands, biscuit::GPR cond) {
    u64 immediate = rec.sextImmediate(operands[0].imm.value.u, operands[0].imm.size);
    u64 address_false = meta.rip - meta.block_start + instruction.length;
    u64 address_true = address_false + immediate;

    biscuit::GPR rip_true = rec.getRip();
    biscuit::GPR rip_false = rec.scratch();

    rec.addi(rip_false, rip_true, address_false);
    rec.addi(rip_true, rip_false, immediate);

    rec.writebackDirtyState();
    rec.jumpAndLinkConditional(cond, rip_true, rip_false, address_true, address_false);
    rec.stopCompiling();
}

FAST_HANDLE(JBE) {
    biscuit::GPR cond = rec.scratch();
    biscuit::GPR cf = rec.flag(X86_REF_CF);
    biscuit::GPR zf = rec.flag(X86_REF_ZF);
    AS.OR(cond, cf, zf);

    JCC(rec, meta, instruction, operands, cond);
}

FAST_HANDLE(JNBE) {
    biscuit::GPR cond = rec.scratch();
    biscuit::GPR cf = rec.flag(X86_REF_CF);
    biscuit::GPR zf = rec.flag(X86_REF_ZF);
    AS.OR(cond, cf, zf);
    AS.XORI(cond, cond, 1);

    JCC(rec, meta, instruction, operands, cond);
}

FAST_HANDLE(JNZ) {
    biscuit::GPR cond = rec.scratch();
    biscuit::GPR zf = rec.flag(X86_REF_ZF);
    AS.XORI(cond, zf, 1);

    JCC(rec, meta, instruction, operands, cond);
}

FAST_HANDLE(JZ) {
    biscuit::GPR zf = rec.flag(X86_REF_ZF);

    JCC(rec, meta, instruction, operands, zf);
}

FAST_HANDLE(JB) {
    biscuit::GPR cf = rec.flag(X86_REF_CF);

    JCC(rec, meta, instruction, operands, cf);
}

FAST_HANDLE(JNB) {
    biscuit::GPR cond = rec.scratch();
    biscuit::GPR cf = rec.flag(X86_REF_CF);
    AS.XORI(cond, cf, 1);

    JCC(rec, meta, instruction, operands, cond);
}

FAST_HANDLE(JP) {
    biscuit::GPR pf = rec.flag(X86_REF_PF);

    JCC(rec, meta, instruction, operands, pf);
}

FAST_HANDLE(JNP) {
    biscuit::GPR cond = rec.scratch();
    biscuit::GPR pf = rec.flag(X86_REF_PF);
    AS.XORI(cond, pf, 1);

    JCC(rec, meta, instruction, operands, cond);
}
