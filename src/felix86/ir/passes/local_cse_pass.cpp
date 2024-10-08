#include "felix86/ir/passes/passes.hpp"

void ir_local_cse_pass(IRFunction* function) {
    for (IRBlock* block : function->GetBlocks()) {
        std::vector<SSAInstruction*> instructions;
        for (auto& inst : block->GetInstructions()) {
            if (!inst.IsLocked()) {
                bool replaced = false;
                for (auto other : instructions) {
                    if (inst.IsSameExpression(*other)) {
                        replaced = true;
                        inst.ReplaceExpressionWithMov(other);
                        break;
                    }
                }

                if (!replaced) {
                    instructions.push_back(&inst);
                }
            }
        }
    }
}