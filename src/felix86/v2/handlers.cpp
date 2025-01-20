#include <Zydis/Zydis.h>
#include "felix86/v2/fast_recompiler.hpp"

#define FAST_HANDLE(name)                                                                                                                            \
    void fast_##name(FastRecompiler& rec, const HandlerMetadata& meta, ZydisDecodedInstruction& instruction, ZydisDecodedOperand* operands)

#define AS (rec.getAssembler())

#define IS_MMX (instruction.attributes & (ZYDIS_ATTRIB_FPU_STATE_CR | ZYDIS_ATTRIB_FPU_STATE_CW))

#define HAS_REP (instruction.attributes & (ZYDIS_ATTRIB_HAS_REP | ZYDIS_ATTRIB_HAS_REPZ | ZYDIS_ATTRIB_HAS_REPNZ))

FAST_HANDLE(MOV) {
    biscuit::GPR src = rec.getOperandGPRDontZext(&operands[1]);
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
        rec.updateParity(result);
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
        biscuit::GPR cf = rec.flagW(X86_REF_CF);
        AS.SLTU(cf, dst, src);
        AS.SLTU(scratch, result, cf);
        AS.OR(cf, cf, scratch);
        rec.popScratch();
    }

    if (rec.shouldEmitFlag(meta.rip, X86_REF_ZF)) {
        rec.updateZero(result);
    }

    if (rec.shouldEmitFlag(meta.rip, X86_REF_SF)) {
        rec.updateSign(result, size);
    }

    rec.setOperandGPR(&operands[0], result_2);
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
    biscuit::GPR src = rec.getOperandGPRDontZext(&operands[1]);
    biscuit::GPR dst = rec.getOperandGPRDontZext(&operands[0]);

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
        rec.updateZero(result);
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
            rec.updateZero(x0);
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
    biscuit::GPR src = rec.getOperandGPRDontZext(&operands[1]);
    biscuit::GPR dst = rec.getOperandGPRDontZext(&operands[0]);

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
        rec.updateZero(result);
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
    biscuit::GPR src = rec.getOperandGPRDontZext(&operands[1]);
    biscuit::GPR dst = rec.getOperandGPRDontZext(&operands[0]);

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
        rec.updateZero(result);
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
        u64 displacement = rec.sextImmediate(operands[0].imm.value.u, operands[0].imm.size);
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
    biscuit::GPR dst = rec.getOperandGPR(&operands[0]);
    biscuit::GPR rsp = rec.getRefGPR(X86_REF_RSP, X86_SIZE_QWORD);

    if (instruction.operand_width == 16) {
        AS.LHU(dst, 0, rsp);
    } else {
        AS.LD(dst, 0, rsp);
    }

    int imm = instruction.operand_width == 16 ? 2 : 8;
    AS.ADDI(rsp, rsp, imm);
    rec.setRefGPR(X86_REF_RSP, X86_SIZE_QWORD, rsp);
    rec.setOperandGPR(&operands[0], dst);
}

FAST_HANDLE(NOP) {}

FAST_HANDLE(ENDBR64) {}

FAST_HANDLE(SHL) {
    biscuit::GPR result = rec.scratch();
    x86_size_e size = rec.getOperandSize(&operands[0]);
    biscuit::GPR dst = rec.getOperandGPR(&operands[0]);
    biscuit::GPR src = rec.getOperandGPR(&operands[1]);

    if (instruction.operand_width == 64) {
        AS.ANDI(src, src, 0x3F);
    } else {
        AS.ANDI(src, src, 0x1F);
    }

    Label zero_source;

    AS.BEQZ(src, &zero_source);

    AS.SLL(result, dst, src);

    if (rec.shouldEmitFlag(meta.rip, X86_REF_PF)) {
        rec.updateParity(result);
    }

    if (rec.shouldEmitFlag(meta.rip, X86_REF_ZF)) {
        rec.updateZero(result);
    }

    if (rec.shouldEmitFlag(meta.rip, X86_REF_SF)) {
        rec.updateSign(result, size);
    }

    if (rec.shouldEmitFlag(meta.rip, X86_REF_CF)) {
        biscuit::GPR cf = rec.flagW(X86_REF_CF);
        AS.LI(cf, rec.getBitSize(size));
        AS.SUB(cf, cf, src);
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

    if (instruction.operand_width == 64) {
        AS.ANDI(src, src, 0x3F);
    } else {
        AS.ANDI(src, src, 0x1F);
    }

    Label zero_source;

    AS.BEQZ(src, &zero_source);

    AS.SRL(result, dst, src);

    if (rec.shouldEmitFlag(meta.rip, X86_REF_PF)) {
        rec.updateParity(result);
    }

    if (rec.shouldEmitFlag(meta.rip, X86_REF_ZF)) {
        rec.updateZero(result);
    }

    if (rec.shouldEmitFlag(meta.rip, X86_REF_SF)) {
        rec.updateSign(result, size);
    }

    if (rec.shouldEmitFlag(meta.rip, X86_REF_CF)) {
        biscuit::GPR cf = rec.flagW(X86_REF_CF);
        AS.ADDI(cf, src, -1);
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

    if (instruction.operand_width == 64) {
        AS.ANDI(src, src, 0x3F);
    } else {
        AS.ANDI(src, src, 0x1F);
    }

    Label zero_source;

    AS.BEQZ(src, &zero_source);

    switch (size) {
    case X86_SIZE_BYTE: {
        AS.SLLI(result, dst, 56);
        AS.SRAI(result, result, 56);
        AS.SRA(result, result, src);
        break;
    }
    case X86_SIZE_WORD: {
        AS.SLLI(result, dst, 48);
        AS.SRAI(result, result, 48);
        AS.SRA(result, result, src);
        break;
    }
    case X86_SIZE_DWORD: {
        AS.SRAW(result, dst, src);
        break;
    }
    case X86_SIZE_QWORD: {
        AS.SRA(result, dst, src);
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
        rec.updateZero(result);
    }

    if (rec.shouldEmitFlag(meta.rip, X86_REF_SF)) {
        rec.updateSign(result, size);
    }

    if (rec.shouldEmitFlag(meta.rip, X86_REF_CF)) {
        biscuit::GPR cf = rec.flagW(X86_REF_CF);
        AS.ADDI(cf, src, -1);
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

        AS.VMV_XS(dst, src);

        rec.setOperandGPR(&operands[0], dst);
    } else if (operands[1].type == ZYDIS_OPERAND_TYPE_MEMORY) {
        biscuit::GPR src = rec.getOperandGPR(&operands[1]);
        biscuit::Vec dst = rec.getOperandVec(&operands[0]);

        rec.setVectorState(SEW::E64, rec.maxVlen() / 64);
        AS.VMV(v0, 0b10);

        // Zero upper 64-bit (this will be useful for when we get to AVX)
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

            // Zero upper 64-bit (this will be useful for when we get to AVX)
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

        rec.setRefGPR(X86_REF_RAX, X86_SIZE_BYTE, ax);
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
    rec.setFlagUndefined(X86_REF_PF);
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
        biscuit::GPR ax = rec.getRefGPR(X86_REF_RAX, X86_SIZE_WORD);
        biscuit::GPR ax_sext = rec.scratch();

        rec.sextb(ax_sext, ax);
        rec.sextb(ax, src);

        AS.REMW(mod, ax_sext, ax);
        AS.DIVW(ax, ax_sext, ax);

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

        AS.DIVW(ax, dx, src);
        AS.REMW(dx, dx, src);

        rec.setRefGPR(X86_REF_RAX, X86_SIZE_WORD, ax);
        rec.setRefGPR(X86_REF_RDX, X86_SIZE_WORD, dx);
        break;
    }
    case X86_SIZE_DWORD: {
        biscuit::GPR eax = rec.getRefGPR(X86_REF_RAX, X86_SIZE_DWORD);
        biscuit::GPR edx = rec.getRefGPR(X86_REF_RDX, X86_SIZE_QWORD);
        AS.SLLI(edx, edx, 32);
        AS.OR(edx, edx, eax);

        AS.DIV(eax, edx, src);
        AS.REM(edx, edx, src);

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
    rec.setFlagUndefined(X86_REF_PF);
    rec.setFlagUndefined(X86_REF_AF);
    rec.setFlagUndefined(X86_REF_ZF);
    rec.setFlagUndefined(X86_REF_SF);
    rec.setFlagUndefined(X86_REF_OF);
}

FAST_HANDLE(TEST) {
    biscuit::GPR result = rec.scratch();

    biscuit::GPR src = rec.getOperandGPRDontZext(&operands[1]);
    biscuit::GPR dst = rec.getOperandGPRDontZext(&operands[0]);

    if (dst == src) {
        result = dst;
    } else {
        AS.AND(result, dst, src);
    }

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
        rec.updateZero(result);
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

    AS.ADDI(dst, dst, 1);

    if (rec.shouldEmitFlag(meta.rip, X86_REF_AF)) {
        biscuit::GPR af = rec.flagW(X86_REF_AF);
        AS.ANDI(af, dst, 0xF);
        AS.SEQZ(af, af);
    }

    if (rec.shouldEmitFlag(meta.rip, X86_REF_OF)) {
        biscuit::GPR of = rec.flagW(X86_REF_OF);
        rec.zext(of, dst, size);
        AS.SEQZ(of, of);
    }

    if (rec.shouldEmitFlag(meta.rip, X86_REF_PF)) {
        rec.updateParity(dst);
    }

    if (rec.shouldEmitFlag(meta.rip, X86_REF_ZF)) {
        rec.updateZero(dst);
    }

    if (rec.shouldEmitFlag(meta.rip, X86_REF_SF)) {
        rec.updateSign(dst, size);
    }

    rec.setOperandGPR(&operands[0], dst);
}

FAST_HANDLE(DEC) {
    x86_size_e size = rec.getOperandSize(&operands[0]);
    biscuit::GPR dst = rec.getOperandGPR(&operands[0]);

    if (rec.shouldEmitFlag(meta.rip, X86_REF_AF)) {
        biscuit::GPR af = rec.flagW(X86_REF_AF);
        AS.ANDI(af, dst, 0xF);
        AS.SEQZ(af, af);
    }

    if (rec.shouldEmitFlag(meta.rip, X86_REF_OF)) {
        biscuit::GPR of = rec.flagW(X86_REF_OF);
        AS.SEQZ(of, dst);
    }

    AS.ADDI(dst, dst, -1);

    if (rec.shouldEmitFlag(meta.rip, X86_REF_PF)) {
        rec.updateParity(dst);
    }

    if (rec.shouldEmitFlag(meta.rip, X86_REF_ZF)) {
        rec.updateZero(dst);
    }

    if (rec.shouldEmitFlag(meta.rip, X86_REF_SF)) {
        rec.updateSign(dst, size);
    }

    rec.setOperandGPR(&operands[0], dst);
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
    biscuit::GPR eax = rec.getRefGPR(X86_REF_RAX, X86_SIZE_DWORD);
    AS.ADDIW(eax, eax, 0);
    rec.setRefGPR(X86_REF_RAX, X86_SIZE_QWORD, eax);
}

FAST_HANDLE(CWD) {
    biscuit::GPR sext = rec.scratch();
    biscuit::GPR ax = rec.getRefGPR(X86_REF_RAX, X86_SIZE_WORD);
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

void JCC(FastRecompiler& rec, const HandlerMetadata& meta, ZydisDecodedInstruction& instruction, ZydisDecodedOperand* operands, biscuit::GPR cond) {
    u64 immediate = rec.sextImmediate(operands[0].imm.value.u, operands[0].imm.size);
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

void CMOV(FastRecompiler& rec, const HandlerMetadata& meta, ZydisDecodedInstruction& instruction, ZydisDecodedOperand* operands, biscuit::GPR cond) {
    biscuit::GPR dst = rec.getOperandGPR(&operands[0]);
    biscuit::GPR src = rec.getOperandGPR(&operands[1]);

    if (Extensions::Xtheadcondmov) {
        AS.TH_MVNEZ(dst, src, cond);
    } else if (Extensions::Zicond) {
        biscuit::GPR tmp = rec.scratch();
        AS.CZERO_NEZ(tmp, dst, cond);
        AS.CZERO_EQZ(dst, src, cond);
        AS.OR(dst, dst, tmp);
    } else {
        Label false_label;
        AS.BEQZ(cond, &false_label);
        AS.MV(dst, src);
        AS.Bind(&false_label);
    }

    rec.setOperandGPR(&operands[0], dst);
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
    x86_size_e size = rec.getOperandSize(&operands[0]);
    biscuit::GPR src = rec.getOperandGPR(&operands[1]);

    if (size == X86_SIZE_QWORD) {
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
            biscuit::GPR al = rec.getRefGPR(X86_REF_RAX, X86_SIZE_BYTE);
            biscuit::GPR sext = rec.scratch();
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
            biscuit::GPR ax = rec.getRefGPR(X86_REF_RAX, X86_SIZE_WORD);
            AS.MULW(result, ax, src);
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
            biscuit::GPR eax = rec.getRefGPR(X86_REF_RAX, X86_SIZE_DWORD);
            AS.MUL(result, eax, src);
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
            rec.sexth(dst, src1);
            rec.sexth(result, src2);
            AS.MULW(result, result, dst);
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
            AS.ADDIW(dst, src1, 0);
            AS.ADDIW(result, src2, 0);
            AS.MUL(result, result, dst);
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

void PUNPCKL(FastRecompiler& rec, const HandlerMetadata& meta, ZydisDecodedInstruction& instruction, ZydisDecodedOperand* operands, SEW sew,
             u8 vlen) {
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
    AS.VRGATHER(result, src, iota, VecMask::Yes);

    AS.VSRL(v0, v0, 1);
    AS.VIOTA(iota, v0);
    AS.VRGATHER(result, dst, iota, VecMask::Yes);

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
    biscuit::Vec src_not = rec.scratchVec();
    biscuit::Vec dst = rec.getOperandVec(&operands[0]);
    biscuit::Vec src = rec.getOperandVec(&operands[1]);
    rec.setVectorState(SEW::E64, rec.maxVlen() / 64);
    AS.VXOR(src_not, src, -1);
    AS.VAND(dst, dst, src_not);
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

void PADD(FastRecompiler& rec, const HandlerMetadata& meta, ZydisDecodedInstruction& instruction, ZydisDecodedOperand* operands, SEW sew, u8 vlen) {
    biscuit::Vec dst = rec.getOperandVec(&operands[0]);
    biscuit::Vec src = rec.getOperandVec(&operands[1]);
    rec.setVectorState(sew, vlen);
    AS.VADD(dst, dst, src);
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

FAST_HANDLE(MOVSD) {
    fast_MOVSB(rec, meta, instruction, operands);
}

FAST_HANDLE(MOVSQ) {
    fast_MOVSB(rec, meta, instruction, operands);
}

FAST_HANDLE(STOSB) {
    Label loop_end, loop_body;
    if (HAS_REP) {
        rec.repPrologue(&loop_end);
        AS.Bind(&loop_body);
    }

    u8 width = instruction.operand_width;
    biscuit::GPR rdi = rec.getRefGPR(X86_REF_RDI, X86_SIZE_QWORD);
    biscuit::GPR rax = rec.getRefGPR(X86_REF_RAX, rec.zydisToSize(width));
    rec.writeMemory(rax, rdi, 0, rec.zydisToSize(width));

    biscuit::GPR temp = rec.scratch();
    AS.LB(temp, offsetof(ThreadState, df), rec.threadStatePointer());

    Label end, false_label;

    AS.BEQZ(temp, &false_label);

    AS.ADDI(rdi, rdi, -width / 8);
    AS.J(&end);

    AS.Bind(&false_label);

    AS.ADDI(rdi, rdi, width / 8);
    AS.Bind(&end);

    rec.setRefGPR(X86_REF_RDI, X86_SIZE_QWORD, rdi);

    if (HAS_REP) {
        rec.repEpilogue(&loop_body);
        AS.Bind(&loop_end);
    }
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

FAST_HANDLE(SHUFPD) {
    u8 imm = operands[2].imm.value.u;
    biscuit::GPR temp = rec.scratch();
    biscuit::Vec vtemp = rec.scratchVec();
    biscuit::Vec dst = rec.getOperandVec(&operands[0]);
    biscuit::Vec src = rec.getOperandVec(&operands[1]);

    rec.setVectorState(SEW::E64, rec.maxVlen() / 64);

    if ((imm & 1) == 0) {
        AS.VMV_XS(temp, src);
    } else {
        AS.VSLIDEDOWN(vtemp, src, 1);
        AS.VMV_XS(temp, vtemp);
    }

    if ((imm & 0b10) != 0) {
        AS.VSLIDEDOWN(dst, dst, 1);
    }

    AS.VSLIDE1UP(vtemp, dst, temp);

    rec.setOperandVec(&operands[0], vtemp);
}

FAST_HANDLE(LEAVE) {
    x86_size_e size = rec.zydisToSize(instruction.operand_width);
    biscuit::GPR rbp = rec.getRefGPR(X86_REF_RBP, X86_SIZE_QWORD);
    AS.ADDI(rbp, rbp, 8);
    rec.setRefGPR(X86_REF_RSP, X86_SIZE_QWORD, rbp);
    rec.readMemory(rbp, rbp, -8, size);
    rec.setRefGPR(X86_REF_RBP, X86_SIZE_QWORD, rbp);
}

void SETCC(FastRecompiler& rec, const HandlerMetadata& meta, ZydisDecodedInstruction& instruction, ZydisDecodedOperand* operands, biscuit::GPR cond) {
    biscuit::GPR dst = rec.allocatedGPR(rec.zydisToRef(operands[0].reg.value));
    rec.setOperandGPR(&operands[0], dst);
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
    biscuit::GPR dst = rec.getOperandGPRDontZext(&operands[0]);
    AS.NOT(dst, dst);
    rec.setOperandGPR(&operands[0], dst);
}

FAST_HANDLE(NEG) {
    x86_size_e size = rec.getOperandSize(&operands[0]);
    biscuit::GPR dst = rec.getOperandGPRDontZext(&operands[0]);
    if (size == X86_SIZE_BYTE) {
        rec.sextb(dst, dst);
        AS.NEG(dst, dst);
    } else if (size == X86_SIZE_WORD) {
        rec.sexth(dst, dst);
        AS.NEG(dst, dst);
    } else if (size == X86_SIZE_DWORD) {
        AS.SUBW(dst, x0, dst);
    } else if (size == X86_SIZE_QWORD) {
        AS.NEG(dst, dst);
    } else {
        UNREACHABLE();
    }
    rec.setOperandGPR(&operands[0], dst);
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