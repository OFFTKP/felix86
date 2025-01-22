#include <fmt/format.h>
#include "felix86/common/log.hpp"
#include "felix86/ir/block.hpp"

std::string IRBlock::Print(const std::function<std::string(const SSAInstruction*)>& callback) const {
    std::string ret;

    ret += printBlock() + "\n";

    for (const SSAInstruction& inst : GetInstructions()) {
        ret += inst.Print(callback);
        ret += "\n";
    }

    ret += printTermination() + "\n";

    return ret;
}

std::string IRBlock::printBlock() const {
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
    return ret;
}

std::string IRBlock::printTermination() const {
    std::string ret;
    switch (GetTermination()) {
    case Termination::Jump: {
        ret += fmt::format("Termination -> Jump to Block {}\n", GetSuccessor(0)->GetName());
        break;
    }
    case Termination::JumpConditional: {
        ret += fmt::format("Termination -> JumpConditional ({}) to Block {} or Block {}\n", GetNameString(GetCondition()->GetName()),
                           GetSuccessor(0)->GetName(), GetSuccessor(1)->GetName());
        break;
    }
    case Termination::BackToDispatcher: {
        ret += "Termination -> Back to dispatcher\n";
        break;
    }
    case Termination::Null: {
        ret += "Termination -> Null\n";
        break;
    }
    }
    return ret;
}

SSAInstruction* IRBlock::InsertAtEnd(SSAInstruction&& instr) {
    instructions.push_back(std::move(instr));
    SSAInstruction* ret = &instructions.back();
    ret->SetName(GetNextName());
    return ret;
}

bool IRBlock::IsUsedInPhi(SSAInstruction* target) const {
    for (auto& instr : instructions) {
        if (instr.IsPhi()) {
            const Phi& phi = instr.AsPhi();
            for (auto& value : phi.values) {
                if (value == target) {
                    return true;
                }
            }
        } else {
            break;
        }
    }

    return false;
}