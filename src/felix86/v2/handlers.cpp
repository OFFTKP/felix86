#include <Zydis/Zydis.h>
#include "felix86/v2/recompiler.hpp"

void felix86_syscall(ThreadState* state);

void felix86_cpuid(ThreadState* state);

#define FAST_HANDLE(name)                                                                                                                            \
    void fast_##name(Recompiler& rec, const HandlerMetadata& meta, Assembler& as, ZydisDecodedInstruction& instruction, ZydisDecodedOperand* operands)

#define IS_MMX (instruction.attributes & (ZYDIS_ATTRIB_FPU_STATE_CR | ZYDIS_ATTRIB_FPU_STATE_CW))

#define HAS_REP (instruction.attributes & (ZYDIS_ATTRIB_HAS_REP | ZYDIS_ATTRIB_HAS_REPZ | ZYDIS_ATTRIB_HAS_REPNZ))

void SetCmpFlags(const HandlerMetadata& meta, Recompiler& rec, Assembler& as, biscuit::GPR dst, biscuit::GPR src, biscuit::GPR result,
                 x86_size_e size, bool zext_src = false) {
    if (rec.shouldEmitFlag(meta.rip, X86_REF_CF)) {
        biscuit::GPR test = rec.scratch();
        if (zext_src) {
            rec.zext(test, src, size);
        } else {
            test = src;
        }
        rec.updateCarrySub(dst, test);
        rec.popScratch();
    }

    if (rec.shouldEmitFlag(meta.rip, X86_REF_PF)) {
        rec.updateParity(result);
    }

    if (rec.shouldEmitFlag(meta.rip, X86_REF_AF)) {
        rec.updateAuxiliarySub(dst, src);
    }

    if (rec.shouldEmitFlag(meta.rip, X86_REF_ZF)) {
        rec.updateZero(result, size);
    }

    if (rec.shouldEmitFlag(meta.rip, X86_REF_SF)) {
        rec.updateSign(result, size);
    }

    if (rec.shouldEmitFlag(meta.rip, X86_REF_OF)) {
        rec.updateOverflowSub(dst, src, result, size);
    }
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

void VEC_function(Recompiler& rec, const HandlerMetadata& meta, Assembler& as, ZydisDecodedInstruction& instruction, ZydisDecodedOperand* operands,
                  u64 func) {
    x86_ref_e dst_ref = rec.zydisToRef(operands[0].reg.value);
    ASSERT(dst_ref >= X86_REF_XMM0 && dst_ref <= X86_REF_XMM15);

    biscuit::GPR temp;
    if (operands[1].type == ZYDIS_OPERAND_TYPE_MEMORY) {
        temp = rec.lea(&operands[1]);
    }
    rec.writebackDirtyState();
    rec.invalidStateUntilJump();

    as.LI(t0, func);

    as.ADDI(a0, rec.threadStatePointer(), offsetof(ThreadState, xmm) + (dst_ref - X86_REF_XMM0) * 16);

    if (operands[1].type == ZYDIS_OPERAND_TYPE_REGISTER) {
        x86_ref_e src_ref = rec.zydisToRef(operands[1].reg.value);
        ASSERT(src_ref >= X86_REF_XMM0 && src_ref <= X86_REF_XMM15);
        as.ADDI(a1, rec.threadStatePointer(), offsetof(ThreadState, xmm) + (src_ref - X86_REF_XMM0) * 16);
    } else {
        as.MV(a1, temp);
    }

    as.JALR(t0);
    rec.restoreRoundingMode();
}

FAST_HANDLE(MOV) {
    biscuit::GPR src = rec.getOperandGPR(&operands[1]);
    rec.setOperandGPR(&operands[0], src);
}

FAST_HANDLE(ADD) {
    biscuit::GPR result = rec.scratch();
    biscuit::GPR src = rec.getOperandGPR(&operands[1]);
    biscuit::GPR dst = rec.getOperandGPR(&operands[0]);

    as.ADD(result, dst, src);

    x86_size_e size = rec.getOperandSize(&operands[0]);

    if (rec.shouldEmitFlag(meta.rip, X86_REF_CF)) {
        rec.updateCarryAdd(dst, result, size);
    }

    if (rec.shouldEmitFlag(meta.rip, X86_REF_PF)) {
        rec.updateParity(result);
    }

    if (rec.shouldEmitFlag(meta.rip, X86_REF_AF)) {
        rec.updateAuxiliaryAdd(dst, result);
    }

    if (rec.shouldEmitFlag(meta.rip, X86_REF_ZF)) {
        rec.updateZero(result, size);
    }

    if (rec.shouldEmitFlag(meta.rip, X86_REF_SF)) {
        rec.updateSign(result, size);
    }

    if (rec.shouldEmitFlag(meta.rip, X86_REF_OF)) {
        rec.updateOverflowAdd(dst, src, result, size);
    }

    rec.setOperandGPR(&operands[0], result);
}

FAST_HANDLE(SUB) {
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
    biscuit::GPR result = rec.scratch();
    biscuit::GPR src = rec.getOperandGPR(&operands[1]);
    biscuit::GPR dst = rec.getOperandGPR(&operands[0]);

    as.OR(result, dst, src);

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

    rec.setFlagUndefined(X86_REF_AF);
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
            rec.updateParity(x0);
        }

        if (rec.shouldEmitFlag(meta.rip, X86_REF_ZF)) {
            rec.setFlag(X86_REF_ZF);
        }

        if (rec.shouldEmitFlag(meta.rip, X86_REF_SF)) {
            rec.updateSign(x0, size);
        }

        if (rec.shouldEmitFlag(meta.rip, X86_REF_OF)) {
            rec.zeroFlag(X86_REF_OF);
        }

        rec.setFlagUndefined(X86_REF_AF);
        return;
    }

    biscuit::GPR result = rec.scratch();
    biscuit::GPR src = rec.getOperandGPR(&operands[1]);
    biscuit::GPR dst = rec.getOperandGPR(&operands[0]);

    as.XOR(result, dst, src);

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

    rec.setFlagUndefined(X86_REF_AF);
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

    rec.setFlagUndefined(X86_REF_AF);
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
    as.AUIPC(host_return_address, 0);
    as.ADDI(host_return_address, host_return_address, 20);
    as.SD(host_return_address, 0, sp);
    if (operands[0].type == ZYDIS_OPERAND_TYPE_IMMEDIATE) {
        rec.jumpAndLink(meta.rip.add(instruction.length + displacement), true /* push to rsb */);
    } else {
        rec.backToDispatcher(true); // true = push to rsb
    }
    u64 here = (u64)as.GetCursorPointer();
    ASSERT(here == start + 20);

    // We could continue compiling instructions in this block. It's a bit tricky with software that use jits though.
    // For example you compile a piece of code until a call, and then garbage may follow so you start compiling garbage instructions.
    // Or it's zeroed out and you compile a bunch of zeroes... not good. So for now we link to the next block after returning
    // and just stop compiling
    rec.jumpAndLink(meta.rip.add(instruction.length));
    rec.stopCompiling();
}

FAST_HANDLE(RET_rsb) {
    x86_size_e size = g_mode32 ? X86_SIZE_DWORD : X86_SIZE_QWORD;
    biscuit::GPR rsp = rec.getRefGPR(X86_REF_RSP, size);
    biscuit::GPR ra = rec.scratch();
    ASSERT(ra == biscuit::ra);
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
    if (g_rsb) {
        return fast_CALL_rsb(rec, meta, as, instruction, operands);
    }

    switch (operands[0].type) {
    case ZYDIS_OPERAND_TYPE_REGISTER:
    case ZYDIS_OPERAND_TYPE_MEMORY: {
        x86_size_e size = g_mode32 ? X86_SIZE_DWORD : X86_SIZE_QWORD;
        biscuit::GPR src = rec.getOperandGPR(&operands[0]);
        rec.setRip(src);
        biscuit::GPR rsp = rec.getRefGPR(X86_REF_RSP, size);
        as.ADDI(rsp, rsp, -rec.stackPointerSize());
        rec.setRefGPR(X86_REF_RSP, size, rsp);

        biscuit::GPR scratch = rec.scratch();
        GuestAddress return_address = meta.rip.add(instruction.length).toGuest();
        as.LI(scratch, return_address.raw());
        rec.writeMemory(scratch, rsp, 0, size);

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

        x86_size_e size = g_mode32 ? X86_SIZE_DWORD : X86_SIZE_QWORD;
        biscuit::GPR rsp = rec.getRefGPR(X86_REF_RSP, size);
        as.ADDI(rsp, rsp, -rec.stackPointerSize());
        rec.setRefGPR(X86_REF_RSP, size, rsp);

        biscuit::GPR scratch = rec.scratch();
        as.LI(scratch, return_address.raw());
        rec.writeMemory(scratch, rsp, 0, size);

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
    if (g_rsb) {
        return fast_RET_rsb(rec, meta, as, instruction, operands);
    }

    x86_size_e size = g_mode32 ? X86_SIZE_DWORD : X86_SIZE_QWORD;
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
    rec.writebackDirtyState();
    rec.invalidStateUntilJump();
    rec.popCalltrace();
    rec.backToDispatcher();
    rec.stopCompiling();
}

FAST_HANDLE(PUSH) {
    x86_size_e size = g_mode32 ? X86_SIZE_DWORD : X86_SIZE_QWORD;
    biscuit::GPR src = rec.getOperandGPR(&operands[0]);
    biscuit::GPR rsp = rec.getRefGPR(X86_REF_RSP, size);
    int imm = -size_to_bytes(instruction.operand_width);
    rec.writeMemory(src, rsp, imm, rec.zydisToSize(instruction.operand_width));

    as.ADDI(rsp, rsp, imm);
    rec.setRefGPR(X86_REF_RSP, size, rsp);
}

FAST_HANDLE(POP) {
    x86_size_e size = g_mode32 ? X86_SIZE_DWORD : X86_SIZE_QWORD;
    biscuit::GPR result = rec.scratch();
    biscuit::GPR rsp = rec.getRefGPR(X86_REF_RSP, size);

    rec.readMemory(result, rsp, 0, rec.zydisToSize(instruction.operand_width));

    int imm = size_to_bytes(instruction.operand_width);
    rec.setOperandGPR(&operands[0], result);

    x86_ref_e ref = rec.zydisToRef(operands[0].reg.value);
    if (ref == X86_REF_RSP) {
        // pop rsp special case
        rec.setRefGPR(X86_REF_RSP, size, result);
    } else {
        as.ADDI(rsp, rsp, imm);
        rec.setRefGPR(X86_REF_RSP, size, rsp);
    }
}

FAST_HANDLE(NOP) {}

FAST_HANDLE(ENDBR32) {}

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

    as.SLL(result, dst, count);

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
        as.LI(cf, rec.getBitSize(size));
        as.SUB(cf, cf, count);
        as.SRL(cf, dst, cf);
        as.ANDI(cf, cf, 1);
    }

    if (rec.shouldEmitFlag(meta.rip, X86_REF_OF)) {
        biscuit::GPR of = rec.flagW(X86_REF_OF);
        as.SRLI(of, result, rec.getBitSize(size) - 1);
        as.ANDI(of, of, 1);
        as.XOR(of, of, rec.flag(X86_REF_CF));
    }

    as.Bind(&zero_source);

    rec.setOperandGPR(&operands[0], result);
}

FAST_HANDLE(SHR) {
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

    as.SRL(result, dst, count);

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
        as.SRLI(of, dst, rec.getBitSize(size) - 1);
        as.ANDI(of, of, 1);
    }

    as.Bind(&zero_source);

    rec.setOperandGPR(&operands[0], result);
}

FAST_HANDLE(SAR) {
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
    case X86_SIZE_BYTE: {
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

        rec.setVectorState(SEW::E64, rec.maxVlen() / 64);
        as.VMV_XS(dst, src);

        rec.setOperandGPR(&operands[0], dst);
    } else if (operands[1].type == ZYDIS_OPERAND_TYPE_MEMORY) {
        ASSERT(operands[0].size == 128);
        ASSERT(operands[1].size == 64);
        biscuit::GPR src = rec.getOperandGPR(&operands[1]);
        biscuit::Vec dst = rec.getOperandVec(&operands[0]);

        rec.setVectorState(SEW::E64, rec.maxVlen() / 64);
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

            rec.setVectorState(SEW::E64, rec.maxVlen() / 64);
            as.VMV(v0, 0b10);

            // Zero upper 64-bit elements (this will be useful for when we get to AVX)
            as.VXOR(dst, dst, dst, VecMask::Yes);
            as.VMV_SX(dst, src);

            rec.setOperandVec(&operands[0], dst);
        } else if (rec.isGPR(operands[0].reg.value)) {
            biscuit::GPR dst = rec.getOperandGPR(&operands[0]);
            biscuit::Vec src = rec.getOperandVec(&operands[1]);

            rec.setVectorState(SEW::E64, rec.maxVlen() / 64);
            as.VMV_XS(dst, src);

            rec.setOperandGPR(&operands[0], dst);
        } else {
            biscuit::Vec result = rec.scratchVec();
            biscuit::Vec src = rec.getOperandVec(&operands[1]);

            rec.setVectorState(SEW::E64, rec.maxVlen() / 64);
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

        rec.setVectorState(SEW::E32, rec.maxVlen() / 32);
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

            rec.setVectorState(SEW::E32, rec.maxVlen() / 32);
            as.VMV(v0, 0b1110);

            // Zero upper 32-bit elements (this will be useful for when we get to AVX)
            as.VXOR(dst, dst, dst, VecMask::Yes);
            as.VMV_SX(dst, src);

            rec.setOperandVec(&operands[0], dst);
        } else if (rec.isGPR(operands[0].reg.value)) {
            biscuit::GPR dst = rec.getOperandGPR(&operands[0]);
            biscuit::Vec src = rec.getOperandVec(&operands[1]);

            rec.setVectorState(SEW::E32, rec.maxVlen() / 32);
            as.VMV_XS(dst, src);

            rec.setOperandGPR(&operands[0], dst);
        } else {
            biscuit::Vec result = rec.scratchVec();
            biscuit::Vec src = rec.getOperandVec(&operands[1]);

            rec.setVectorState(SEW::E32, rec.maxVlen() / 32);
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
        rec.backToDispatcher();
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

FAST_HANDLE(DIV) {
    x86_size_e size = rec.getOperandSize(&operands[0]);
    // we don't need to move src to scratch because the rdx and rax in all these cases are in scratches
    biscuit::GPR src = rec.getOperandGPR(&operands[0]);

    switch (size) {
    case X86_SIZE_BYTE: {
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
    rec.setFlagUndefined(X86_REF_AF);
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
    biscuit::GPR af = rec.flagW(X86_REF_AF);
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
        case X86_SIZE_BYTE: {
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

    rec.setFlagUndefined(X86_REF_AF);
    rec.setFlagUndefined(X86_REF_ZF);
    rec.setFlagUndefined(X86_REF_SF);
}

void PUNPCKL(Recompiler& rec, const HandlerMetadata& meta, Assembler& as, ZydisDecodedInstruction& instruction, ZydisDecodedOperand* operands,
             SEW sew, u8 vlen) {
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
    as.LI(mask, 0b10101010);

    rec.setVectorState(sew, vlen);
    as.VMV(v0, mask);
    as.VIOTA(iota, v0);
    as.VMV(result, 0);
    rec.vrgather(result, src, iota, VecMask::Yes);

    as.VSRL(v0, v0, 1);
    as.VIOTA(iota, v0);
    rec.vrgather(result, dst, iota, VecMask::Yes);

    rec.setOperandVec(&operands[0], result);
}

void PUNPCKH(Recompiler& rec, const HandlerMetadata& meta, Assembler& as, ZydisDecodedInstruction& instruction, ZydisDecodedOperand* operands,
             SEW sew, u8 vlen) {
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
    as.LI(mask, 0b10101010);

    rec.setVectorState(sew, vlen);
    as.VMV(v0, mask);
    as.VIOTA(iota, v0);
    as.VMV(result, 0);
    as.VADD(iota, iota, num);
    rec.vrgather(result, src, iota, VecMask::Yes);

    as.VSRL(v0, v0, 1);
    as.VIOTA(iota, v0);
    as.VADD(iota, iota, num);
    rec.vrgather(result, dst, iota, VecMask::Yes);

    rec.setOperandVec(&operands[0], result);
}

FAST_HANDLE(PUNPCKLBW) {
    PUNPCKL(rec, meta, as, instruction, operands, SEW::E8, rec.maxVlen() / 8);
}

FAST_HANDLE(PUNPCKLWD) {
    PUNPCKL(rec, meta, as, instruction, operands, SEW::E16, rec.maxVlen() / 16);
}

FAST_HANDLE(PUNPCKLDQ) {
    PUNPCKL(rec, meta, as, instruction, operands, SEW::E32, rec.maxVlen() / 32);
}

FAST_HANDLE(PUNPCKLQDQ) {
    PUNPCKL(rec, meta, as, instruction, operands, SEW::E64, rec.maxVlen() / 64);
}

FAST_HANDLE(PUNPCKHBW) {
    PUNPCKH(rec, meta, as, instruction, operands, SEW::E8, rec.maxVlen() / 8);
}

FAST_HANDLE(PUNPCKHWD) {
    PUNPCKH(rec, meta, as, instruction, operands, SEW::E16, rec.maxVlen() / 16);
}

FAST_HANDLE(PUNPCKHDQ) {
    PUNPCKH(rec, meta, as, instruction, operands, SEW::E32, rec.maxVlen() / 32);
}

FAST_HANDLE(PUNPCKHQDQ) {
    PUNPCKH(rec, meta, as, instruction, operands, SEW::E64, rec.maxVlen() / 64);
}

FAST_HANDLE(UNPCKLPS) { // Fuzzed
    biscuit::Vec scratch = rec.scratchVec();
    biscuit::Vec iota = rec.scratchVec();
    biscuit::Vec src1 = rec.getOperandVec(&operands[0]);
    biscuit::Vec src2 = rec.getOperandVec(&operands[1]);

    rec.setVectorState(SEW::E32, rec.maxVlen() / 32);
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

    rec.setVectorState(SEW::E32, rec.maxVlen() / 32);
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

    rec.setVectorState(SEW::E64, rec.maxVlen() / 64);
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

    rec.setVectorState(SEW::E64, rec.maxVlen() / 64);
    as.VSLIDEDOWN(scratch, src1, 1);
    as.VMV(v0, 0b10);
    as.VMERGE(result, scratch, src2);

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
    as.RDTIME(tsc);
    rec.setRefGPR(X86_REF_RAX, X86_SIZE_DWORD, tsc);
    as.SRLI(tsc, tsc, 32);
    rec.setRefGPR(X86_REF_RDX, X86_SIZE_DWORD, tsc);
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
    if (!g_strace && !g_dont_inline_syscalls) {
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

    biscuit::GPR address = rec.scratch();
    as.LI(address, (u64)&felix86_syscall);
    as.MV(a0, rec.threadStatePointer());
    as.JALR(address);
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
    as.VXOR(dst, dst, src);
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
    as.VAND(dst, dst, src);
    rec.setOperandVec(&operands[0], dst);
}

FAST_HANDLE(POR) {
    biscuit::Vec dst = rec.getOperandVec(&operands[0]);
    biscuit::Vec src = rec.getOperandVec(&operands[1]);
    rec.setVectorState(SEW::E64, rec.maxVlen() / 64);
    as.VOR(dst, dst, src);
    rec.setOperandVec(&operands[0], dst);
}

FAST_HANDLE(PANDN) { // Fuzzed
    biscuit::Vec dst_not = rec.scratchVec();
    biscuit::Vec dst = rec.getOperandVec(&operands[0]);
    biscuit::Vec src = rec.getOperandVec(&operands[1]);
    rec.setVectorState(SEW::E64, rec.maxVlen() / 64);
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
    PADD(rec, meta, as, instruction, operands, SEW::E8, rec.maxVlen() / 8);
}

FAST_HANDLE(PADDW) {
    PADD(rec, meta, as, instruction, operands, SEW::E16, rec.maxVlen() / 16);
}

FAST_HANDLE(PADDD) {
    PADD(rec, meta, as, instruction, operands, SEW::E32, rec.maxVlen() / 32);
}

FAST_HANDLE(PADDQ) {
    PADD(rec, meta, as, instruction, operands, SEW::E64, rec.maxVlen() / 64);
}

FAST_HANDLE(PADDSB) {
    PADDS(rec, meta, as, instruction, operands, SEW::E8, rec.maxVlen() / 8);
}

FAST_HANDLE(PADDSW) {
    PADDS(rec, meta, as, instruction, operands, SEW::E16, rec.maxVlen() / 16);
}

FAST_HANDLE(PSUBSB) {
    PSUBS(rec, meta, as, instruction, operands, SEW::E8, rec.maxVlen() / 8);
}

FAST_HANDLE(PSUBSW) {
    PSUBS(rec, meta, as, instruction, operands, SEW::E16, rec.maxVlen() / 16);
}

FAST_HANDLE(PADDUSB) {
    PADDSU(rec, meta, as, instruction, operands, SEW::E8, rec.maxVlen() / 8);
}

FAST_HANDLE(PADDUSW) { // Fuzzed
    PADDSU(rec, meta, as, instruction, operands, SEW::E16, rec.maxVlen() / 16);
}

FAST_HANDLE(PSUBUSB) {
    PSUBSU(rec, meta, as, instruction, operands, SEW::E8, rec.maxVlen() / 8);
}

FAST_HANDLE(PSUBUSW) {
    PSUBSU(rec, meta, as, instruction, operands, SEW::E16, rec.maxVlen() / 16);
}

FAST_HANDLE(PSUBB) {
    PSUB(rec, meta, as, instruction, operands, SEW::E8, rec.maxVlen() / 8);
}

FAST_HANDLE(PSUBW) {
    PSUB(rec, meta, as, instruction, operands, SEW::E16, rec.maxVlen() / 16);
}

FAST_HANDLE(PSUBD) {
    PSUB(rec, meta, as, instruction, operands, SEW::E32, rec.maxVlen() / 32);
}

FAST_HANDLE(PSUBQ) {
    PSUB(rec, meta, as, instruction, operands, SEW::E64, rec.maxVlen() / 64);
}

FAST_HANDLE(ADDPS) {
    biscuit::Vec dst = rec.getOperandVec(&operands[0]);
    biscuit::Vec src = rec.getOperandVec(&operands[1]);
    rec.setVectorState(SEW::E32, rec.maxVlen() / 32);
    as.VFADD(dst, dst, src);
    rec.setOperandVec(&operands[0], dst);
}

FAST_HANDLE(ADDPD) {
    biscuit::Vec dst = rec.getOperandVec(&operands[0]);
    biscuit::Vec src = rec.getOperandVec(&operands[1]);
    rec.setVectorState(SEW::E64, rec.maxVlen() / 64);
    as.VFADD(dst, dst, src);
    rec.setOperandVec(&operands[0], dst);
}

FAST_HANDLE(SUBPS) {
    biscuit::Vec dst = rec.getOperandVec(&operands[0]);
    biscuit::Vec src = rec.getOperandVec(&operands[1]);
    rec.setVectorState(SEW::E32, rec.maxVlen() / 32);
    as.VFSUB(dst, dst, src);
    rec.setOperandVec(&operands[0], dst);
}

FAST_HANDLE(SUBPD) {
    biscuit::Vec dst = rec.getOperandVec(&operands[0]);
    biscuit::Vec src = rec.getOperandVec(&operands[1]);
    rec.setVectorState(SEW::E64, rec.maxVlen() / 64);
    as.VFSUB(dst, dst, src);
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
    biscuit::Vec dst = rec.getOperandVec(&operands[0]);
    biscuit::Vec src = rec.getOperandVec(&operands[1]);
    biscuit::Vec nan_mask_1 = rec.scratchVec();
    biscuit::Vec nan_mask_2 = rec.scratchVec();
    biscuit::Vec equal_mask = rec.scratchVec();
    biscuit::Vec zero_mask = rec.scratchVec();
    biscuit::Vec neg_zero_mask = rec.scratchVec();
    rec.setVectorState(SEW::E64, rec.maxVlen() / 64);

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
    rec.setVectorState(SEW::E8, rec.maxVlen() / 8);
    as.VMINU(dst, dst, src);
    rec.setOperandVec(&operands[0], dst);
}

FAST_HANDLE(PMINUW) {
    biscuit::Vec dst = rec.getOperandVec(&operands[0]);
    biscuit::Vec src = rec.getOperandVec(&operands[1]);
    rec.setVectorState(SEW::E16, rec.maxVlen() / 16);
    as.VMINU(dst, dst, src);
    rec.setOperandVec(&operands[0], dst);
}

FAST_HANDLE(PMINUD) {
    biscuit::Vec dst = rec.getOperandVec(&operands[0]);
    biscuit::Vec src = rec.getOperandVec(&operands[1]);
    rec.setVectorState(SEW::E32, rec.maxVlen() / 32);
    as.VMINU(dst, dst, src);
    rec.setOperandVec(&operands[0], dst);
}

FAST_HANDLE(PMAXUB) {
    biscuit::Vec dst = rec.getOperandVec(&operands[0]);
    biscuit::Vec src = rec.getOperandVec(&operands[1]);
    rec.setVectorState(SEW::E8, rec.maxVlen() / 8);
    as.VMAXU(dst, dst, src);
    rec.setOperandVec(&operands[0], dst);
}

FAST_HANDLE(PMAXUW) {
    biscuit::Vec dst = rec.getOperandVec(&operands[0]);
    biscuit::Vec src = rec.getOperandVec(&operands[1]);
    rec.setVectorState(SEW::E16, rec.maxVlen() / 16);
    as.VMAXU(dst, dst, src);
    rec.setOperandVec(&operands[0], dst);
}

FAST_HANDLE(PMAXUD) {
    biscuit::Vec dst = rec.getOperandVec(&operands[0]);
    biscuit::Vec src = rec.getOperandVec(&operands[1]);
    rec.setVectorState(SEW::E32, rec.maxVlen() / 32);
    as.VMAXU(dst, dst, src);
    rec.setOperandVec(&operands[0], dst);
}

FAST_HANDLE(PMINSB) {
    biscuit::Vec dst = rec.getOperandVec(&operands[0]);
    biscuit::Vec src = rec.getOperandVec(&operands[1]);
    rec.setVectorState(SEW::E8, rec.maxVlen() / 8);
    as.VMIN(dst, dst, src);
    rec.setOperandVec(&operands[0], dst);
}

FAST_HANDLE(PMINSW) {
    biscuit::Vec dst = rec.getOperandVec(&operands[0]);
    biscuit::Vec src = rec.getOperandVec(&operands[1]);
    rec.setVectorState(SEW::E16, rec.maxVlen() / 16);
    as.VMIN(dst, dst, src);
    rec.setOperandVec(&operands[0], dst);
}

FAST_HANDLE(PMINSD) {
    biscuit::Vec dst = rec.getOperandVec(&operands[0]);
    biscuit::Vec src = rec.getOperandVec(&operands[1]);
    rec.setVectorState(SEW::E32, rec.maxVlen() / 32);
    as.VMIN(dst, dst, src);
    rec.setOperandVec(&operands[0], dst);
}

FAST_HANDLE(PMAXSB) {
    biscuit::Vec dst = rec.getOperandVec(&operands[0]);
    biscuit::Vec src = rec.getOperandVec(&operands[1]);
    rec.setVectorState(SEW::E8, rec.maxVlen() / 8);
    as.VMAX(dst, dst, src);
    rec.setOperandVec(&operands[0], dst);
}

FAST_HANDLE(PMAXSW) {
    biscuit::Vec dst = rec.getOperandVec(&operands[0]);
    biscuit::Vec src = rec.getOperandVec(&operands[1]);
    rec.setVectorState(SEW::E16, rec.maxVlen() / 16);
    as.VMAX(dst, dst, src);
    rec.setOperandVec(&operands[0], dst);
}

FAST_HANDLE(PMAXSD) {
    biscuit::Vec dst = rec.getOperandVec(&operands[0]);
    biscuit::Vec src = rec.getOperandVec(&operands[1]);
    rec.setVectorState(SEW::E32, rec.maxVlen() / 32);
    as.VMAX(dst, dst, src);
    rec.setOperandVec(&operands[0], dst);
}

FAST_HANDLE(PMULHW) { // Fuzzed
    biscuit::Vec dst = rec.getOperandVec(&operands[0]);
    biscuit::Vec src = rec.getOperandVec(&operands[1]);
    rec.setVectorState(SEW::E16, rec.maxVlen() / 16);
    as.VMULH(dst, dst, src);
    rec.setOperandVec(&operands[0], dst);
}

FAST_HANDLE(PMULHUW) { // Fuzzed
    biscuit::Vec dst = rec.getOperandVec(&operands[0]);
    biscuit::Vec src = rec.getOperandVec(&operands[1]);
    rec.setVectorState(SEW::E16, rec.maxVlen() / 16);
    as.VMULHU(dst, dst, src);
    rec.setOperandVec(&operands[0], dst);
}

FAST_HANDLE(PMULLW) { // Fuzzed
    biscuit::Vec dst = rec.getOperandVec(&operands[0]);
    biscuit::Vec src = rec.getOperandVec(&operands[1]);
    rec.setVectorState(SEW::E16, rec.maxVlen() / 16);
    as.VMUL(dst, dst, src);
    rec.setOperandVec(&operands[0], dst);
}

FAST_HANDLE(PMULLD) { // Fuzzed
    biscuit::Vec dst = rec.getOperandVec(&operands[0]);
    biscuit::Vec src = rec.getOperandVec(&operands[1]);
    rec.setVectorState(SEW::E32, rec.maxVlen() / 32);
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

    rec.setVectorState(SEW::E64, rec.maxVlen() / 64);
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

    rec.setVectorState(SEW::E64, rec.maxVlen() / 64);
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
    as.SLLI(mask, mask, 1);
    as.VMV(vec_mask, mask);
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
    biscuit::Vec dst = rec.getOperandVec(&operands[0]);
    biscuit::Vec src = rec.getOperandVec(&operands[1]);
    biscuit::Vec nan_mask_1 = rec.scratchVec();
    biscuit::Vec nan_mask_2 = rec.scratchVec();
    biscuit::Vec equal_mask = rec.scratchVec();
    biscuit::Vec zero_mask = rec.scratchVec();
    biscuit::Vec neg_zero_mask = rec.scratchVec();
    rec.setVectorState(SEW::E32, rec.maxVlen() / 32);

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
    biscuit::Vec dst = rec.getOperandVec(&operands[0]);
    biscuit::Vec src = rec.getOperandVec(&operands[1]);
    biscuit::Vec nan_mask_1 = rec.scratchVec();
    biscuit::Vec nan_mask_2 = rec.scratchVec();
    biscuit::Vec equal_mask = rec.scratchVec();
    biscuit::Vec zero_mask = rec.scratchVec();
    biscuit::Vec neg_zero_mask = rec.scratchVec();
    rec.setVectorState(SEW::E64, rec.maxVlen() / 64);

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
    rec.setVectorState(SEW::E32, rec.maxVlen() / 32);
    as.VFMUL(dst, dst, src);
    rec.setOperandVec(&operands[0], dst);
}

FAST_HANDLE(MULPD) {
    biscuit::Vec dst = rec.getOperandVec(&operands[0]);
    biscuit::Vec src = rec.getOperandVec(&operands[1]);
    rec.setVectorState(SEW::E64, rec.maxVlen() / 64);
    as.VFMUL(dst, dst, src);
    rec.setOperandVec(&operands[0], dst);
}

FAST_HANDLE(SQRTPS) { // Fuzzed, TODO: needs NaN handling
    biscuit::Vec dst = rec.allocatedVec(rec.zydisToRef(operands[0].reg.value));
    biscuit::Vec src = rec.getOperandVec(&operands[1]);
    rec.setVectorState(SEW::E32, rec.maxVlen() / 32);
    as.VFSQRT(dst, src);
    rec.setOperandVec(&operands[0], dst);
}

FAST_HANDLE(SQRTPD) {
    biscuit::Vec dst = rec.allocatedVec(rec.zydisToRef(operands[0].reg.value));
    biscuit::Vec src = rec.getOperandVec(&operands[1]);
    rec.setVectorState(SEW::E64, rec.maxVlen() / 64);
    as.VFSQRT(dst, src);
    rec.setOperandVec(&operands[0], dst);
}

FAST_HANDLE(DIVPS) {
    biscuit::Vec dst = rec.getOperandVec(&operands[0]);
    biscuit::Vec src = rec.getOperandVec(&operands[1]);
    rec.setVectorState(SEW::E32, rec.maxVlen() / 32);
    as.VFDIV(dst, dst, src);
    rec.setOperandVec(&operands[0], dst);
}

FAST_HANDLE(DIVPD) {
    biscuit::Vec dst = rec.getOperandVec(&operands[0]);
    biscuit::Vec src = rec.getOperandVec(&operands[1]);
    rec.setVectorState(SEW::E64, rec.maxVlen() / 64);
    as.VFDIV(dst, dst, src);
    rec.setOperandVec(&operands[0], dst);
}

FAST_HANDLE(RCPPS) {
    biscuit::Vec dst = rec.allocatedVec(rec.zydisToRef(operands[0].reg.value));
    biscuit::Vec src = rec.getOperandVec(&operands[1]);
    biscuit::Vec ones = rec.scratchVec();
    rec.setVectorState(SEW::E32, rec.maxVlen() / 32);
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
    rec.setVectorState(SEW::E32, rec.maxVlen() / 32);
    biscuit::GPR scratch = rec.scratch();
    as.LI(scratch, 0x3f800000);
    as.VMV(ones, scratch);
    as.VFSQRT(temp, src);
    as.VFDIV(dst, ones, temp);
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

    rec.setRefGPR(X86_REF_RDI, X86_SIZE_QWORD, rdi);
    rec.setRefGPR(X86_REF_RSI, X86_SIZE_QWORD, rsi);
    rec.setRefGPR(X86_REF_RCX, X86_SIZE_QWORD, rcx);
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
        biscuit::Vec result = rec.scratchVec();
        biscuit::Vec dst = rec.getOperandVec(&operands[0]);
        biscuit::Vec src = rec.getOperandVec(&operands[1]);
        rec.setVectorState(SEW::E64, rec.maxVlen() / 64);
        as.VMV(v0, 1);
        if (operands[1].type == ZYDIS_OPERAND_TYPE_MEMORY) {
            // Only when src is memory are the upper bits zeroed
            as.VMV(result, 0);
            as.VOR(result, src, 0, VecMask::Yes);
        } else {
            as.VMERGE(result, dst, src);
        }
        rec.setOperandVec(&operands[0], result);
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
    u8 width = instruction.operand_width;
    biscuit::GPR rdi = rec.getRefGPR(X86_REF_RDI, X86_SIZE_QWORD);
    biscuit::GPR rsi = rec.getRefGPR(X86_REF_RSI, X86_SIZE_QWORD);
    biscuit::GPR rcx = rec.getRefGPR(X86_REF_RCX, X86_SIZE_QWORD);
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

    SetCmpFlags(meta, rec, as, src1, src2, result, size);

    as.ADD(rdi, rdi, temp);
    as.ADD(rsi, rsi, temp);

    if (HAS_REP) {
        rec.repzEpilogue(&loop_body, &loop_end, rcx, instruction.attributes & ZYDIS_ATTRIB_HAS_REPZ);
        as.Bind(&loop_end);
    }

    rec.setRefGPR(X86_REF_RDI, X86_SIZE_QWORD, rdi);
    rec.setRefGPR(X86_REF_RSI, X86_SIZE_QWORD, rsi);
    rec.setRefGPR(X86_REF_RCX, X86_SIZE_QWORD, rcx);
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
    u8 width = instruction.operand_width;
    x86_size_e size = rec.zydisToSize(width);
    biscuit::GPR rax = rec.getRefGPR(X86_REF_RAX, size);
    biscuit::GPR rdi = rec.getRefGPR(X86_REF_RDI, X86_SIZE_QWORD);
    biscuit::GPR rcx = rec.getRefGPR(X86_REF_RCX, X86_SIZE_QWORD);
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

    SetCmpFlags(meta, rec, as, rax, src2, result, size);

    as.ADD(rdi, rdi, temp);

    if (HAS_REP) {
        rec.repzEpilogue(&loop_body, &loop_end, rcx, instruction.attributes & ZYDIS_ATTRIB_HAS_REPZ);
        as.Bind(&loop_end);
    }

    rec.setRefGPR(X86_REF_RDI, X86_SIZE_QWORD, rdi);
    rec.setRefGPR(X86_REF_RCX, X86_SIZE_QWORD, rcx);
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

FAST_HANDLE(STOSB) {
    Label loop_end, loop_body;
    u8 width = instruction.operand_width;
    biscuit::GPR rdi = rec.getRefGPR(X86_REF_RDI, X86_SIZE_QWORD);
    biscuit::GPR rcx = rec.getRefGPR(X86_REF_RCX, X86_SIZE_QWORD);
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

    rec.setRefGPR(X86_REF_RDI, X86_SIZE_QWORD, rdi);
    rec.setRefGPR(X86_REF_RCX, X86_SIZE_QWORD, rcx);
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
        rec.setVectorState(SEW::E64, rec.maxVlen() / 64);
        as.VSLIDEDOWN(temp, src, 1);
        rec.setOperandVec(&operands[0], temp);
    } else if (operands[1].type == ZYDIS_OPERAND_TYPE_MEMORY) {
        biscuit::Vec temp = rec.scratchVec();
        biscuit::Vec dst = rec.getOperandVec(&operands[0]);
        biscuit::Vec src = rec.getOperandVec(&operands[1]);
        rec.setVectorState(SEW::E64, rec.maxVlen() / 64);
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

    rec.setVectorState(SEW::E64, rec.maxVlen() / 64);

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
        biscuit::GPR af = rec.flagW(X86_REF_AF);
        as.ANDI(af, dst, 0xF);
        as.SNEZ(af, af);
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
    rec.setVectorState(SEW::E64, rec.maxVlen() / 64);
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
    rec.setVectorState(SEW::E64, rec.maxVlen() / 64);
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

    rec.setVectorState(SEW::E8, rec.maxVlen() / 8);
    as.VMSLT(temp, src, x0);

    rec.setVectorState(SEW::E64, rec.maxVlen() / 64);
    as.VMV_XS(scratch, temp);

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
    as.VMSLT(mask, src, x0);
    as.VMV_XS(dst, mask);
    as.ANDI(dst, dst, 0b1111);
    rec.setOperandGPR(&operands[0], dst);
}

FAST_HANDLE(MOVMSKPD) {
    biscuit::Vec mask = rec.scratchVec();
    biscuit::Vec src = rec.getOperandVec(&operands[1]);
    biscuit::GPR dst = rec.scratch();

    rec.setVectorState(SEW::E64, rec.maxVlen() / 64);
    as.VMSLT(mask, src, x0);
    as.VMV_XS(dst, mask);
    as.ANDI(dst, dst, 0b11);
    rec.setOperandGPR(&operands[0], dst);
}

FAST_HANDLE(PMOVZXBQ) {
    biscuit::GPR mask = rec.scratch();
    biscuit::Vec iota = rec.scratchVec();
    biscuit::Vec result = rec.scratchVec();
    biscuit::Vec src = rec.getOperandVec(&operands[1]);

    rec.setVectorState(SEW::E64, rec.maxVlen() / 64);
    as.VID(iota); // iota with 64-bit elements will place the indices at the right locations
    rec.setVectorState(SEW::E8, rec.maxVlen() / 8);
    as.LI(mask, 0b00000001'00000001'00000001'00000001);
    as.VMV(result, 0);
    as.VMV(v0, mask);
    as.VRGATHER(result, src, iota, VecMask::Yes);

    rec.setOperandVec(&operands[0], result);
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
    PCMPEQ(rec, meta, as, instruction, operands, SEW::E8, rec.maxVlen() / 8);
}

FAST_HANDLE(PCMPEQW) {
    PCMPEQ(rec, meta, as, instruction, operands, SEW::E16, rec.maxVlen() / 16);
}

FAST_HANDLE(PCMPEQD) {
    PCMPEQ(rec, meta, as, instruction, operands, SEW::E32, rec.maxVlen() / 32);
}

FAST_HANDLE(PCMPEQQ) {
    PCMPEQ(rec, meta, as, instruction, operands, SEW::E64, rec.maxVlen() / 64);
}

FAST_HANDLE(PCMPGTB) {
    PCMPGT(rec, meta, as, instruction, operands, SEW::E8, rec.maxVlen() / 8);
}

FAST_HANDLE(PCMPGTW) {
    PCMPGT(rec, meta, as, instruction, operands, SEW::E16, rec.maxVlen() / 16);
}

FAST_HANDLE(PCMPGTD) {
    PCMPGT(rec, meta, as, instruction, operands, SEW::E32, rec.maxVlen() / 32);
}

FAST_HANDLE(PCMPGTQ) {
    PCMPGT(rec, meta, as, instruction, operands, SEW::E64, rec.maxVlen() / 64);
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
    CMPP(rec, meta, as, instruction, operands, SEW::E32, rec.maxVlen() / 32);
}

FAST_HANDLE(CMPPD) { // Fuzzed
    CMPP(rec, meta, as, instruction, operands, SEW::E64, rec.maxVlen() / 64);
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

    rec.setVectorState(SEW::E32, rec.maxVlen() / 32);
    as.VRGATHEREI16(result, src, iota);

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
    as.VMV(iota2, el3);
    as.LI(temp, el2);
    as.VSLIDE1UP(iota, iota2, temp);
    as.LI(temp, el1);
    as.VSLIDE1UP(iota2, iota, temp);
    as.LI(temp, el0);
    as.VSLIDE1UP(iota, iota2, temp);

    as.VMV(v0, 0b11);
    as.VMV(result, 0);
    as.VRGATHER(result, dst, iota, VecMask::Yes);
    as.VMV(v0, 0b1100);
    as.VRGATHER(result, src, iota, VecMask::Yes);

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
    as.LI(bitmask, 0b10001111);
    as.VAND(mask_masked, mask, bitmask);
    as.VRGATHER(tmp, dst, mask_masked);

    rec.setOperandVec(&operands[0], tmp);
}

FAST_HANDLE(PBLENDW) { // Fuzzed
    u8 imm = rec.getImmediate(&operands[2]);
    biscuit::GPR mask = rec.scratch();
    biscuit::Vec result = rec.scratchVec();
    biscuit::Vec dst = rec.getOperandVec(&operands[0]);
    biscuit::Vec src = rec.getOperandVec(&operands[1]);

    rec.setVectorState(SEW::E16, rec.maxVlen() / 16);
    as.LI(mask, imm);
    as.VMV(v0, mask);
    as.VMERGE(result, dst, src);

    rec.setOperandVec(&operands[0], result);
}

FAST_HANDLE(BLENDPS) {
    u8 imm = rec.getImmediate(&operands[2]) & 0b1111;
    biscuit::Vec result = rec.scratchVec();
    biscuit::Vec dst = rec.getOperandVec(&operands[0]);
    biscuit::Vec src = rec.getOperandVec(&operands[1]);

    rec.setVectorState(SEW::E32, rec.maxVlen() / 32);
    as.VMV(v0, imm);
    as.VMERGE(result, dst, src);

    rec.setOperandVec(&operands[0], result);
}

FAST_HANDLE(BLENDVPS) {
    biscuit::Vec result = rec.scratchVec();
    biscuit::Vec dst = rec.getOperandVec(&operands[0]);
    biscuit::Vec src = rec.getOperandVec(&operands[1]);
    biscuit::Vec mask = rec.getRefVec(X86_REF_XMM0); // I see where VMERGE took inspiration from

    rec.setVectorState(SEW::E32, rec.maxVlen() / 32);
    as.VMSLT(v0, mask, x0);
    as.VMERGE(result, dst, src);

    rec.setOperandVec(&operands[0], result);
}

FAST_HANDLE(BLENDPD) {
    u8 imm = rec.getImmediate(&operands[2]) & 0b11;
    biscuit::Vec result = rec.scratchVec();
    biscuit::Vec dst = rec.getOperandVec(&operands[0]);
    biscuit::Vec src = rec.getOperandVec(&operands[1]);

    rec.setVectorState(SEW::E64, rec.maxVlen() / 64);
    as.VMV(v0, imm);
    as.VMERGE(result, dst, src);

    rec.setOperandVec(&operands[0], result);
}

FAST_HANDLE(BLENDVPD) {
    biscuit::Vec result = rec.scratchVec();
    biscuit::Vec dst = rec.getOperandVec(&operands[0]);
    biscuit::Vec src = rec.getOperandVec(&operands[1]);
    biscuit::Vec mask = rec.getRefVec(X86_REF_XMM0);

    rec.setVectorState(SEW::E64, rec.maxVlen() / 64);
    as.VMSLT(v0, mask, x0);
    as.VMERGE(result, dst, src);

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
    as.VMV(iota, 0);
    as.VID(iota2);
    // Slide down 4 words, so then the register looks like 8 7 6 5, then we can slide up the other 4 elements
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

    rec.setVectorState(SEW::E16, rec.maxVlen() / 16);
    as.VMV(result, src); // to move the low words

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

    rec.setVectorState(SEW::E64, rec.maxVlen() / 64);

    if (imm > 31) {
        as.VMV(dst, 0);
        rec.setOperandVec(&operands[0], dst);
        return;
    }

    if (16 - imm > 0) {
        as.LI(temp, ~((1ull << (16 - imm)) - 1));
        as.VMV_SX(v0, temp);
        rec.setVectorState(SEW::E8, rec.maxVlen() / 8);
        as.VMV(result, 0);
        as.VSLIDEDOWN(result, src, imm);
        as.VAND(result, result, 0, VecMask::Yes);
        as.VMV(slide_up, 0);
        as.VSLIDEUP(slide_up, dst, 16 - imm);
        as.VOR(result, result, slide_up);
    } else {
        as.LI(temp, ~((1ull << (32 - imm)) - 1));
        as.VMV_SX(v0, temp);
        rec.setVectorState(SEW::E8, rec.maxVlen() / 8);
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
    rec.setFlagUndefined(X86_REF_AF);
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
    rec.setFlagUndefined(X86_REF_AF);
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
    rec.setFlagUndefined(X86_REF_AF);
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
    rec.setFlagUndefined(X86_REF_AF);
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
    rec.setFlagUndefined(X86_REF_AF);
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
    rec.setFlagUndefined(X86_REF_AF);
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
    rec.setFlagUndefined(X86_REF_AF);
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
    rec.setFlagUndefined(X86_REF_AF);
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

        rec.setVectorState(SEW::E64, rec.maxVlen() / 64);
        as.VMV(v0, 0b10);
        as.VMERGE(dst, src, dst);

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

    rec.setVectorState(SEW::E64, rec.maxVlen() / 64);
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
    rec.setVectorState(SEW::E8, rec.maxVlen() / 8);
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
        rec.setVectorState(SEW::E64, rec.maxVlen() / 64);
        as.VMV(temp, 0);
    } else {
        rec.setVectorState(SEW::E64, rec.maxVlen() / 64);
        biscuit::GPR mask = rec.scratch();
        as.LI(mask, ~((1ull << (16 - imm)) - 1));
        as.VMV_SX(v0, mask);
        rec.setVectorState(SEW::E8, rec.maxVlen() / 8);
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
        rec.setVectorState(SEW::E64, rec.maxVlen() / 64);
        as.VMV_XS(shift, src);
    }
    rec.setVectorState(SEW::E16, rec.maxVlen() / 16);
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
        rec.setVectorState(SEW::E64, rec.maxVlen() / 64);
        as.VMV_XS(shift, src);
    }
    biscuit::Vec dst = rec.getOperandVec(&operands[0]);
    biscuit::GPR max = rec.scratch();
    biscuit::Label dont_zero;
    rec.setVectorState(SEW::E64, rec.maxVlen() / 64);
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
        rec.setVectorState(SEW::E64, rec.maxVlen() / 64);
        as.VMV_XS(shift, src);
    }
    rec.setVectorState(SEW::E32, rec.maxVlen() / 32);
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
        rec.setVectorState(SEW::E64, rec.maxVlen() / 64);
        as.VMV_XS(shift, src);
    }
    rec.setVectorState(SEW::E32, rec.maxVlen() / 32);
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
        rec.setVectorState(SEW::E64, rec.maxVlen() / 64);
        as.VMV_XS(shift, src);
    }
    rec.setVectorState(SEW::E16, rec.maxVlen() / 16);
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
        rec.setVectorState(SEW::E64, rec.maxVlen() / 64);
        as.VMV_XS(shift, src);
    }
    biscuit::Vec dst = rec.getOperandVec(&operands[0]);
    rec.setVectorState(SEW::E64, rec.maxVlen() / 64);
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
    u8 shift = rec.getImmediate(&operands[1]);
    biscuit::Vec dst = rec.getOperandVec(&operands[0]);
    rec.setVectorState(SEW::E16, rec.maxVlen() / 16);
    if (shift > 15)
        shift = 15;
    as.VSRA(dst, dst, shift);
    rec.setOperandVec(&operands[0], dst);
}

FAST_HANDLE(PSRAD) {
    u8 shift = rec.getImmediate(&operands[1]);
    biscuit::Vec dst = rec.getOperandVec(&operands[0]);
    rec.setVectorState(SEW::E32, rec.maxVlen() / 32);
    if (shift > 31)
        shift = 31;
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
    biscuit::GPR af = rec.flagW(X86_REF_AF);
    biscuit::GPR sf = rec.flagW(X86_REF_SF);
    biscuit::GPR of = rec.flagW(X86_REF_OF);

    as.LI(of, 0);
    as.LI(af, 0);
    as.LI(sf, 0);

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

    rec.setVectorState(SEW::E8, rec.maxVlen() / 8);
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

    rec.setVectorState(SEW::E16, rec.maxVlen() / 16);
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

    rec.setVectorState(SEW::E32, rec.maxVlen() / 32);
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

    rec.setVectorState(SEW::E64, rec.maxVlen() / 64);
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

    rec.setVectorState(SEW::E8, rec.maxVlen() / 8);
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

    rec.setVectorState(SEW::E16, rec.maxVlen() / 16);
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

    rec.setVectorState(SEW::E32, rec.maxVlen() / 32);
    as.VSLIDEDOWN(temp, src, imm);
    as.VMV_XS(result, temp);

    rec.setOperandGPR(&operands[0], result);
}

FAST_HANDLE(PEXTRQ) {
    biscuit::Vec temp = rec.scratchVec();
    biscuit::Vec src = rec.getOperandVec(&operands[1]);
    biscuit::GPR result = rec.scratch();
    u8 imm = rec.getImmediate(&operands[2]) & 0b1;

    rec.setVectorState(SEW::E64, rec.maxVlen() / 64);
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
    biscuit::GPR result = rec.scratch();
    biscuit::GPR dst = rec.scratch();

    switch (size) {
    case X86_SIZE_DWORD: {
        biscuit::Label not_equal;
        biscuit::Label start;
        biscuit::GPR scratch = rec.scratch();
        as.Bind(&start);
        as.LR_W(Ordering::AQRL, dst, address);
        as.ZEXTW(dst, dst); // LR sign extends
        as.BNE(dst, rax, &not_equal);
        as.SC_W(Ordering::AQRL, scratch, src, address);
        as.BNEZ(scratch, &start);
        as.Bind(&not_equal);
        rec.popScratch();
        break;
    }
    case X86_SIZE_QWORD: {
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
        break;
    }
    default: {
        UNREACHABLE();
        break;
    }
    }

    as.SUB(result, rax, dst);

    SetCmpFlags(meta, rec, as, dst, src, result, size);

    biscuit::Label end, equal;
    as.BEQ(dst, rax, &equal);

    // Not equal
    rec.setRefGPR(X86_REF_RAX, size, dst);

    // The SC instruction already wrote to memory
    as.Bind(&equal);
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

    SetCmpFlags(meta, rec, as, dst, src, result, size);

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
    biscuit::Vec temp = rec.scratchVec();
    biscuit::Vec dst = rec.getOperandVec(&operands[0]);
    biscuit::Vec src = rec.getOperandVec(&operands[1]);
    rec.setVectorState(sew, vlen);
    (as.*func)(temp, dst, src, VecMask::No);

    if (sew == SEW::E32) {
        rec.setVectorState(SEW::E32, rec.maxVlen() / 32);
    } else if (sew == SEW::E64) {
        rec.setVectorState(SEW::E64, rec.maxVlen() / 64);
    } else {
        UNREACHABLE();
    }

    biscuit::Vec result = rec.scratchVec();
    as.VMV(v0, 1);
    as.VMERGE(result, dst, temp);

    rec.setOperandVec(&operands[0], result);
}

void SCALAR(Recompiler& rec, const HandlerMetadata& meta, Assembler& as, ZydisDecodedInstruction& instruction, ZydisDecodedOperand* operands, SEW sew,
            u8 vlen, void (Assembler::*func)(Vec, Vec, VecMask)) {
    biscuit::Vec temp = rec.scratchVec();
    biscuit::Vec dst = rec.getOperandVec(&operands[0]);
    biscuit::Vec src = rec.getOperandVec(&operands[1]);
    rec.setVectorState(sew, vlen);
    (as.*func)(temp, src, VecMask::No);

    if (sew == SEW::E32) {
        rec.setVectorState(SEW::E32, rec.maxVlen() / 32);
    } else if (sew == SEW::E64) {
        rec.setVectorState(SEW::E64, rec.maxVlen() / 64);
    } else {
        UNREACHABLE();
    }

    biscuit::Vec result = rec.scratchVec();
    as.VMV(v0, 1);
    as.VMERGE(result, dst, temp);

    rec.setOperandVec(&operands[0], result);
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

    rec.setVectorState(SEW::E32, rec.maxVlen() / 32, LMUL::MF2);
    as.VFNCVT_F_F(result, src);

    as.VMV(v0, 0b1100);
    as.VAND(result, result, 0, VecMask::Yes);

    rec.setOperandVec(&operands[0], result);
}

FAST_HANDLE(CVTPS2PD) { // Fuzzed, inaccuracies with NaNs
    biscuit::Vec result = rec.scratchVec();
    biscuit::Vec src = rec.getOperandVec(&operands[1]);

    rec.setVectorState(SEW::E32, rec.maxVlen() / 32, LMUL::MF2);
    as.VFWCVT_F_F(result, src);

    rec.setOperandVec(&operands[0], result);
}

FAST_HANDLE(CVTTPS2DQ) { // Fuzzed, returns 0x7FFF'FFFF instead of 0x8000'0000
    biscuit::Vec result = rec.scratchVec();
    biscuit::Vec src = rec.getOperandVec(&operands[1]);

    rec.setVectorState(SEW::E32, rec.maxVlen() / 32);
    as.VFCVT_RTZ_X_F(result, src);

    rec.setOperandVec(&operands[0], result);
}

FAST_HANDLE(CVTPS2DQ) {
    biscuit::Vec result = rec.scratchVec();
    biscuit::Vec src = rec.getOperandVec(&operands[1]);

    rec.setVectorState(SEW::E32, rec.maxVlen() / 32);
    as.VFCVT_X_F(result, src);

    rec.setOperandVec(&operands[0], result);
}

FAST_HANDLE(CVTTPD2DQ) { // Fuzzed, same problem as cvttps2dq
    biscuit::Vec result = rec.scratchVec();
    biscuit::Vec src = rec.getOperandVec(&operands[1]);

    rec.setVectorState(SEW::E32, rec.maxVlen() / 32, LMUL::MF2);
    as.VFNCVT_RTZ_X_F(result, src);

    as.VMV(v0, 0b1100);
    as.VAND(result, result, 0, VecMask::Yes);

    rec.setOperandVec(&operands[0], result);
}

FAST_HANDLE(CVTPD2DQ) {
    biscuit::Vec result = rec.scratchVec();
    biscuit::Vec src = rec.getOperandVec(&operands[1]);

    rec.setVectorState(SEW::E32, rec.maxVlen() / 32, LMUL::MF2);
    as.VFNCVT_X_F(result, src);

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
        biscuit::Vec result = rec.scratchVec();
        rec.setVectorState(SEW::E32, rec.maxVlen() / 32);
        as.VMV(v0, 1);
        if (operands[1].type == ZYDIS_OPERAND_TYPE_MEMORY) {
            // Only when src is memory are the upper bits zeroed
            as.VMV(result, 0);
            as.VOR(result, src, 0, VecMask::Yes);
        } else if (operands[1].type == ZYDIS_OPERAND_TYPE_REGISTER) {
            as.VMERGE(result, dst, src);
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
    biscuit::GPR ones = rec.scratch();
    as.LI(ones, 0x3F800000);
    as.VMV(temp, ones);
    as.VFDIV(temp, temp, src);

    rec.setVectorState(SEW::E32, rec.maxVlen() / 32);

    biscuit::Vec result = rec.scratchVec();
    as.VMV(v0, 1);
    as.VMERGE(result, dst, temp);

    rec.setOperandVec(&operands[0], result);
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
    as.VFDIV(temp, temp, temp2);

    rec.setVectorState(SEW::E32, rec.maxVlen() / 32);

    biscuit::Vec result = rec.scratchVec();
    as.VMV(v0, 1);
    as.VMERGE(result, dst, temp);

    rec.setOperandVec(&operands[0], result);
}

FAST_HANDLE(MOVLHPS) { // TODO: vmerge
    biscuit::Vec temp = rec.scratchVec();
    biscuit::Vec iota = rec.scratchVec();
    biscuit::Vec dst = rec.getOperandVec(&operands[0]);
    biscuit::Vec src = rec.getOperandVec(&operands[1]);
    rec.setVectorState(SEW::E64, rec.maxVlen() / 64);
    as.VMV(v0, 0b10);
    as.VMV(temp, dst);
    as.VMV(iota, 0);
    as.VRGATHER(temp, src, iota, VecMask::Yes); // make only high element pick low from src
    rec.setOperandVec(&operands[0], temp);
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
            as.AMOADD_W(Ordering::AQRL, dst, src, address);
        } else if (instruction.operand_width == 64) {
            as.AMOADD_D(Ordering::AQRL, dst, src, address);
        } else {
            UNREACHABLE();
        }

        // Still perform the addition in registers to calculate the flags
        // AMOADD stores the loaded value in Rd
        as.ADD(result, dst, src);
        rec.setOperandGPR(&operands[1], dst);
        rec.popScratch(); // pop LEA scratch
        rec.popScratch();
        writeback = false;
    } else {
        if (needs_atomic) {
            WARN("Atomic XADD with 8 or 16 bit operands encountered");
        }

        dst = rec.getOperandGPR(&operands[0]);
        as.ADD(result, dst, src);
        rec.setOperandGPR(&operands[1], dst);
    }

    x86_size_e size = rec.getOperandSize(&operands[0]);

    if (rec.shouldEmitFlag(meta.rip, X86_REF_CF)) {
        rec.updateCarryAdd(dst, result, size);
    }

    if (rec.shouldEmitFlag(meta.rip, X86_REF_PF)) {
        rec.updateParity(result);
    }

    if (rec.shouldEmitFlag(meta.rip, X86_REF_AF)) {
        rec.updateAuxiliaryAdd(dst, src);
    }

    if (rec.shouldEmitFlag(meta.rip, X86_REF_ZF)) {
        rec.updateZero(result, size);
    }

    if (rec.shouldEmitFlag(meta.rip, X86_REF_SF)) {
        rec.updateSign(result, size);
    }

    if (rec.shouldEmitFlag(meta.rip, X86_REF_OF)) {
        rec.updateOverflowAdd(dst, src, result, size);
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

    rec.setVectorState(SEW::E32, rec.maxVlen() / 32, LMUL::MF2);
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

    rec.setVectorState(SEW::E32, rec.maxVlen() / 32);
    as.VSLIDEDOWN(tmp, src, imm);
    as.VMV_XS(dst, tmp);

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
        as.VSLIDEDOWN(tmp, src, count_s);
    } else {
        as.VMV(tmp, src);
    }

    if (count_d != 0) {
        as.VSLIDEUP(tmp2, tmp, count_d);
    } else {
        as.VMV(tmp2, tmp);
    }

    u8 mask = 1 << count_d;
    as.VMV(v0, mask);
    as.VMERGE(result, dst, tmp2);

    as.VMV(v0, zmask);
    as.VXOR(result_masked, result, result, VecMask::Yes);

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
    as.ADDI(rsp, rsp, -8);
    rec.setRefGPR(X86_REF_RSP, X86_SIZE_QWORD, rsp);
    rec.writeMemory(src, rsp, 0, X86_SIZE_QWORD);
}

FAST_HANDLE(POPFQ) {
    biscuit::GPR flags = rec.scratch();
    biscuit::GPR rsp = rec.getRefGPR(X86_REF_RSP, X86_SIZE_QWORD);
    as.LD(flags, 0, rsp);
    as.ADDI(rsp, rsp, 8);
    rec.setRefGPR(X86_REF_RSP, X86_SIZE_QWORD, rsp);

    biscuit::GPR cf = rec.flagW(X86_REF_CF);
    biscuit::GPR af = rec.flagW(X86_REF_AF);
    biscuit::GPR zf = rec.flagW(X86_REF_ZF);
    biscuit::GPR sf = rec.flagW(X86_REF_SF);
    biscuit::GPR of = rec.flagW(X86_REF_OF);
    biscuit::GPR temp = rec.scratch();

    as.ANDI(cf, flags, 1);

    biscuit::GPR pf = rec.scratch();
    as.SRLI(pf, flags, 2);
    as.ANDI(pf, pf, 1);
    as.SB(pf, offsetof(ThreadState, pf), rec.threadStatePointer());

    as.SRLI(af, flags, 4);
    as.ANDI(af, af, 1);

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

FAST_HANDLE(MOVDDUP) {
    biscuit::Vec result = rec.scratchVec();
    biscuit::Vec iota = rec.scratchVec();
    biscuit::Vec src = rec.getOperandVec(&operands[1]);

    rec.setVectorState(SEW::E64, rec.maxVlen() / 64);
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
    rec.setVectorState(SEW::E8, rec.maxVlen() / 8);
    as.VAADDU(dst, dst, src);
    rec.setOperandVec(&operands[0], dst);
}

FAST_HANDLE(PAVGW) {
    biscuit::Vec dst = rec.getOperandVec(&operands[0]);
    biscuit::Vec src = rec.getOperandVec(&operands[1]);
    rec.setVectorState(SEW::E16, rec.maxVlen() / 16);
    as.VAADDU(dst, dst, src);
    rec.setOperandVec(&operands[0], dst);
}