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

    for (auto [start, size] : regions) {
        if (adr >= start && adr < start + size) {
            return false;
        }
    }

    // If the access is not within the known volatile range, then assume volatility.
    return true;
}
