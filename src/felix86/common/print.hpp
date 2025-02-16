#pragma once

#include <string>
#include "felix86/common/state.hpp"

std::string print_guest_register(x86_ref_e guest);
__attribute__((visibility("default"))) extern "C" void print_state(ThreadState* state);
__attribute__((visibility("default"))) extern "C" void print_gprs(ThreadState* state);