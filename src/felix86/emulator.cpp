#include <fmt/base.h>
#include <fmt/format.h>
#include <stdlib.h>
#include "felix86/backend/disassembler.hpp"
#include "felix86/emulator.hpp"
#include "felix86/frontend/frontend.hpp"
#include "felix86/ir/passes/passes.hpp"

void Emulator::Run() {
    IRFunction* function = function_cache.CreateOrGetFunctionAt(GetRip());
    frontend_compile_function(function);
    ir_ssa_pass(function);
    ir_copy_propagation_pass(function);
    ir_extraneous_writeback_pass(function);
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
    std::string disassembly = Disassembler::Disassemble(emit, 0x100);
    fmt::print("{}\n", disassembly);
    // if recompiler testing, exit...
}