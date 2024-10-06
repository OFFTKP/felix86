#pragma once

#include "biscuit/assembler.hpp"
#include "felix86/ir/instruction.hpp"

struct Backend;

struct Emitter {
    static void Emit(Backend& backend, biscuit::Assembler& assembler, const IRInstruction& instruction);
    static void EmitJump(Backend& backend, biscuit::Assembler& assembler, void* target);
    static void EmitJumpConditional(Backend& backend, biscuit::Assembler& assembler, const IRInstruction& condition, void* target_true,
                                    void* target_false);

private:
#define X(stuff) static void Emit##stuff(Backend& backend, biscuit::Assembler& assembler, const IRInstruction& instruction);
    IR_OPCODES
#undef X
};