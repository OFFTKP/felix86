#include "fex_test_loader.hpp"

#define OP_SIZE(opcode)                                                                                                                              \
    CATCH_TEST_CASE("OpSize_" #opcode, "[FEX][OpSize]") {                                                                                            \
        FEXTestLoader::RunTest("ASM/OpSize/" #opcode ".asm");                                                                                        \
    }

OP_SIZE(66_28)
OP_SIZE(66_60)
OP_SIZE(66_74)
OP_SIZE(66_75)
OP_SIZE(66_76)
OP_SIZE(66_DB)