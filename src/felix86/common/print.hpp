#pragma once

#include "felix86/common/state.h"
#include "felix86/frontend/instruction.h"

void print_guest_register(x86_ref_e guest);
void print_state(x86_thread_state_t* state);
