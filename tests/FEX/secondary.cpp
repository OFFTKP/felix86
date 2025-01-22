#include "FEX/fex_test_loader.hpp"

#define SECONDARY_TEST(opcode)                                                                                                                       \
    CATCH_TEST_CASE(#opcode, "[Secondary]") {                                                                                                        \
        FEXTestLoader::RunTest("ASM/Secondary/" #opcode ".asm");                                                                                     \
    }

SECONDARY_TEST(8_66_04_2)
SECONDARY_TEST(8_F2_04_2)
SECONDARY_TEST(8_F2_07)
SECONDARY_TEST(8_F3_04_2)
SECONDARY_TEST(8_XX_04_2)
SECONDARY_TEST(8_XX_05_2)
SECONDARY_TEST(8_XX_06_2)
SECONDARY_TEST(8_XX_07_2)
SECONDARY_TEST(14_66_02)
SECONDARY_TEST(14_66_06)
SECONDARY_TEST(14_66_07)
SECONDARY_TEST(15_F3_02)
SECONDARY_TEST(15_XX_5)
SECONDARY_TEST(15_XX_6)
SECONDARY_TEST(15_XX_7)
