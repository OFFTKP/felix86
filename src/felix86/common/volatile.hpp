#pragma once

#include <filesystem>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include "felix86/common/types.hpp"

/// Whether a memory region is registered as volatile memory.
struct VolatileMemory {
    void RegisterRegion(u64 start, u64 size);
    void RegisterInstruction(u64 adr);
    bool AdrIsVolatile(u64 adr);

private:
    /// Keeps track of loaded DLLs and executable properties.
    std::vector<std::pair<u32, u32>> regions;
    std::unordered_set<u64> instructions;
};
