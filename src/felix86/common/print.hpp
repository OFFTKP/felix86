#pragma once

#include "felix86/common/state.hpp"
#include "felix86/frontend/instruction.hpp"

void print_guest_register(x86_ref_e guest);
void print_state(x86_thread_state_t* state);
