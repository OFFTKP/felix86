#pragma once

#include "felix86/ir/function.hpp"

// TODO: get rid of this struct
typedef struct {
    IRFunction* function;
} FrontendState;

void frontend_compile_block(IRFunction* function, IRBlock* block);
void frontend_compile_function(IRFunction* function);
