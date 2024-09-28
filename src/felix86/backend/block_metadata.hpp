#pragma once

#include "felix86/common/utility.hpp"
#include "felix86/backend/block_metadata.hpp"

#include <tsl/robin_map.h>

#include <vector>

struct BlockMetadata {
    void MapCompiledFunction(u64 address, void* function) {
        map[address] = function;
    }

    void* GetCompiledFunction(u64 address) {
        if (map.find(address) != map.end()) {
            return map[address];
        }

        return nullptr;
    }

private:
    tsl::robin_map<u64, void*> map; // map functions to host code
};
