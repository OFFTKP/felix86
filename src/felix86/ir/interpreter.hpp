#pragma once

#include "felix86/common/state.hpp"
#include "felix86/felix86.hpp"
#include "felix86/ir/block.hpp"

void ir_interpret_function(felix86_recompiler_t* recompiler, ir_function_t* function,
                           x86_thread_state_t* state);
