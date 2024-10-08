#include <fmt/format.h>
#include "felix86/common/log.hpp"
#include "felix86/ir/block.hpp"

std::string IRBlock::Print(const std::function<std::string(const IRInstruction*)>& callback) const {
    std::string ret;

    if (GetIndex() == 0) {
        ret += "Entry Block";
    } else if (GetIndex() == 1) {
        ret += "Exit Block";
    } else {
        ret += fmt::format("Block {}", GetIndex() - 2);
    }
    if (GetStartAddress() != IR_NO_ADDRESS) {
        ret += fmt::format(" @ 0x{:016x}", GetStartAddress());
    }
    ret += "\n";

    for (const IRInstruction& inst : GetInstructions()) {
        ret += inst.Print(callback);
        ret += "\n";
    }

    switch (GetTermination()) {
    case Termination::Jump: {
        ret += fmt::format("Termination -> Jump to Block {}\n", GetSuccessor(0)->GetIndex() - 2);
        break;
    }
    case Termination::JumpConditional: {
        ret += fmt::format("Termination -> JumpConditional ({}) to Block {} or Block {}\n", GetCondition()->GetNameString(),
                           GetSuccessor(0)->GetIndex() - 2, GetSuccessor(1)->GetIndex() - 2);
        break;
    }
    case Termination::Exit: {
        ret += "Termination -> Exit\n";
        break;
    }
    case Termination::Null: {
        ret += "Termination -> Null\n";
        break;
    }
    }

    ret += "\n";

    return ret;
}

IRInstruction* IRBlock::InsertAtEnd(IRInstruction&& instr) {
    instructions.push_back(std::move(instr));
    IRInstruction* ret = &instructions.back();
    ret->SetName((GetIndex() << 20) | next_name++);
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