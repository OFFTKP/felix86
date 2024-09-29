#pragma once

#include "felix86/common/utility.hpp"
#include "felix86/ir/block.hpp"
#include <tsl/robin_map.h>

struct FunctionCache {
    IRFunction* CreateOrGetFunctionAt(u64 address) {
        auto it = map.find(address);
        if (it != map.end()) {
            return it->second;
        }

        storage.push_back(IRFunction(address));

        IRFunction* function = &storage.back();
        map[address] = function;
        return function;
    }
private:
    std::vector<IRFunction> storage {};
    tsl::robin_map<u64, IRFunction*> map {};
};