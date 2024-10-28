#include "fex_test_loader.hpp"

#define OP_SIZE(opcode)                                                                                                                              \
    CATCH_TEST_CASE("OpSize_" #opcode, "[FEX][OpSize]") {                                                                                            \
        FEXTestLoader::RunTest("ASM/OpSize/" #opcode ".asm");                                                                                        \
    }

OP_SIZE(66_60)
OP_SIZE(66_DB)