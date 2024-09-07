#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "felix86/frontend/instruction.h"
#include "felix86/frontend/frontend.h"

#define INSTS (state->current_block->instructions)

typedef void (*ir_handle_fn_t)(frontend_state_t* state, x86_instruction_t* inst);

#define X(opcode, name, modrm, immsize) void ir_handle_##name(frontend_state_t* state, x86_instruction_t* inst);
#include "felix86/frontend/primary.inc"
#undef X

#define X(opcode, name, modrm, immsize) void ir_handle_##name(frontend_state_t* state, x86_instruction_t* inst);
#include "felix86/frontend/secondary.inc"
#undef X

#define X(opcode, name, modrm, immsize) void ir_handle_##name(frontend_state_t* state, x86_instruction_t* inst);
#include "felix86/frontend/secondary_66.inc"
#undef X

#define X(opcode, name, modrm, immsize) void ir_handle_##name(frontend_state_t* state, x86_instruction_t* inst);
#include "felix86/frontend/secondary_f2.inc"
#undef X

#define X(opcode, name, modrm, immsize) void ir_handle_##name(frontend_state_t* state, x86_instruction_t* inst);
#include "felix86/frontend/secondary_f3.inc"
#undef X

#ifdef __cplusplus
}
#endif