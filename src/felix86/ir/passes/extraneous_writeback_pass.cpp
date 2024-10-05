#include "felix86/ir/passes/passes.hpp"

// On entry blocks we load *all* state from VM (so that each use is dominated by a definition) and
// on exit blocks we store back all state. But if the exit block stores the exact same variable loaded on entry,
// that can be removed.
// We can find out only after moving to SSA and copy propagating the IR mov/set_guest instructions.
void ir_extraneous_writeback_pass(IRFunction* function) {
    std::array<IRInstruction*, X86_REF_COUNT> entry_defs;

    IRBlock* entry = function->GetEntry();
    for (auto& inst : entry->GetInstructions()) {
        if (inst.GetOpcode() == IROpcode::LoadGuestFromMemory) {
            entry_defs[inst.AsGetGuest().ref] = &inst;
        }
    }

    IRBlock* exit = function->GetExit();
    std::list<IRInstruction>& insts_exit = exit->GetInstructions();
    auto it = insts_exit.begin();
    auto end = insts_exit.end();
    while (it != end) {
        IRInstruction& inst = *it;
        if (inst.GetOpcode() == IROpcode::StoreGuestToMemory) {
            const SetGuest& set_guest = inst.AsSetGuest();
            if (entry_defs[set_guest.ref] == set_guest.source) {
                // It's the same one that was loaded in entry block, store can be removed
                inst.Unlock();
                inst.Invalidate();
                it = insts_exit.erase(it);
                continue;
            }
        }
        it++;
    }
}