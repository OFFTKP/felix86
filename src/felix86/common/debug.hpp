#pragma once

#include <string>
#include "felix86/common/utility.hpp"

struct MemoryMetadata {
    static void AddRegion(const std::string& name, u64 start, u64 end);

    static void AddInterpreterRegion(u64 start, u64 end);

    static std::string GetRegionName(u64 address);

    static std::pair<u64, u64> GetRegionByName(const std::string& name);

    static bool IsInInterpreterRegion(u64 address);

    static u64 GetOffset(u64 address);

    static void AddDeferredBreakpoint(const std::string& name, u64 offset);
};