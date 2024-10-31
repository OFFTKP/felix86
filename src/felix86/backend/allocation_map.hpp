#pragma once

#include <tsl/robin_map.h>
#include "felix86/backend/allocation.hpp"
#include "felix86/common/log.hpp"

struct AllocationMap {
    AllocationMap() = default;
    AllocationMap(const AllocationMap&) = default;
    AllocationMap& operator=(const AllocationMap&) = default;
    AllocationMap(AllocationMap&&) = default;
    AllocationMap& operator=(AllocationMap&&) = default;

    void Allocate(u32 name, biscuit::GPR gpr) {
        allocations[name] = gpr;
    }

    void Allocate(u32 name, biscuit::Vec vec) {
        allocations[name] = vec;
    }

    void Allocate(u32 name, AllocationType type, u32 index) {
        switch (type) {
        case AllocationType::GPR:
            Allocate(name, biscuit::GPR(index));
            break;
        case AllocationType::Vec:
            Allocate(name, biscuit::Vec(index));
            break;
        default:
            ASSERT_MSG(false, "Invalid allocation type");
        }
    }

    Allocation GetAllocation(u32 name) const {
        auto it = allocations.find(name);
        ASSERT_MSG(it != allocations.end(), "Allocation not found for name %s", GetNameString(name).c_str());
        return it->second;
    }

    u32 GetAllocationIndex(u32 name) const {
        Allocation allocation = GetAllocation(name);
        if (allocation.IsGPR()) {
            return allocation.AsGPR().Index();
        } else if (allocation.IsVec()) {
            return allocation.AsVec().Index();
        } else {
            ASSERT_MSG(false, "Invalid allocation type");
            return 0;
        }
    }

    bool IsAllocated(u32 name) const {
        return allocations.find(name) != allocations.end();
    }

    void SetSpillSize(u32 size) {
        spill_size = size;
    }

    u32 GetSpillSize() const {
        return spill_size;
    }

    auto begin() const {
        return allocations.begin();
    }

    auto end() const {
        return allocations.end();
    }

    auto size() const {
        return allocations.size();
    }

private:
    tsl::robin_map<u32, Allocation> allocations;
    u32 spill_size = 0;
};