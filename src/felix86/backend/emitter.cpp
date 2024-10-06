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

void Emitter::EmitNull(biscuit::Assembler&, const IRInstruction&) {
    UNREACHABLE();
}

void Emitter::EmitPhi(biscuit::Assembler&, const IRInstruction&) {
    UNREACHABLE();
}

void Emitter::EmitPushHost(biscuit::Assembler& as, const IRInstruction& inst) {
    const PushHost& push_host = inst.AsPushHost();
    switch (push_host.ref) {
    case RISCV_REF_X0 ... RISCV_REF_X31: {
        u32 index = push_host.ref - RISCV_REF_X0;
        u32 offset = index * sizeof(u64);
        biscuit::GPR to_push = biscuit::GPR(index);
        biscuit::GPR base = Registers::VMStatePointer();
        as.SD(to_push, offset, base);
        break;
    }
    case RISCV_REF_F0 ... RISCV_REF_F31: {
        u32 index = push_host.ref - RISCV_REF_F0;
        u32 offset = index * sizeof(double) + (32 * sizeof(u64));
        biscuit::FPR to_push = biscuit::FPR(index);
        biscuit::GPR base = Registers::VMStatePointer();
        as.FSD(to_push, offset, base);
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
    switch (pop_host.ref) {
    case RISCV_REF_X0 ... RISCV_REF_X31: {
        u32 index = pop_host.ref - RISCV_REF_X0;
        u32 offset = index * sizeof(u64);
        biscuit::GPR to_pop = biscuit::GPR(index);
        biscuit::GPR base = Registers::VMStatePointer();
        as.LD(to_pop, offset, base);
        break;
    }
    case RISCV_REF_F0 ... RISCV_REF_F31: {
        u32 index = pop_host.ref - RISCV_REF_F0;
        u32 offset = index * sizeof(double) + (32 * sizeof(u64));
        biscuit::FPR to_pop = biscuit::FPR(index);
        biscuit::GPR base = Registers::VMStatePointer();
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