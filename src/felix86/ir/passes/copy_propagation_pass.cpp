#include "felix86/ir/passes/passes.hpp"

bool PassManager::copyPropagationPassBlock(IRBlock* block) {
    bool changed = false;
    for (SSAInstruction& inst : block->GetInstructions()) {
        if (inst.GetOpcode() != IROpcode::Mov) {
            changed |= inst.PropagateMovs();
        }
    }
    return changed;
}

bool PassManager::CopyPropagationPass(IRFunction* function) {
    bool changed = false;
    for (IRBlock* block : function->GetBlocks()) {
        changed |= copyPropagationPassBlock(block);
    }
    return changed;
}
