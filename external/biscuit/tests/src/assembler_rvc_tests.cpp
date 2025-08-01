#include <catch/catch.hpp>

#include <biscuit/assembler.hpp>

#include "assembler_test_utils.hpp"

using namespace biscuit;

TEST_CASE("C.ADD", "[rvc]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.C_ADD(x31, x31);
    REQUIRE(value == 0x9FFE);

    as.RewindBuffer();

    as.C_ADD(x15, x8);
    REQUIRE(value == 0x97A2);
}

TEST_CASE("C.ADDI", "[rvc]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.C_ADDI(x15, -1);
    REQUIRE(value == 0x17FD);

    as.RewindBuffer();

    as.C_ADDI(x15, -32);
    REQUIRE(value == 0x1781);

    as.RewindBuffer();

    as.C_ADDI(x15, 31);
    REQUIRE(value == 0x07FD);
}

TEST_CASE("C.ADDIW", "[rvc]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.C_ADDIW(x15, -1);
    REQUIRE(value == 0x37FD);

    as.RewindBuffer();

    as.C_ADDIW(x15, -32);
    REQUIRE(value == 0x3781);

    as.RewindBuffer();

    as.C_ADDIW(x15, 31);
    REQUIRE(value == 0x27FD);
}

TEST_CASE("C.ADDI4SPN", "[rvc]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.C_ADDI4SPN(x15, 252);
    REQUIRE(value == 0x19FC);

    as.RewindBuffer();

    as.C_ADDI4SPN(x8, 1020);
    REQUIRE(value == 0x1FE0);

    as.RewindBuffer();

    as.C_ADDI4SPN(x15, 1020);
    REQUIRE(value == 0x1FFC);
}

TEST_CASE("C.ADDI16SP", "[rvc]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.C_ADDI16SP(16);
    REQUIRE(value == 0x6141);

    as.RewindBuffer();

    as.C_ADDI16SP(64);
    REQUIRE(value == 0x6121);
}

TEST_CASE("C.ADDW", "[rvc]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.C_ADDW(x15, x15);
    REQUIRE(value == 0x9FBD);

    as.RewindBuffer();

    as.C_ADDW(x15, x8);
    REQUIRE(value == 0x9FA1);
}

TEST_CASE("C.AND", "[rvc]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.C_AND(x15, x15);
    REQUIRE(value == 0x8FFD);

    as.RewindBuffer();

    as.C_AND(x15, x8);
    REQUIRE(value == 0x8FE1);
}

TEST_CASE("C.ANDI", "[rvc]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.C_ANDI(x15, 16);
    REQUIRE(value == 0x8BC1);

    as.RewindBuffer();

    as.C_ANDI(x15, 31);
    REQUIRE(value == 0x8BFD);
}

TEST_CASE("C.EBREAK", "[rvc]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.C_EBREAK();
    REQUIRE(value == 0x9002);
}

TEST_CASE("C.FLD", "[rvc]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.C_FLD(f15, 8, x15);
    REQUIRE(value == 0x279C);

    as.RewindBuffer();

    as.C_FLD(f15, 24, x15);
    REQUIRE(value == 0x2F9C);
}

TEST_CASE("C.FLDSP", "[rvc]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.C_FLDSP(f15, 8);
    REQUIRE(value == 0x27A2);

    as.RewindBuffer();

    as.C_FLDSP(f15, 24);
    REQUIRE(value == 0x27E2);
}

TEST_CASE("C.FLW", "[rvc]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.C_FLW(f15, 16, x15);
    REQUIRE(value == 0x6B9C);

    as.RewindBuffer();

    as.C_FLW(f15, 24, x15);
    REQUIRE(value == 0x6F9C);
}

TEST_CASE("C.FLWSP", "[rvc]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.C_FLWSP(f15, 16);
    REQUIRE(value == 0x67C2);

    as.RewindBuffer();

    as.C_FLWSP(f15, 24);
    REQUIRE(value == 0x67E2);
}

TEST_CASE("C.FSD", "[rvc]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.C_FSD(f15, 8, x15);
    REQUIRE(value == 0xA79C);

    as.RewindBuffer();

    as.C_FSD(f15, 24, x15);
    REQUIRE(value == 0xAF9C);
}

TEST_CASE("C.FSDSP", "[rvc]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.C_FSDSP(f15, 8);
    REQUIRE(value == 0xA43E);

    as.RewindBuffer();

    as.C_FSDSP(f15, 24);
    REQUIRE(value == 0xAC3E);
}

TEST_CASE("C.FSW", "[rvc]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.C_FSW(f15, 16, x15);
    REQUIRE(value == 0xEB9C);

    as.RewindBuffer();

    as.C_FSW(f15, 24, x15);
    REQUIRE(value == 0xEF9C);
}

TEST_CASE("C.FSWSP", "[rvc]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.C_FSWSP(f15, 16);
    REQUIRE(value == 0xE83E);

    as.RewindBuffer();

    as.C_FSWSP(f15, 24);
    REQUIRE(value == 0xEC3E);
}

TEST_CASE("C.JAL", "[rvc]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.C_JAL(1028);
    REQUIRE(value == 0x2111);

    as.RewindBuffer();

    as.C_JAL(-1028);
    REQUIRE(value == 0x3EF5);
}

TEST_CASE("C.J", "[rvc]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.C_J(1028);
    REQUIRE(value == 0xA111);

    as.RewindBuffer();

    as.C_J(-1028);
    REQUIRE(value == 0xBEF5);
}

TEST_CASE("C.JALR", "[rvc]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.C_JALR(x31);
    REQUIRE(value == 0x9F82);

    as.RewindBuffer();

    as.C_JALR(x15);
    REQUIRE(value == 0x9782);
}

TEST_CASE("C.JR", "[rvc]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.C_JR(x31);
    REQUIRE(value == 0x8F82);

    as.RewindBuffer();

    as.C_JR(x15);
    REQUIRE(value == 0x8782);
}

TEST_CASE("C.LD", "[rvc]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.C_LD(x15, 8, x15);
    REQUIRE(value == 0x679C);

    as.RewindBuffer();

    as.C_LD(x15, 24, x15);
    REQUIRE(value == 0x6F9C);
}

TEST_CASE("C.LDSP", "[rvc]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.C_LDSP(x15, 8);
    REQUIRE(value == 0x67A2);

    as.RewindBuffer();

    as.C_LDSP(x15, 24);
    REQUIRE(value == 0x67E2);
}

TEST_CASE("C.LI", "[rvc]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.C_LI(x15, -1);
    REQUIRE(value == 0x57FD);

    as.RewindBuffer();

    as.C_LI(x15, -32);
    REQUIRE(value == 0x5781);

    as.RewindBuffer();

    as.C_LI(x15, 31);
    REQUIRE(value == 0x47FD);
}

TEST_CASE("C.LQ", "[rvc]") {
    uint32_t value = 0;
    auto as = MakeAssembler128(value);

    as.C_LQ(x15, 16, x15);
    REQUIRE(value == 0x2B9C);

    as.RewindBuffer();

    as.C_LQ(x15, 256, x15);
    REQUIRE(value == 0x279C);
}

TEST_CASE("C.LQSP", "[rvc]") {
    uint32_t value = 0;
    auto as = MakeAssembler128(value);

    as.C_LQSP(x15, 16);
    REQUIRE(value == 0x27C2);

    as.RewindBuffer();

    as.C_LQSP(x15, 256);
    REQUIRE(value == 0x2792);
}

TEST_CASE("C.LUI", "[rvc]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.C_LUI(x15, 0x3F);
    REQUIRE(value == 0x77FD);

    as.RewindBuffer();

    as.C_LUI(x15, 0x0F);
    REQUIRE(value == 0x67BD);
}

TEST_CASE("C.LW", "[rvc]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.C_LW(x15, 16, x15);
    REQUIRE(value == 0x4B9C);

    as.RewindBuffer();

    as.C_LW(x15, 24, x15);
    REQUIRE(value == 0x4F9C);
}

TEST_CASE("C.LWSP", "[rvc]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.C_LWSP(x15, 16);
    REQUIRE(value == 0x47C2);

    as.RewindBuffer();

    as.C_LWSP(x15, 24);
    REQUIRE(value == 0x47E2);
}

TEST_CASE("C.MV", "[rvc]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.C_MV(x31, x31);
    REQUIRE(value == 0x8FFE);

    as.RewindBuffer();

    as.C_MV(x15, x8);
    REQUIRE(value == 0x87A2);
}

TEST_CASE("C.NOP", "[rvc]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.C_NOP();
    REQUIRE(value == 0x0001);
}

TEST_CASE("C.OR", "[rvc]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.C_OR(x15, x15);
    REQUIRE(value == 0x8FDD);

    as.RewindBuffer();

    as.C_OR(x15, x8);
    REQUIRE(value == 0x8FC1);
}

TEST_CASE("C.SD", "[rvc]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.C_SD(x15, 8, x15);
    REQUIRE(value == 0xE79C);

    as.RewindBuffer();

    as.C_SD(x15, 24, x15);
    REQUIRE(value == 0xEF9C);
}

TEST_CASE("C.SDSP", "[rvc]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.C_SDSP(x15, 8);
    REQUIRE(value == 0xE43E);

    as.RewindBuffer();

    as.C_SDSP(x15, 24);
    REQUIRE(value == 0xEC3E);
}

TEST_CASE("C.SLLI", "[rvc]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.C_SLLI(x15, 15);
    REQUIRE(value == 0x07BE);

    as.RewindBuffer();

    as.C_SLLI(x15, 31);
    REQUIRE(value == 0x07FE);
}

TEST_CASE("C.SLLI (RV128)", "[rv128c]") {
    uint32_t value = 0;
    auto as = MakeAssembler128(value);

    as.C_SLLI(x15, 64);
    REQUIRE(value == 0x0782);
}

TEST_CASE("C.SQ", "[rvc]") {
    uint32_t value = 0;
    auto as = MakeAssembler128(value);

    as.C_SQ(x15, 16, x15);
    REQUIRE(value == 0xAB9C);

    as.RewindBuffer();

    as.C_SQ(x15, 256, x15);
    REQUIRE(value == 0xA79C);
}

TEST_CASE("C.SQSP", "[rvc]") {
    uint32_t value = 0;
    auto as = MakeAssembler128(value);

    as.C_SQSP(x15, 16);
    REQUIRE(value == 0xA83E);

    as.RewindBuffer();

    as.C_SQSP(x15, 256);
    REQUIRE(value == 0xA23E);
}

TEST_CASE("C.SRAI", "[rvc]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.C_SRAI(x15, 16);
    REQUIRE(value == 0x87C1);

    as.RewindBuffer();

    as.C_SRAI(x15, 31);
    REQUIRE(value == 0x87FD);
}

TEST_CASE("C.SRAI (RV128)", "[rv128c]") {
    uint32_t value = 0;
    auto as = MakeAssembler128(value);

    as.C_SRAI(x15, 64);
    REQUIRE(value == 0x8781);
}

TEST_CASE("C.SRLI", "[rvc]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.C_SRLI(x15, 16);
    REQUIRE(value == 0x83C1);

    as.RewindBuffer();

    as.C_SRLI(x15, 31);
    REQUIRE(value == 0x83FD);
}

TEST_CASE("C.SRLI (RV128)", "[rv128c]") {
    uint32_t value = 0;
    auto as = MakeAssembler128(value);

    as.C_SRLI(x15, 64);
    REQUIRE(value == 0x8381);
}

TEST_CASE("C.SUB", "[rvc]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.C_SUB(x15, x15);
    REQUIRE(value == 0x8F9D);

    as.RewindBuffer();

    as.C_SUB(x15, x8);
    REQUIRE(value == 0x8F81);
}

TEST_CASE("C.SUBW", "[rvc]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.C_SUBW(x15, x15);
    REQUIRE(value == 0x9F9D);

    as.RewindBuffer();

    as.C_SUBW(x15, x8);
    REQUIRE(value == 0x9F81);
}

TEST_CASE("C.SW", "[rvc]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.C_SW(x15, 16, x15);
    REQUIRE(value == 0xCB9C);

    as.RewindBuffer();

    as.C_SW(x15, 24, x15);
    REQUIRE(value == 0xCF9C);
}

TEST_CASE("C.SWSP", "[rvc]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.C_SWSP(x15, 16);
    REQUIRE(value == 0xC83E);

    as.RewindBuffer();

    as.C_SWSP(x15, 24);
    REQUIRE(value == 0xCC3E);
}

TEST_CASE("C.UNDEF", "[rvc]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.C_UNDEF();
    REQUIRE(value == 0);
}

TEST_CASE("C.XOR", "[rvc]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.C_XOR(x15, x15);
    REQUIRE(value == 0x8FBD);

    as.RewindBuffer();

    as.C_XOR(x15, x8);
    REQUIRE(value == 0x8FA1);
}
