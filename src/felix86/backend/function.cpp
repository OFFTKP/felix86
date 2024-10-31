#include "felix86/backend/function.hpp"

namespace {
struct ParallelMove {
    std::vector<AllocationType> types_lhs;
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
    auto type = move.types_lhs;

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
                        BackendInstruction instr = BackendInstruction::FromMove(block->GetNextName(), src[j], type[j]);
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

            BackendInstruction instr = BackendInstruction::FromMove(dst[i], src[i], type[i]);
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
        move.names_lhs.resize(phis.size());
        move.names_rhs.resize(phis.size());

        for (size_t j = 0; j < phis.size(); j++) {
            const NamedPhi& named_phi = phis[j];
            u32 name = named_phi.name;
            u32 value = 0;
            for (size_t k = 0; k < named_phi.phi->blocks.size(); k++) {
                if (named_phi.phi->blocks[k] == ir_pred) {
                    value = named_phi.phi->values[k]->GetName();
                    break;
                }
            }
            ASSERT_MSG(value != 0, "Phi predecessor not found");

            move.types_lhs[j] = GetTypeFromRef(named_phi.phi->ref);
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

    // Separate the phis from the instructions so we can
    // convert them to moves
    std::vector<std::vector<NamedPhi>> phis(blocks.size());

    for (size_t i = 0; i < blocks.size(); i++) {
        backend_function.blocks[i] = BackendBlock::FromIRBlock(blocks[i], phis[i]);
    }

    for (size_t i = 0; i < blocks.size(); i++) {
        if (phis[i].empty()) {
            continue;
        }

        BreakupPhis(&backend_function, blocks[i], phis[i]);
    }

    backend_function.start_address = function->GetStartAddress();

    return backend_function;
}

static void postorder(u32 current, const std::vector<BackendBlock>& blocks, std::vector<const BackendBlock*>& output, bool* visited) {
    if (visited[current]) {
        return;
    }

    visited[current] = true;

    const BackendBlock* block = &blocks[current];

    for (u8 i = 0; i < block->GetSuccessorCount(); i++) {
        postorder(block->GetSuccessors()[i], blocks, output, visited);
    }

    output.push_back(block); // TODO: don't use vector in the future
}

static void postorder_creation(const BackendFunction* function, std::vector<const BackendBlock*>& order) {
    const std::vector<BackendBlock>& blocks = function->GetBlocks();

    bool* visited = (bool*)alloca(function->GetBlocks().size());
    memset(visited, 0, function->GetBlocks().size());
    postorder(0, blocks, order, visited);

    for (size_t i = 0; i < function->GetBlocks().size(); i++) {
        ASSERT(visited[i]);
    }
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