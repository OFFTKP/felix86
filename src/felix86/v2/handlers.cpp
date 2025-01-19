#include <Zydis/Zydis.h>
#include "felix86/v2/fast_recompiler.hpp"

#define FAST_HANDLE(name)                                                                                                                            \
    void fast_##name(FastRecompiler& rec, const HandlerMetadata& meta, ZydisDecodedInstruction& instruction, ZydisDecodedOperand* operands)

#define AS (rec.getAssembler())

#define IS_MMX (instruction.attributes & (ZYDIS_ATTRIB_FPU_STATE_CR | ZYDIS_ATTRIB_FPU_STATE_CW))

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
    biscuit::GPR src = rec.getOperandGPR(&operands[1]);
    biscuit::GPR dst = rec.getOperandGPR(&operands[0]);

    AS.XOR(result, dst, src);

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

    AS.BNEZ(src, &zero_source);

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

    AS.BNEZ(src, &zero_source);

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

    AS.BNEZ(src, &zero_source);

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
    if (instruction.opcode == 0x6E) {
        biscuit::GPR src = rec.getOperandGPR(&operands[1]);
        biscuit::Vec dst = rec.getOperandVec(&operands[0]);

        rec.setVectorState(SEW::E64, rec.maxVlen() / 64);
        AS.VMV(v0, 0b10);

        // Zero upper 64-bit (this will be useful for when we get to AVX)
        AS.VXOR(dst, dst, dst, VecMask::Yes);
        AS.VMV_SX(dst, src);

        rec.setOperandVec(&operands[0], dst);
    } else if (instruction.opcode == 0x7E) {
        biscuit::GPR dst = rec.scratch();
        biscuit::Vec src = rec.getOperandVec(&operands[1]);

        rec.setVectorState(SEW::E64, rec.maxVlen() / 64);
        AS.VMV_XS(dst, src);

        rec.setOperandGPR(&operands[0], dst);
    } else {
        UNREACHABLE();
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

FAST_HANDLE(TEST) {
    biscuit::GPR result = rec.scratch();
    biscuit::GPR src = rec.getOperandGPR(&operands[1]);
    biscuit::GPR dst = rec.getOperandGPR(&operands[0]);

    if (dst == src) {
        result = dst;
    } else {
        AS.AND(result, dst, src);
    }

    x86_size_e size = rec.getOperandSize(&operands[0]);

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

FAST_HANDLE(XCHG) {
    biscuit::GPR temp = rec.scratch();
    biscuit::GPR src = rec.getOperandGPR(&operands[1]);
    biscuit::GPR dst = rec.getOperandGPR(&operands[0]);

    AS.MV(temp, src);

    rec.setOperandGPR(&operands[1], dst);
    rec.setOperandGPR(&operands[0], temp);
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

FAST_HANDLE(JNL) {
    biscuit::GPR cond = rec.scratch();
    biscuit::GPR sf = rec.flag(X86_REF_SF);
    biscuit::GPR of = rec.flag(X86_REF_OF);
    AS.SUB(cond, sf, of);
    AS.SEQZ(cond, cond);

    JCC(rec, meta, instruction, operands, cond);
}

FAST_HANDLE(JNLE) {
    biscuit::GPR cond = rec.scratch();
    biscuit::GPR sf = rec.flag(X86_REF_SF);
    biscuit::GPR of = rec.flag(X86_REF_OF);
    biscuit::GPR zf = rec.flag(X86_REF_ZF);
    AS.XOR(cond, sf, of);
    AS.OR(cond, cond, zf);
    AS.XORI(cond, cond, 1);

    JCC(rec, meta, instruction, operands, cond);
}

FAST_HANDLE(JO) {
    biscuit::GPR of = rec.flag(X86_REF_OF);

    JCC(rec, meta, instruction, operands, of);
}

FAST_HANDLE(JNO) {
    biscuit::GPR cond = rec.scratch();
    biscuit::GPR of = rec.flag(X86_REF_OF);
    AS.XORI(cond, of, 1);

    JCC(rec, meta, instruction, operands, cond);
}

FAST_HANDLE(JLE) {
    biscuit::GPR cond = rec.scratch();
    biscuit::GPR sf = rec.flag(X86_REF_SF);
    biscuit::GPR of = rec.flag(X86_REF_OF);
    biscuit::GPR zf = rec.flag(X86_REF_ZF);
    AS.XOR(cond, sf, of);
    AS.OR(cond, cond, zf);

    JCC(rec, meta, instruction, operands, cond);
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
        rec.setFlagUndefined(X86_REF_PF);
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
        rec.setFlagUndefined(X86_REF_PF);
        rec.setFlagUndefined(X86_REF_ZF);
        rec.setFlagUndefined(X86_REF_SF);
    } else {
        UNREACHABLE();
    }
}