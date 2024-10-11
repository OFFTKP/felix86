#include "felix86/ir/passes/passes.hpp"

// Going backwards can expose more dead code upwards as uses are removed
void ir_dead_code_elimination_pass(IRFunction* function) {
    for (IRBlock* block : function->GetBlocksPostorder()) {
        auto& insts = block->GetInstructions();
        auto it = insts.rbegin();
        while (it != insts.rend()) {
            if (it->GetUseCount() == 0 && !it->IsLocked()) {
                it->Invalidate();
                insts.erase(--(it.base()));
            } else {
                ++it;
            }
        }
    }
}