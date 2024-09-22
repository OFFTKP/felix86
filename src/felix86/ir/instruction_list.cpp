#include <stddef.h>
#include <stdlib.h>
#include "felix86/ir/instruction_list.h"

ir_instruction_list_t* ir_ilist_create() {
    ir_instruction_list_t* ilist = calloc(1, sizeof(ir_instruction_list_t));
    ilist->instruction.opcode = IR_START_OF_BLOCK;
    ilist->instruction.type = IR_TYPE_NULL;
    return ilist;
}

ir_instruction_t* ir_ilist_push_back(ir_instruction_list_t* ilist) {
    ir_instruction_list_t* current = ilist;
    while (current->next) {
        current = current->next;
    }

    ir_instruction_list_t* new_node = calloc(1, sizeof(ir_instruction_list_t));
    new_node->previous = current;
    new_node->next = NULL;
    current->next = new_node;
    return &new_node->instruction;
}

ir_instruction_t* ir_ilist_insert_before(ir_instruction_list_t* current) {
    ir_instruction_list_t* new_node = calloc(1, sizeof(ir_instruction_list_t));
    new_node->previous = current->previous;
    new_node->next = current;
    if (current->previous) {
        current->previous->next = new_node;
    }
    current->previous = new_node;
    return &new_node->instruction;
}

ir_instruction_t* ir_ilist_insert_after(ir_instruction_list_t* current) {
    ir_instruction_list_t* new_node = calloc(1, sizeof(ir_instruction_list_t));
    new_node->previous = current;
    new_node->next = current->next;
    if (current->next) {
        current->next->previous = new_node;
    }
    current->next = new_node;
    return &new_node->instruction;
}

void ir_ilist_remove(ir_instruction_list_t* ilist) {
    if (ilist->previous) {
        ilist->previous->next = ilist->next;
    }

    if (ilist->next) {
        ilist->next->previous = ilist->previous;
    }
}

void ir_ilist_free(ir_instruction_list_t* ilist) {
    free(ilist);
}

void ir_ilist_destroy(ir_instruction_list_t* ilist) {
    ir_instruction_list_t* current = ilist;
    while (current) {
        ir_instruction_list_t* next = current->next;
        if (current->instruction.opcode == IR_RUNTIME_COMMENT) {
            free((void*)current->instruction.runtime_comment.comment);
        }
        ir_ilist_free(current);
        current = next;
    }
}