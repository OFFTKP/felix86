#pragma once

#include "felix86/v2/code_cache.hpp"

struct PrivateCodeCache final : CodeCache {
    PrivateCodeCache();

    BlockMetadata* copyBlock(const PublishInfo& info) override;

    void clear() override;

    bool canPatchPublishedCode() const override {
        return true;
    }

    bool canLinkTo(const BlockMetadata*) const override {
        return true;
    }

    void recordLink(u8*, const BlockMetadata*, u32) override {}

    u64 codeGeneration(u64) const override {
        return 0;
    }

    void flushCode(u64 start, u64 end) const override;

private:
    u8* getMappedEnd() const;

    bool grow();

    u64 size_index = 0;
    u8* cursor{};
};
