#include "felix86/ir/passes/passes.hpp"

// Gets rid of terminations and replaces them with jump instructions
// Should be executed last
void PassManager::BlockShenanigansPass(BackendFunction* function) {
    // example failure: libc.so.6 0xAB560
    ERROR("FIXME: this pass is buggy and broken, causes bugs apparently the deleted blocks remain used sometimes?");
    std::vector<u32> to_remove;

    // Skip entry & exit block
    for (size_t i = 2; i < function->GetBlocks().size(); i++) {
        BackendBlock* block = &function->GetBlock(i);

        // Single jump instruction
        if (block->GetInstructions().size() == 1) {
            if (block->GetSuccessorCount() != 1) {
                continue;
            }

            // This block can be removed
            BackendBlock* final = block->GetSuccessor(0);
            final->RemovePredecessor(block);
            for (BackendBlock* pred : block->GetPredecessors()) {
                bool found = false;
                for (size_t j = 0; j < pred->GetSuccessors().size(); j++) {
                    BackendBlock* succ = pred->GetSuccessor(j);
                    if (succ == block) {
                        pred->SetSuccessor(j, final);
                        final->AddPredecessor(pred);
                        found = true;
                        break;
                    }
                }
                ASSERT(found);
            }
            to_remove.push_back(block->GetIndex());
        }
    }

    size_t size = function->GetBlocks().size();
    for (u32 index : to_remove) {
        function->RemoveBlock(index);
    }
    ASSERT(size - to_remove.size() == function->GetBlocks().size());

    // Fixup indices
    std::unordered_map<u32, u32> index_map;
    for (size_t i = 0; i < function->GetBlocks().size(); i++) {
        index_map[function->GetBlock(i).GetIndex()] = i;
    }

    for (auto& block : function->GetBlocks()) {
        block->SetIndex(index_map[block->GetIndex()]);
    }
}