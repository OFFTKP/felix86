#include <cerrno>
#include <cstring>
#include <iterator>
#include <sys/mman.h>
#include "felix86/common/log.hpp"
#include "felix86/common/utility.hpp"
#include "felix86/v2/private_code_cache.hpp"

constexpr static u64 code_cache_sizes[] = {
    4 * MB,
    8 * MB,
    16 * MB,
    32 * MB,
};

constexpr static u64 code_cache_sizes_count = std::size(code_cache_sizes);
constexpr static u64 max_code_cache_size = code_cache_sizes[code_cache_sizes_count - 1];

PrivateCodeCache::PrivateCodeCache() {
    reserve(max_code_cache_size, code_cache_sizes[0]);
    setAreas(mapping_start, mapping_start + thunk_area_size, max_code_cache_size - thunk_area_size);
    cursor = getBlockArea();
}

u8* PrivateCodeCache::getMappedEnd() const {
    return mapping_start + code_cache_sizes[size_index];
}

bool PrivateCodeCache::grow() {
    if (size_index == code_cache_sizes_count - 1) {
        return false;
    }

    // Allocate more of our reserved buffer
    u64 old_size = code_cache_sizes[size_index];
    u8* past_end = mapping_start + old_size;
    ASSERT(!((u64)past_end & 0xFFF));
    size_index++;
    u64 new_size = code_cache_sizes[size_index];
    u64 size_difference = new_size - old_size;

    void* address = ::mmap(past_end, size_difference, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);
    if (address != past_end) {
        // TODO: investigate
        // Unsure what causes this to happen, but it does. In that case, clear code cache and carry on
        // Perhaps PR_MDWE_REFUSE_EXEC_GAIN
        WARN("Couldn't increment code cache because mmap returned %p (errno: %s), clearing code cache", address, strerror(errno));

        // Undo the size increment
        size_index--;
        return false;
    }

    madvise(address, size_difference, MADV_HUGEPAGE);
    return true;
}

BlockMetadata* PrivateCodeCache::copyBlock(const PublishInfo& info) {
    u64 code_size = (info.size + block_code_alignment - 1) & ~(block_code_alignment - 1);
    u64 metadata_size = sizeof(BlockMetadata) + info.translation_counts.size() * sizeof(TranslationSize);
    metadata_size = (metadata_size + alignof(BlockMetadata) - 1) & ~(alignof(BlockMetadata) - 1);
    while (cursor + code_size + metadata_size > getMappedEnd()) {
        if (!grow()) {
            return nullptr;
        }
    }

    u8* code = cursor;
    BlockMetadata* metadata = writeBlock(code, code_size, code + code_size, info);
    cursor += code_size + metadata_size;
    return metadata;
}

void PrivateCodeCache::flushCode(u64 start, u64 end) const {
    flush_icache(start, end);
}

void PrivateCodeCache::clear() {
    {
        auto guard = page_map_lock.lock();
        page_map.clear();
    }

    {
        auto guard = slots_lock.lock();
        block_slots.clear();
    }

    {
        auto guard = host_pc_lock.lock();
        host_pc_map.clear();
    }

    {
        auto guard = pending_links_lock.lock();
        pending_links.clear();
    }

    cursor = getBlockArea();
}
