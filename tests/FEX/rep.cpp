#include "FEX/fex_test_loader.hpp"

#define REP_TEST(opcode)                                                                                                                             \
    CATCH_TEST_CASE("REP_" #opcode, "[FEX][REP]") {                                                                                                  \
        FEXTestLoader::RunTest("ASM/REP/F3_" #opcode ".asm");                                                                                        \
    }

#define REPNE_TEST(opcode)                                                                                                                           \
    CATCH_TEST_CASE("REPNE_" #opcode, "[FEX][REP]") {                                                                                                \
        FEXTestLoader::RunTest("ASM/REPNE/F2_" #opcode ".asm");                                                                                      \
    }

REP_TEST(58)

REPNE_TEST(58)