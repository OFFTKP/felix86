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
std::unordered_map<std::string, std::vector<u64>> deferred_breakpoints;

void check_deferred_breakpoints(const std::string& name) {
    if (deferred_breakpoints.find(name) != deferred_breakpoints.end()) {
        for (u64 offset : deferred_breakpoints[name]) {
            guest_breakpoint(name.c_str(), offset);
            LOG("Added deferred breakpoint %s@%016lx", name.c_str(), offset);
        }
        deferred_breakpoints.erase(name);
    }
}

void MemoryMetadata::AddRegion(const std::string& name, u64 start, u64 end) {
    Region region;
    region.name = name;
    region.start = start;
    region.end = end;
    regions.push_back(region);
    VERBOSE("Added region %s: %016lx-%016lx", name.c_str(), start, end);
    check_deferred_breakpoints(name);
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
    check_deferred_breakpoints("Interpreter");
}

std::string MemoryMetadata::GetRegionName(u64 address) {
    std::string name = "Unknown";
    for (const Region& region : regions) {
        if (address >= region.start && address < region.end) {
            name = region.name;
            break;
        }
    }
    return name;
}

u64 MemoryMetadata::GetOffset(u64 address) {
    u64 offset = 0;
    for (const Region& region : regions) {
        if (address >= region.start && address < region.end) {
            offset = address - region.start;
            break;
        }
    }
    return offset;
}

bool MemoryMetadata::IsInInterpreterRegion(u64 address) {
    bool found = false;
    for (const Region& region : regions) {
        if (address >= region.start && address < region.end && region.is_interpreter) {
            found = true;
            break;
        }
    }
    return found;
}

std::pair<u64, u64> MemoryMetadata::GetRegionByName(const std::string& name) {
    std::pair<u64, u64> result = {0, 0};
    for (const Region& region : regions) {
        if (region.name == name) {
            result = {region.start, region.end};
            break;
        }
    }
    return result;
}

void MemoryMetadata::AddDeferredBreakpoint(const std::string& name, u64 offset) {
    deferred_breakpoints[name].push_back(offset);
}