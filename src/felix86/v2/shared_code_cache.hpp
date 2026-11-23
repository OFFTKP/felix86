#pragma once

#include <array>
#include <atomic>
#include "felix86/v2/code_cache.hpp"

constexpr static u64 shared_code_cache_size = 256 * MB;
constexpr static u64 segment_count = 32;
constexpr static u64 segment_size = shared_code_cache_size / segment_count;
static_assert(segment_count >= 2);

// This layout allows us to atomically increment both offsets with a single fetch_add
// Since we know each segment is always going to be < 32 bits, the low counter won't overflow into the top one
union SegmentOffsets {
    struct {
        u64 instructions_offset : 32;
        u64 metadata_offset : 32;
    } combined;
    u64 raw;
};
static_assert(sizeof(SegmentOffsets) == sizeof(u64));

enum SegmentState : u8 { Free, Active, Full, Retired };

struct Segment {
    Segment() : base(nullptr) {};
    explicit Segment(u8* base) : base(base) {
        offsets.raw = 0;
    };

    std::pair<u8*, u8*> allocatePointers(u32 instructions_size, u32 metadata_size) {
        u64 add = (u64)metadata_size << 32 | instructions_size;
        SegmentOffsets old = {.raw = __atomic_fetch_add(&offsets.raw, add, __ATOMIC_RELAXED)};
        SegmentOffsets added = {.raw = old.raw + add};
        ASSERT(old.combined.instructions_offset < added.combined.instructions_offset);
        ASSERT(old.combined.metadata_offset < added.combined.metadata_offset);
        ASSERT(base);
        u8* instructions_pointer = base + old.combined.instructions_offset;
        u8* metadata_pointer = base + segment_size - added.combined.metadata_offset;
        if (metadata_pointer < base + added.combined.instructions_offset) {
            return {nullptr, nullptr}; // out of space
        }
        return {instructions_pointer, metadata_pointer};
    }

    void setState(SegmentState value) {
        __atomic_store(&state, &value, __ATOMIC_RELEASE);
    }

    SegmentState getState() const {
        SegmentState value;
        __atomic_load(&state, &value, __ATOMIC_ACQUIRE);
        return value;
    }

    void reset() {
        __atomic_store_n(&offsets.raw, 0, __ATOMIC_RELAXED);
    }

    u8* getBase() const {
        return base;
    }

    void retire(u64 epoch) {
        retire_epoch = epoch;
        setState(SegmentState::Retired);
    }

    u64 getRetireEpoch() const {
        return retire_epoch;
    }

private:
    SegmentOffsets offsets;
    u8* base;
    SegmentState state = Free;
    u64 retire_epoch = 0;
};

struct SharedCodeCache final : CodeCache {
    SharedCodeCache();

    BlockMetadata* copyBlock(const PublishInfo& info) override;

    void clear() override;

    bool canPatchPublishedCode() const override {
        return false;
    }

    bool canLinkTo(const BlockMetadata* target) const override;

    void recordLink(u8* link_site, const BlockMetadata* target, u32 unlink_instruction) override;

    u64 codeGeneration(u64 pc) const override;

    void flushCode(u64 start, u64 end) const override;

    void lockSegmentsBeforeFork() override {
        segment_lock.lock_before_fork();
    }

    void unlockSegmentsAfterFork() override {
        segment_lock.unlock_after_fork();
    }

private:
    struct IncomingLink {
        u8* link_site;
        u32 unlink_instruction;
    };

    u64 segmentIndex(const void* pointer) const {
        return ((u64)pointer - (u64)getBlockArea()) / segment_size;
    }

    void unlinkIncoming(u64 index);

    void dropOutgoing(u8* start, u8* end);

    Segment* newActiveSegment(Segment* full);

    void retireSegment(Segment* segment);

    void forgetSegmentBlocks(u8* start, u8* end);

    void forgetSegmentCode(u8* start, u8* end);

    std::array<Segment, segment_count> segments{};
    std::atomic<Segment*> active{};
    Semaphore segment_lock;

    std::array<std::vector<IncomingLink>, segment_count> incoming_links{};
    std::array<bool, segment_count> link_target_closed{};
    std::array<u64, segment_count> segment_generation{};
};
