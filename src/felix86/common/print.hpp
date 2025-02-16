#pragma once

#include <string>
#include "felix86/common/state.hpp"

std::string print_guest_register(x86_ref_e guest);
extern "C" __attribute__((visibility("default"))) void print_state(ThreadState* state);
extern "C" __attribute__((visibility("default"))) void print_gprs(ThreadState* state);