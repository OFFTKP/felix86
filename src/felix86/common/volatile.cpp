#include "volatile.hpp"

void VolatileMemory::RegisterInstruction(u64 adr) {
    instructions.insert(adr);
}

void VolatileMemory::RegisterRegion(u64 start, u64 size) {
    regions.push_back({start, start + size});
}

bool VolatileMemory::AdrIsVolatile(u64 adr) {
    if (instructions.contains(adr)) {
        return true;
    }

    for (auto [start, end] : regions) {
        if (adr >= start && adr < end) {
            return false;
        }
    }

    // If the access is not within the known volatile range, then assume volatility.
    return true;
}

void VolatileMemory::Clear() {
    regions.clear();
    instructions.clear();
}
