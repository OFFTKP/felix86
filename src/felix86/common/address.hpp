#pragma once

#include <cassert>
#include "felix86/common/utility.hpp"

extern u64 g_address_space_base;

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
        // Add the address space base pointer. On 64-bit mode this does nothing as the address space base is 0.
        // On 32-bit mode this converts to a host accessible pointer.
        return HostAddress(g_address_space_base + address);
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
    // Subtract the address space base pointer. On 64-bit mode this does nothing as the address space base is 0.
    // On 32-bit mode this converts to a guest accessible pointer, as it will now be in the 32-bit address space.
    return GuestAddress(address - g_address_space_base);
}
