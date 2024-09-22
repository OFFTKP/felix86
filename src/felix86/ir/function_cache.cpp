#include "felix86/ir/function_cache.h"

#include <tsl/robin_map.h>

#include "felix86/common/log.h"

extern "C" struct ir_function_cache_s {
    tsl::robin_map<u64, ir_function_t*> functions;
};

ir_function_cache_t* ir_function_cache_create() {
    return new ir_function_cache_s();
}

ir_function_t* ir_function_cache_get_function(ir_function_cache_t* cache, u64 address) {
    auto it = cache->functions.find(address);
    if (it != cache->functions.end()) {
        return it->second;
    }

    ir_function_t* function = ir_function_create(address);

    cache->functions[address] = function;

    return function;
}

void ir_function_cache_destroy(ir_function_cache_t* cache) {
    for (auto& pair : cache->functions) {
        ir_function_destroy(pair.second);
    }

    delete cache;
}