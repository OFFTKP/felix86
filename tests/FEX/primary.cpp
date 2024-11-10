#include "FEX/fex_test_loader.hpp"

#define PRIMARY_TEST(opcode)                                                                                                                         \
    CATCH_TEST_CASE(#opcode, "Primary") {                                                                                                            \
        FEXTestLoader::RunTest("ASM/Primary/Primary_" #opcode ".asm");                                                                               \
    }

#define PRIMARY_TEST_NO_PREFIX(opcode)                                                                                                               \
    CATCH_TEST_CASE(#opcode, "Primary") {                                                                                                            \
        FEXTestLoader::RunTest("ASM/Primary/" #opcode ".asm");                                                                                       \
    }

#define PRIMARY_TEST_BASE(name)                                                                                                                      \
    CATCH_TEST_CASE(#name, "[FEX]") {                                                                                                                \
        FEXTestLoader::RunTest("ASM/" #name ".asm");                                                                                                 \
    }

#define PRIMARY_TEST_KNOWN_FAILURE(opcode)

PRIMARY_TEST(00)
PRIMARY_TEST(08)
PRIMARY_TEST(20)
PRIMARY_TEST(28)
PRIMARY_TEST(30)
PRIMARY_TEST(38)
PRIMARY_TEST(3A)
PRIMARY_TEST(3B)
PRIMARY_TEST(3C)
PRIMARY_TEST(3D)
PRIMARY_TEST(50)
PRIMARY_TEST(50_2)
PRIMARY_TEST(63)
PRIMARY_TEST(68)
PRIMARY_TEST(69)
PRIMARY_TEST(6A)
PRIMARY_TEST(6A_2)
PRIMARY_TEST(6B)
PRIMARY_TEST(84)
PRIMARY_TEST(84_2)
PRIMARY_TEST(85)
PRIMARY_TEST(86)
PRIMARY_TEST(87)
PRIMARY_TEST(87_2)
PRIMARY_TEST(87_3)
PRIMARY_TEST(8D)
PRIMARY_TEST(8D_2)
PRIMARY_TEST(90)
PRIMARY_TEST(90_2)
PRIMARY_TEST(90_3)
PRIMARY_TEST(90_4)
PRIMARY_TEST(98)
PRIMARY_TEST(98_2)
PRIMARY_TEST(99)
PRIMARY_TEST(99_2)
PRIMARY_TEST(9E)
PRIMARY_TEST(A0)
PRIMARY_TEST(A2)
PRIMARY_TEST(A4)
PRIMARY_TEST(A4_REP)
PRIMARY_TEST(A4_REP_many)
PRIMARY_TEST(A4_REPNE)
PRIMARY_TEST(A4_REPNE_many)
PRIMARY_TEST(A5)
PRIMARY_TEST(A5_dword)
PRIMARY_TEST(A5_qword)
PRIMARY_TEST(A5_REP)
PRIMARY_TEST(A5_dword_REP)
PRIMARY_TEST(A5_qword_REP)
PRIMARY_TEST(A5_REPNE)
PRIMARY_TEST(A5_dword_REPNE)
PRIMARY_TEST(A5_qword_REPNE)
PRIMARY_TEST(A9)
PRIMARY_TEST(AB_word)
PRIMARY_TEST(AB_dword)
PRIMARY_TEST(AB_qword)
PRIMARY_TEST(AB_word_REP)
PRIMARY_TEST(AB_dword_REP)
PRIMARY_TEST(AB_qword_REP)
PRIMARY_TEST(AB_word_REPNE)
PRIMARY_TEST(AB_dword_REPNE)
PRIMARY_TEST(AB_qword_REPNE)
PRIMARY_TEST(B0)
PRIMARY_TEST(B8)
PRIMARY_TEST(B8_2)
PRIMARY_TEST(B8_3)
PRIMARY_TEST(C2)
PRIMARY_TEST(C3)
PRIMARY_TEST(C9)
PRIMARY_TEST(E8)
PRIMARY_TEST(E9)
PRIMARY_TEST(EB)

PRIMARY_TEST_BASE(lea)

PRIMARY_TEST_NO_PREFIX(SHL)
PRIMARY_TEST_NO_PREFIX(SHR)
PRIMARY_TEST_NO_PREFIX(ROL_Flags)
PRIMARY_TEST_NO_PREFIX(ROL_OF)
PRIMARY_TEST_NO_PREFIX(ROR_Flags)
PRIMARY_TEST_NO_PREFIX(ROR_OF)

// Need unaligned atomics
PRIMARY_TEST_KNOWN_FAILURE(01_Atomic16)
PRIMARY_TEST_KNOWN_FAILURE(01_Atomic32)
PRIMARY_TEST_KNOWN_FAILURE(01_Atomic64)
PRIMARY_TEST_KNOWN_FAILURE(09_Atomic16)
PRIMARY_TEST_KNOWN_FAILURE(09_Atomic32)
PRIMARY_TEST_KNOWN_FAILURE(09_Atomic64)
PRIMARY_TEST_KNOWN_FAILURE(23_Atomic16)
PRIMARY_TEST_KNOWN_FAILURE(23_Atomic32)
PRIMARY_TEST_KNOWN_FAILURE(23_Atomic64)
PRIMARY_TEST_KNOWN_FAILURE(29_Atomic16)
PRIMARY_TEST_KNOWN_FAILURE(29_Atomic32)
PRIMARY_TEST_KNOWN_FAILURE(29_Atomic64)
PRIMARY_TEST_KNOWN_FAILURE(31_Atomic16)
PRIMARY_TEST_KNOWN_FAILURE(31_Atomic32)
PRIMARY_TEST_KNOWN_FAILURE(31_Atomic64)
PRIMARY_TEST_KNOWN_FAILURE(87_Atomic16)
PRIMARY_TEST_KNOWN_FAILURE(87_Atomic32)
PRIMARY_TEST_KNOWN_FAILURE(87_Atomic64)

// Needs me to care enough to implement direction flag
PRIMARY_TEST_KNOWN_FAILURE(A4_REP_Down)
PRIMARY_TEST_KNOWN_FAILURE(A4_REPNE_Down)
PRIMARY_TEST_KNOWN_FAILURE(A5_REP_Down)
PRIMARY_TEST_KNOWN_FAILURE(A5_REPNE_Down)
PRIMARY_TEST_KNOWN_FAILURE(A5_dword_REP_Down)
PRIMARY_TEST_KNOWN_FAILURE(A5_dword_REPNE_Down)
PRIMARY_TEST_KNOWN_FAILURE(A5_qword_REP_Down)
PRIMARY_TEST_KNOWN_FAILURE(A5_qword_REPNE_Down)

// Needs me to care enough to implement ENTER
PRIMARY_TEST_KNOWN_FAILURE(C8)
PRIMARY_TEST_KNOWN_FAILURE(C8_2)