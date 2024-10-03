#pragma once

#include "felix86/ir/function.hpp"

void ir_naming_pass(IRFunction* function);
void ir_ssa_pass(IRFunction* function);
void ir_entry_exit_unused_pass(IRFunction* function);
void ir_copy_propagation_pass(IRFunction* function);