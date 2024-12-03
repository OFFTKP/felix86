#pragma once

#include "felix86/backend/block.hpp"
#include "felix86/backend/serialized_function.hpp"
#include "felix86/ir/function.hpp"

struct BackendFunction {
    BackendFunction() = default;
    ~BackendFunction() {
        for (BackendBlock* block : blocks) {
            delete block;
        }
    }
    BackendFunction(const BackendFunction&) = delete;
    BackendFunction& operator=(const BackendFunction&) = delete;
    BackendFunction(BackendFunction&&) = default;
    BackendFunction& operator=(BackendFunction&&) = default;

    static BackendFunction FromIRFunction(const IRFunction* function);

    const std::vector<BackendBlock*>& GetBlocks() const {
        return blocks;
    }

    std::vector<BackendBlock*>& GetBlocks() {
        return blocks;
    }

    const BackendBlock& GetBlock(u32 index) const {
        return *blocks[index];
    }

    BackendBlock& GetBlock(u32 index) {
        return *blocks[index];
    }

    std::vector<const BackendBlock*> GetBlocksPostorder() const;

    [[nodiscard]] std::string Print() const;

    u64 GetStartAddress() const {
        return start_address;
    }

    void SetStartAddress(u64 address) {
        start_address = address;
    }

    void RemoveBlock(u32 block) {
        auto it = blocks.begin();
        while (it != blocks.end()) {
            if ((*it)->GetIndex() == block) {
                blocks.erase(it);
                return;
            }
            ++it;
        }
        ASSERT(false);
    }

    SerializedFunction Serialize() const;
    static BackendFunction Deserialize(const SerializedFunction& data);

private:
    std::vector<BackendBlock*> blocks;
    u64 start_address = 0;
};