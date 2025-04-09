#pragma once

#include <cassert>
#include "felix86/common/utility.hpp"

// TODO: remove all of this. We no longer allocate a 32-bit address space in some random address, we allocate it at the start of the address space

// Constructs for explicit conversion between guest and host addresses
struct GuestAddress;

struct HostAddress {
    HostAddress() : address(0) {}
    explicit constexpr HostAddress(u64 address) : address(address) {}

    [[nodiscard]] GuestAddress toGuest() const;

    [[nodiscard]] u64 raw() const {
        return address;
    }

    bool operator==(const HostAddress& other) const {
        return address == other.address;
    }

    bool operator!=(const HostAddress& other) const {
        return address != other.address;
    }

    bool operator<(const HostAddress& other) const {
        return address < other.address;
    }

    bool operator>(const HostAddress& other) const {
        return address > other.address;
    }

    bool operator<=(const HostAddress& other) const {
        return address <= other.address;
    }

    bool operator>=(const HostAddress& other) const {
        return address >= other.address;
    }

    HostAddress& operator+=(u64 offset) {
        address += offset;
        return *this;
    }

    bool isNull() const {
        return address == 0;
    }

    [[nodiscard]] HostAddress add(u64 offset) const {
        return HostAddress(address + offset);
    }

private:
    u64 address;
};

struct GuestAddress {
    GuestAddress() : address(0) {}
    explicit constexpr GuestAddress(u64 address) : address(address) {
        assert(address < (u64)UINT32_MAX);
    }

    [[nodiscard]] HostAddress toHost() const {
        // This would add the address space base in 32-bit mode. However we no longer use it and now it does nothing.
        return HostAddress(address);
    }

    [[nodiscard]] u64 raw() const {
        return address;
    }

    bool isNull() {
        return address == 0;
    }

    bool operator==(const GuestAddress& other) const {
        return address == other.address;
    }

private:
    u64 address;
};

inline GuestAddress HostAddress::toGuest() const {
    return GuestAddress(address);
}
