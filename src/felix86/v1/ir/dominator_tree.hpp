#pragma once

#include <vector>
#include "felix86/ir/block.hpp"

struct IRDominatorTreeNode {
    IRBlock* block = nullptr;
    std::vector<IRDominatorTreeNode*> children = {};
};

struct IRDominatorTree {
    IRDominatorTree() = default;
    ~IRDominatorTree() = default;
    IRDominatorTree(const IRDominatorTree&) = delete;
    IRDominatorTree& operator=(const IRDominatorTree&) = delete;
    IRDominatorTree(IRDominatorTree&&) = default;
    IRDominatorTree& operator=(IRDominatorTree&&) = default;

    std::vector<IRDominatorTreeNode> nodes;
};