#include "FEX/fex_test_loader.hpp"

#define SECONDARY_MODRM_TEST(opcode)                                                                                                                 \
    CATCH_TEST_CASE(#opcode, "[SecondaryModRM]") {                                                                                                   \
        FEXTestLoader::RunTest("ASM/SecondaryModRM/" #opcode ".asm");                                                                                \
    }

SECONDARY_MODRM_TEST(Reg_2_0)