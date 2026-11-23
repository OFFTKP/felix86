#include <algorithm>
#include <cstring>
#include "felix86/common/log.hpp"
#include "felix86/common/qsbr.hpp"
#include "felix86/common/utility.hpp"
#include "felix86/v2/shared_code_cache.hpp"

SharedCodeCache::SharedCodeCache() {
    reserve(shared_code_cache_size + thunk_area_size, shared_code_cache_size + thunk_area_size);
    setAreas(mapping_start + shared_code_cache_size, mapping_start, shared_code_cache_size);

    for (u64 i = 0; i < segment_count; i++) {
        segments[i] = Segment(getBlockArea() + i * segment_size);
    }
    active = &segments[0];
    active.load()->setState(Active);
}

BlockMetadata* SharedCodeCache::copyBlock(const PublishInfo& info) {
    u32 code_size = (info.size + block_code_alignment - 1) & ~(block_code_alignment - 1);
    u32 metadata_size = sizeof(BlockMetadata) + info.translation_counts.size() * sizeof(TranslationSize);
    metadata_size = (metadata_size + alignof(BlockMetadata) - 1) & ~(alignof(BlockMetadata) - 1);
    ASSERT(code_size + metadata_size <= segment_size);
    u8 *instructions_pointer{}, *metadata_pointer{};
    Segment* chosen_segment = active.load(std::memory_order::acquire);
    while (true) {
        auto [instrs, meta] = chosen_segment->allocatePointers(code_size, metadata_size);
        if (instrs) {
            instructions_pointer = instrs;
            metadata_pointer = meta;
            break;
        }

        // If allocation fails then choose a new active segment
        QSBR::mark_offline();
        chosen_segment = newActiveSegment(chosen_segment);
        QSBR::mark_online();
    }
    return writeBlock(instructions_pointer, code_size, metadata_pointer, info);
}

void SharedCodeCache::flushCode(u64 start, u64 end) const {
    flush_icache_global(start, end);
}

Segment* SharedCodeCache::newActiveSegment(Segment* full) {
    auto guard = segment_lock.lock();

    // Check if another thread raced us
    Segment* current = active.load(std::memory_order::acquire);
    if (current != full) {
        return current;
    }

    full->setState(SegmentState::Full);
    u64 next_index = (full - segments.data() + 1) % segment_count;
    Segment* next = &segments[next_index];
    if (next->getState() == SegmentState::Retired) {
        if (!QSBR::is_all_past_epoch(next->getRetireEpoch())) {
            QSBR::wait_all_past_epoch(next->getRetireEpoch());
        }
    }
    ASSERT(next->getState() == SegmentState::Free || next->getState() == SegmentState::Retired);

    if (next->getState() == SegmentState::Retired) {
        __atomic_fetch_add(&segment_generation[next_index], 1, __ATOMIC_SEQ_CST);
        forgetSegmentCode(next->getBase(), next->getBase() + segment_size);
        dropOutgoing(next->getBase(), next->getBase() + segment_size);
    }

    {
        auto links_guard = lockLinks();
        link_target_closed[next_index] = false;
    }

    next->reset();
    next->setState(SegmentState::Active);
    active.store(next, std::memory_order::release);

    Segment* ahead = &segments[(next_index + 1) % segment_count];
    if (ahead->getState() == SegmentState::Full) {
        retireSegment(ahead);
    }
    return next;
}

void SharedCodeCache::retireSegment(Segment* segment) {
    forgetSegmentBlocks(segment->getBase(), segment->getBase() + segment_size);
    unlinkIncoming(segment - segments.data());
    __atomic_fetch_add(&segment_generation[segment - segments.data()], 1, __ATOMIC_SEQ_CST);
    segment->retire(QSBR::increment_global_epoch());
}

u64 SharedCodeCache::codeGeneration(u64 pc) const {
    if (!isInBlockArea(pc)) {
        return 0;
    }
    return __atomic_load_n(&segment_generation[segmentIndex((void*)pc)], __ATOMIC_SEQ_CST);
}

bool SharedCodeCache::canLinkTo(const BlockMetadata* target) const {
    return !link_target_closed[segmentIndex(getBlockCode(target))];
}

void SharedCodeCache::recordLink(u8* link_site, const BlockMetadata* target, u32 unlink_instruction) {
    u64 target_index = segmentIndex(getBlockCode(target));
    if (segmentIndex(link_site) != target_index) {
        incoming_links[target_index].push_back({link_site, unlink_instruction});
    }
}

void SharedCodeCache::unlinkIncoming(u64 index) {
    auto guard = lockLinks();
    link_target_closed[index] = true;
    std::vector<IncomingLink>& links = incoming_links[index];
    if (links.empty()) {
        return;
    }

    for (const IncomingLink& link : links) {
        u32* first_instruction = (u32*)link.link_site;
        ASSERT(((u64)first_instruction & 0x3) == 0);
        __atomic_store_n(first_instruction, link.unlink_instruction, __ATOMIC_RELEASE);
    }
    links.clear();
    flushCode((u64)getBlockArea(), (u64)getBlockArea() + getBlockAreaSize());
}

void SharedCodeCache::dropOutgoing(u8* start, u8* end) {
    auto guard = lockLinks();
    for (std::vector<IncomingLink>& links : incoming_links) {
        std::erase_if(links, [&](const IncomingLink& link) { return link.link_site >= start && link.link_site < end; });
    }
}

void SharedCodeCache::forgetSegmentBlocks(u8* start, u8* end) {
    auto in_segment = [start, end](u64 pointer) { return pointer >= (u64)start && pointer < (u64)end; };

    {
        auto guard = page_map_lock.lock();
        for (auto it = page_map.begin(); it != page_map.end();) {
            std::erase_if(it->second, [&](BlockMetadata* block) { return in_segment((u64)block); });
            if (it->second.empty()) {
                it = page_map.erase(it);
            } else {
                ++it;
            }
        }
    }

    auto guard = slots_lock.lock();
    for (auto& [address, slot] : block_slots) {
        BlockMetadata* metadata = __atomic_load_n(&slot.metadata, __ATOMIC_ACQUIRE);
        if (metadata && in_segment((u64)metadata)) {
            __atomic_compare_exchange_n(&slot.metadata, &metadata, nullptr, false, __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE);
        }
        if (in_segment(slot.getRetStub())) {
            slot.setRetStub(0);
        }
    }
}

void SharedCodeCache::forgetSegmentCode(u8* start, u8* end) {
    {
        auto guard = host_pc_lock.lock();
        host_pc_map.erase(host_pc_map.lower_bound((u64)start), host_pc_map.lower_bound((u64)end));
    }

    auto guard = pending_links_lock.lock();
    for (auto it = pending_links.begin(); it != pending_links.end();) {
        std::erase_if(it->second, [&](u64 link) {
            intptr_t signed_link = link;
            u64 link_address = (signed_link << 7) >> 7;
            return link_address >= (u64)start && link_address < (u64)end;
        });
        if (it->second.empty()) {
            it = pending_links.erase(it);
        } else {
            ++it;
        }
    }
}

void SharedCodeCache::clear() {
    if (invalidateRange(0, UINT64_MAX, InvalidationKind::Reset) > 0) {
        flushCode((u64)getBlockArea(), (u64)getBlockArea() + getBlockAreaSize());
    }

    {
        auto guard = slots_lock.lock();
        for (auto& [address, slot] : block_slots) {
            slot.setRetStub(0);
        }
    }

    auto guard = pending_links_lock.lock();
    pending_links.clear();
}
