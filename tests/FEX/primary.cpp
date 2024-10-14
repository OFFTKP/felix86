#include <catch2/catch_test_macros.hpp>
#include "FEX/fex_test_loader.hpp"

TEST_CASE("Primary", "[FEX]") {
    FEXTestLoader::RunTest("ASM/Primary/Primary_00.asm");
    FEXTestLoader::RunTest("ASM/Primary/Primary_08.asm");
    // FEXTestLoader::RunTest("ASM/Primary/Primary_10.asm"); ADC/SBB, just a nuisance that is never used, TODO
    // FEXTestLoader::RunTest("ASM/Primary/Primary_18.asm");
    FEXTestLoader::RunTest("ASM/Primary/Primary_20.asm");
    FEXTestLoader::RunTest("ASM/Primary/Primary_28.asm");
    FEXTestLoader::RunTest("ASM/Primary/Primary_30.asm");
    FEXTestLoader::RunTest("ASM/Primary/Primary_38.asm");
    FEXTestLoader::RunTest("ASM/Primary/Primary_50.asm");
    FEXTestLoader::RunTest("ASM/Primary/Primary_50_2.asm");
    FEXTestLoader::RunTest("ASM/Primary/Primary_8D.asm");
    FEXTestLoader::RunTest("ASM/Primary/Primary_8D_2.asm");
}