#pragma once

#include <string>

struct Backend;

struct DiskCache {
    DiskCache(const std::string& hash, Backend& backend);
};