#pragma once

#include <utility>
#include <vector>
#include <Zydis/Zydis.h>
#include "felix86/common/zydis_types.hpp"

// TODO: fuse with the one in recompiler.hpp in separate file
using Operands = ZydisDecodedOperand[ZYDIS_MAX_OPERAND_COUNT];
using Pair = std::pair<ZydisDecodedInstruction, Operands>;
using InstructionVector = std::vector<Pair>;

namespace Optimizer {
void pass(u64 start_rip, InstructionVector& instructions, const FlagAccessData& flag_access);
} // namespace Optimizer
