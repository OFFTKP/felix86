#pragma once

#include <tsl/robin_map.h>
#include "felix86/backend/allocation.hpp"
#include "felix86/common/log.hpp"

struct AllocationMap {
    AllocationMap() = default;
    AllocationMap(const AllocationMap&) = delete;
    AllocationMap& operator=(const AllocationMap&) = delete;
    AllocationMap(AllocationMap&&) = default;
    AllocationMap& operator=(AllocationMap&&) = default;

    void Allocate(u32 name, biscuit::GPR gpr) {
        allocations[name] = gpr;
    }

    void Allocate(u32 name, biscuit::FPR fpr) {
        allocations[name] = fpr;
    }

    void Allocate(u32 name, biscuit::Vec vec) {
        allocations[name] = vec;
    }

    Allocation GetAllocation(u32 name) const {
        auto it = allocations.find(name);
        ASSERT_MSG(it != allocations.end(), "Allocation not found for name %s", GetNameString(name).c_str());
        return it->second;
    }

    bool IsAllocated(u32 name) const {
        return allocations.find(name) != allocations.end();
    }

    u32 IncrementSpillSize(u32 amount) {
        u32 old_size = spill_size;
        spill_size += amount;
        return old_size;
    }

    u32 GetSpillSize() const {
        return spill_size;
    }

private:
    tsl::robin_map<u32, Allocation> allocations;
    u32 spill_size = 0;
};