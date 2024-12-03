#pragma once

#include <variant>
#include "biscuit/registers.hpp"
#include "felix86/common/log.hpp"
#include "felix86/common/utility.hpp"

enum class AllocationType : u8 {
    Null,
    GPR,
    Vec,
};

// Don't change their order and make sure to properly update stuff if you add to the end
using AllocationInner = std::variant<std::monostate, biscuit::GPR, biscuit::Vec>;
static_assert(std::variant_size_v<AllocationInner> == 3);
static_assert(std::is_same_v<std::monostate, std::variant_alternative_t<(u8)AllocationType::Null, AllocationInner>>);
static_assert(std::is_same_v<biscuit::GPR, std::variant_alternative_t<(u8)AllocationType::GPR, AllocationInner>>);
static_assert(std::is_same_v<biscuit::Vec, std::variant_alternative_t<(u8)AllocationType::Vec, AllocationInner>>);

struct Allocation {
    Allocation() = default;
    Allocation(biscuit::GPR gpr) : allocation(gpr) {}
    Allocation(biscuit::Vec vec) : allocation(vec) {}

    bool IsGPR() const {
        return GetAllocationType() == AllocationType::GPR;
    }

    bool IsVec() const {
        return GetAllocationType() == AllocationType::Vec;
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

    operator biscuit::GPR() const {
        return AsGPR();
    }

    operator biscuit::Vec() const {
        return AsVec();
    }

private:
    AllocationInner allocation;
};
