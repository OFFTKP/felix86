#pragma once

#include <functional>
#include "felix86/common/types.hpp"

union TaggedAddress {
    struct {
        u64 address_bits : 56;
        u64 unused : 7;
        u64 mode32 : 1;
    };

    u64 raw;

    bool operator==(const TaggedAddress& other) const {
        return raw == other.raw;
    }

    static TaggedAddress make(u64 rip, bool mode32) {
        TaggedAddress t{};
        t.address_bits = rip;
        t.mode32 = mode32;
        return t;
    }
};
static_assert(sizeof(TaggedAddress) == sizeof(u64));

template <>
struct std::hash<TaggedAddress> {
    size_t operator()(const TaggedAddress& address) const noexcept {
        return std::hash<u64>{}(address.raw);
    }
};
