#include <catch/catch.hpp>

#include <biscuit/assembler.hpp>

#include "assembler_test_utils.hpp"

using namespace biscuit;

TEST_CASE("ADD.UW", "[rvb]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.ADDUW(x31, x7, x15);
    REQUIRE(value == 0x08F38FBB);

    as.RewindBuffer();

    // Pseudo instruction

    as.ZEXTW(x31, x7);
    REQUIRE(value == 0x08038FBB);
}

TEST_CASE("ANDN", "[rvb]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.ANDN(x31, x7, x15);
    REQUIRE(value == 0x40F3FFB3);
}

TEST_CASE("BCLR", "[rvb]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.BCLR(x31, x7, x15);
    REQUIRE(value == 0x48F39FB3);
}

TEST_CASE("BCLRI", "[rvb]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.BCLRI(x31, x7, 0);
    REQUIRE(value == 0x48039F93);

    as.RewindBuffer();

    as.BCLRI(x31, x7, 15);
    REQUIRE(value == 0x48F39F93);

    as.RewindBuffer();

    as.BCLRI(x31, x7, 31);
    REQUIRE(value == 0x49F39F93);
}

TEST_CASE("BCLRI (RV64)", "[rvb]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.BCLRI(x31, x7, 0);
    REQUIRE(value == 0x48039F93);

    as.RewindBuffer();

    as.BCLRI(x31, x7, 15);
    REQUIRE(value == 0x48F39F93);

    as.RewindBuffer();

    as.BCLRI(x31, x7, 31);
    REQUIRE(value == 0x49F39F93);

    as.RewindBuffer();

    as.BCLRI(x31, x7, 63);
    REQUIRE(value == 0x4BF39F93);
}

TEST_CASE("BEXT", "[rvb]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.BEXT(x31, x7, x15);
    REQUIRE(value == 0x48F3DFB3);
}

TEST_CASE("BEXTI", "[rvb]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.BEXTI(x31, x7, 0);
    REQUIRE(value == 0x4803DF93);

    as.RewindBuffer();

    as.BEXTI(x31, x7, 15);
    REQUIRE(value == 0x48F3DF93);

    as.RewindBuffer();

    as.BEXTI(x31, x7, 31);
    REQUIRE(value == 0x49F3DF93);
}

TEST_CASE("BEXTI (RV64)", "[rvb]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.BEXTI(x31, x7, 0);
    REQUIRE(value == 0x4803DF93);

    as.RewindBuffer();

    as.BEXTI(x31, x7, 15);
    REQUIRE(value == 0x48F3DF93);

    as.RewindBuffer();

    as.BEXTI(x31, x7, 31);
    REQUIRE(value == 0x49F3DF93);

    as.RewindBuffer();

    as.BEXTI(x31, x7, 63);
    REQUIRE(value == 0x4BF3DF93);
}

TEST_CASE("BINV", "[rvb]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.BINV(x31, x7, x15);
    REQUIRE(value == 0x68F39FB3);
}

TEST_CASE("BINVI", "[rvb]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.BINVI(x31, x7, 0);
    REQUIRE(value == 0x68039F93);

    as.RewindBuffer();

    as.BINVI(x31, x7, 15);
    REQUIRE(value == 0x68F39F93);

    as.RewindBuffer();

    as.BINVI(x31, x7, 31);
    REQUIRE(value == 0x69F39F93);
}

TEST_CASE("BINVI (RV64)", "[rvb]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.BINVI(x31, x7, 0);
    REQUIRE(value == 0x68039F93);

    as.RewindBuffer();

    as.BINVI(x31, x7, 15);
    REQUIRE(value == 0x68F39F93);

    as.RewindBuffer();

    as.BINVI(x31, x7, 31);
    REQUIRE(value == 0x69F39F93);

    as.RewindBuffer();

    as.BINVI(x31, x7, 63);
    REQUIRE(value == 0x6BF39F93);
}

TEST_CASE("BREV8", "[rvb]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.BREV8(x31, x31);
    REQUIRE(value == 0x687FDF93);

    as.RewindBuffer();

    as.BREV8(x1, x2);
    REQUIRE(value == 0x68715093);
}

TEST_CASE("BSET", "[rvb]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.BSET(x31, x7, x15);
    REQUIRE(value == 0x28F39FB3);
}

TEST_CASE("BSETI", "[rvb]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.BSETI(x31, x7, 0);
    REQUIRE(value == 0x28039FB3);

    as.RewindBuffer();

    as.BSETI(x31, x7, 15);
    REQUIRE(value == 0x28F39FB3);

    as.RewindBuffer();

    as.BSETI(x31, x7, 31);
    REQUIRE(value == 0x29F39FB3);
}

TEST_CASE("BSETI (RV64)", "[rvb]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.BSETI(x31, x7, 0);
    REQUIRE(value == 0x28039FB3);

    as.RewindBuffer();

    as.BSETI(x31, x7, 15);
    REQUIRE(value == 0x28F39FB3);

    as.RewindBuffer();

    as.BSETI(x31, x7, 31);
    REQUIRE(value == 0x29F39FB3);

    as.RewindBuffer();

    as.BSETI(x31, x7, 63);
    REQUIRE(value == 0x2BF39FB3);
}

TEST_CASE("CLMUL", "[rvb]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.CLMUL(x31, x7, x15);
    REQUIRE(value == 0x0AF39FB3);
}

TEST_CASE("CLMULH", "[rvb]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.CLMULH(x31, x7, x15);
    REQUIRE(value == 0x0AF3BFB3);
}

TEST_CASE("CLMULR", "[rvb]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.CLMULR(x31, x7, x15);
    REQUIRE(value == 0x0AF3AFB3);
}

TEST_CASE("CLZ", "[rvb]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.CLZ(x31, x7);
    REQUIRE(value == 0x60039F93);
}

TEST_CASE("CLZW", "[rvb]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.CLZW(x31, x7);
    REQUIRE(value == 0x60039F9B);
}

TEST_CASE("CPOP", "[rvb]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.CPOP(x31, x7);
    REQUIRE(value == 0x60239F93);
}

TEST_CASE("CPOPW", "[rvb]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.CPOPW(x31, x7);
    REQUIRE(value == 0x60239F9B);
}

TEST_CASE("CTZ", "[rvb]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.CTZ(x31, x7);
    REQUIRE(value == 0x60139F93);
}

TEST_CASE("CTZW", "[rvb]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.CTZW(x31, x7);
    REQUIRE(value == 0x60139F9B);
}

TEST_CASE("MAX", "[rvb]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.MAX(x31, x7, x15);
    REQUIRE(value == 0x0AF3EFB3);
}

TEST_CASE("MAXU", "[rvb]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.MAXU(x31, x7, x15);
    REQUIRE(value == 0x0AF3FFB3);
}

TEST_CASE("MIN", "[rvb]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.MIN(x31, x7, x15);
    REQUIRE(value == 0x0AF3CFB3);
}

TEST_CASE("MINU", "[rvb]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.MINU(x31, x7, x15);
    REQUIRE(value == 0x0AF3DFB3);
}

TEST_CASE("ORC.B", "[rvb]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.ORCB(x31, x7);
    REQUIRE(value == 0x2873DF93);
}

TEST_CASE("ORN", "[rvb]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.ORN(x31, x7, x15);
    REQUIRE(value == 0x40F3EFB3);
}

TEST_CASE("PACK", "[rvb]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.PACK(x31, x7, x2);
    REQUIRE(value == 0x0823CFB3);
}

TEST_CASE("PACKH", "[rvb]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.PACKH(x31, x7, x2);
    REQUIRE(value == 0x0823FFB3);
}

TEST_CASE("PACKW", "[rvb]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.PACKW(x31, x7, x2);
    REQUIRE(value == 0x0823CFBB);
}

TEST_CASE("REV8", "[rvb]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.REV8(x31, x7);
    REQUIRE(value == 0x6983DF93);
}

TEST_CASE("REV8 (RV64)", "[rvb]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.REV8(x31, x7);
    REQUIRE(value == 0x6B83DF93);
}

TEST_CASE("ROL", "[rvb]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.ROL(x31, x7, x15);
    REQUIRE(value == 0x60F39FB3);
}

TEST_CASE("ROLW", "[rvb]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.ROLW(x31, x7, x15);
    REQUIRE(value == 0x60F39FBB);
}

TEST_CASE("ROR", "[rvb]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.ROR(x31, x7, x15);
    REQUIRE(value == 0x60F3DFB3);
}

TEST_CASE("RORW", "[rvb]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.RORW(x31, x7, x15);
    REQUIRE(value == 0x60F3DFBB);
}

TEST_CASE("RORI", "[rvb]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.RORI(x31, x7, 0);
    REQUIRE(value == 0x6003DF93);

    as.RewindBuffer();

    as.RORI(x31, x7, 63);
    REQUIRE(value == 0x63F3DF93);
}

TEST_CASE("RORIW", "[rvb]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.RORIW(x31, x7, 0);
    REQUIRE(value == 0x6003DF9B);

    as.RewindBuffer();

    as.RORIW(x31, x7, 63);
    REQUIRE(value == 0x63F3DF9B);
}

TEST_CASE("SEXT.B", "[rvb]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.SEXTB(x31, x7);
    REQUIRE(value == 0x60439F93);
}

TEST_CASE("SEXT.H", "[rvb]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.SEXTH(x31, x7);
    REQUIRE(value == 0x60539F93);
}

TEST_CASE("SH1ADD", "[rvb]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.SH1ADD(x31, x7, x15);
    REQUIRE(value == 0x20F3AFB3);
}

TEST_CASE("SH1ADD.UW", "[rvb]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.SH1ADDUW(x31, x7, x15);
    REQUIRE(value == 0x20F3AFBB);
}

TEST_CASE("SH2ADD", "[rvb]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.SH2ADD(x31, x7, x15);
    REQUIRE(value == 0x20F3CFB3);
}

TEST_CASE("SH2ADD.UW", "[rvb]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.SH2ADDUW(x31, x7, x15);
    REQUIRE(value == 0x20F3CFBB);
}

TEST_CASE("SH3ADD", "[rvb]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.SH3ADD(x31, x7, x15);
    REQUIRE(value == 0x20F3EFB3);
}

TEST_CASE("SH3ADD.UW", "[rvb]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.SH3ADDUW(x31, x7, x15);
    REQUIRE(value == 0x20F3EFBB);
}

TEST_CASE("SLLI.UW", "[rvb]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.SLLIUW(x31, x7, 0);
    REQUIRE(value == 0x08039F9B);

    as.RewindBuffer();

    as.SLLIUW(x31, x7, 63);
    REQUIRE(value == 0x0BF39F9B);
}

TEST_CASE("UNZIP", "[rvb]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.UNZIP(x31, x31);
    REQUIRE(value == 0x09FFDF93);

    as.RewindBuffer();

    as.UNZIP(x1, x2);
    REQUIRE(value == 0x09F15093);
}

TEST_CASE("XNOR", "[rvb]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.XNOR(x31, x7, x15);
    REQUIRE(value == 0x40F3CFB3);
}

TEST_CASE("XPERM4", "[rvb]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.XPERM4(x31, x31, x31);
    REQUIRE(value == 0x29FFAFB3);

    as.RewindBuffer();

    as.XPERM4(x1, x2, x3);
    REQUIRE(value == 0x283120B3);
}

TEST_CASE("XPERM8", "[rvb]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.XPERM8(x31, x31, x31);
    REQUIRE(value == 0x29FFCFB3);

    as.RewindBuffer();

    as.XPERM8(x1, x2, x3);
    REQUIRE(value == 0x283140B3);
}

TEST_CASE("ZEXT.H", "[rvb]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.ZEXTH(x31, x7);
    REQUIRE(value == 0x0803CFB3);
}

TEST_CASE("ZEXT.H (RV64)", "[rvb]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.ZEXTH(x31, x7);
    REQUIRE(value == 0x0803CFBB);
}

TEST_CASE("ZIP", "[rvb]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.ZIP(x31, x31);
    REQUIRE(value == 0x09EF9F93);

    as.RewindBuffer();

    as.ZIP(x1, x2);
    REQUIRE(value == 0x09E11093);
}
