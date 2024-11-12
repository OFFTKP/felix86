#include "fex_test_loader.hpp"

// We have some tests that are not part of the FEX project, but use the same infrastructure
#define BUG_TEST(name)                                                                                                                               \
    CATCH_TEST_CASE(#name, "[felix86_bugs]") {                                                                                                       \
        FEXTestLoader::RunTest("ASM/felix86_bugs/" #name ".asm");                                                                                    \
    }

BUG_TEST(dl_aux_init_stuck)
BUG_TEST(regalloc_overload)
BUG_TEST(ssa_phi_bug)
BUG_TEST(ror_clearof)
BUG_TEST(add8_lock)
BUG_TEST(cmpxchg64_lock)
BUG_TEST(strlen_sse2)
BUG_TEST(pmovmskb)
BUG_TEST(cfmerge)
