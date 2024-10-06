#include "felix86/backend/emitter.hpp"

// Dispatch to correct function
void Emitter::Emit(biscuit::Assembler& as, const IRInstruction& instruction) {
#define X(stuff)                                                                                                                                     \
    case IROpcode::stuff: {                                                                                                                          \
        return Emit##stuff(as, instruction);                                                                                                         \
        break;                                                                                                                                       \
    }

    switch (instruction.GetOpcode()) {
        IR_OPCODES
    default: {
        UNREACHABLE();
    }
    }

#undef X
}

void Emitter::EmitNull(biscuit::Assembler& as, const IRInstruction& inst) {
    UNREACHABLE();
}

void Emitter::EmitPhi(biscuit::Assembler& as, const IRInstruction& inst) {
    UNREACHABLE();
}

void Emitter::EmitComment(biscuit::Assembler& as, const IRInstruction& inst) {
    UNREACHABLE();
}

void Emitter::EmitMov(biscuit::Assembler& as, const IRInstruction& inst) {
    UNREACHABLE();
}

void Emitter::EmitImmediate(biscuit::Assembler& as, const IRInstruction& inst) {
    UNREACHABLE();
}

void Emitter::EmitPopcount(biscuit::Assembler& as, const IRInstruction& inst) {
    UNREACHABLE();
}

void Emitter::EmitSext8(biscuit::Assembler& as, const IRInstruction& inst) {
    UNREACHABLE();
}

void Emitter::EmitSext16(biscuit::Assembler& as, const IRInstruction& inst) {
    UNREACHABLE();
}

void Emitter::EmitSext32(biscuit::Assembler& as, const IRInstruction& inst) {
    UNREACHABLE();
}

void Emitter::EmitSyscall(biscuit::Assembler& as, const IRInstruction& inst) {
    UNREACHABLE();
}

void Emitter::EmitCpuid(biscuit::Assembler& as, const IRInstruction& inst) {
    UNREACHABLE();
}

void Emitter::EmitRdtsc(biscuit::Assembler& as, const IRInstruction& inst) {
    UNREACHABLE();
}

void Emitter::EmitGetGuest(biscuit::Assembler& as, const IRInstruction& inst) {
    UNREACHABLE();
}

void Emitter::EmitSetGuest(biscuit::Assembler& as, const IRInstruction& inst) {
    UNREACHABLE();
}

void Emitter::EmitLoadGuestFromMemory(biscuit::Assembler& as, const IRInstruction& inst) {
    UNREACHABLE();
}

void Emitter::EmitStoreGuestToMemory(biscuit::Assembler& as, const IRInstruction& inst) {
    UNREACHABLE();
}

void Emitter::EmitPushHost(biscuit::Assembler& as, const IRInstruction& inst) {
    const PushHost& push_host = inst.AsPushHost();
    biscuit::GPR vm_state = backend.GetRegisters().AcquireScratchGPR();
    as.LI(vm_state, backend.GetVMStatePointer());
    switch (push_host.ref) {
    case RISCV_REF_X0 ... RISCV_REF_X31: {
        u32 index = push_host.ref - RISCV_REF_X0;
        u32 offset = index * sizeof(u64);
        biscuit::GPR to_push = biscuit::GPR(index);
        as.SD(to_push, offset, vm_state);
        break;
    }
    case RISCV_REF_F0 ... RISCV_REF_F31: {
        u32 index = push_host.ref - RISCV_REF_F0;
        u32 offset = index * sizeof(double) + (32 * sizeof(u64));
        biscuit::FPR to_push = biscuit::FPR(index);
        as.FSD(to_push, offset, vm_state);
        break;
    }
    case RISCV_REF_VEC0 ... RISCV_REF_VEC31: {
        ERROR("Implme");
        break;
    }
    default: {
        UNREACHABLE();
    }
    }
}

void Emitter::EmitPopHost(biscuit::Assembler& as, const IRInstruction& inst) {
    const PopHost& pop_host = inst.AsPopHost();
    biscuit::GPR vm_state = backend.GetRegisters().AcquireScratchGPR();
    as.LI(vm_state, backend.GetVMStatePointer());
    switch (pop_host.ref) {
    case RISCV_REF_X0 ... RISCV_REF_X31: {
        u32 index = pop_host.ref - RISCV_REF_X0;
        u32 offset = index * sizeof(u64);
        biscuit::GPR to_pop = biscuit::GPR(index);
        biscuit::GPR base = vm_state;
        as.LD(to_pop, offset, base);
        break;
    }
    case RISCV_REF_F0 ... RISCV_REF_F31: {
        u32 index = pop_host.ref - RISCV_REF_F0;
        u32 offset = index * sizeof(double) + (32 * sizeof(u64));
        biscuit::FPR to_pop = biscuit::FPR(index);
        biscuit::GPR base = vm_state;
        as.FLD(to_pop, offset, base);
        break;
    }
    case RISCV_REF_VEC0 ... RISCV_REF_VEC31: {
        ERROR("Implme");
        break;
    }
    default: {
        UNREACHABLE();
    }
    }
}

void Emitter::EmitAdd(biscuit::Assembler& as, const IRInstruction& inst) {
    UNREACHABLE();
}

void Emitter::EmitSub(biscuit::Assembler& as, const IRInstruction& inst) {
    UNREACHABLE();
}

void Emitter::EmitDivu(biscuit::Assembler& as, const IRInstruction& inst) {
    UNREACHABLE();
}

void Emitter::EmitDiv(biscuit::Assembler& as, const IRInstruction& inst) {
    UNREACHABLE();
}

void Emitter::EmitRemu(biscuit::Assembler& as, const IRInstruction& inst) {
    UNREACHABLE();
}

void Emitter::EmitRem(biscuit::Assembler& as, const IRInstruction& inst) {
    UNREACHABLE();
}

void Emitter::EmitDivuw(biscuit::Assembler& as, const IRInstruction& inst) {
    UNREACHABLE();
}

void Emitter::EmitDivw(biscuit::Assembler& as, const IRInstruction& inst) {
    UNREACHABLE();
}

void Emitter::EmitRemuw(biscuit::Assembler& as, const IRInstruction& inst) {
    UNREACHABLE();
}

void Emitter::EmitRemw(biscuit::Assembler& as, const IRInstruction& inst) {
    UNREACHABLE();
}

void Emitter::EmitDiv128(biscuit::Assembler& as, const IRInstruction& inst) {
    UNREACHABLE();
}

void Emitter::EmitDivu128(biscuit::Assembler& as, const IRInstruction& inst) {
    UNREACHABLE();
}

void Emitter::EmitMul(biscuit::Assembler& as, const IRInstruction& inst) {
    UNREACHABLE();
}

void Emitter::EmitMulh(biscuit::Assembler& as, const IRInstruction& inst) {
    UNREACHABLE();
}

void Emitter::EmitMulhu(biscuit::Assembler& as, const IRInstruction& inst) {
    UNREACHABLE();
}

void Emitter::EmitClz(biscuit::Assembler& as, const IRInstruction& inst) {
    UNREACHABLE();
}

void Emitter::EmitCtz(biscuit::Assembler& as, const IRInstruction& inst) {
    UNREACHABLE();
}

void Emitter::EmitShiftLeft(biscuit::Assembler& as, const IRInstruction& inst) {
    UNREACHABLE();
}

void Emitter::EmitShiftRight(biscuit::Assembler& as, const IRInstruction& inst) {
    UNREACHABLE();
}

void Emitter::EmitShiftRightArithmetic(biscuit::Assembler& as, const IRInstruction& inst) {
    UNREACHABLE();
}

void Emitter::EmitLeftRotate8(biscuit::Assembler& as, const IRInstruction& inst) {
    UNREACHABLE();
}

void Emitter::EmitLeftRotate16(biscuit::Assembler& as, const IRInstruction& inst) {
    UNREACHABLE();
}

void Emitter::EmitLeftRotate32(biscuit::Assembler& as, const IRInstruction& inst) {
    UNREACHABLE();
}

void Emitter::EmitLeftRotate64(biscuit::Assembler& as, const IRInstruction& inst) {
    UNREACHABLE();
}

void Emitter::EmitSelect(biscuit::Assembler& as, const IRInstruction& inst) {
    UNREACHABLE();
}

void Emitter::EmitAnd(biscuit::Assembler& as, const IRInstruction& inst) {
    UNREACHABLE();
}

void Emitter::EmitOr(biscuit::Assembler& as, const IRInstruction& inst) {
    UNREACHABLE();
}

void Emitter::EmitXor(biscuit::Assembler& as, const IRInstruction& inst) {
    UNREACHABLE();
}

void Emitter::EmitNot(biscuit::Assembler& as, const IRInstruction& inst) {
    UNREACHABLE();
}

void Emitter::EmitLea(biscuit::Assembler& as, const IRInstruction& inst) {
    UNREACHABLE();
}

void Emitter::EmitEqual(biscuit::Assembler& as, const IRInstruction& inst) {
    UNREACHABLE();
}

void Emitter::EmitNotEqual(biscuit::Assembler& as, const IRInstruction& inst) {
    UNREACHABLE();
}

void Emitter::EmitIGreaterThan(biscuit::Assembler& as, const IRInstruction& inst) {
    UNREACHABLE();
}

void Emitter::EmitILessThan(biscuit::Assembler& as, const IRInstruction& inst) {
    UNREACHABLE();
}

void Emitter::EmitUGreaterThan(biscuit::Assembler& as, const IRInstruction& inst) {
    UNREACHABLE();
}

void Emitter::EmitULessThan(biscuit::Assembler& as, const IRInstruction& inst) {
    UNREACHABLE();
}

void Emitter::EmitReadByte(biscuit::Assembler& as, const IRInstruction& inst) {
    UNREACHABLE();
}

void Emitter::EmitReadWord(biscuit::Assembler& as, const IRInstruction& inst) {
    UNREACHABLE();
}

void Emitter::EmitReadDWord(biscuit::Assembler& as, const IRInstruction& inst) {
    UNREACHABLE();
}

void Emitter::EmitReadQWord(biscuit::Assembler& as, const IRInstruction& inst) {
    UNREACHABLE();
}

void Emitter::EmitReadXmmWord(biscuit::Assembler& as, const IRInstruction& inst) {
    UNREACHABLE();
}

void Emitter::EmitWriteByte(biscuit::Assembler& as, const IRInstruction& inst) {
    UNREACHABLE();
}

void Emitter::EmitWriteWord(biscuit::Assembler& as, const IRInstruction& inst) {
    UNREACHABLE();
}

void Emitter::EmitWriteDWord(biscuit::Assembler& as, const IRInstruction& inst) {
    UNREACHABLE();
}

void Emitter::EmitWriteQWord(biscuit::Assembler& as, const IRInstruction& inst) {
    UNREACHABLE();
}

void Emitter::EmitWriteXmmWord(biscuit::Assembler& as, const IRInstruction& inst) {
    UNREACHABLE();
}

void Emitter::EmitCastIntegerToVector(biscuit::Assembler& as, const IRInstruction& inst) {
    UNREACHABLE();
}

void Emitter::EmitCastVectorToInteger(biscuit::Assembler& as, const IRInstruction& inst) {
    UNREACHABLE();
}

void Emitter::EmitVInsertInteger(biscuit::Assembler& as, const IRInstruction& inst) {
    UNREACHABLE();
}

void Emitter::EmitVExtractInteger(biscuit::Assembler& as, const IRInstruction& inst) {
    UNREACHABLE();
}

void Emitter::EmitVUnpackByteLow(biscuit::Assembler& as, const IRInstruction& inst) {
    UNREACHABLE();
}

void Emitter::EmitVUnpackWordLow(biscuit::Assembler& as, const IRInstruction& inst) {
    UNREACHABLE();
}

void Emitter::EmitVUnpackDWordLow(biscuit::Assembler& as, const IRInstruction& inst) {
    UNREACHABLE();
}

void Emitter::EmitVUnpackQWordLow(biscuit::Assembler& as, const IRInstruction& inst) {
    UNREACHABLE();
}

void Emitter::EmitVAnd(biscuit::Assembler& as, const IRInstruction& inst) {
    UNREACHABLE();
}

void Emitter::EmitVOr(biscuit::Assembler& as, const IRInstruction& inst) {
    UNREACHABLE();
}

void Emitter::EmitVXor(biscuit::Assembler& as, const IRInstruction& inst) {
    UNREACHABLE();
}

void Emitter::EmitVShr(biscuit::Assembler& as, const IRInstruction& inst) {
    UNREACHABLE();
}

void Emitter::EmitVShl(biscuit::Assembler& as, const IRInstruction& inst) {
    UNREACHABLE();
}

void Emitter::EmitVPackedSubByte(biscuit::Assembler& as, const IRInstruction& inst) {
    UNREACHABLE();
}

void Emitter::EmitVPackedAddQWord(biscuit::Assembler& as, const IRInstruction& inst) {
    UNREACHABLE();
}

void Emitter::EmitVPackedEqualByte(biscuit::Assembler& as, const IRInstruction& inst) {
    UNREACHABLE();
}

void Emitter::EmitVPackedEqualWord(biscuit::Assembler& as, const IRInstruction& inst) {
    UNREACHABLE();
}

void Emitter::EmitVPackedEqualDWord(biscuit::Assembler& as, const IRInstruction& inst) {
    UNREACHABLE();
}

void Emitter::EmitVPackedShuffleDWord(biscuit::Assembler& as, const IRInstruction& inst) {
    UNREACHABLE();
}

void Emitter::EmitVMoveByteMask(biscuit::Assembler& as, const IRInstruction& inst) {
    UNREACHABLE();
}

void Emitter::EmitVPackedMinByte(biscuit::Assembler& as, const IRInstruction& inst) {
    UNREACHABLE();
}

void Emitter::EmitVZext64(biscuit::Assembler& as, const IRInstruction& inst) {
    UNREACHABLE();
}

void Emitter::EmitCount(biscuit::Assembler& as, const IRInstruction& inst) {
    UNREACHABLE();
}
