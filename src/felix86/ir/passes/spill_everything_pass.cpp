// A bad register allocator for debugging purposes, just spills everything

#include "felix86/backend/registers.hpp"
#include "felix86/common/log.hpp"
#include "felix86/ir/passes/passes.hpp"

AllocationMap ir_spill_everything_pass(const BackendFunction& function) {
    AllocationMap allocations;
    u32 spill_counter = 0;

    for (const BackendBlock& block : function.GetBlocks()) {
        for (const BackendInstruction& inst : block.GetInstructions()) {
            switch (inst.GetOpcode()) {
            case IROpcode::Immediate: {
                if (inst.GetImmediateData() == 0) {
                    allocations.Allocate(inst.GetName(), Registers::Zero());
                } else {
                    allocations.Allocate(inst.GetName(), spill_counter++, SpillSize::QWord, AllocationType::GPR);
                }
                break;
            }
            case IROpcode::GetThreadStatePointer: {
                allocations.Allocate(inst.GetName(), Registers::ThreadStatePointer());
                break;
            }
            default: {
                if (inst.GetDesiredType() != AllocationType::Null) { // if not void
                    if (inst.GetDesiredType() == AllocationType::GPR) {
                        allocations.Allocate(inst.GetName(), spill_counter++, SpillSize::QWord, AllocationType::GPR);
                    } else {
                        UNIMPLEMENTED();
                    }
                }
                break;
            }
            }
        }
    }

    return allocations;
}