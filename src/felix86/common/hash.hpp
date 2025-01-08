#pragma once

#include "felix86/common/utility.hpp"

struct Hash {
    u64 values[2] = {0, 0};

    std::string ToString();
};

Hash felix86_hash(const void* data, size_t size, Hash hash = {0, 0});