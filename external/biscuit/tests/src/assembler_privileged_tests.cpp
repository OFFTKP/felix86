#include <catch/catch.hpp>

#include <biscuit/assembler.hpp>

#include "assembler_test_utils.hpp"

using namespace biscuit;

TEST_CASE("HFENCE.VVMA", "[rvpriv]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.HFENCE_VVMA(x0, x0);
    REQUIRE(value == 0x22000073);

    as.RewindBuffer();

    as.HFENCE_VVMA(x15, x15);
    REQUIRE(value == 0x22F78073);
}

TEST_CASE("HFENCE.GVMA", "[rvpriv]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.HFENCE_GVMA(x0, x0);
    REQUIRE(value == 0x62000073);

    as.RewindBuffer();

    as.HFENCE_GVMA(x15, x15);
    REQUIRE(value == 0x62F78073);
}

TEST_CASE("HINVAL.VVMA", "[rvpriv]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.HINVAL_VVMA(x0, x0);
    REQUIRE(value == 0x26000073);

    as.RewindBuffer();

    as.HINVAL_VVMA(x15, x15);
    REQUIRE(value == 0x26F78073);
}

TEST_CASE("HINVAL.GVMA", "[rvpriv]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.HINVAL_GVMA(x0, x0);
    REQUIRE(value == 0x66000073);

    as.RewindBuffer();

    as.HINVAL_GVMA(x15, x15);
    REQUIRE(value == 0x66F78073);
}

TEST_CASE("HLV.B", "[rvpriv]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.HLV_B(x0, x0);
    REQUIRE(value == 0x60004073);

    as.RewindBuffer();

    as.HLV_B(x15, x14);
    REQUIRE(value == 0x600747F3);
}

TEST_CASE("HLV.BU", "[rvpriv]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.HLV_BU(x0, x0);
    REQUIRE(value == 0x60104073);

    as.RewindBuffer();

    as.HLV_BU(x15, x14);
    REQUIRE(value == 0x601747F3);
}

TEST_CASE("HLV.D", "[rvpriv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.HLV_D(x0, x0);
    REQUIRE(value == 0x6C004073);

    as.RewindBuffer();

    as.HLV_D(x15, x14);
    REQUIRE(value == 0x6C0747F3);
}

TEST_CASE("HLV.H", "[rvpriv]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.HLV_H(x0, x0);
    REQUIRE(value == 0x64004073);

    as.RewindBuffer();

    as.HLV_H(x15, x14);
    REQUIRE(value == 0x640747F3);
}

TEST_CASE("HLV.HU", "[rvpriv]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.HLV_HU(x0, x0);
    REQUIRE(value == 0x64104073);

    as.RewindBuffer();

    as.HLV_HU(x15, x14);
    REQUIRE(value == 0x641747F3);
}

TEST_CASE("HLV.W", "[rvpriv]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.HLV_W(x0, x0);
    REQUIRE(value == 0x68004073);

    as.RewindBuffer();

    as.HLV_W(x15, x14);
    REQUIRE(value == 0x680747F3);
}

TEST_CASE("HLV.WU", "[rvpriv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.HLV_WU(x0, x0);
    REQUIRE(value == 0x68104073);

    as.RewindBuffer();

    as.HLV_WU(x15, x14);
    REQUIRE(value == 0x681747F3);
}

TEST_CASE("HLVX.HU", "[rvpriv]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.HLVX_HU(x0, x0);
    REQUIRE(value == 0x64304073);

    as.RewindBuffer();

    as.HLVX_HU(x15, x14);
    REQUIRE(value == 0x643747F3);
}

TEST_CASE("HLVX.WU", "[rvpriv]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.HLVX_WU(x0, x0);
    REQUIRE(value == 0x68304073);

    as.RewindBuffer();

    as.HLVX_WU(x15, x14);
    REQUIRE(value == 0x683747F3);
}

TEST_CASE("HSV.B", "[rvpriv]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.HSV_B(x0, x0);
    REQUIRE(value == 0x62004073);

    as.RewindBuffer();

    as.HSV_B(x15, x14);
    REQUIRE(value == 0x62F74073);
}

TEST_CASE("HSV.D", "[rvpriv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.HSV_D(x0, x0);
    REQUIRE(value == 0x6E004073);

    as.RewindBuffer();

    as.HSV_D(x15, x14);
    REQUIRE(value == 0x6EF74073);
}

TEST_CASE("HSV.H", "[rvpriv]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.HSV_H(x0, x0);
    REQUIRE(value == 0x66004073);

    as.RewindBuffer();

    as.HSV_H(x15, x14);
    REQUIRE(value == 0x66F74073);
}

TEST_CASE("HSV.W", "[rvpriv]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.HSV_W(x0, x0);
    REQUIRE(value == 0x6A004073);

    as.RewindBuffer();

    as.HSV_W(x15, x14);
    REQUIRE(value == 0x6AF74073);
}

TEST_CASE("MRET", "[rvpriv]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.MRET();
    REQUIRE(value == 0x30200073);
}

TEST_CASE("SCTRCLR", "[rvpriv]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.SCTRCLR();
    REQUIRE(value == 0x10400073);
}

TEST_CASE("SFENCE.INVAL.IR", "[rvpriv]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.SFENCE_INVAL_IR();
    REQUIRE(value == 0x18100073);
}

TEST_CASE("SFENCE.VMA", "[rvpriv]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.SFENCE_VMA(x0, x0);
    REQUIRE(value == 0x12000073);

    as.RewindBuffer();

    as.SFENCE_VMA(x15, x15);
    REQUIRE(value == 0x12F78073);
}

TEST_CASE("SFENCE.W.INVAL", "[rvpriv]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.SFENCE_W_INVAL();
    REQUIRE(value == 0x18000073);
}

TEST_CASE("SINVAL.VMA", "[rvpriv]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.SINVAL_VMA(x0, x0);
    REQUIRE(value == 0x16000073);

    as.RewindBuffer();

    as.SINVAL_VMA(x15, x15);
    REQUIRE(value == 0x16F78073);
}

TEST_CASE("SRET", "[rvpriv]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.SRET();
    REQUIRE(value == 0x10200073);
}

TEST_CASE("URET", "[rvpriv]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.URET();
    REQUIRE(value == 0x00200073);
}

TEST_CASE("WFI", "[rvpriv]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.WFI();
    REQUIRE(value == 0x10500073);
}
