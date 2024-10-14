#include <catch2/catch_test_macros.hpp>
#include "FEX/fex_test_loader.hpp"

TEST_CASE("Primary", "[FEX]") {
    FEXTestLoader::RunTest("ASM/Primary/Primary_00.asm");
    FEXTestLoader::RunTest("ASM/Primary/Primary_20.asm");
    FEXTestLoader::RunTest("ASM/Primary/Primary_28.asm");
    FEXTestLoader::RunTest("ASM/Primary/Primary_30.asm");
    FEXTestLoader::RunTest("ASM/Primary/Primary_38.asm");
}