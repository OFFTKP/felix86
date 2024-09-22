#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "felix86/common/utility.h"
#include "felix86/frontend/instruction.h"

typedef enum : u8 {
    IR_VALUE_TYPE_NULL,
    IR_VALUE_TYPE_IMMEDIATE,
    IR_VALUE_TYPE_VARIABLE,
    IR_VALUE_TYPE_REGISTER,
} ir_value_type_t;

typedef u32 ir_temp_t;

typedef struct {
    union {
        u64 imm;
        ir_temp_t var;
        x86_ref_e reg;
    };

    ir_value_type_t type;
} ir_value_t;

#ifdef __cplusplus
}
#endif