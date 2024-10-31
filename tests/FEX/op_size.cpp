#include "fex_test_loader.hpp"

#define OP_SIZE(opcode)                                                                                                                              \
    CATCH_TEST_CASE("OpSize_" #opcode, "[FEX][OpSize]") {                                                                                            \
        FEXTestLoader::RunTest("ASM/OpSize/66_" #opcode ".asm");                                                                                     \
    }

CATCH_TEST_CASE("15Byte", "[FEX][OpSize]") {
    FEXTestLoader::RunTest("ASM/OpSize/15_BYTE.asm");
}

OP_SIZE(28)
OP_SIZE(51)
OP_SIZE(54)
OP_SIZE(55)
OP_SIZE(56)
OP_SIZE(57)
OP_SIZE(58)
OP_SIZE(59)
OP_SIZE(5C)
OP_SIZE(5D)
OP_SIZE(5E)
OP_SIZE(5F)
OP_SIZE(60)
OP_SIZE(61)
OP_SIZE(62)
OP_SIZE(6C)
OP_SIZE(6E)
OP_SIZE(74)
OP_SIZE(75)
OP_SIZE(76)
OP_SIZE(D6)
OP_SIZE(DB)