#include <stdlib.h>
#include "felix86/common/log.hpp"
#include "felix86/ir/block.hpp"
#include "felix86/ir/emitter.hpp"

IRFunction::IRFunction(u64 address) {
    blocks.push_back(allocateBlock());
    blocks.push_back(allocateBlock());
    entry = blocks[0];
    exit = blocks[1];

    for (u8 i = 0; i < X86_REF_COUNT; i++) {
        // Load all state from memory and run the set_guest instruction
        // See ssa_pass.cpp for more information
        IRInstruction* value = ir_emit_load_guest_from_memory(entry, x86_ref_e(i));
        ir_emit_set_guest(entry, x86_ref_e(i), value);
    }

    for (u8 i = 0; i < X86_REF_COUNT; i++) {
        // Emit get_guest for every piece of state and store it to memory
        // These get_guests will be replaced with movs from a temporary or a phi
        // during the ssa pass
        IRInstruction* value = ir_emit_get_guest(exit, x86_ref_e(i));
        ir_emit_store_guest_to_memory(exit, x86_ref_e(i), value);
    }

    entry->SetIndex(0);
    exit->SetIndex(1);

    start_address_block = CreateBlockAt(address);

    entry->TerminateJump(start_address_block);
    exit->TerminateExit();
}

IRFunction::~IRFunction() {
    deallocateAll();
}

IRBlock* IRFunction::CreateBlockAt(u64 address) {
    if (address != 0 && block_map.find(address) != block_map.end()) {
        return block_map[address];
    }

    blocks.push_back(new IRBlock(address));
    IRBlock* block = blocks.back();
    block->SetIndex(blocks.size() - 1);

    if (address != 0) {
        block_map[address] = block;
    }

    return block;
}

IRBlock* IRFunction::GetBlockAt(u64 address) {
    if (block_map.find(address) != block_map.end()) {
        return block_map[address];
    }

    ERROR("Block not found: %016lx", address);
}

IRBlock* IRFunction::CreateBlock() {
    blocks.push_back(allocateBlock());
    IRBlock* block = blocks.back();
    block->SetIndex(blocks.size() - 1);
    return block;
}

IRBlock* IRFunction::allocateBlock() {
    return new IRBlock(); // TODO: use a memory pool
}

void IRFunction::deallocateAll() {
    for (auto& pair : block_map) {
        delete pair.second;
    }
}