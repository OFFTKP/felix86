#include "felix86/ir/block.h"
#include "felix86/frontend/frontend.h"
#include "felix86/ir/emitter.h"
#include <stdlib.h>

ir_block_list_t* ir_block_list_create(ir_block_t* block)
{
    ir_block_list_t* list = calloc(1, sizeof(ir_block_list_t));
    list->block = block;
    return list;
}

ir_block_t* ir_block_create(u64 address)
{
    ir_block_t* block = calloc(1, sizeof(ir_block_t));
    block->instructions = ir_ilist_create();
    block->start_address = address;
    return block;
}

void ir_block_list_node_destroy(ir_block_list_t* list)
{
    ir_ilist_destroy(list->block->instructions);
    free(list->block);
    free(list);
}

void ir_block_list_insert(ir_block_list_t* list, ir_block_t* block)
{
    ir_block_list_t* next = list->next;
    list->next = ir_block_list_create(block);
    list->next->next = next;
}

ir_function_t* ir_function_create(u64 address)
{
    ir_function_t* function = calloc(sizeof(ir_function_t), 1);
    function->entry = ir_block_create(IR_NO_ADDRESS);
    function->exit = ir_block_create(IR_NO_ADDRESS);

    ir_instruction_list_t* insts = ir_ilist_create();
    for (int i = 0; i < X86_REF_COUNT; i++) {
        ir_instruction_t* load = ir_emit_load_guest_from_memory(insts, i);
        ir_emit_set_guest(insts, i, load);
    }
    function->entry->instructions = insts;

    insts = ir_ilist_create();
    for (int i = 0; i < X86_REF_COUNT; i++) {
        ir_instruction_t* guest = ir_emit_get_guest(insts, i);
        ir_emit_store_guest_to_memory(insts, i, guest);
    }
    ir_emit_exit(insts);
    function->exit->instructions = insts;

    function->list = ir_block_list_create(function->entry);
    ir_block_list_t* list = ir_block_list_create(function->exit);
    function->list->next = list;

    function->compiled = false;

    return function;
}

void ir_function_destroy(ir_function_t* function)
{
    ir_block_list_t* current = function->list;
    while (current)
    {
        ir_block_list_t* predecessors = current->block->predecessors;
        while (predecessors)
        {
            ir_block_list_t* next = predecessors->next;
            free(predecessors);
            predecessors = next;
        }

        ir_block_list_t* successors = current->block->successors;
        while (successors)
        {
            ir_block_list_t* next = successors->next;
            free(successors);
            successors = next;
        }

        ir_block_list_t* next = current->next;
        ir_block_list_node_destroy(current);
        current = next;
    }

    free(function);
}

// Gets a block from the function. The function keeps a list of all blocks so it
// can go through them linearly if needed. If address is IR_NO_ADDRESS, a new
// block is created and not looked up in the list. This is for blocks that are
// not tied to an actual address but are used as auxiliary blocks, for example
// for the rep instruction loop bodies or in the future for breaking up critical
// edges etc.
ir_block_t* ir_function_get_block(ir_function_t* function, ir_block_t* predecessor, u64 address)
{
    if (address != IR_NO_ADDRESS)
    {
        for (ir_block_list_t* current = function->list; current; current = current->next)
        {
            if (current->block->start_address == address)
            {
                if (predecessor)
                {
                    ir_add_predecessor(current->block, predecessor);
                }
                return current->block;
            }
        }
    }

    ir_block_t* block = ir_block_create(address);
    ir_block_list_insert(function->list, block);

    // Add block to predecessor's successors
    if (predecessor)
    {
        ir_add_successor(predecessor, block);
    }

    return block;
}

void ir_add_predecessor(ir_block_t* block, ir_block_t* predecessor)
{
    if (!block->predecessors)
    {
        block->predecessors = ir_block_list_create(predecessor);
    }
    else
    {
        ir_block_list_insert(block->predecessors, predecessor);
    }
    block->predecessors_count++;
    if (!predecessor->successors)
    {
        predecessor->successors = ir_block_list_create(block);
    }
    else
    {
        ir_block_list_insert(predecessor->successors, block);
    }
}

void ir_add_successor(ir_block_t* block, ir_block_t* successor)
{
    if (!block->successors)
    {
        block->successors = ir_block_list_create(successor);
    }
    else
    {
        ir_block_list_insert(block->successors, successor);
    }
    block->successors_count++;
    if (!successor->predecessors)
    {
        successor->predecessors = ir_block_list_create(block);
    }
    else
    {
        ir_block_list_insert(successor->predecessors, block);
    }
}