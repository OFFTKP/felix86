#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "felix86/ir/block.h"

void ir_print_block(ir_block_t* block);
void ir_print_function_graphviz(u64 program_entrypoint, ir_function_t* function);

#ifdef __cplusplus
}
#endif