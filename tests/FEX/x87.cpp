#include "FEX/fex_test_loader.hpp"

#define X87(opcode)                                                                                                                                  \
    CATCH_TEST_CASE(#opcode "_F64", "[X87]") {                                                                                                       \
        FEXTestLoader::RunTest("ASM/X87_F64/" #opcode "_F64.asm");                                                                                   \
    }

X87(D8_00)
X87(D8_04)
X87(D8_05)
X87(D8_06)
X87(D8_07)
X87(D8_C0)
X87(D8_D9)
X87(D8_E0)
X87(D8_E8)
X87(D8_F0)
X87(D8_F8)
X87(D9_03)
X87(D9_C0)
X87(D9_C8)
X87(D9_E0)
X87(D9_E1)
X87(D9_E8)
X87(D9_E9)
X87(D9_EA)
X87(D9_EB)
X87(D9_EC)
X87(D9_ED)
X87(D9_EE)
X87(D9_F0)
X87(D9_F1)
X87(D9_F3)
X87(D9_F8)
X87(D9_F9)
X87(D9_FA)
X87(D9_FB)
X87(D9_FC)
X87(D9_FD)
X87(D9_FD_2)
X87(D9_FE)
X87(D9_FF)
X87(DA_01)
X87(DA_04)
X87(DA_05)
X87(DA_06)
X87(DA_07)
X87(DA_C0)
X87(DA_C8)
X87(DA_D0)
X87(DA_D8)
X87(DA_D9)
X87(DB_01)
X87(DB_02)
X87(DB_03)
X87(DB_C0)
X87(DB_C8)
X87(DB_D0)
X87(DB_D8)
X87(DB_E8)
X87(DB_F0)
X87(DC_00)
X87(DC_04)
X87(DC_05)
X87(DC_06)
X87(DC_07)
X87(DC_C0)
X87(DC_C8)
X87(DC_E0)
X87(DC_E8)
X87(DC_F0)
X87(DC_F8)
X87(DD_01)
// X87(DD_04)
// X87(DD_04_2)
X87(DE_00)
X87(DE_01)
X87(DE_04)
X87(DE_05)
X87(DE_06)
X87(DE_07)
X87(DE_C8)
X87(DE_E0)
X87(DE_F0)
X87(DE_F8)
X87(DF_00)
X87(DF_01)
X87(DF_03)
X87(DF_05)
X87(DF_07)
X87(DF_E8)
X87(DF_F0)

CATCH_TEST_CASE("FCOM_F64", "[X87]") {
    FEXTestLoader::RunTest("ASM/X87_F64/FCOM_F64.asm");
}

CATCH_TEST_CASE("FScale-Zero_F64", "[X87]") {
    FEXTestLoader::RunTest("ASM/X87_F64/FScale-Zero_F64.asm");
}

CATCH_TEST_CASE("FILD_NEG_F64", "[X87]") {
    FEXTestLoader::RunTest("ASM/X87_F64/FILD_NEG_F64.asm");
}

CATCH_TEST_CASE("FLDCW_F64", "[X87]") {
    FEXTestLoader::RunTest("ASM/X87_F64/FLDCW_F64.asm");
}

CATCH_TEST_CASE("FIST_F64", "[X87]") {
    FEXTestLoader::RunTest("ASM/X87_F64/FIST_F64.asm");
}

CATCH_TEST_CASE("Rounding_F64", "[X87]") {
    FEXTestLoader::RunTest("ASM/X87_F64/Rounding_F64.asm");
}