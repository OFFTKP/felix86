#pragma once

#include <deque>
#include <unordered_map>
#include "felix86/backend/allocation.hpp"
#include "felix86/backend/registers.hpp"
#include "felix86/backend/serialized_function.hpp"
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

    void Spill(u32 name, AllocationType type, u32 location) {
        switch (type) {
        case AllocationType::StaticSpillGPR: {
            allocations[name] = Allocation(SpillGPR{location});
            break;
        }
        case AllocationType::StaticSpillVec: {
            allocations[name] = Allocation(SpillVec{location});
            break;
        }
        default:
            ASSERT_MSG(false, "Invalid allocation type");
        }
    }

    Allocation GetAllocation(u32 name) {
        auto it = allocations.find(name);
        ASSERT_MSG(it != allocations.end(), "Allocation not found for name %s", GetNameString(name).c_str());

        auto type = it->second.GetAllocationType();
        switch (type) {
        case AllocationType::StaticSpillGPR: {
            ASSERT(!available_spill_gprs.empty());
            biscuit::GPR gpr = available_spill_gprs.back();
            available_spill_gprs.pop_back();
            return gpr;
        }
        case AllocationType::StaticSpillVec: {
            ASSERT(!available_spill_vecs.empty());
            biscuit::Vec vec = available_spill_vecs.back();
            available_spill_vecs.pop_back();
            return vec;
        }
        default:
            break;
        }

        return it->second;
    }

    AllocationType GetAllocationType(u32 name) const {
        auto it = allocations.find(name);
        ASSERT_MSG(it != allocations.end(), "Allocation not found for name %s", GetNameString(name).c_str());
        return it->second.GetAllocationType();
    }

    u32 GetSpillLocation(u32 name) const {
        auto it = allocations.find(name);
        ASSERT_MSG(it != allocations.end(), "Allocation not found for name %s", GetNameString(name).c_str());
        return it->second.GetSpillLocation();
    }

    void ResetSpillRegisters() {
        if (available_spill_gprs.size() != 3) {
            available_spill_gprs.clear();
            auto it = Registers::GetAllocatableGPRs().end() - 3;
            for (auto i = it; i != Registers::GetAllocatableGPRs().end(); i++) {
                available_spill_gprs.push_back(*i);
            }
        }

        if (available_spill_vecs.size() != 3) {
            available_spill_vecs.clear();
            auto it = Registers::GetAllocatableVecs().end() - 3;
            for (auto i = it; i != Registers::GetAllocatableVecs().end(); i++) {
                available_spill_vecs.push_back(*i);
            }
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

    void Serialize(SerializedFunction& function) const {
        function.Push(spill_size);
        function.Push((u32)allocations.size());
        for (const auto& [name, allocation] : allocations) {
            function.Push(name);
            function.Push((u8)allocation.GetAllocationType());
            function.Push((u8)allocation.GetIndex());
        }
    }

    static AllocationMap Deserialize(const SerializedFunction& function) {
        AllocationMap allocations;
        allocations.spill_size = function.Pop<u32>();
        u32 count = function.Pop<u32>();
        for (u32 i = 0; i < count; i++) {
            u32 name = function.Pop<u32>();
            AllocationType type = (AllocationType)function.Pop<u8>();
            u32 index = function.Pop<u8>();
            allocations.Allocate(name, type, index);
        }
        return allocations;
    }

private:
    std::unordered_map<u32, Allocation> allocations;
    u32 spill_size = 0;

    std::deque<biscuit::GPR> available_spill_gprs;
    std::deque<biscuit::Vec> available_spill_vecs;
};