#include "felix86/common/log.hpp"
#include "felix86/ir/opcode.hpp"

std::string Opcode::GetOpcodeString(IROpcode opcode) {
    switch (opcode) {
#define X(stuff)                                                                                                                                     \
    case IROpcode::stuff:                                                                                                                            \
        return #stuff;
        IR_OPCODES
#undef X
    default:
        UNREACHABLE();
    }
}

bool Opcode::IsAuxiliary(IROpcode opcode) {
    switch (opcode) {
    case IROpcode::Null: {
        UNREACHABLE();
        break;
    }
    case IROpcode::Phi:
    case IROpcode::SetGuest:
    case IROpcode::GetGuest:
    case IROpcode::StoreGuestToMemory:
    case IROpcode::LoadGuestFromMemory:
    case IROpcode::Comment: {
        return true;
    }
    default: {
        return false;
    }
    }
}