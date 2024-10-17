#include "fex_test_loader.hpp"

// We have some tests that are not part of the FEX project, but use the same infrastructure
#define BUG_TEST(name)                                                                                                                               \
    CATCH_TEST_CASE("felix86_bugs_" #name, "[felix86_bugs]") {                                                                                       \
        FEXTestLoader::RunTest("ASM/felix86_bugs/" #name ".asm");                                                                                    \
    }

BUG_TEST(dl_aux_init_stuck)
BUG_TEST(regalloc_overload)
BUG_TEST(ssa_phi_bug)