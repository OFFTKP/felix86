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

constexpr static u64 block_code_alignment = 4;

struct TranslationSize {
    u16 x86_instruction_size : 4;
    u16 riscv_instructions_size : 12;
};
static_assert(sizeof(TranslationSize) == sizeof(u16));

struct LinkSite {
    u32 offset;
    bool return_hint;
    TaggedAddress target;
};

struct PublishInfo {
    void* riscv_code;
    u64 size;
    u64 guest_rip;
    u64 guest_rip_end;
    bool mode32;
    bool self_validating;
};

using InterruptibleSyscallFunc = u64 (*)(u64* effective_deferred_signals, u64 nr, u64 arg1, u64 arg2, u64 arg3, u64 arg4, u64 arg5, u64 arg6);

struct ThreadState;
struct felix86_frame;

enum class PendingLinkFlags : u64 {
    HasReturnHint = 0x8000'0000'0000'0000,
};

struct BlockMetadata {
    u64 host_address{};
    u64 guest_address{};
    // This gives us a count of x86 instructions per block, the size of each instruction
    // and the size of the risc-v instructions used to translate it
    std::vector<TranslationSize> translation_sizes{};
    std::vector<u16> dirty_upper_gprs{};
    union {
        struct {
            // How many times this block was recompiled. After smc_threshold it will no longer protect the page
            // and instead emit a self-validating block
            u64 smc_hits : 8;
            u64 self_validating : 1;
            u64 last_smc_ms : 32;
            u64 unused : 23;
        };
        u64 raw{};
    } misc_info{};
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
    CodeCache();
    ~CodeCache();
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
        return mapping_start;
    }

    u8* getBlockArea() const {
        return mapping_start + thunk_area_size;
    }

    u64 getBlockAreaSize() const;

    bool isInUsedArea(u64 pc) const {
        return pc >= (u64)getBlockArea() && pc < (u64)cursor;
    }

    u8* copyBlock(const PublishInfo& info);

    BlockMetadata& getBlockMetadata(TaggedAddress address) {
        return block_metadata[address];
    }

    bool blockExists(TaggedAddress address) {
        return getBlockMetadata(address).host_address != 0;
    }

    void addBlockToPages(BlockMetadata* block, u64 guest_start, u64 guest_end);

    void addHostPcLookup(u64 last_host_byte, BlockMetadata* block) {
        host_pc_map[last_host_byte] = block;
    }

    BlockMetadata* getBlockByHostPc(u64 host_pc);

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

    void clear();

private:
    u8* getMappedEnd() const;

    bool grow();

    u8* mapping_start{};
    void* reservation_base{};
    u64 reservation_size{};
    u64 size_index = 0;
    u8* cursor{};
    CodeCachePointers pointers{};

    std::unordered_map<TaggedAddress, BlockMetadata> block_metadata{};

    Semaphore page_map_lock;
    std::map<u64, std::vector<BlockMetadata*>> page_map{};

    // For fast host pc -> block metadata lookup (binary search vs looking up one by one)
    // on signal handlers
    std::map<u64, BlockMetadata*> host_pc_map{};
};
