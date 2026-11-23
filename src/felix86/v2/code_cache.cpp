#include <algorithm>
#include <cerrno>
#include <cstring>
#include <sys/mman.h>
#include "felix86/common/global.hpp"
#include "felix86/common/log.hpp"
#include "felix86/common/perf.hpp"
#include "felix86/common/utility.hpp"
#include "felix86/v2/code_cache.hpp"
#include "felix86/v2/dispatcher.hpp"
#include "felix86/v2/private_code_cache.hpp"
#include "felix86/v2/recompiler.hpp"
#include "felix86/v2/shared_code_cache.hpp"

CodeCache* g_shared_code_cache = nullptr;

CodeCache::~CodeCache() {
    munmap(reservation_base, reservation_size);
}

void CodeCache::reserve(u64 total_size, u64 initial_size) {
    void* address = MAP_FAILED;
    // Try allocating code cache near the program so that rip-relative immediates can be made in fewer instructions
    u64 min = std::min(g_executable_start, g_interpreter_start);
    // If the program is allocated in 32-bit address space then it's not worth performing this optimization
    // as to not interfere with MAP_32BIT and because immediates can be made in 2 instructions
    if (min > 5 * GB) {
        for (int i = 0; i < 56; i++) {
            min -= total_size;
            min &= ~(2 * MB - 1);
            address = ::mmap((void*)min, total_size, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE | MAP_FIXED_NOREPLACE, -1, 0);
            if (address != MAP_FAILED) {
                reservation_base = address;
                reservation_size = total_size;
                break;
            }
        }

        if (address == MAP_FAILED) {
            WARN("Failed to allocate code cache near executable");
        }
    }
    if (address == MAP_FAILED) {
        // Allocate an extra 2MB of virtual address space and skip it so that the mapping is aligned
        // to 2MB for the MADV_HUGEPAGE to work
        void* raw = ::mmap(nullptr, total_size + 2 * MB, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE, -1, 0);
        if (raw != MAP_FAILED) {
            reservation_base = raw;
            reservation_size = total_size + 2 * MB;
            address = (void*)(((u64)raw + 2 * MB - 1) & ~(2 * MB - 1));
        } else {
            address = MAP_FAILED;
        }
    }
    ASSERT_MSG(address != MAP_FAILED, "Failed to reserve code cache for thread %d?", gettid());
    u8* first_chunk = (u8*)::mmap((u8*)address, initial_size, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);
    ASSERT(first_chunk == (u8*)address);
    mapping_start = first_chunk;

    int result = madvise(address, total_size, MADV_HUGEPAGE);
    if (result != 0) {
        WARN("Failed to set MADV_HUGEPAGE for code cache");
    }
}

void CodeCache::setAreas(u8* thunk_area, u8* block_area, u64 block_area_size) {
    this->thunk_area = thunk_area;
    this->block_area = block_area;
    this->block_area_size = block_area_size;
    pointers.start_of_code_cache = (u64)getBlockArea();

    if (g_config.perf_global) {
        std::string name = "felix86 code cache " + std::to_string(gettid());
        g_process_globals.perf->addToFile(pointers.start_of_code_cache, getBlockAreaSize(), name);
    }
}

BlockMetadata* CodeCache::writeBlock(u8* code, u64 code_size, u8* record, const PublishInfo& info) {
    ASSERT(code_size >= info.size);
    memcpy(code, info.riscv_code, info.size);
    memset(code + info.size, 0, code_size - info.size);
    BlockMetadata* metadata = (BlockMetadata*)record;
    metadata->offset = code - mapping_start;
    metadata->count = info.translation_counts.size();
    metadata->guest_address = TaggedAddress::make(info.guest_rip, info.mode32);
    metadata->self_validating = info.self_validating;
    metadata->unused = 0;
    std::copy(info.translation_counts.begin(), info.translation_counts.end(), metadata->translation_sizes);
    return metadata;
}

void CodeCache::addBlockToPages(BlockMetadata* block, u64 guest_start, u64 guest_end) {
    auto guard = page_map_lock.lock();
    u64 start_masked = guest_start & ~0xFFFull;
    u64 end_masked = (guest_end - 1) & ~0xFFFull;
    for (u64 page = start_masked; page <= end_masked; page += 0x1000) {
        std::vector<BlockMetadata*>& blocks_in_page = page_map[page];
        if (std::find(blocks_in_page.begin(), blocks_in_page.end(), block) == blocks_in_page.end()) {
            blocks_in_page.push_back(block);
        }
    }
}

BlockMetadata* CodeCache::getBlockByHostPc(u64 host_pc) {
    BlockMetadata* block;
    {
        auto guard = host_pc_lock.lock();
        auto it = host_pc_map.lower_bound(host_pc);
        if (it == host_pc_map.end()) {
            WARN("PC: %lx not inside any block?", host_pc);
            return nullptr;
        }
        block = it->second;
    }
    u64 address_start = (u64)getBlockCode(block);
    u64 address_end = address_start;
    for (u32 i = 0; i < block->count; i++) {
        address_end += block->translation_sizes[i].riscv_instructions_size;
    }
    if (!(host_pc >= address_start && host_pc < address_end)) {
        WARN("PC: %lx not inside range %lx-%lx?", host_pc, address_start, address_end);
        return nullptr;
    }
    return block;
}

int CodeCache::invalidateRange(u64 start, u64 end, InvalidationKind kind) {
    const bool is_smc = kind != InvalidationKind::Reset;
    const bool keep_self_validating = kind == InvalidationKind::Modify;
    return eraseBlocksInRange(start, end, [&](BlockMetadata* block) {
        if (keep_self_validating && block->self_validating) {
            // Don't invalidate self-validating blocks
            return false;
        }
        invalidateBlock(block, is_smc);
        return true;
    });
}

void CodeCache::invalidateBlock(BlockMetadata* block, bool is_smc) {
    BlockSlot& slot = getSlot(block->guest_address);
    if (!is_smc) {
        slot.smc.smc_hits = 0;
    }

    Recompiler::patchBlockEntry((u64)getBlockCode(block));

    BlockMetadata* expected = block;
    bool cleared = __atomic_compare_exchange_n(&slot.metadata, &expected, nullptr, false, __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE);
    if (cleared && is_smc) {
        Recompiler::addSmcHit(slot);
    }
}

void CodeCache::lockBeforeFork() {
    links_lock.lock_before_fork();
    page_map_lock.lock_before_fork();
    slots_lock.lock_before_fork();
    host_pc_lock.lock_before_fork();
    pending_links_lock.lock_before_fork();
}

void CodeCache::unlockAfterFork() {
    pending_links_lock.unlock_after_fork();
    host_pc_lock.unlock_after_fork();
    slots_lock.unlock_after_fork();
    page_map_lock.unlock_after_fork();
    links_lock.unlock_after_fork();
}

CodeCache* CodeCacheManager::createPrivate() {
    return new PrivateCodeCache();
}

CodeCache* CodeCacheManager::getShared() {
    if (!g_shared_code_cache) {
        g_shared_code_cache = new SharedCodeCache();
        Dispatcher::emitDispatcher(g_shared_code_cache->getThunkArea(), CodeCache::thunk_area_size, g_shared_code_cache->getPointers(), false);
    }
    return g_shared_code_cache;
}
