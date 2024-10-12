#include "felix86/ir/passes/passes.hpp"

bool ir_copy_propagate_block_v2(IRBlock* block) {
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
        ir_copy_propagate_block_v2(block);
    }
    return changed;
}
