#include <vector>
#include "felix86/common/types.hpp"

/// Whether a memory region is registered as volatile memory.
struct VolatileMemory {
    void RegisterRegion(u64 start, u64 size);
    bool AdrInRegion(u64 adr);

private:
    std::vector<std::pair<u32, u32>> regions;
};
