#include <catch2/catch_test_macros.hpp>
#include <xbyak/xbyak.h>
#include "felix86/common/utility.h"
#include "felix86/frontend/frontend.h"
#include "felix86/felix86.h"
#include "felix86/ir/emitter.h"
#include "felix86/ir/block.h"
#include "felix86/ir/passes.h"
#include "felix86/ir/print.h"
#include "felix86/ir/interpreter.h"

#define START_IR_TEST() \
    ir_function_t* function = ir_function_create(IR_NO_ADDRESS); \
    ir_block_t* current_block = function->first->block;\
    ir_block_t* entry = current_block

#define END_IR_TEST() \
    ir_naming_pass(function)

#define SWITCH_TO_BLOCK(block) \
    current_block = block

#define INSTS \
    current_block->instructions

#define CREATE_BLOCK(predecessor) \
    (ir_function_get_block(function, predecessor, IR_NO_ADDRESS))
