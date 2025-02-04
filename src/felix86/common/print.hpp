#pragma once

#include <string>
#include "felix86/common/state.hpp"

std::string print_guest_register(x86_ref_e guest);
void print_state(ThreadState* state);
void print_gprs(ThreadState* state);