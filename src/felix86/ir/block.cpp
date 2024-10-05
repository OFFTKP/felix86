#include <fmt/format.h>
#include "felix86/common/log.hpp"
#include "felix86/common/print.hpp"
#include "felix86/ir/block.hpp"

std::string IRBlock::Print(const std::function<std::string(const IRInstruction*)>& callback) const {
    std::string ret;

    ret += fmt::format("Block {}", GetIndex());
    if (GetStartAddress() != IR_NO_ADDRESS) {
        ret += fmt::format(" @ 0x{:016x}", GetStartAddress());
    }
    ret += "\n";

    for (const IRInstruction& inst : GetInstructions()) {
        ret += inst.Print(callback);
        ret += "\n";
    }

    return ret;
}

IRInstruction* IRBlock::InsertAtEnd(IRInstruction&& instr) {
    instructions.push_back(std::move(instr));
    IRInstruction* ret = &instructions.back();
    return ret;
}

bool IRBlock::IsUsedInPhi(IRInstruction* target) const {
    for (auto& instr : instructions) {
        if (instr.IsPhi()) {
            const Phi& phi = instr.AsPhi();
            for (auto& value : phi.values) {
                if (value == target) {
                    return true;
                }
            }
        }
    }

    return false;
}