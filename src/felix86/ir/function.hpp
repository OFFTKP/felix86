#pragma once

#include "felix86/common/hash.hpp"
#include "felix86/ir/block.hpp"
#include "felix86/ir/dominator_tree.hpp"
#include "tsl/robin_map.h"

struct IRFunction {
    IRFunction(u64 address);

    ~IRFunction();

    IRBlock* GetEntry() {
        return entry;
    }

    const IRBlock* GetEntry() const {
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

    [[nodiscard]] std::string Print(const std::function<std::string(const SSAInstruction*)>& callback);

    void UnvisitAll() const;

    bool Validate() const;

    bool ValidatePhis() const;

    const IRDominatorTree& GetDominatorTree() const {
        return dominator_tree;
    }

    void SetDominatorTree(IRDominatorTree&& tree) {
        dominator_tree = std::move(tree);

        if (dominator_tree.nodes[0].block != entry) {
            ERROR("Dominator tree first node isn't entry");
        }
    }

    SSAInstruction* ThreadStatePointer() {
        return thread_state_pointer;
    }

    std::vector<IRBlock*> GetBlocksPostorder();

    Hash& GetHashRef() {
        return hash;
    }

    Hash GetHash() const {
        return hash;
    }

private:
    void deallocateAll();

    Hash hash{};
    IRBlock* entry = nullptr;
    IRBlock* exit = nullptr;
    IRBlock* start_address_block = nullptr;
    SSAInstruction* thread_state_pointer = nullptr;
    std::vector<IRBlock*> blocks;
    tsl::robin_map<u64, IRBlock*> block_map;
    IRDominatorTree dominator_tree;
    bool compiled = false;
};