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

void convert_to_reduced_form(IRFunction* function) {
    for (IRBlock* block : function->GetBlocks()) {
        for (const SSAInstruction& inst : block->GetInstructions()) {
            if (inst.IsOperands() || inst.IsSetGuest() || inst.IsGetGuest()) {
                ReducedInstruction rir_inst = inst.AsReducedInstruction();
                block->InsertReducedInstruction(std::move(rir_inst));
            }
        }
    }
}

// Break critical edges that lead to blocks with phis
void critical_edge_splitting_pass(IRFunction* function) {
    // Ugly, its due to vector invalidating iterators while we loop and add blocks
    // TODO: cleanup
    while (true) {
        bool changed = false;
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
                        changed = true;
                        break;
                    }
                }

                if (changed) {
                    break;
                }
            }
        }

        if (!changed) {
            break;
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
void insert_parallel_move(IRBlock* block, ParallelMove& move) {
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
                        ReducedInstruction rinstr = {};
                        rinstr.opcode = IROpcode::Mov;
                        rinstr.name = block->GetNextName();
                        rinstr.operands[0] = src[j];
                        rinstr.operand_count = 1;
                        block->InsertReducedInstruction(std::move(rinstr));
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

void breakup_phis(IRBlock* block, const std::vector<InstIterator>& phis) {
    // For each predecessor let's construct a list of its outputs <- inputs
    size_t pred_count = block->GetPredecessors().size();
    if (pred_count < 2) {
        ERROR("Less than 2 predecessors on block with phis???");
    }

    for (size_t i = 0; i < pred_count; i++) {
        IRBlock* pred = block->GetPredecessors()[i];
        ParallelMove move = {};
        move.names_lhs.resize(phis.size());
        move.names_rhs.resize(phis.size());
        for (size_t j = 0; j < phis.size(); j++) {
            SSAInstruction* phi = &*phis[j];
            u32 name = phi->GetName();
            u32 value = phi->AsPhi().values[i]->GetName();
            move.names_lhs[j] = name;
            move.names_rhs[j] = value;
        }

        insert_parallel_move(pred, move);
    }
}

void phi_replacement_pass(IRFunction* function) {
    for (IRBlock* block : function->GetBlocks()) {
        if (block->HasPhis()) {
            std::vector<InstIterator> phis;
            InstIterator inst = block->GetInstructions().begin();
            InstIterator end = block->GetInstructions().end();
            while (inst != end) {
                if (inst->IsPhi()) {
                    phis.push_back(inst);
                } else {
                    break;
                }
                inst++;
            }

            if (phis.empty()) {
                ERROR("Block has phis but none were found???");
            }

            breakup_phis(block, phis);
        }
    }
}

void ir_ssa_destruction_pass(IRFunction* function) {
    if (!function->ValidatePhis()) {
        ERROR("Phis are not all gathered at the start of the block");
    }

    convert_to_reduced_form(function);
    critical_edge_splitting_pass(function);
    phi_replacement_pass(function);
}