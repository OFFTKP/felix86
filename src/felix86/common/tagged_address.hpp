#pragma once

#include <functional>
#include "felix86/common/log.hpp"
#include "felix86/common/types.hpp"

// This struct exists for the reason that it's possible a program decides to jump to the same code as 32-bit mode
// and later as 64-bit mode without changing it. Nothing disallows it from happening, so DRM may choose to do it.
// We don't want to invalidate the entire code cache and address cache on mode switches as they can be quite common
// e.g. in WoW64. We also don't want to use a second code cache and consume even more memory. So one code cache
// can contain both 64-bit and 32-bit blocks at a time.
//
// A 32-bit lookup can't hit a 64-bit entry:
// Take an address like 0x400000. In both 32-bit mode and 64-bit mode, a block at that address will live at
// address_cache[0]. However, if the block was compiled as 32-bit, the {guest, host} entry will be
// {~0x400000, host}, while in 64-bit mode it will be {0x400000, host}. This way, the entry will only match
// if it runs as the same mode it was compiled. We don't allow entries with invalid addresses in the address cache
// or the block map, we instead return our not_mapped thunk. So there will never be a time that a block with actual
// negative guest address exists. This means that reserving all negative rips for 32-bit blocks is safe.
//
// A 64-bit lookup can't hit a 32-bit entry either:
// Say it jumps to 0xFFFF'FFFF'FFBF'FFFF (which is ~0x400000). It goes through address cache before getCompiledBlock.
// It will lookup entry address_cache[0x1FFFF]. Can it be equal to 0xFFFF'FFFF'FFBF'FFFF? No. In 64-bit mode, such a rip can never
// be inserted. In 32-bit mode, this value CAN be stored as a tag, but only if rip is 0x400000. But in that case, it goes in slot address_cache[0],
// not address_cache[0x1FFFF]. So it always misses.
struct TaggedAddress {
    u64 raw;

    bool operator==(const TaggedAddress& other) const {
        return raw == other.raw;
    }

    bool mode32() const {
        return raw >> 63;
    }

    // We store 32-bit addresses as ~address in address cache and block map, such that no collisions can happen
    // with 64-bit blocks. If we didn't do this and instead used the top bit to mark 32-bit blocks, if a thread
    // tried to jump to the invalid address 0x8000'0000'0001'0000, we would either have to catch it in the addressCacheLookup
    // which would add instructions to the 64-bit lookup, or it could jump to a 32-bit block instead of to our
    // not_mapped thunk. A 64-bit entry can't have these upper bits set, as isAddressable function catches it and properly faults.
    static TaggedAddress make(u64 rip, bool mode32) {
        ASSERT((rip >> 63) == 0);
        return TaggedAddress{mode32 ? ~rip : rip};
    }

    // A tagged address that never matches its slot, as the lowest bit is different. For example, when clearing the address cache
    // we can memset it to 0, but we also have to mark slot 0 as invalid, otherwise address_cache[0] would match rip == 0
    // and jump to null.
    static TaggedAddress invalid(u64 rip) {
        return TaggedAddress{rip ^ 1};
    }
};
static_assert(sizeof(TaggedAddress) == sizeof(u64));

template <>
struct std::hash<TaggedAddress> {
    size_t operator()(const TaggedAddress& address) const noexcept {
        return std::hash<u64>{}(address.raw);
    }
};
