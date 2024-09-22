#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "felix86/frontend/frontend.h"
#include "felix86/frontend/instruction.h"

#define INSTS (state->current_block->instructions)

typedef void (*ir_handle_fn_t)(frontend_state_t* state, x86_instruction_t* inst);

#define X(opcode, name, modrm, immsize)                                                            \
    void ir_handle_##name(frontend_state_t* state, x86_instruction_t* inst);
#include "felix86/frontend/primary.inc"
#include "felix86/frontend/secondary.inc"
#include "felix86/frontend/secondary_66.inc"
#include "felix86/frontend/secondary_f2.inc"
#include "felix86/frontend/secondary_f3.inc"
#include "felix86/frontend/tertiary_38.inc"
#include "felix86/frontend/tertiary_38_66.inc"
#include "felix86/frontend/tertiary_38_f2.inc"
#include "felix86/frontend/tertiary_3a.inc"
#include "felix86/frontend/tertiary_3a_66.inc"
#undef X

#ifdef __cplusplus
}
#endif