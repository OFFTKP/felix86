#include <catch2/catch_test_macros.hpp>

#undef WARN

#include "felix86/ir/block.hpp"
#include "felix86/ir/emitter.hpp"
#include "felix86/ir/passes/passes.hpp"

TEST_CASE("Peephole test", "[opt]") {
    IRBlock storage;
    IRBlock* block = &storage;

    SSAInstruction* inst = ir_emit_immediate(block, 1ull << 63);
    SSAInstruction* inst2 = ir_emit_immediate(block, 1ull << 63);
    SSAInstruction* address = ir_emit_immediate(block, 0xDEADBEEF);
    SSAInstruction* inst3 = ir_emit_read_word(block, address);
    SSAInstruction* inst4 = ir_emit_add(block, inst, inst3);
    SSAInstruction* inst5 = ir_emit_add(block, inst2, inst3);
    ir_emit_write_word(block, address, inst4);
    ir_emit_write_word(block, address, inst5);

    PassManager::localCSEPassBlock(block);
    PassManager::peepholePassBlock(block);
    PassManager::copyPropagationPassBlock(block);
    PassManager::localCSEPassBlock(block);

    fmt::print("{}", block->Print({}));
}