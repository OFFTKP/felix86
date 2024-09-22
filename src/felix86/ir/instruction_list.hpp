#pragma once

#include "felix86/ir/instruction.h"

struct ir_instruction_list_s;

typedef struct ir_instruction_list_s {
    ir_instruction_t instruction;
    struct ir_instruction_list_s* previous;
    struct ir_instruction_list_s* next;
} ir_instruction_list_t;

ir_instruction_list_t* ir_ilist_create();
void ir_ilist_destroy(ir_instruction_list_t* ilist);
ir_instruction_t* ir_ilist_push_back(ir_instruction_list_t* ilist);
ir_instruction_t* ir_ilist_insert_before(ir_instruction_list_t* before);
ir_instruction_t* ir_ilist_insert_after(ir_instruction_list_t* after);
void ir_ilist_remove(ir_instruction_list_t* ilist);
void ir_ilist_free(ir_instruction_list_t* ilist);
