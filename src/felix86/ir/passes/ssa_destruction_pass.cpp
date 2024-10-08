#include "felix86/ir/passes/passes.hpp"

/*
    SSA destruction essentially requires breaking up phi instructions.
    Naively, this could be done by replacing each phi(value1 @ pred1, ..., valueN @ predN) with N copies, one for
    each predecessor.
    However this alone can lead into the lost-copy problem and the swap problem.
    Exiting SSA is thus not trivial.
    The solution we are going to employ for lost-copy problem is breaking up critical edges.
    For swap problem we are going to implement correctly the parallel move semantics of phi instructions.

    There's several papers on the correct process for moving out of SSA.
    Good presentation about it: https://www.clear.rice.edu/comp512/Lectures/13SSA-2.pdf
    More resources: https://www.cs.cmu.edu/~411/rec/02-sol.pdf

    Here's an algorithm for sequentializing parallel moves:
    Tilting at windmills with Coq
    https://xavierleroy.org/publi/parallel-move.pdf
    And a blog post based on the algorithm with a neat demo: https://compiler.club/parallel-moves/
*/

// Break critical edges that lead to blocks with phis
void critical_edge_splitting_pass(IRFunction* function) {
    for (IRBlock* block : function->GetBlocks()) {
        if (block->GetTermination() == Termination::JumpConditional) {
            // Only termination variety with more than one successor
            for (IRBlock* successor : block->GetSuccessors()) {
                if (successor->GetPredecessors().size() > 1 && successor->HasPhis()) {
                    // Critical edge, must be split
                    successor->RemovePredecessor(block);
                    IRBlock* new_block = function->CreateBlock();
                    new_block->TerminateJump(successor);
                    block->ReplaceSuccessor(successor, new_block);
                }
            }
        }
    }
}

using InstIterator = IRBlock::iterator;

struct ParallelMove {
    std::vector<u32> names_lhs;
    std::vector<u32> names_rhs;
};

// Sequentializing a parallel move at the end of the block
// We use the u32 names after translating out of SSA because
// there can now be multiple definitions for the same variable after
// breaking the phis
void insert_parallel_move(IRBlock* block, const ParallelMove& move) {
    ARGH it dont work cus you remove the instructions thus making the pointers invalid and we cant check the name of the operands
}

void breakup_phis(IRBlock* block, const std::vector<InstIterator>& phis) {
    // For each predecessor let's construct a list of its outputs <- inputs
    size_t pred_count = block->GetPredecessors().size();
    if (pred_count < 2) {
        ERROR("Less than 2 predecessors on block with phis???");
    }

    for (size_t i = 0; i < pred_count; i++) {
        IRBlock* pred = block->GetPredecessors()[i];
        ParallelMove move = {};
        move.names_lhs.resize(pred_count);
        move.names_rhs.resize(pred_count);
        for (InstIterator phi : phis) {
            if (phi->AsPhi().blocks[i] != pred) {
                ERROR("Predecessor mismatch");
            }

            u32 name = phi->GetName();
            u32 value = phi->AsPhi().values[i]->GetName();
            move.names_lhs[i] = name;
            move.names_rhs[i] = value;
        }

        insert_parallel_move(pred, move);
    }
}

void phi_replacement_pass(IRFunction* function) {
    for (IRBlock* block : function->GetBlocks()) {
        if (block->HasPhis()) {
            std::vector<InstIterator> phis;
            for (InstIterator inst = block->GetInstructions().begin(); inst->IsPhi(); ++inst) {
                phis.push_back(inst);
            }

            if (phis.empty()) {
                ERROR("Block has phis but none were found???");
            }

            breakup_phis(block, phis);

            // Now that we have broken up the phis, we can safely remove them
            for (InstIterator inst : phis) {
                block->GetInstructions().erase(inst);
            }
        }
    }
}

void ir_ssa_destruction_pass(IRFunction* function) {
    if (!function->ValidatePhis()) {
        ERROR("Phis are not all gathered at the start of the block");
    }

    critical_edge_splitting_pass(function);
    phi_replacement_pass(function);
}