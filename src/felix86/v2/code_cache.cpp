#include <algorithm>
#include <cerrno>
#include <cstring>
#include <iterator>
#include <sys/mman.h>
#include "felix86/common/global.hpp"
#include "felix86/common/log.hpp"
#include "felix86/common/perf.hpp"
#include "felix86/common/utility.hpp"
#include "felix86/v2/code_cache.hpp"
#include "felix86/v2/dispatcher.hpp"

CodeCache* g_shared_code_cache = nullptr;

CodeCache::CodeCache(CodeCacheType type) {
    void* address = MAP_FAILED;
    const u64 total_size = shared_code_cache_size;
    const u64 initial_size = shared_code_cache_size;
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
    pointers.start_of_code_cache = (u64)getBlockArea();

    int result = madvise(address, total_size, MADV_HUGEPAGE);
    if (result != 0) {
        WARN("Failed to set MADV_HUGEPAGE for code cache");
    }

    if (g_config.perf_global) {
        std::string name = "felix86 code cache " + std::to_string(gettid());
        u64 end_of_reservation = (u64)getBlockArea() + getBlockAreaSize();
        g_process_globals.perf->addToFile(pointers.start_of_code_cache, end_of_reservation - pointers.start_of_code_cache, name);
    }

    for (int i = 0; i < segment_count; i++) {
        segments[i] = Segment(mapping_start + i * segment_size);
    }
}

CodeCache::~CodeCache() {
    munmap(reservation_base, reservation_size);
}

BlockMetadata* CodeCache::publish(const PublishInfo& info) {
    Segment chosen_segment = chooseSegment(info);
    auto [instructions_pointer, metadata_pointer] =
        chosen_segment.allocatePointers(info.size, sizeof(BlockMetadata) + info.translation_counts.size());
    memcpy(instructions_pointer, info.riscv_code, info.size);
    BlockMetadata* metadata = (BlockMetadata*)metadata_pointer;
    metadata->count = info.translation_counts.size();
    metadata->guest_address = TaggedAddress::make(info.guest_rip, info.mode32);
    metadata->self_validating = info.self_validating;
    metadata->unused = 0;
    std::copy(info.translation_counts.begin(), info.translation_counts.end(), metadata->translation_sizes);
    return metadata;
}

Segment& CodeCache::chooseSegment(const PublishInfo& info) {}

u64 CodeCache::getBlockAreaSize() const {
    return (u64)mapping_start + shared_code_cache_size - (u64)getBlockArea();
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
    auto it = host_pc_map.lower_bound(host_pc);
    ASSERT(it != host_pc_map.end());
    BlockMetadata* block = it->second;
    u64 address_end = block->host_address;
    for (auto& sizes : block->translation_sizes) {
        address_end += sizes.riscv_instructions_size;
    }
    if (!(host_pc >= block->host_address && host_pc < address_end)) {
        WARN("PC: %lx not inside range %lx-%lx?", host_pc, block->host_address, address_end);
        return nullptr;
    }
    return block;
}

void CodeCache::clear() {
    auto guard = page_map_lock.lock();
    block_metadata.clear();
    host_pc_map.clear();
    page_map.clear();
}

CodeCache* CodeCacheManager::createPrivate() {
    return new CodeCache(CodeCacheType::Private);
}

CodeCache* CodeCacheManager::getShared() {
    if (!g_shared_code_cache) {
        g_shared_code_cache = new CodeCache(CodeCacheType::Shared);
        Dispatcher::emitDispatcher(g_shared_code_cache->getThunkArea(), CodeCache::thunk_area_size, g_shared_code_cache->getPointers(), false);
    }
    return g_shared_code_cache;
}
