#include <catch/catch.hpp>

#include <biscuit/assembler.hpp>

#include "assembler_test_utils.hpp"

using namespace biscuit;

TEST_CASE("SSAMOSWAP.D", "[Zicfiss]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.SSAMOSWAP_D(Ordering::None, x31, x7, x15);
    REQUIRE(value == 0x4877BFAF);

    as.RewindBuffer();

    as.SSAMOSWAP_D(Ordering::AQ, x31, x7, x15);
    REQUIRE(value == 0x4C77BFAF);

    as.RewindBuffer();

    as.SSAMOSWAP_D(Ordering::RL, x31, x7, x15);
    REQUIRE(value == 0x4A77BFAF);

    as.RewindBuffer();

    as.SSAMOSWAP_D(Ordering::AQRL, x31, x7, x15);
    REQUIRE(value == 0x4E77BFAF);
}

TEST_CASE("SSAMOSWAP.W", "[Zicfiss]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.SSAMOSWAP_W(Ordering::None, x31, x7, x15);
    REQUIRE(value == 0x4877AFAF);

    as.RewindBuffer();

    as.SSAMOSWAP_W(Ordering::AQ, x31, x7, x15);
    REQUIRE(value == 0x4C77AFAF);

    as.RewindBuffer();

    as.SSAMOSWAP_W(Ordering::RL, x31, x7, x15);
    REQUIRE(value == 0x4A77AFAF);

    as.RewindBuffer();

    as.SSAMOSWAP_W(Ordering::AQRL, x31, x7, x15);
    REQUIRE(value == 0x4E77AFAF);
}

TEST_CASE("SSRDP", "[Zicfiss]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    for (uint32_t i = 1; i <= 31; i++) {
        as.SSRDP(GPR{i});
        REQUIRE(value == (0xCDC04073U | (i << 7)));

        as.RewindBuffer();
    }
}

TEST_CASE("SSPOPCHK", "[Zicfiss]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.SSPOPCHK(x1);
    REQUIRE(value == 0xCDC0C073);

    as.RewindBuffer();

    as.SSPOPCHK(x5);
    REQUIRE(value == 0xCDC2C073);
}

TEST_CASE("C.SSPOPCHK", "[Zicfiss]") {
    uint16_t value = 0;
    auto as = MakeAssembler32(value);

    as.C_SSPOPCHK();
    REQUIRE(value == 0x6281U);
}

TEST_CASE("SSPUSH", "[Zicfiss]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.SSPUSH(x1);
    REQUIRE(value == 0xCE104073);

    as.RewindBuffer();

    as.SSPUSH(x5);
    REQUIRE(value == 0xCE504073);
}

TEST_CASE("C.SSPUSH", "[Zicfiss]") {
    uint16_t value = 0;
    auto as = MakeAssembler32(value);

    as.C_SSPUSH();
    REQUIRE(value == 0x6081U);
}

TEST_CASE("LPAD", "[Zicfilp]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.LPAD(-1);
    REQUIRE(value == 0xFFFFF017);

    as.RewindBuffer();

    as.LPAD(0);
    REQUIRE(value == 0x00000017);

    as.RewindBuffer();

    as.LPAD(0x00FF00FF);
    REQUIRE(value == 0xF00FF017);
}
