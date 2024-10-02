#pragma once

#include <tsl/robin_map.h>
#include "felix86/common/utility.hpp"
#include "felix86/ir/function.hpp"

struct FunctionCache {
    ~FunctionCache() {
        deallocateAll();
    }

    IRFunction* CreateOrGetFunctionAt(u64 address) {
        auto it = map.find(address);
        if (it != map.end()) {
            return it->second;
        }

        IRFunction* function = allocateFunction(address);
        map[address] = function;
        return function;
    }

private:
    IRFunction* allocateFunction(u64 address) {
        return new IRFunction(address); // TODO: use a memory pool
    }

    void deallocateAll() {
        printf("Deallocator\n");
        for (auto& pair : map) {
            delete pair.second;
        }
    }

    tsl::robin_map<u64, IRFunction*> map{};
};