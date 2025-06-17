#pragma once

#include <Zydis/Zydis.h>
#include "felix86/common/types.hpp"

struct Recompiler;

namespace biscuit {
struct Assembler;
}

using HandlerPtr = void (*)(Recompiler& rec, u64 rip, biscuit::Assembler& as, ZydisDecodedInstruction& instruction, ZydisDecodedOperand* operands);

struct Handlers {
    static void initialize();

#define X(name) static HandlerPtr ptr_##name;
#include "mnemonics.inc"
#undef X
};