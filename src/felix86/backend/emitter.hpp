#pragma once

#include "biscuit/assembler.hpp"
#include "felix86/ir/instruction.hpp"

struct Backend;

struct Emitter {
    static void Emit(Backend& backend, biscuit::Assembler& assembler, const IRInstruction& instruction);

private:
#define X(stuff) static void Emit##stuff(Backend& backend, biscuit::Assembler& assembler, const IRInstruction& instruction);
    IR_OPCODES
#undef X
};