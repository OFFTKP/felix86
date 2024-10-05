#include "felix86/ir/passes/passes.hpp"

void ir_naming_pass(IRFunction* function) {
    function->UnvisitAll();

    // Now add a name to every instruction
    u32 name = 1;

    auto name_block = [&name](IRBlock* block) {
        for (auto& inst : block->GetInstructions()) {
            inst.SetName(name++);
        }
    };

    std::vector<IRBlock*> worklist;
    worklist.push_back(function->GetEntry());

    while (!worklist.empty()) {
        IRBlock* work = worklist.back();
        worklist.pop_back();
        work->SetVisited(true);

        name_block(work);

        IRBlock* succ1 = work->GetSuccessor(false);
        IRBlock* succ2 = work->GetSuccessor(true);

        if (succ1 && !succ1->IsVisited()) {
            worklist.push_back(succ1);
        }

        if (succ2 && !succ2->IsVisited()) {
            worklist.push_back(succ2);
        }
    }
}