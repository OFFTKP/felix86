#pragma once

#include <string>
#include "felix86/backend/serialized_function.hpp"

struct DiskCache {
    static bool Has(const std::string& key);
    static std::vector<u8> Read(const std::string& key);
    static void Write(const std::string& key, void* data, size_t size);
    static void Clear();
};