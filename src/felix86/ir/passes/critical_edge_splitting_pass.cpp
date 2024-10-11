#include "felix86/ir/passes/passes.hpp"

void ir_critical_edge_splitting_pass(IRFunction* function) {
    std::vector<std::tuple<IRBlock*, IRBlock*>> edges_to_split;

    for (IRBlock* block : function->GetBlocks()) {
        if (block->GetTermination() == Termination::JumpConditional) {
            for (IRBlock* successor : block->GetSuccessors()) {
                if (successor->GetPredecessors().size() > 1 && successor->HasPhis()) {
                    // Collect critical edges that need to be split
                    edges_to_split.emplace_back(block, successor);
                }
            }
        }
    }

    for (const auto& [block, successor] : edges_to_split) {
        successor->RemovePredecessor(block);
        IRBlock* new_block = function->CreateBlock();
        new_block->TerminateJump(successor);
        block->ReplaceSuccessor(successor, new_block);
    }
}