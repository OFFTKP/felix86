#include "felix86/backend/function.hpp"

namespace {
struct ParallelMove {
    std::vector<u32> names_lhs;
    std::vector<u32> names_rhs;
};

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

    VERBOSE("Constructing parallel move:");
    for (size_t i = 0; i < size; i++) {
        VERBOSE("t%s = t%s", GetNameString(dst[i]).c_str(), GetNameString(src[i]).c_str());
    }

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
                        ReducedInstruction rinstr = {};
                        rinstr.opcode = IROpcode::Mov;
                        rinstr.name = block->GetNextName();
                        rinstr.operands[0] = src[j];
                        rinstr.operand_count = 1;
                        block->InsertReducedInstruction(std::move(rinstr));
                        VERBOSE("temp%s = t%s", GetNameString(rinstr.name).c_str(), GetNameString(src[j]).c_str());
                        src[j] = rinstr.name;
                        break;
                    }
                    case Moved: {
                        break;
                    }
                    }
                }
            }

            ReducedInstruction rinstr = {};
            rinstr.opcode = IROpcode::Mov;
            rinstr.name = dst[i];
            rinstr.operands[0] = src[i];
            VERBOSE("t%s = t%s", GetNameString(dst[i]).c_str(), GetNameString(src[i]).c_str());
            rinstr.operand_count = 1;
            block->InsertReducedInstruction(std::move(rinstr));
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
        move.names_lhs.resize(phis.size());
        move.names_rhs.resize(phis.size());
        for (size_t j = 0; j < phis.size(); j++) {
            const NamedPhi& named_phi = phis[j];
            u32 name = named_phi.name;
            u32 value = named_phi.phi->values[i]->GetName();
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

        BreakupPhis(blocks[i], phis[i]);
    }

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