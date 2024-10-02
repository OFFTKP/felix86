#pragma once

#include "felix86/ir/block.hpp"
#include "felix86/ir/function.hpp"

void ir_print_block(const IRBlock& block);
void ir_print_function_graphviz(const IRFunction& function);
