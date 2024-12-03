#include <openssl/md5.h>
#include "felix86/common/hash.hpp"

Hash felix86_hash(const void* data, size_t size, Hash seed) {
    MD5((const u8*)data, size, (u8*)&seed.values);
    return seed;
}