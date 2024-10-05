#include "felix86/backend/emitter.hpp"

// Dispatch to correct function
void Emitter::Emit(const IRInstruction& instruction) {
#define X(stuff)                                                                                                                                     \
    case IROpcode::stuff: {                                                                                                                          \
        return Emit##stuff(instruction);                                                                                                             \
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

void Emitter::EmitNull(const IRInstruction&) {
    UNREACHABLE();
}

void Emitter::EmitPhi(const IRInstruction&) {
    UNREACHABLE();
}
