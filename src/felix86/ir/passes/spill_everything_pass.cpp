// A bad register allocator for debugging purposes, just spills everything

#include "felix86/ir/passes/passes.hpp"

void ir_spill_everything_pass(IRFunction* function) {
    u32 spill_loc = 0;
    for (IRBlock* block : function->GetBlocks()) {
        for (IRInstruction& inst : block->GetInstructions()) {
            if (inst.NeedsAllocation()) {
                inst.Allocate(spill_loc++);
            }
        }
    }
}