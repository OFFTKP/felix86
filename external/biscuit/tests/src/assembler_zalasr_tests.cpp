#include <catch/catch.hpp>

#include <biscuit/assembler.hpp>

#include "assembler_test_utils.hpp"

using namespace biscuit;

TEST_CASE("LB.AQRL", "[Zalasr]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.LB(Ordering::AQ, x15, x31);
    REQUIRE(value == 0x340F87AF);

    as.RewindBuffer();

    as.LB(Ordering::AQRL, x15, x31);
    REQUIRE(value == 0x360F87AF);
}

TEST_CASE("LH.AQRL", "[Zalasr]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.LH(Ordering::AQ, x15, x31);
    REQUIRE(value == 0x340F97AF);

    as.RewindBuffer();

    as.LH(Ordering::AQRL, x15, x31);
    REQUIRE(value == 0x360F97AF);
}

TEST_CASE("LW.AQRL", "[Zalasr]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.LW(Ordering::AQ, x15, x31);
    REQUIRE(value == 0x340FA7AF);

    as.RewindBuffer();

    as.LW(Ordering::AQRL, x15, x31);
    REQUIRE(value == 0x360FA7AF);
}

TEST_CASE("LD.AQRL", "[Zalasr]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.LD(Ordering::AQ, x15, x31);
    REQUIRE(value == 0x340FB7AF);

    as.RewindBuffer();

    as.LD(Ordering::AQRL, x15, x31);
    REQUIRE(value == 0x360FB7AF);
}

TEST_CASE("SB.AQRL", "[Zalasr]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.SB(Ordering::RL, x15, x31);
    REQUIRE(value == 0x3AFF802F);

    as.RewindBuffer();

    as.SB(Ordering::AQRL, x15, x31);
    REQUIRE(value == 0x3EFF802F);
}

TEST_CASE("SH.AQRL", "[Zalasr]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.SH(Ordering::RL, x15, x31);
    REQUIRE(value == 0x3AFF902F);

    as.RewindBuffer();

    as.SH(Ordering::AQRL, x15, x31);
    REQUIRE(value == 0x3EFF902F);
}

TEST_CASE("SW.AQRL", "[Zalasr]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.SW(Ordering::RL, x15, x31);
    REQUIRE(value == 0x3AFFA02F);

    as.RewindBuffer();

    as.SW(Ordering::AQRL, x15, x31);
    REQUIRE(value == 0x3EFFA02F);
}

TEST_CASE("SD.AQRL", "[Zalasr]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.SD(Ordering::RL, x15, x31);
    REQUIRE(value == 0x3AFFB02F);

    as.RewindBuffer();

    as.SD(Ordering::AQRL, x15, x31);
    REQUIRE(value == 0x3EFFB02F);
}
