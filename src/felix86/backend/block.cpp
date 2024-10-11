#include "felix86/backend/block.hpp"
#include "felix86/common/log.hpp"

BackendBlock BackendBlock::FromIRBlock(const IRBlock* block, std::vector<NamedPhi>& phis) {
    BackendBlock backend_block;
    backend_block.list_index = block->GetIndex();
    backend_block.termination = block->GetTermination();

    for (const SSAInstruction& inst : block->GetInstructions()) {
        if (!Opcode::IsAuxiliary(inst.GetOpcode())) {
            ASSERT(inst.IsOperands());

            backend_block.instructions.push_back(BackendInstruction::FromSSAInstruction(&inst));

            if (&inst == block->GetCondition()) {
                backend_block.condition = &backend_block.instructions.back();
            }
        } else if (inst.GetOpcode() == IROpcode::Phi) {
            NamedPhi named_phi;
            named_phi.name = inst.GetName();
            named_phi.phi = &inst.AsPhi();
            phis.push_back(named_phi);
        }
    }

    for (size_t i = 0; i < 2; i++) {
        if (block->GetSuccessor(i)) {
            backend_block.successors[i] = block->GetSuccessor(i)->GetIndex();
        }
    }

    return backend_block;
}