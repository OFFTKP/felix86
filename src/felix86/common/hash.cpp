#include "felix86/common/hash.hpp"
#include "xxhash.h"

u64 felix86_hash(const void* data, size_t size, u64 seed) {
    return XXH64(data, size, seed);
}