#pragma once

#include "biscuit/assembler.hpp"
#include "felix86/ir/instruction.hpp"

struct Emitter {
    void Emit(biscuit::Assembler& assembler, const IRInstruction& instruction);

private:
#define X(stuff) void Emit##stuff(biscuit::Assembler& assembler, const IRInstruction& instruction);
    IR_OPCODES
#undef X
};