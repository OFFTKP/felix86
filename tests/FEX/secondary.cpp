#include "FEX/fex_test_loader.hpp"

#define SECONDARY_TEST(opcode)                                                                                                                       \
    CATCH_TEST_CASE(#opcode, "[Secondary]") {                                                                                                        \
        FEXTestLoader::RunTest("ASM/Secondary/" #opcode ".asm");                                                                                     \
    }

#define SECONDARY_TEST32(opcode)                                                                                                                     \
    CATCH_TEST_CASE(#opcode, "[Secondary32]") {                                                                                                      \
        FEXTestLoader::RunTest("32Bit_ASM/Secondary/" #opcode ".asm");                                                                               \
    }

SECONDARY_TEST(07_XX_04)
SECONDARY_TEST(08_66_04_2)
SECONDARY_TEST(08_F2_04_2)
SECONDARY_TEST(08_F2_07)
SECONDARY_TEST(08_XX_04)
SECONDARY_TEST(08_XX_04_2)
SECONDARY_TEST(08_XX_04_3)
SECONDARY_TEST(08_XX_05)
SECONDARY_TEST(08_XX_05_2)
SECONDARY_TEST(08_XX_05_3)
SECONDARY_TEST(08_XX_06)
SECONDARY_TEST(08_XX_06_2)
SECONDARY_TEST(08_XX_06_3)
SECONDARY_TEST(08_XX_07)
SECONDARY_TEST(08_XX_07_2)
SECONDARY_TEST(08_XX_07_3)
SECONDARY_TEST(09_XX_01)
SECONDARY_TEST(09_XX_01_2)
SECONDARY_TEST(09_XX_01_3)
SECONDARY_TEST(09_XX_01_4)
SECONDARY_TEST(09_XX_01_5)
SECONDARY_TEST(09_XX_01_6)
SECONDARY_TEST(09_XX_01_7)
SECONDARY_TEST(09_XX_01_8)
SECONDARY_TEST(09_XX_01_9)
SECONDARY_TEST(09_XX_01_10)
SECONDARY_TEST(09_XX_01_11)
SECONDARY_TEST(09_XX_01_12)
SECONDARY_TEST(09_XX_01_13)
SECONDARY_TEST(09_XX_01_14)
SECONDARY_TEST(09_XX_01_15)
SECONDARY_TEST(09_XX_01_16)
SECONDARY_TEST(09_XX_01_17)
SECONDARY_TEST(09_XX_01_18)
SECONDARY_TEST(09_XX_01_19)
SECONDARY_TEST(12_66_02)
SECONDARY_TEST(12_66_04)
SECONDARY_TEST(12_66_06)
SECONDARY_TEST(13_66_02)
SECONDARY_TEST(13_66_04)
SECONDARY_TEST(13_66_06)
SECONDARY_TEST(14_66_02)
SECONDARY_TEST(14_66_06)
SECONDARY_TEST(14_66_07)
SECONDARY_TEST(14_XX_02)
SECONDARY_TEST(15_F3_02)
SECONDARY_TEST(15_XX_0)
SECONDARY_TEST(15_XX_5)
SECONDARY_TEST(15_XX_6)
SECONDARY_TEST(15_XX_7)
SECONDARY_TEST(shufps_optimization)
SECONDARY_TEST(shufps_optimization_2)

SECONDARY_TEST32(07_XX_04)
