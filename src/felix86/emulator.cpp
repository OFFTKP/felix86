#include <stdlib.h>
#include "felix86/emulator.hpp"
#include "felix86/frontend/frontend.hpp"

void Emulator::Run() {
    IRFunction* function = cache.CreateOrGetFunctionAt(GetRip());
    frontend_compile_function(function);

    // if recompiler testing, exit...
}