#include "fex_test_loader.hpp"

// We have some tests that are not part of the FEX project, but use the same infrastructure
#define BUG_TEST(name)                                                                                                                               \
    CATCH_TEST_CASE(#name, "[felix86_bugs]") {                                                                                                       \
        FEXTestLoader::RunTest("ASM/felix86_bugs/" #name ".asm");                                                                                    \
    }

#define FEX_BUG_TEST(name)                                                                                                                           \
    CATCH_TEST_CASE(#name, "[felix86_bugs]") {                                                                                                       \
        FEXTestLoader::RunTest("ASM/FEX_bugs/" #name ".asm");                                                                                        \
    }

FEX_BUG_TEST(xor_flags)
FEX_BUG_TEST(Test_JP)
FEX_BUG_TEST(IMUL_garbagedata_negative)
FEX_BUG_TEST(LongSignedDivide)
FEX_BUG_TEST(add_sub_carry)
FEX_BUG_TEST(sbbNZCVBug)
FEX_BUG_TEST(ShiftPF)
BUG_TEST(dl_aux_init_stuck)
BUG_TEST(regalloc_overload)
BUG_TEST(ssa_phi_bug)
BUG_TEST(add8_lock)
BUG_TEST(cmpxchg64_lock)
BUG_TEST(strlen_sse2)
BUG_TEST(pmovmskb)
BUG_TEST(cfmerge)
BUG_TEST(test_high)
BUG_TEST(ror_clearof)
