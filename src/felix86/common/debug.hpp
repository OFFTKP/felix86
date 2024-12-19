#pragma once

#include <string>
#include "felix86/common/utility.hpp"

struct MemoryMetadata {
    static void AddRegion(const std::string& name, u64 start, u64 end);

    static void AddInterpreterRegion(u64 start, u64 end);

    static std::string GetRegionName(u64 address);

    static bool IsInInterpreterRegion(u64 address);

    static u64 GetOffset(u64 address);
};