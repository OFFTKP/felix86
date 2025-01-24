#include "FEX/fex_test_loader.hpp"

#define H0F3A_TEST(opcode)                                                                                                                           \
    CATCH_TEST_CASE(#opcode, "[Tertiary]") {                                                                                                         \
        FEXTestLoader::RunTest("ASM/H0F3A/" #opcode ".asm");                                                                                         \
    }

#define H0F38_TEST(opcode)                                                                                                                           \
    CATCH_TEST_CASE(#opcode, "[Tertiary]") {                                                                                                         \
        FEXTestLoader::RunTest("ASM/H0F38/" #opcode ".asm");                                                                                         \
    }

H0F3A_TEST(66_15)
H0F3A_TEST(0_66_0F)
H0F38_TEST(66_38)
H0F38_TEST(66_39)
H0F38_TEST(66_3A)
H0F38_TEST(66_3B)
H0F38_TEST(66_3C)
H0F38_TEST(66_3D)
H0F38_TEST(66_3E)
H0F38_TEST(66_3F)
