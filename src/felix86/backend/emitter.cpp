#include "felix86/backend/emitter.hpp"

// Dispatch to correct function
void RISCVEmitter::Emit(const IRInstruction& instruction) {
#define X(stuff)                                                                                                                                     \
    case IROpcode::stuff: {                                                                                                                          \
        return Emit##stuff(instruction);                                                                                                             \
        break;                                                                                                                                       \
    }
    switch (instruction.GetOpcode()) {
        IR_OPCODES
    default: {
        ERROR("Unreachable");
    }
    }
#undef X
}

void RISCVEmitter::EmitNull(const IRInstruction&) {
    ERROR("Unreachable");
}

void RISCVEmitter::EmitPhi(const IRInstruction&) {
    ERROR("Unreachable");
}
