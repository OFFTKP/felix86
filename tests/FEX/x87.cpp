#include "FEX/fex_test_loader.hpp"

#define X87(opcode)                                                                                                                                  \
    CATCH_TEST_CASE(#opcode "_F64", "[X87]") {                                                                                                       \
        FEXTestLoader::RunTest("ASM/X87_F64/" #opcode "_F64.asm");                                                                                   \
    }

X87(D9_C8)
X87(D9_FE)
X87(D9_FF)
