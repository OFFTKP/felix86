#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "felix86/common/utility.h"
#include "felix86/ir/block.h"

typedef struct ir_function_cache_s ir_function_cache_t;

ir_function_cache_t* ir_function_cache_create();
ir_function_t* ir_function_cache_get_function(ir_function_cache_t* cache, u64 address);
void ir_function_cache_destroy(ir_function_cache_t* cache);

#ifdef __cplusplus
}
#endif