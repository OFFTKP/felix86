#pragma once

#include "felix86/common/x86.hpp"
#include "felix86/frontend/instruction.hpp"

void print_guest_register(x86_ref_e guest);
void print_state(ThreadState* state);
