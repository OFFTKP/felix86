#pragma once

#include "felix86/common/utility.hpp"
#include "felix86/ir/function.hpp"

typedef struct {
    IRFunction* function;
    IRBlock* current_block;
    u64 current_address;
    bool exit;
} FrontendState;

void frontend_compile_block(IRFunction* function, IRBlock* block);
void frontend_compile_function(IRFunction* function);
