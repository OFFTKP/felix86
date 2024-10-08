#include "felix86/ir/passes/passes.hpp"

void ir_replace_setguest_pass(IRFunction* function) {
    for (IRBlock* block : function->GetBlocksPostorder()) {
        std::list<SSAInstruction>& insts = block->GetInstructions();
        for (auto& inst : insts) {
            if (inst.GetOpcode() == IROpcode::SetGuest) {
                inst.ReplaceExpressionWithMov(inst.AsSetGuest().source);
            }
        }
    }
}