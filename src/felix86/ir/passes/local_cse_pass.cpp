#include "felix86/ir/passes/passes.hpp"

bool PassManager::LocalCSEPass(IRFunction* function) {
    bool changed = false;
    for (IRBlock* block : function->GetBlocks()) {
        std::vector<SSAInstruction*> instructions;
        for (auto& inst : block->GetInstructions()) {
            if (!inst.IsLocked()) {
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
    }
    return changed;
}