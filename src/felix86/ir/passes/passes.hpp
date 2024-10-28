#pragma once

#include "felix86/backend/allocation_map.hpp"
#include "felix86/backend/function.hpp"
#include "felix86/ir/function.hpp"

// TODO: move to regalloc class
[[nodiscard]] AllocationMap ir_spill_everything_pass(const BackendFunction& function);
[[nodiscard]] AllocationMap ir_graph_coloring_pass(BackendFunction& function);

// TODO: use Function&
struct PassManager {
    static void SSAPass(IRFunction* function);

    static void CriticalEdgeSplittingPass(IRFunction* function);

    // These return bool to know if something has changed so that they can be run until fixpoint if necessary
    static bool CopyPropagationPass(IRFunction* function);

    static bool DeadCodeEliminationPass(IRFunction* function);

    static bool LocalCSEPass(IRFunction* function);

    static bool PeepholePass(IRFunction* function);

    static void VectorStatePass(BackendFunction* function);

private:
    // Only used by the SSA pass to get rid of unused writebacks and replace store/loads with actual writes/reads
    static void extraneousWritebackPass(IRFunction* function);

    static bool copyPropagationPassBlock(IRBlock* block);

    static bool localCSEPassBlock(IRBlock* block);

    static bool peepholePassBlock(IRBlock* block);
};