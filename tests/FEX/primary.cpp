#include <catch2/catch_test_macros.hpp>
#include "FEX/fex_test_loader.hpp"

TEST_CASE("Primary", "[FEX]") {
    FEXTestLoader::RunTest("ASM/Primary/Primary_00.asm");
}