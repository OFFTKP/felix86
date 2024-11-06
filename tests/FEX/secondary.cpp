#include "FEX/fex_test_loader.hpp"

#define SECONDARY_TEST(opcode)                                                                                                                       \
    CATCH_TEST_CASE(#opcode, "Secondary") {                                                                                                          \
        FEXTestLoader::RunTest("ASM/Secondary/" #opcode ".asm");                                                                                     \
    }

SECONDARY_TEST(14_66_02)
SECONDARY_TEST(14_66_06)
SECONDARY_TEST(14_66_07)