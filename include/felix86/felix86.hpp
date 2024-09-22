#pragma once

#include "felix86/common/state.h"
#include "felix86/frontend/instruction.h"
#include "felix86/ir/block.h"

typedef struct {
	bool testing;
	bool optimize;
	bool print_blocks;
	bool use_interpreter;
	u64 base_address;
	bool verify;
	u64 brk_base_address;
} felix86_recompiler_config_t;

typedef enum : u8 {
	Error,
	FunctionLookup,
	DoneTesting,
} felix86_exit_reason_e;

typedef struct {
    struct ir_function_cache_s* function_cache;
    x86_thread_state_t state;
    bool testing;
    bool optimize;
    bool print_blocks;
    bool use_interpreter;
    u64 base_address;
    bool verify;
    u64 brk_base_address;
    u64 brk_current_address;
} felix86_recompiler_t;

felix86_recompiler_t* felix86_recompiler_create(felix86_recompiler_config_t* config);
void felix86_recompiler_destroy(felix86_recompiler_t* recompiler);
u64 felix86_get_guest(felix86_recompiler_t* recompiler, x86_ref_e ref);
void felix86_set_guest(felix86_recompiler_t* recompiler, x86_ref_e ref, u64 value);
xmm_reg_t felix86_get_guest_xmm(felix86_recompiler_t* recompiler, x86_ref_e ref);
void felix86_set_guest_xmm(felix86_recompiler_t* recompiler, x86_ref_e ref, xmm_reg_t value);
ir_function_t* felix86_get_function(felix86_recompiler_t* recompiler, u64 address);
felix86_exit_reason_e felix86_recompiler_run(felix86_recompiler_t* recompiler);
