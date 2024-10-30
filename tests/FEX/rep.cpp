#include "FEX/fex_test_loader.hpp"

#define REP_TEST(opcode)                                                                                                                             \
    CATCH_TEST_CASE("REP_" #opcode, "[FEX][REP]") {                                                                                                  \
        FEXTestLoader::RunTest("ASM/REP/F3_" #opcode ".asm");                                                                                        \
    }

REP_TEST(58)