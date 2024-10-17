#pragma once

#include "felix86/common/utility.hpp"
#include "felix86/ir/function.hpp"

struct Emulator;

typedef struct {
    Emulator* emulator;
    IRFunction* function;
    IRBlock* current_block;
    u64 current_address;
    bool exit;
} FrontendState;

void frontend_compile_block(Emulator& emulator, IRFunction* function, IRBlock* block);
void frontend_compile_function(Emulator& emulator, IRFunction* function);
