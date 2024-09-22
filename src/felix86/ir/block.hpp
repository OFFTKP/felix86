#pragma once

#include "felix86/common/utility.hpp"
#include "felix86/ir/instruction_list.hpp"

#define IR_NO_ADDRESS (-1ull)

typedef struct ir_block_s {
    u64 start_address;
    ir_instruction_list_t* instructions;
    struct ir_block_list_s* predecessors;
    struct ir_block_list_s* successors;
    u8 predecessors_count;
    u8 successors_count;
    bool compiled;
} ir_block_t;

typedef struct ir_block_list_s {
    ir_block_t* block;
    struct ir_block_list_s* next;
} ir_block_list_t;

// A function has an entry block and an exit block. A function always starts at
// the entry block and ends at the exit block.
typedef struct ir_function_t {
    ir_block_t* entry;
    ir_block_t* exit;
    ir_block_list_t* list; // total list of blocks
    bool compiled;
} ir_function_t;

struct ir_function_cache_s;

ir_block_list_t* ir_block_list_create(ir_block_t* block);
void ir_block_list_insert(ir_block_list_t* list, ir_block_t* block);
void ir_block_list_node_destroy(ir_block_list_t* list);
ir_block_t* ir_block_create(u64 address);
ir_function_t* ir_function_create(u64 address);
void ir_function_destroy(ir_function_t* function);
ir_block_t* ir_function_get_block(ir_function_t* function, ir_block_t* predecessor, u64 address);
void ir_add_predecessor(ir_block_t* block, ir_block_t* predecessor);
void ir_add_successor(ir_block_t* block, ir_block_t* successor);
