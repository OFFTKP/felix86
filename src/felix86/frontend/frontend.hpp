#pragma once

#include "felix86/common/utility.h"
#include "felix86/ir/block.h"

typedef struct {
    ir_function_t* function;
    ir_block_t* current_block;
    u64 current_address;
    bool exit;
} frontend_state_t;

void frontend_compile_block(ir_function_t* function, ir_block_t* block);
void frontend_compile_function(ir_function_t* function, u64 address);
