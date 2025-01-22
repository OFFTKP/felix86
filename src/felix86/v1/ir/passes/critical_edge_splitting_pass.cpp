#include "felix86/ir/passes/passes.hpp"

void PassManager::CriticalEdgeSplittingPass(IRFunction* function) {
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
        new_block->AddPredecessor(block);
        block->ReplaceSuccessor(successor, new_block);

        if (successor->HasPhis()) {
            for (SSAInstruction& instr : successor->GetInstructions()) {
                if (instr.IsPhi()) {
                    Phi& phi = instr.AsPhi();
                    for (size_t i = 0; i < phi.blocks.size(); i++) {
                        if (phi.blocks[i] == block) {
                            phi.blocks[i] = new_block;
                        }
                    }
                } else {
                    break;
                }
            }
        }
    }
}