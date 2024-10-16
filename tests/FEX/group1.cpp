#include "fex_test_loader.hpp"

#define GROUP1_TEST(opcode)                                                                                                                          \
    CATCH_TEST_CASE("1_" #opcode, "[FEX][Group 1]") {                                                                                                \
        FEXTestLoader::RunTest("ASM/PrimaryGroup/1_" #opcode ".asm");                                                                                \
    }

GROUP1_TEST(80_00)
GROUP1_TEST(80_01)
GROUP1_TEST(80_02)
GROUP1_TEST(80_03)
GROUP1_TEST(80_04)
GROUP1_TEST(80_05)
GROUP1_TEST(80_06)
GROUP1_TEST(80_07)

GROUP1_TEST(81_00)
GROUP1_TEST(81_01)
GROUP1_TEST(81_02)
GROUP1_TEST(81_03)
GROUP1_TEST(81_04)
GROUP1_TEST(81_05)
GROUP1_TEST(81_06)
GROUP1_TEST(81_07)

GROUP1_TEST(83_00)
GROUP1_TEST(83_01)
GROUP1_TEST(83_02)
GROUP1_TEST(83_03)
GROUP1_TEST(83_04)
GROUP1_TEST(83_05)
GROUP1_TEST(83_06)
GROUP1_TEST(83_07)