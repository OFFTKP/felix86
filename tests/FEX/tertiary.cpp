#include "FEX/fex_test_loader.hpp"

#define H0F3A_TEST(opcode)                                                                                                                           \
    CATCH_TEST_CASE(#opcode, "[Tertiary]") {                                                                                                         \
        FEXTestLoader::RunTest("ASM/H0F3A/" #opcode ".asm");                                                                                         \
    }

H0F3A_TEST(0_66_0F)