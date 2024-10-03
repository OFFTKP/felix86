#include "felix86/ir/passes.hpp"

void ir_naming_pass(IRFunction* function) {
    function->UnvisitAll();

    // Now add a name to every instruction
    std::array<u32, (u8)IRType::Count> names;
    std::fill(names.begin(), names.end(), 1);

    auto get_name = [&names](IRType type) { return names[(u8)type]++; };

    auto name_block = [&get_name](IRBlock* block) {
        for (auto& inst : block->GetPhiInstructions()) {
            inst.SetName(get_name(inst.GetType()));
        }

        for (auto& inst : block->GetInstructions()) {
            inst.SetName(get_name(inst.GetType()));
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