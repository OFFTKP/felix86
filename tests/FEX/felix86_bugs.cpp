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

FEX_BUG_TEST(Push)
FEX_BUG_TEST(xor_flags)
FEX_BUG_TEST(SHRD_OF)
FEX_BUG_TEST(Test_JP)
FEX_BUG_TEST(Test_PF_Zero_Shift)
FEX_BUG_TEST(IMUL_garbagedata_negative)
FEX_BUG_TEST(LongSignedDivide)
FEX_BUG_TEST(add_sub_carry)
FEX_BUG_TEST(add_sub_carry_2)
FEX_BUG_TEST(sbbNZCVBug)
FEX_BUG_TEST(ShiftPF)
FEX_BUG_TEST(Test_CmpSelect_Merge_branch)
FEX_BUG_TEST(Test_CmpSelect_Merge)
FEX_BUG_TEST(Test_CmpSelect_Merge_Float)
FEX_BUG_TEST(Test_CmpSelect_Merge_Float_branch)
FEX_BUG_TEST(smallvectorload_regreg)
FEX_BUG_TEST(SBCSmall)
FEX_BUG_TEST(ShiftZeroFlagsUpdate)
FEX_BUG_TEST(pcmpestri_garbage_rcx)
FEX_BUG_TEST(SegmentAddressOverride)
// FEX_BUG_TEST(rotate_zero_extend_with_zero)
// FEX_BUG_TEST(BLSR_flags)
BUG_TEST(dl_aux_init_stuck)
BUG_TEST(bsr_reg)
BUG_TEST(regalloc_overload)
BUG_TEST(ssa_phi_bug)
BUG_TEST(add8_lock)
BUG_TEST(cmpxchg64_lock)
BUG_TEST(strlen_sse2)
BUG_TEST(pmovmskb)
BUG_TEST(cfmerge)
BUG_TEST(test_high)
BUG_TEST(bt_jc)
BUG_TEST(movd)
BUG_TEST(bts)
BUG_TEST(llvmpipe_rast_triangle)
BUG_TEST(packsswb)
BUG_TEST(adc)
BUG_TEST(why)
BUG_TEST(fpu_stack)
// BUG_TEST(crc_simple)
// BUG_TEST(ror_clearof)
