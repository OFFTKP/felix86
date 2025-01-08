#include <openssl/md5.h>
#include "felix86/common/hash.hpp"
#include "fmt/format.h"

Hash felix86_hash(const void* data, size_t size, Hash seed) {
    MD5((const u8*)data, size, (u8*)&seed.values);
    return seed;
}

std::string Hash::ToString() {
    return fmt::format("{:016x}{:016x}", values[1], values[0]);
}