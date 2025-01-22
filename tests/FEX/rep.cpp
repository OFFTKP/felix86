#include "FEX/fex_test_loader.hpp"

#define REP_TEST(opcode)                                                                                                                             \
    CATCH_TEST_CASE("F3_" #opcode, "[FEX][REP]") {                                                                                                   \
        FEXTestLoader::RunTest("ASM/REP/F3_" #opcode ".asm");                                                                                        \
    }

#define REPNE_TEST(opcode)                                                                                                                           \
    CATCH_TEST_CASE("F2_" #opcode, "[FEX][REP]") {                                                                                                   \
        FEXTestLoader::RunTest("ASM/REPNE/F2_" #opcode ".asm");                                                                                      \
    }

REP_TEST(10)
REP_TEST(11)
REP_TEST(2A)
REP_TEST(2A_1)
REP_TEST(2A_2)
REP_TEST(2C)
REP_TEST(2D)
REP_TEST(51)
REP_TEST(52)
REP_TEST(53)
REP_TEST(58)
REP_TEST(59)
REP_TEST(5C)
REP_TEST(5D)
REP_TEST(5E)
REP_TEST(5F)

REPNE_TEST(10)
REPNE_TEST(11)
REPNE_TEST(2A)
REPNE_TEST(2A_1)
REPNE_TEST(2A_2)
REPNE_TEST(2C)
REPNE_TEST(2D)
REPNE_TEST(2D_1)
REPNE_TEST(51)
REPNE_TEST(58)
REPNE_TEST(59)
REPNE_TEST(5C)
REPNE_TEST(5D)
REPNE_TEST(5E)
REPNE_TEST(5F)
REPNE_TEST(70)
