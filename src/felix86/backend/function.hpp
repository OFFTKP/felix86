#pragma once

#include "felix86/backend/block.hpp"
#include "felix86/ir/function.hpp"

struct BackendFunction {
    BackendFunction() = default;
    BackendFunction(const BackendFunction&) = delete;
    BackendFunction& operator=(const BackendFunction&) = delete;
    BackendFunction(BackendFunction&&) = default;
    BackendFunction& operator=(BackendFunction&&) = default;

    static BackendFunction FromIRFunction(const IRFunction* function);

    const std::vector<BackendBlock>& GetBlocks() const {
        return blocks;
    }

    std::vector<BackendBlock>& GetBlocks() {
        return blocks;
    }

    const BackendBlock& GetBlock(u32 index) const {
        return blocks[index];
    }

    BackendBlock& GetBlock(u32 index) {
        return blocks[index];
    }

    std::vector<const BackendBlock*> GetBlocksPostorder() const;

    [[nodiscard]] std::string Print() const;

private:
    std::vector<BackendBlock> blocks;
};