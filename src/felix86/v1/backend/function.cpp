#include "felix86/backend/function.hpp"
#include "felix86/common/version.hpp"

namespace {
struct ParallelMove {
    std::vector<AllocationType> types_lhs;
    std::vector<AllocationType> types_rhs;
    std::vector<u32> names_lhs;
    std::vector<u32> names_rhs;
};

AllocationType GetTypeFromRef(x86_ref_e ref) {
    switch (ref) {
    case X86_REF_RAX ... X86_REF_R15:
        return AllocationType::GPR;
    case X86_REF_CF ... X86_REF_OF:
        return AllocationType::GPR;
    case X86_REF_RIP:
        return AllocationType::GPR;
    case X86_REF_FS:
        return AllocationType::GPR;
    case X86_REF_GS:
        return AllocationType::GPR;
    case X86_REF_XMM0 ... X86_REF_XMM15:
        return AllocationType::Vec;
    case X86_REF_ST0 ... X86_REF_ST7:
        return AllocationType::Vec;
    case X86_REF_COUNT:
        UNREACHABLE();
        return AllocationType::Null;
    }

    UNREACHABLE();
    return AllocationType::Null;
}

// Sequentializing a parallel move at the end of the block
// We use the u32 names after translating out of SSA because
// there can now be multiple definitions for the same variable after
// breaking the phis
void InsertParallelMove(BackendBlock* block, ParallelMove& move) {
    // This only modifies the ReducedInstruction part of the block so we can always have a valid SSA form
    // to analyze and because ReducedInstruction deals with names instead of pointers.
    size_t size = move.names_lhs.size();
    enum Status { To_move, Being_moved, Moved };

    std::vector<Status> status(size, To_move);

    auto& dst = move.names_lhs;
    auto& src = move.names_rhs;

    std::function<void(int)> move_one = [&](int i) {
        if (src[i] != dst[i]) {
            status[i] = Being_moved;

            for (size_t j = 0; j < size; j++) {
                if (src[j] == dst[i]) {
                    switch (status[j]) {
                    case To_move: {
                        move_one(j);
                        break;
                    }
                    case Being_moved: {
                        BackendInstruction instr = BackendInstruction::FromMove(block->GetNextName(), src[j], move.types_lhs[j], move.types_rhs[j]);
                        src[j] = instr.GetName();
                        WARN("Parallel move cycle detected, breaking it");
                        block->InsertAtEnd(std::move(instr));
                        break;
                    }
                    case Moved: {
                        break;
                    }
                    }
                }
            }

            BackendInstruction instr = BackendInstruction::FromMove(dst[i], src[i], move.types_lhs[i], move.types_rhs[i]);
            block->InsertAtEnd(std::move(instr));
            status[i] = Moved;
        }
    };

    for (size_t i = 0; i < size; ++i) {
        if (status[i] == To_move) {
            move_one(i);
        }
    }
}

void BreakupPhis(BackendFunction* function, IRBlock* block, const std::vector<NamedPhi>& phis) {
    // For each predecessor let's construct a list of its outputs <- inputs
    size_t pred_count = block->GetPredecessors().size();
    if (pred_count < 2) {
        ERROR("Less than 2 predecessors on block with phis???");
    }

    for (size_t i = 0; i < pred_count; i++) {
        IRBlock* ir_pred = block->GetPredecessors()[i];
        BackendBlock* pred = &function->GetBlock(ir_pred->GetIndex());

        ASSERT(pred->GetIndex() == ir_pred->GetIndex());

        ParallelMove move = {};
        move.types_lhs.resize(phis.size());
        move.types_rhs.resize(phis.size());
        move.names_lhs.resize(phis.size());
        move.names_rhs.resize(phis.size());

        for (size_t j = 0; j < phis.size(); j++) {
            const NamedPhi& named_phi = phis[j];
            u32 name = named_phi.name;
            u32 value = 0;
            AllocationType type_rhs = AllocationType::Null;
            for (size_t k = 0; k < named_phi.phi->blocks.size(); k++) {
                if (named_phi.phi->blocks[k] == ir_pred) {
                    value = named_phi.phi->values[k]->GetName();
                    type_rhs = BackendInstruction::GetAllocationType(named_phi.phi->values[k]);
                    break;
                }
            }
            ASSERT_MSG(value != 0, "Phi predecessor not found");

            move.types_lhs[j] = GetTypeFromRef(named_phi.phi->ref);
            move.types_rhs[j] = type_rhs;
            move.names_lhs[j] = name;
            move.names_rhs[j] = value;
        }

        InsertParallelMove(pred, move);
    }
}
} // namespace

BackendFunction BackendFunction::FromIRFunction(const IRFunction* function) {
    const std::vector<IRBlock*> blocks = function->GetBlocks();

    BackendFunction backend_function;
    backend_function.blocks.resize(blocks.size());

    for (size_t i = 0; i < blocks.size(); i++) {
        backend_function.blocks[i] = new BackendBlock();
    }

    // Separate the phis from the instructions so we can
    // convert them to moves
    std::vector<std::vector<NamedPhi>> phis(blocks.size());

    for (size_t i = 0; i < blocks.size(); i++) {
        *backend_function.blocks[i] = BackendBlock::FromIRBlock(blocks[i], phis[i]);
    }

    for (size_t j = 0; j < blocks.size(); j++) {
        IRBlock* block = blocks[j];
        BackendBlock& backend_block = *backend_function.blocks[j];
        for (size_t i = 0; i < 2; i++) {
            if (block->GetSuccessor(i)) {
                backend_block.SetSuccessor(i, backend_function.blocks[block->GetSuccessor(i)->GetIndex()]);
            }
        }

        for (auto& pred : block->GetPredecessors()) {
            backend_block.AddPredecessor(backend_function.blocks[pred->GetIndex()]);
        }
    }

    for (size_t i = 0; i < blocks.size(); i++) {
        if (phis[i].empty()) {
            continue;
        }

        BreakupPhis(&backend_function, blocks[i], phis[i]);
    }

    for (size_t i = 0; i < blocks.size(); i++) {
        IRBlock* block = blocks[i];
        BackendBlock& backend_block = *backend_function.blocks[i];
        BackendInstruction termination_instruction;
        switch (block->GetTermination()) {
        case Termination::Jump:
            termination_instruction.opcode = IROpcode::Jump;
            break;
        case Termination::JumpConditional:
            termination_instruction.opcode = IROpcode::JumpConditional;
            termination_instruction.operand_count = 1;
            termination_instruction.operand_names[0] = block->GetCondition()->GetName();
            break;
        case Termination::BackToDispatcher:
            termination_instruction.opcode = IROpcode::BackToDispatcher;
            break;
        case Termination::Null:
            UNREACHABLE();
            break;
        }
        backend_block.instructions.push_back(termination_instruction);
    }

    backend_function.start_address = function->GetStartAddress();

    return backend_function;
}

static void postorder(const BackendBlock* block, std::vector<const BackendBlock*>& output) {
    if (block->IsVisited()) {
        return;
    }

    block->SetVisited(true);

    for (u8 i = 0; i < block->GetSuccessorCount(); i++) {
        postorder(block->GetSuccessors()[i], output);
    }

    output.push_back(block); // TODO: don't use vector in the future
}

static void postorder_creation(const BackendFunction* function, std::vector<const BackendBlock*>& order) {
    for (auto& block : function->GetBlocks()) {
        block->SetVisited(false);
    }

    postorder(&function->GetBlock(0), order);

    for (size_t i = 0; i < function->GetBlocks().size(); i++) {
        ASSERT(function->GetBlock(i).IsVisited());
    }

    ASSERT(order.size() == function->GetBlocks().size());
}

std::vector<const BackendBlock*> BackendFunction::GetBlocksPostorder() const {
    std::vector<const BackendBlock*> order;
    postorder_creation(this, order);
    return order;
}

std::string BackendFunction::Print() const {
    std::string ret;

    auto blocks = GetBlocksPostorder();

    for (auto it = blocks.rbegin(); it != blocks.rend(); it++) {
        ret += (*it)->Print();
    }

    return ret;
}

/**
    This is unused but kept in case I ever need it again
    Serialized function format, likely to change in the future
    u32 version
    u32 block_count

    // block_count times, in order from 0 to block_count - 1
    {
        u64 start_address

        u32 predecessor_count
        // predecessor_count times
        {
            u32 index
        }

        u32 successor_count
        // successor_count times
        {
            u32 index
        }

        u32 instruction_count
        // instruction_count times
        {
            instruction serialized data, see BackendInstruction::Serialize
        }
    }
*/

SerializedFunction BackendFunction::Serialize() const {
    SerializedFunction function;
    function.Push(FELIX86_VERSION_U32);
    function.Push((u32)blocks.size());

    for (size_t i = 0; i < blocks.size(); i++) {
        // Make sure that each block index is correct in case we change stuff in the future
        ASSERT(blocks[i]->GetIndex() == i);
        blocks[i]->Serialize(function);
    }

    return function;
}

BackendFunction BackendFunction::Deserialize(const SerializedFunction& data) {
    ASSERT(data.Pop<u32>() == FELIX86_VERSION_U32);

    BackendFunction function;
    function.blocks.resize(data.Pop<u32>());

    for (size_t i = 0; i < function.blocks.size(); i++) {
        function.blocks[i] = new BackendBlock();
    }

    for (size_t i = 0; i < function.blocks.size(); i++) {
        *function.blocks[i] = BackendBlock::Deserialize(data, function.blocks);
    }

    return function;
}