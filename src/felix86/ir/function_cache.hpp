#pragma once

#include "felix86/common/utility.hpp"
#include "felix86/ir/block.hpp"

typedef struct ir_function_cache_s ir_function_cache_t;

ir_function_cache_t* ir_function_cache_create();
IRFunction* ir_function_cache_get_function(ir_function_cache_t* cache, u64 address);
void ir_function_cache_destroy(ir_function_cache_t* cache);
