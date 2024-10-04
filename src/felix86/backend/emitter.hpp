#pragma once

#include "felix86/ir/instruction.hpp"

struct RISCVEmitter {
    void Emit(const IRInstruction& instruction);

private:
#define X(stuff) void Emit##stuff(const IRInstruction& instruction);
    IR_OPCODES
#undef X
};