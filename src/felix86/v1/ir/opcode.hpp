#pragma once

#include <string>
#include "felix86/common/utility.hpp"

enum class IROpcode : u8 {
#define X(stuff, ...) stuff,
#include "felix86/ir/opcodes.inc"
#undef X
};

namespace Opcode {
std::string GetOpcodeString(IROpcode opcode);
bool IsAuxiliary(IROpcode opcode);
} // namespace Opcode