#include "felix86/ir/passes/passes.hpp"

void ir_dead_code_elimination_pass(IRFunction* function) {
    for (IRBlock* block : function->GetBlocksPostorder()) {
        auto& insts = block->GetInstructions();
        auto it = insts.rbegin();
        while (it != insts.rend()) {
            if (it->GetUseCount() == 0 && !it->IsLocked()) {
                it->Invalidate();
                printf("Erased: %s\n", it->GetNameString().c_str());
                insts.erase(--(it.base()));
            } else {
                ++it;
            }
        }
    }
}