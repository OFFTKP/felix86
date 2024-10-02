#pragma once

#include "tsl/robin_map.h"
#include "felix86/ir/block.hpp"

struct IRFunction {
    IRFunction(u64 address);

    ~IRFunction();

    IRBlock* GetEntry() {
        return entry;
    }

    IRBlock* GetExit() {
        return exit;
    }

    IRBlock* CreateBlockAt(u64 address);

    IRBlock* GetBlockAt(u64 address);

    IRBlock* CreateBlock();

    std::vector<IRBlock*>& GetBlocks() {
        return blocks;
    }

    const std::vector<IRBlock*>& GetBlocks() const {
        return blocks;
    }

    bool IsCompiled() const {
        return compiled;
    }

    void SetCompiled() {
        compiled = true;
    }

    u64 GetStartAddress() const {
        return start_address_block->GetStartAddress();
    }

private:
    IRBlock* allocateBlock();

    void deallocateAll();

    IRBlock* entry = nullptr;
    IRBlock* exit = nullptr;
    IRBlock* start_address_block = nullptr;
    std::vector<IRBlock*> blocks;
    tsl::robin_map<u64, IRBlock*> block_map;
    bool compiled = false;
};