#include <vector>
#include "felix86/common/debug.hpp"
#include "felix86/common/log.hpp"

struct Region {
    std::string name{};
    u64 start = 0;
    u64 end = 0;
    bool is_interpreter = false;
};

bool interpreter_added = false;
std::vector<Region> regions;

void MemoryMetadata::AddRegion(const std::string& name, u64 start, u64 end) {
    Region region;
    region.name = name;
    region.start = start;
    region.end = end;
    regions.push_back(region);
    VERBOSE("Added region %s: %016lx-%016lx", name.c_str(), start, end);
}

void MemoryMetadata::AddInterpreterRegion(u64 start, u64 end) {
    ASSERT(!interpreter_added);
    interpreter_added = true;
    Region region;
    region.name = "Interpreter";
    region.start = start;
    region.end = end;
    region.is_interpreter = true;
    regions.push_back(region);
    VERBOSE("Added interpreter region: %016lx-%016lx", start, end);
}

std::string MemoryMetadata::GetRegionName(u64 address) {
    for (const Region& region : regions) {
        if (address >= region.start && address < region.end) {
            return region.name;
        }
    }
    return "Unknown";
}

u64 MemoryMetadata::GetOffset(u64 address) {
    for (const Region& region : regions) {
        if (address >= region.start && address < region.end) {
            return address - region.start;
        }
    }
    return 0;
}

bool MemoryMetadata::IsInInterpreterRegion(u64 address) {
    for (const Region& region : regions) {
        if (address >= region.start && address < region.end && region.is_interpreter) {
            return true;
        }
    }
    return false;
}