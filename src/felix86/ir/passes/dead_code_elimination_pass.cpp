#include "felix86/ir/passes/passes.hpp"

// Going backwards can expose more dead code upwards as uses are removed
bool PassManager::DeadCodeEliminationPass(IRFunction* function) {
    bool changed = false;
    for (IRBlock* block : function->GetBlocksPostorder()) {
        auto& insts = block->GetInstructions();
        auto it = insts.rbegin();
        while (it != insts.rend()) {
            if (it->GetUseCount() == 0 && !it->IsLocked()) {
                it->Invalidate();
                insts.erase(--(it.base()));
                changed = true;
            } else {
                ++it;
            }
        }
    }
    return changed;
}