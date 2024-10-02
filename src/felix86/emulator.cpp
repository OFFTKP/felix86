#include <stdlib.h>
#include "felix86/emulator.hpp"
#include "felix86/frontend/frontend.hpp"
#include "felix86/ir/passes.hpp"

void Emulator::Run() {
    IRFunction* function = cache.CreateOrGetFunctionAt(GetRip());
    frontend_compile_function(function);
    ir_ssa_pass(function);

    // if recompiler testing, exit...
}