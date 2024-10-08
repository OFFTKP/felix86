#include "felix86/common/log.hpp"
#include "felix86/ir/opcode.hpp"

std::string GetOpcodeString(IROpcode opcode) {
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