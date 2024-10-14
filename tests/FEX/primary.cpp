#include <catch2/catch_test_macros.hpp>
#include "FEX/fex_test_loader.hpp"

#define PRIMARY_TEST(opcode)                                                                                                                         \
    TEST_CASE("Primary_" #opcode, "[FEX][Primary]") {                                                                                                \
        FEXTestLoader::RunTest("ASM/Primary/Primary_" #opcode ".asm");                                                                               \
    }

PRIMARY_TEST(00)
PRIMARY_TEST(08)
PRIMARY_TEST(20)
PRIMARY_TEST(28)
PRIMARY_TEST(30)
PRIMARY_TEST(38)
PRIMARY_TEST(50)
PRIMARY_TEST(50_2)
PRIMARY_TEST(63)
PRIMARY_TEST(6A)
PRIMARY_TEST(6A_2)
PRIMARY_TEST(84)
PRIMARY_TEST(84_2)
PRIMARY_TEST(86)
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
PRIMARY_TEST(C9)