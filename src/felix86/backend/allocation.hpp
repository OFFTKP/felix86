#pragma once

#include <variant>
#include "biscuit/registers.hpp"
#include "felix86/common/utility.hpp"

enum class AllocationType : u8 {
    Null,
    GPR,
    FPR,
    Vec,
    Spill,
};

enum class SpillSize : u8 {
    Null = 0,
    Byte = 1,
    QWord = 8,
    XmmWord = 16,
};

struct Spill {
    u32 location = 0;
    SpillSize size = SpillSize::Null;
    AllocationType reg_type = AllocationType::Null;
};

// Don't change their order and make sure to properly update stuff if you add to the end
using AllocationInner = std::variant<std::monostate, biscuit::GPR, biscuit::FPR, biscuit::Vec, Spill>;
static_assert(std::variant_size_v<AllocationInner> == 5);
static_assert(std::is_same_v<std::monostate, std::variant_alternative_t<(u8)AllocationType::Null, AllocationInner>>);
static_assert(std::is_same_v<biscuit::GPR, std::variant_alternative_t<(u8)AllocationType::GPR, AllocationInner>>);
static_assert(std::is_same_v<biscuit::FPR, std::variant_alternative_t<(u8)AllocationType::FPR, AllocationInner>>);
static_assert(std::is_same_v<biscuit::Vec, std::variant_alternative_t<(u8)AllocationType::Vec, AllocationInner>>);
static_assert(std::is_same_v<Spill, std::variant_alternative_t<(u8)AllocationType::Spill, AllocationInner>>);

struct Allocation {
    Allocation() = default;
    Allocation(biscuit::GPR gpr) : allocation(gpr) {}
    Allocation(biscuit::FPR fpr) : allocation(fpr) {}
    Allocation(biscuit::Vec vec) : allocation(vec) {}
    Allocation(u32 spill, SpillSize size, AllocationType type) : allocation(Spill{spill, size, type}) {}

    bool IsGPR() const {
        return GetAllocationType() == AllocationType::GPR;
    }

    bool IsFPR() const {
        return GetAllocationType() == AllocationType::FPR;
    }

    bool IsVec() const {
        return GetAllocationType() == AllocationType::Vec;
    }

    bool IsSpilled() const {
        return GetAllocationType() == AllocationType::Spill;
    }

    bool IsValid() const {
        return GetAllocationType() != AllocationType::Null;
    }

    biscuit::GPR AsGPR() const {
        return std::get<biscuit::GPR>(allocation);
    }

    biscuit::FPR AsFPR() const {
        return std::get<biscuit::FPR>(allocation);
    }

    biscuit::Vec AsVec() const {
        return std::get<biscuit::Vec>(allocation);
    }

    Spill AsSpill() const {
        return std::get<Spill>(allocation);
    }

    AllocationType GetAllocationType() const {
        return (AllocationType)allocation.index();
    }

private:
    AllocationInner allocation;
};
