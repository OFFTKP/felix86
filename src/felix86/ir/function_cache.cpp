#include "felix86/ir/function_cache.hpp"

#include <tsl/robin_map.h>

#include "felix86/common/log.hpp"

extern "C" struct ir_function_cache_s {
    std::vector<IRFunction> storage;
    tsl::robin_map<u64, IRFunction*> functions;
};

ir_function_cache_t* ir_function_cache_create() {
    return new ir_function_cache_s();
}

IRFunction* ir_function_cache_get_function(ir_function_cache_t* cache, u64 address) {
    auto it = cache->functions.find(address);
    if (it != cache->functions.end()) {
        return it->second;
    }

    cache->storage.push_back(IRFunction(address));

    IRFunction* function = &cache->storage.back();

    cache->functions[address] = function;

    return function;
}

void ir_function_cache_destroy(ir_function_cache_t* cache) {
    delete cache;
}