#pragma once

#include <variant>
#include "biscuit/registers.hpp"
#include "felix86/common/log.hpp"
#include "felix86/common/utility.hpp"

enum class AllocationType : u8 {
    Null,
    GPR,
    Vec,
    StaticSpillGPR, // Spill to a static register, for linear scan, it reserves the last 3 available registers for this
    StaticSpillVec,
};

struct SpillGPR {
    u32 index;
};

struct SpillVec {
    u32 location;
};

// Don't change their order and make sure to properly update stuff if you add to the end
using AllocationInner = std::variant<std::monostate, biscuit::GPR, biscuit::Vec, SpillGPR, SpillVec>;
static_assert(std::variant_size_v<AllocationInner> == 5);
static_assert(std::is_same_v<std::monostate, std::variant_alternative_t<(u8)AllocationType::Null, AllocationInner>>);
static_assert(std::is_same_v<biscuit::GPR, std::variant_alternative_t<(u8)AllocationType::GPR, AllocationInner>>);
static_assert(std::is_same_v<biscuit::Vec, std::variant_alternative_t<(u8)AllocationType::Vec, AllocationInner>>);
static_assert(std::is_same_v<SpillGPR, std::variant_alternative_t<(u8)AllocationType::StaticSpillGPR, AllocationInner>>);
static_assert(std::is_same_v<SpillVec, std::variant_alternative_t<(u8)AllocationType::StaticSpillVec, AllocationInner>>);

struct Allocation {
    Allocation() = default;
    Allocation(biscuit::GPR gpr) : allocation(gpr) {}
    Allocation(biscuit::Vec vec) : allocation(vec) {}
    Allocation(SpillGPR spill) : allocation(spill) {}
    Allocation(SpillVec spill) : allocation(spill) {}

    bool IsGPR() const {
        return GetAllocationType() == AllocationType::GPR;
    }

    bool IsVec() const {
        return GetAllocationType() == AllocationType::Vec;
    }

    bool IsSpillGPR() const {
        return GetAllocationType() == AllocationType::StaticSpillGPR;
    }

    bool IsSpillVec() const {
        return GetAllocationType() == AllocationType::StaticSpillVec;
    }

    bool IsValid() const {
        return GetAllocationType() != AllocationType::Null;
    }

    biscuit::GPR AsGPR() const {
        return std::get<biscuit::GPR>(allocation);
    }

    biscuit::Vec AsVec() const {
        return std::get<biscuit::Vec>(allocation);
    }

    SpillGPR AsSpillGPR() const {
        return std::get<SpillGPR>(allocation);
    }

    SpillVec AsSpillVec() const {
        return std::get<SpillVec>(allocation);
    }

    AllocationType GetAllocationType() const {
        return (AllocationType)allocation.index();
    }

    u32 GetIndex() const {
        if (IsGPR()) {
            return AsGPR().Index();
        } else if (IsVec()) {
            return AsVec().Index();
        } else {
            UNREACHABLE();
            return 0;
        }
    }

    u32 GetSpillLocation() const {
        if (IsSpillGPR()) {
            return AsSpillGPR().index;
        } else if (IsSpillVec()) {
            return AsSpillVec().location;
        } else {
            UNREACHABLE();
            return 0;
        }
    }

    operator biscuit::GPR() const {
        return AsGPR();
    }

    operator biscuit::Vec() const {
        return AsVec();
    }

private:
    AllocationInner allocation;
};
