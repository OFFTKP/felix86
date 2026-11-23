#pragma once

#include <map>
#include <unordered_map>
#include <vector>
#include "felix86/common/process_lock.hpp"
#include "felix86/common/tagged_address.hpp"
#include "felix86/common/types.hpp"

#define FELIX86_HINT_NOT_MAPPED 0x4e
#define FELIX86_HINT_NOT_READ 0x15
#define FELIX86_HINT_NOT_EXEC 0xec
#define FELIX86_ERESTARTNOINTR 513

constexpr static u64 block_code_alignment = 8;

struct TranslationSize {
    u16 x86_instruction_size : 4;
    u16 riscv_instructions_size : 12;
};
static_assert(sizeof(TranslationSize) == sizeof(u16));

struct LinkSite {
    u32 offset;
    bool return_hint;
    bool skip_lookup;
    TaggedAddress target;
};

struct PublishInfo {
    void* riscv_code;
    u64 size;
    u64 guest_rip;
    u64 guest_rip_end;
    std::vector<TranslationSize> translation_counts;
    std::vector<LinkSite> link_sites;
    // If this bool is set, this code is part of a block that changes often. This makes it go
    // into specific segments, so as to not pollute and cause clears in the static code segments
    bool volatile_code;
    bool mode32;
    bool self_validating;
};

using InterruptibleSyscallFunc = u64 (*)(u64* effective_deferred_signals, u64 nr, u64 arg1, u64 arg2, u64 arg3, u64 arg4, u64 arg5, u64 arg6);

struct ThreadState;
struct felix86_frame;

enum class PendingLinkFlags : u64 {
    HasReturnHint = 0x8000'0000'0000'0000,
};

enum class InvalidationKind {
    Reset,
    Modify,
    NoExec,
};

// This metadata is part of the code cache. This way once a segment is cleared, the block metadata associated with
// the blocks in that segment go with it. BlockSlot points to this, and BlockSlot lives in the heap. When blocks get invalidated
// the BlockMetadata pointer in BlockSlot becomes null, so the smc info survive past invalidates.
struct BlockMetadata {
    u32 offset;              // offset into the code cache
    u32 count : 24;          // instruction count
    u32 self_validating : 1; // whether it's a sv block
    u32 unused : 7;
    TaggedAddress guest_address;
    // This gives us a count of x86 instructions per block, the size of each instruction
    // and the size of the risc-v instructions used to translate it
    TranslationSize translation_sizes[/* count sized */];
};
static_assert(sizeof(BlockMetadata) == sizeof(u64) * 2);

union SmcInfo {
    struct {
        // How many times this block was recompiled. After smc_threshold it will no longer protect the page
        // and instead emit a self-validating block
        u64 smc_hits : 8;
        u64 last_smc_ms : 32;
        u64 unused : 24;
    };
    u64 raw{};
};

struct BlockSlot {
    BlockMetadata* metadata{};
    // We always end blocks on CALL. This is because some games (e.g. Celeste) may include garbage or
    // unmapped pages if we don't end sequences on CALL. Since we want to do the CALL/RET optimization,
    // and the hardware RSB needs to match the return address, but we can't continue scanning after the call,
    // we place a jumpAndLink after the CALL, and populate the address cache with it. But if the address cache
    // entry of the return block gets evicted, the next return misses and the dispatcher re-inserts an entry.
    // When the block would get recompiled the address cache entry would point to the new block and RSB prediction
    // would fail. We want RSB to succeed and to not compile past the CALL, so this is our compromise.
    u64 ret_stub{};
    SmcInfo smc{};

    u64 getRetStub() const {
        return __atomic_load_n(&ret_stub, __ATOMIC_ACQUIRE);
    }

    void setRetStub(u64 stub) {
        __atomic_store_n(&ret_stub, stub, __ATOMIC_RELEASE);
    }
};

struct CodeCachePointers {
    u64 not_mapped{};
    u64 not_read{};
    u64 not_exec{};
    u64 start_of_code_cache{};
    InterruptibleSyscallFunc interruptible_syscall{};
    u64 interruptible_syscall_ecall{};
    u64 interruptible_syscall_eintr{};
    void (*enter_dispatcher)(ThreadState*){};
    void (*exit_dispatcher)(felix86_frame*){};
    u64 compile_next{};
    u64 compile_next_32{};
    u64 compile_next_skip_address_cache{};
    u64 restore_state{};
};

struct CodeCache {
    virtual ~CodeCache();
    CodeCache(const CodeCache&) = delete;
    CodeCache& operator=(const CodeCache&) = delete;

    static constexpr u64 thunk_area_size = 4096;

    const CodeCachePointers& getPointers() const {
        return pointers;
    }

    CodeCachePointers& getPointers() {
        return pointers;
    }

    u8* getThunkArea() const {
        return thunk_area;
    }

    u8* getBlockArea() const {
        return block_area;
    }

    u64 getBlockAreaSize() const {
        return block_area_size;
    }

    bool isInBlockArea(u64 pc) const {
        return pc >= (u64)getBlockArea() && pc < (u64)getBlockArea() + getBlockAreaSize();
    }

    bool isInThunkArea(u64 pc) const {
        return pc >= (u64)getThunkArea() && pc < (u64)getThunkArea() + thunk_area_size;
    }

    u8* getBlockCode(const BlockMetadata* metadata) const {
        return mapping_start + metadata->offset;
    }

    BlockSlot& getSlot(TaggedAddress address) {
        auto guard = slots_lock.lock();
        return block_slots[address];
    }

    BlockMetadata* getPublishedBlock(TaggedAddress address) {
        return __atomic_load_n(&getSlot(address).metadata, __ATOMIC_ACQUIRE);
    }

    bool blockExists(TaggedAddress address) {
        return getPublishedBlock(address) != nullptr;
    }

    void addBlockToPages(BlockMetadata* block, u64 guest_start, u64 guest_end);

    void addHostPcLookup(u64 last_host_byte, BlockMetadata* block) {
        auto guard = host_pc_lock.lock();
        host_pc_map[last_host_byte] = block;
    }

    BlockMetadata* getBlockByHostPc(u64 host_pc);

    void addPendingLink(TaggedAddress address, u64 link) {
        auto guard = pending_links_lock.lock();
        pending_links[address].push_back(link);
    }

    std::vector<u64> takePendingLinks(TaggedAddress address) {
        auto guard = pending_links_lock.lock();
        auto it = pending_links.find(address);
        if (it == pending_links.end()) {
            return {};
        }
        std::vector<u64> links = std::move(it->second);
        pending_links.erase(it);
        return links;
    }

    template <typename ShouldErase>
    int eraseBlocksInRange(u64 start, u64 end, ShouldErase&& should_erase) {
        auto guard = page_map_lock.lock();
        auto lower = page_map.lower_bound(start);
        auto upper = page_map.upper_bound(end - 1);

        int blocks = 0;
        for (auto it = lower; it != upper; it++) {
            blocks += std::erase_if(it->second, should_erase);
        }
        return blocks;
    }

    int invalidateRange(u64 start, u64 end, InvalidationKind kind);

    void invalidateBlock(BlockMetadata* block, bool is_smc);

    virtual BlockMetadata* copyBlock(const PublishInfo& info) = 0;

    virtual void clear() = 0;

    virtual bool canPatchPublishedCode() const = 0;

    [[nodiscard]] SemaphoreGuard lockLinks() {
        return links_lock.lock();
    }

    virtual bool canLinkTo(const BlockMetadata* target) const = 0;

    virtual u64 codeGeneration(u64 pc) const = 0;

    virtual void flushCode(u64 start, u64 end) const = 0;

    virtual void lockSegmentsBeforeFork() {}

    virtual void unlockSegmentsAfterFork() {}

    void lockBeforeFork();

    void unlockAfterFork();

    virtual void recordLink(u8* link_site, const BlockMetadata* target, u32 unlink_instruction) = 0;

protected:
    CodeCache() = default;

    void reserve(u64 total_size, u64 initial_size);

    void setAreas(u8* thunk_area, u8* block_area, u64 block_area_size);

    BlockMetadata* writeBlock(u8* code, u64 code_size, u8* record, const PublishInfo& info);

    u8* mapping_start{};
    void* reservation_base{};
    u64 reservation_size{};
    u8* thunk_area{};
    u8* block_area{};
    u64 block_area_size{};
    CodeCachePointers pointers{};

    Semaphore slots_lock;
    std::unordered_map<TaggedAddress, BlockSlot> block_slots{};

    Semaphore pending_links_lock;
    // Pointers to blocks to be linked. The top 7 bits are used to denote various flags
    // related to the block as a bitset.
    std::unordered_map<TaggedAddress, std::vector<u64>> pending_links{};

    Semaphore page_map_lock;
    std::map<u64, std::vector<BlockMetadata*>> page_map{};

    Semaphore links_lock;

    // For fast host pc -> block metadata lookup (binary search vs looking up one by one)
    // on signal handlers
    Semaphore host_pc_lock;
    std::map<u64, BlockMetadata*> host_pc_map{};
};

struct CodeCacheManager {
    static CodeCache* createPrivate();
    static CodeCache* getShared();
};
