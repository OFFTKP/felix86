#include "volatile.hpp"

void VolatileMemory::RegisterRegion(u64 start, u64 size) {
    regions.push_back({start, start + size});
}

bool VolatileMemory::AdrInRegion(u64 adr) {
    for (auto [start, size] : regions) {
        if (adr >= start && adr < start + size) {
            return true;
        }
    }
    return false;
}
