#include "FEX/fex_test_loader.hpp"

#define SECONDARY_TEST(opcode)                                                                                                                       \
    CATCH_TEST_CASE("Secondary_" #opcode, "[FEX][Secondary]") {                                                                                      \
        FEXTestLoader::RunTest("ASM/Secondary/" #opcode ".asm");                                                                                     \
    }
