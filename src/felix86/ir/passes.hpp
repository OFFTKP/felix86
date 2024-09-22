#pragma once

#include "felix86/ir/block.h"

void ir_naming_pass(ir_function_t* function);
void ir_ssa_pass(ir_function_t* function);
void ir_copy_propagation_pass(ir_function_t* function);
