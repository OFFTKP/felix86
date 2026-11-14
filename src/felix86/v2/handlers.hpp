#pragma once

#include <Zydis/Zydis.h>
#include "felix86/common/types.hpp"

struct Recompiler;
struct CompileState;

namespace biscuit {
class Assembler;
}

using HandlerPtr = void (*)(Recompiler& rec, biscuit::Assembler& as, const CompileState& state);

struct Handlers {
    static void initialize();

    static bool isImplemented(ZydisMnemonic mnemonic);

#define X(name) static HandlerPtr ptr_##name;
#include "mnemonics.inc"
#undef X
};