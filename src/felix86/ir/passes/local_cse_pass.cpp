#include "felix86/ir/passes/passes.hpp"

bool PassManager::localCSEPassBlock(IRBlock* block) {
    bool changed = false;

    std::vector<SSAInstruction*> instructions;
    for (auto& inst : block->GetInstructions()) {
        if (!inst.IsLocked() && !inst.IsRead()) {
            bool replaced = false;
            for (auto other : instructions) {
                if (inst.IsSameExpression(*other)) {
                    replaced = true;
                    changed = true;
                    inst.ReplaceWithMov(other);
                    break;
                }
            }

            if (!replaced) {
                instructions.push_back(&inst);
            }
        }
    }

    return changed;
}

bool PassManager::LocalCSEPass(IRFunction* function) {
    bool changed = false;
    for (IRBlock* block : function->GetBlocks()) {
        changed |= localCSEPassBlock(block);
    }
    return changed;
}