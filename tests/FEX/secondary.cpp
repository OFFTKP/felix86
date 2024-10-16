#include "FEX/fex_test_loader.hpp"

#define SECONDARY_TEST(opcode)                                                                                                                       \
    CATCH_TEST_CASE("0F_" #opcode, "[FEX][Secondary]") {                                                                                             \
        FEXTestLoader::RunTest("ASM/TwoByte/" #opcode ".asm");                                                                                       \
    }

SECONDARY_TEST(80)
SECONDARY_TEST(81)
SECONDARY_TEST(82)
SECONDARY_TEST(83)
SECONDARY_TEST(84)
SECONDARY_TEST(85)
SECONDARY_TEST(86)
SECONDARY_TEST(87)
SECONDARY_TEST(88)
SECONDARY_TEST(89)
SECONDARY_TEST(8A)
SECONDARY_TEST(8B)
SECONDARY_TEST(8C)
SECONDARY_TEST(8D)
SECONDARY_TEST(8E)
SECONDARY_TEST(8F)