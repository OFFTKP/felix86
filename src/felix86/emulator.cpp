#include <fmt/base.h>
#include <stdlib.h>
#include "felix86/emulator.hpp"
#include "felix86/frontend/frontend.hpp"
#include "felix86/ir/passes.hpp"

void Emulator::Run() {
    IRFunction* function = cache.CreateOrGetFunctionAt(GetRip());
    frontend_compile_function(function);
    ir_ssa_pass(function);
    ir_naming_pass(function);
    ir_copy_propagation_pass(function);
    ir_entry_exit_unused_pass(function);

    fmt::print("{}", function->Print());

    if (!function->Validate()) {
        ERROR("Function did not validate");
    }

    // if recompiler testing, exit...
}