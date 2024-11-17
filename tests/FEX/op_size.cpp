#include "fex_test_loader.hpp"

#define OP_SIZE(opcode)                                                                                                                              \
    CATCH_TEST_CASE("66_" #opcode, "[OpSize]") {                                                                                                     \
        FEXTestLoader::RunTest("ASM/OpSize/66_" #opcode ".asm");                                                                                     \
    }

CATCH_TEST_CASE("15Byte", "[OpSize]") {
    FEXTestLoader::RunTest("ASM/OpSize/15_BYTE.asm");
}

OP_SIZE(12)
OP_SIZE(16)
OP_SIZE(17)
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
OP_SIZE(68)
OP_SIZE(69)
OP_SIZE(6A)
OP_SIZE(6C)
OP_SIZE(6D)
OP_SIZE(6E)
OP_SIZE(70)
OP_SIZE(74)
OP_SIZE(75)
OP_SIZE(76)
OP_SIZE(C6)
OP_SIZE(D4)
OP_SIZE(D6)
OP_SIZE(D7)
OP_SIZE(DA)
OP_SIZE(DB)
OP_SIZE(DE)
OP_SIZE(DF)
OP_SIZE(E7)
OP_SIZE(EB)
OP_SIZE(EF)
OP_SIZE(F8)
OP_SIZE(F9)
OP_SIZE(FA)
OP_SIZE(FB)
OP_SIZE(FC)
OP_SIZE(FD)
OP_SIZE(FE)