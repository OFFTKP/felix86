#include "felix86/backend/block.hpp"
#include "felix86/backend/serialized_function.hpp"
#include "felix86/common/log.hpp"

BackendBlock BackendBlock::FromIRBlock(const IRBlock* block, std::vector<NamedPhi>& phis) {
    BackendBlock backend_block;
    backend_block.list_index = block->GetIndex();

    u32 highest_name = 0;

    for (const SSAInstruction& inst : block->GetInstructions()) {
        if (inst.GetName() > highest_name) {
            highest_name = inst.GetName();
        }

        if (!Opcode::IsAuxiliary(inst.GetOpcode())) {
            ASSERT(inst.IsOperands());

            backend_block.instructions.push_back(BackendInstruction::FromSSAInstruction(&inst));
        } else if (inst.GetOpcode() == IROpcode::Phi) {
            NamedPhi named_phi;
            named_phi.name = inst.GetName();
            named_phi.phi = &inst.AsPhi();
            phis.push_back(named_phi);
        }
    }

    backend_block.next_name = highest_name + 1;
    backend_block.start_address = block->GetStartAddress();
    backend_block.critical = block->IsCriticalSection();

    return backend_block;
}

std::string BackendBlock::Print() const {
    std::string ret;

    ret += "Block " + std::to_string(list_index) + ":\n";

    for (const BackendInstruction& inst : instructions) {
        ret += inst.Print() + "\n";
    }

    ret += "\n\n";

    return ret;
}

void BackendBlock::Serialize(SerializedFunction& function) const {
    function.Push(start_address);

    function.Push(GetPredecessorCount());
    for (const BackendBlock* pred : predecessors) {
        function.Push(pred->GetIndex());
    }

    function.Push(GetSuccessorCount());
    for (const BackendBlock* succ : successors) {
        if (succ) {
            function.Push(succ->GetIndex());
        }
    }

    function.Push(static_cast<u32>(instructions.size()));
    for (const BackendInstruction& inst : instructions) {
        inst.Serialize(function);
    }
}

BackendBlock BackendBlock::Deserialize(const SerializedFunction& function, std::vector<BackendBlock*> blocks) {
    BackendBlock block;
    block.start_address = function.Pop<u64>();

    u32 pred_count = function.Pop<u32>();
    for (u32 i = 0; i < pred_count; i++) {
        u32 index = function.Pop<u32>();
        block.predecessors.push_back(blocks[index]);
    }

    u32 succ_count = function.Pop<u32>();
    ASSERT(succ_count <= 2);
    for (u32 i = 0; i < succ_count; i++) {
        u32 index = function.Pop<u32>();
        block.successors[i] = blocks[index];
    }

    u32 inst_count = function.Pop<u32>();
    for (u32 i = 0; i < inst_count; i++) {
        block.instructions.push_back(BackendInstruction::Deserialize(function));
    }

    return block;
}