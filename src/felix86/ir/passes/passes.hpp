#pragma once

#include "felix86/backend/allocation_map.hpp"
#include "felix86/backend/function.hpp"
#include "felix86/ir/function.hpp"

void ir_ssa_pass(IRFunction* function);
void ir_extraneous_writeback_pass(IRFunction* function);
void ir_copy_propagation_pass(IRFunction* function);
void ir_dead_code_elimination_pass(IRFunction* function);
void ir_replace_setguest_pass(IRFunction* function);
void ir_local_cse_pass(IRFunction* function);
void ir_critical_edge_splitting_pass(IRFunction* function);

[[nodiscard]] AllocationMap ir_spill_everything_pass(const BackendFunction& function);
void ir_graph_coloring_pass(BackendFunction* function);
