#include "felix86/ir/passes/passes.hpp"

void ir_copy_propagate_block_v2(IRBlock* block) {
    for (SSAInstruction& inst : block->GetInstructions()) {
        if (inst.GetOpcode() != IROpcode::Mov) {
            inst.PropagateMovs();
        }
    }
}

void ir_copy_propagation_pass(IRFunction* function) {
    for (IRBlock* block : function->GetBlocks()) {
        ir_copy_propagate_block_v2(block);
    }
}
