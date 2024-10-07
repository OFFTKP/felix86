#include <fmt/base.h>
#include <fmt/format.h>
#include <stdlib.h>
#include "felix86/backend/disassembler.hpp"
#include "felix86/emulator.hpp"
#include "felix86/frontend/frontend.hpp"
#include "felix86/ir/passes/passes.hpp"

void Emulator::Run() {
    u64 rip = GetRip();
    IRFunction* function = function_cache.CreateOrGetFunctionAt(rip);
    frontend_compile_function(function);
    ir_ssa_pass(function);
    ir_replace_setguest_pass(function);
    ir_copy_propagation_pass(function);
    ir_extraneous_writeback_pass(function);
    ir_dead_code_elimination_pass(function);
    ir_local_cse_pass(function);
    ir_copy_propagation_pass(function);
    ir_dead_code_elimination_pass(function);
    ir_naming_pass(function);
    // ir_graph_coloring_pass(function);
    ir_spill_everything_pass(function);

    auto test = [](const IRInstruction* inst) { return fmt::format(" 0x{:x}", (u64)inst); };
    fmt::print("{}", function->Print(test));

    if (!function->Validate()) {
        ERROR("Function did not validate");
    }

    void* emit = backend.EmitFunction(function);
    std::string disassembly = Disassembler::Disassemble(emit, 0x200);
    fmt::print("{}\n", disassembly);
    // if recompiler testing, exit...
}