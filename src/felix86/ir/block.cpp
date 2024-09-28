#include <stdlib.h>
#include "felix86/common/log.hpp"
#include "felix86/frontend/frontend.hpp"
#include "felix86/ir/block.hpp"
#include "felix86/ir/emitter.hpp"

IRFunction::IRFunction(u64 address) {
    blocks.push_back(IRBlock());
    blocks.push_back(IRBlock());
    entry = &blocks[0];
    exit = &blocks[1];

    entry->SetIndex(0);
    exit->SetIndex(1);

    blocks.push_back(IRBlock());
    IRBlock* block = &blocks.back();
    block_map[address] = block;

    entry->TerminateJump(block);
    exit->TerminateExit();
}

IRBlock* IRFunction::CreateBlockAt(u64 address) {
    if (address != 0 && block_map.find(address) != block_map.end()) {
        return block_map[address];
    }

    blocks.push_back(IRBlock(address));
    IRBlock* block = &blocks.back();
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
    blocks.push_back(IRBlock());
    IRBlock* block = &blocks.back();
    block->SetIndex(blocks.size() - 1);
    return block;
}