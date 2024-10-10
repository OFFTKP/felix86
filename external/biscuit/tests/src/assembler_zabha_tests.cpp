#include <catch/catch.hpp>

#include <biscuit/assembler.hpp>

#include "assembler_test_utils.hpp"

using namespace biscuit;

TEST_CASE("AMOADD.H", "[Zabha]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.AMOADD_H(Ordering::None, x31, x7, x15);
    REQUIRE(value == 0x00779FAF);

    as.RewindBuffer();

    as.AMOADD_H(Ordering::AQ, x31, x7, x15);
    REQUIRE(value == 0x04779FAF);

    as.RewindBuffer();

    as.AMOADD_H(Ordering::RL, x31, x7, x15);
    REQUIRE(value == 0x02779FAF);

    as.RewindBuffer();

    as.AMOADD_H(Ordering::AQRL, x31, x7, x15);
    REQUIRE(value == 0x06779FAF);
}

TEST_CASE("AMOADD.B", "[Zabha]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.AMOADD_B(Ordering::None, x31, x7, x15);
    REQUIRE(value == 0x00778FAF);

    as.RewindBuffer();

    as.AMOADD_B(Ordering::AQ, x31, x7, x15);
    REQUIRE(value == 0x04778FAF);

    as.RewindBuffer();

    as.AMOADD_B(Ordering::RL, x31, x7, x15);
    REQUIRE(value == 0x02778FAF);

    as.RewindBuffer();

    as.AMOADD_B(Ordering::AQRL, x31, x7, x15);
    REQUIRE(value == 0x06778FAF);
}

TEST_CASE("AMOAND.H", "[Zabha]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.AMOAND_H(Ordering::None, x31, x7, x15);
    REQUIRE(value == 0x60779FAF);

    as.RewindBuffer();

    as.AMOAND_H(Ordering::AQ, x31, x7, x15);
    REQUIRE(value == 0x64779FAF);

    as.RewindBuffer();

    as.AMOAND_H(Ordering::RL, x31, x7, x15);
    REQUIRE(value == 0x62779FAF);

    as.RewindBuffer();

    as.AMOAND_H(Ordering::AQRL, x31, x7, x15);
    REQUIRE(value == 0x66779FAF);
}

TEST_CASE("AMOAND.B", "[Zabha]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.AMOAND_B(Ordering::None, x31, x7, x15);
    REQUIRE(value == 0x60778FAF);

    as.RewindBuffer();

    as.AMOAND_B(Ordering::AQ, x31, x7, x15);
    REQUIRE(value == 0x64778FAF);

    as.RewindBuffer();

    as.AMOAND_B(Ordering::RL, x31, x7, x15);
    REQUIRE(value == 0x62778FAF);

    as.RewindBuffer();

    as.AMOAND_B(Ordering::AQRL, x31, x7, x15);
    REQUIRE(value == 0x66778FAF);
}

TEST_CASE("AMOMAX.H", "[Zabha]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.AMOMAX_H(Ordering::None, x31, x7, x15);
    REQUIRE(value == 0xA0779FAF);

    as.RewindBuffer();

    as.AMOMAX_H(Ordering::AQ, x31, x7, x15);
    REQUIRE(value == 0xA4779FAF);

    as.RewindBuffer();

    as.AMOMAX_H(Ordering::RL, x31, x7, x15);
    REQUIRE(value == 0xA2779FAF);

    as.RewindBuffer();

    as.AMOMAX_H(Ordering::AQRL, x31, x7, x15);
    REQUIRE(value == 0xA6779FAF);
}

TEST_CASE("AMOMAX.B", "[Zabha]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.AMOMAX_B(Ordering::None, x31, x7, x15);
    REQUIRE(value == 0xA0778FAF);

    as.RewindBuffer();

    as.AMOMAX_B(Ordering::AQ, x31, x7, x15);
    REQUIRE(value == 0xA4778FAF);

    as.RewindBuffer();

    as.AMOMAX_B(Ordering::RL, x31, x7, x15);
    REQUIRE(value == 0xA2778FAF);

    as.RewindBuffer();

    as.AMOMAX_B(Ordering::AQRL, x31, x7, x15);
    REQUIRE(value == 0xA6778FAF);
}

TEST_CASE("AMOMAXU.H", "[Zabha]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.AMOMAXU_H(Ordering::None, x31, x7, x15);
    REQUIRE(value == 0xE0779FAF);

    as.RewindBuffer();

    as.AMOMAXU_H(Ordering::AQ, x31, x7, x15);
    REQUIRE(value == 0xE4779FAF);

    as.RewindBuffer();

    as.AMOMAXU_H(Ordering::RL, x31, x7, x15);
    REQUIRE(value == 0xE2779FAF);

    as.RewindBuffer();

    as.AMOMAXU_H(Ordering::AQRL, x31, x7, x15);
    REQUIRE(value == 0xE6779FAF);
}

TEST_CASE("AMOMAXU.B", "[Zabha]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.AMOMAXU_B(Ordering::None, x31, x7, x15);
    REQUIRE(value == 0xE0778FAF);

    as.RewindBuffer();

    as.AMOMAXU_B(Ordering::AQ, x31, x7, x15);
    REQUIRE(value == 0xE4778FAF);

    as.RewindBuffer();

    as.AMOMAXU_B(Ordering::RL, x31, x7, x15);
    REQUIRE(value == 0xE2778FAF);

    as.RewindBuffer();

    as.AMOMAXU_B(Ordering::AQRL, x31, x7, x15);
    REQUIRE(value == 0xE6778FAF);
}

TEST_CASE("AMOMIN.H", "[Zabha]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.AMOMIN_H(Ordering::None, x31, x7, x15);
    REQUIRE(value == 0x80779FAF);

    as.RewindBuffer();

    as.AMOMIN_H(Ordering::AQ, x31, x7, x15);
    REQUIRE(value == 0x84779FAF);

    as.RewindBuffer();

    as.AMOMIN_H(Ordering::RL, x31, x7, x15);
    REQUIRE(value == 0x82779FAF);

    as.RewindBuffer();

    as.AMOMIN_H(Ordering::AQRL, x31, x7, x15);
    REQUIRE(value == 0x86779FAF);
}

TEST_CASE("AMOMIN.B", "[Zabha]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.AMOMIN_B(Ordering::None, x31, x7, x15);
    REQUIRE(value == 0x80778FAF);

    as.RewindBuffer();

    as.AMOMIN_B(Ordering::AQ, x31, x7, x15);
    REQUIRE(value == 0x84778FAF);

    as.RewindBuffer();

    as.AMOMIN_B(Ordering::RL, x31, x7, x15);
    REQUIRE(value == 0x82778FAF);

    as.RewindBuffer();

    as.AMOMIN_B(Ordering::AQRL, x31, x7, x15);
    REQUIRE(value == 0x86778FAF);
}

TEST_CASE("AMOMINU.H", "[Zabha]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.AMOMINU_H(Ordering::None, x31, x7, x15);
    REQUIRE(value == 0xC0779FAF);

    as.RewindBuffer();

    as.AMOMINU_H(Ordering::AQ, x31, x7, x15);
    REQUIRE(value == 0xC4779FAF);

    as.RewindBuffer();

    as.AMOMINU_H(Ordering::RL, x31, x7, x15);
    REQUIRE(value == 0xC2779FAF);

    as.RewindBuffer();

    as.AMOMINU_H(Ordering::AQRL, x31, x7, x15);
    REQUIRE(value == 0xC6779FAF);
}

TEST_CASE("AMOMINU.B", "[Zabha]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.AMOMINU_B(Ordering::None, x31, x7, x15);
    REQUIRE(value == 0xC0778FAF);

    as.RewindBuffer();

    as.AMOMINU_B(Ordering::AQ, x31, x7, x15);
    REQUIRE(value == 0xC4778FAF);

    as.RewindBuffer();

    as.AMOMINU_B(Ordering::RL, x31, x7, x15);
    REQUIRE(value == 0xC2778FAF);

    as.RewindBuffer();

    as.AMOMINU_B(Ordering::AQRL, x31, x7, x15);
    REQUIRE(value == 0xC6778FAF);
}

TEST_CASE("AMOOR.H", "[Zabha]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.AMOOR_H(Ordering::None, x31, x7, x15);
    REQUIRE(value == 0x40779FAF);

    as.RewindBuffer();

    as.AMOOR_H(Ordering::AQ, x31, x7, x15);
    REQUIRE(value == 0x44779FAF);

    as.RewindBuffer();

    as.AMOOR_H(Ordering::RL, x31, x7, x15);
    REQUIRE(value == 0x42779FAF);

    as.RewindBuffer();

    as.AMOOR_H(Ordering::AQRL, x31, x7, x15);
    REQUIRE(value == 0x46779FAF);
}

TEST_CASE("AMOOR.B", "[Zabha]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.AMOOR_B(Ordering::None, x31, x7, x15);
    REQUIRE(value == 0x40778FAF);

    as.RewindBuffer();

    as.AMOOR_B(Ordering::AQ, x31, x7, x15);
    REQUIRE(value == 0x44778FAF);

    as.RewindBuffer();

    as.AMOOR_B(Ordering::RL, x31, x7, x15);
    REQUIRE(value == 0x42778FAF);

    as.RewindBuffer();

    as.AMOOR_B(Ordering::AQRL, x31, x7, x15);
    REQUIRE(value == 0x46778FAF);
}

TEST_CASE("AMOSWAP.H", "[Zabha]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.AMOSWAP_H(Ordering::None, x31, x7, x15);
    REQUIRE(value == 0x08779FAF);

    as.RewindBuffer();

    as.AMOSWAP_H(Ordering::AQ, x31, x7, x15);
    REQUIRE(value == 0x0C779FAF);

    as.RewindBuffer();

    as.AMOSWAP_H(Ordering::RL, x31, x7, x15);
    REQUIRE(value == 0x0A779FAF);

    as.RewindBuffer();

    as.AMOSWAP_H(Ordering::AQRL, x31, x7, x15);
    REQUIRE(value == 0x0E779FAF);
}

TEST_CASE("AMOSWAP.B", "[Zabha]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.AMOSWAP_B(Ordering::None, x31, x7, x15);
    REQUIRE(value == 0x08778FAF);

    as.RewindBuffer();

    as.AMOSWAP_B(Ordering::AQ, x31, x7, x15);
    REQUIRE(value == 0x0C778FAF);

    as.RewindBuffer();

    as.AMOSWAP_B(Ordering::RL, x31, x7, x15);
    REQUIRE(value == 0x0A778FAF);

    as.RewindBuffer();

    as.AMOSWAP_B(Ordering::AQRL, x31, x7, x15);
    REQUIRE(value == 0x0E778FAF);
}

TEST_CASE("AMOXOR.H", "[Zabha]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.AMOXOR_H(Ordering::None, x31, x7, x15);
    REQUIRE(value == 0x20779FAF);

    as.RewindBuffer();

    as.AMOXOR_H(Ordering::AQ, x31, x7, x15);
    REQUIRE(value == 0x24779FAF);

    as.RewindBuffer();

    as.AMOXOR_H(Ordering::RL, x31, x7, x15);
    REQUIRE(value == 0x22779FAF);

    as.RewindBuffer();

    as.AMOXOR_H(Ordering::AQRL, x31, x7, x15);
    REQUIRE(value == 0x26779FAF);
}

TEST_CASE("AMOXOR.B", "[Zabha]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.AMOXOR_B(Ordering::None, x31, x7, x15);
    REQUIRE(value == 0x20778FAF);

    as.RewindBuffer();

    as.AMOXOR_B(Ordering::AQ, x31, x7, x15);
    REQUIRE(value == 0x24778FAF);

    as.RewindBuffer();

    as.AMOXOR_B(Ordering::RL, x31, x7, x15);
    REQUIRE(value == 0x22778FAF);

    as.RewindBuffer();

    as.AMOXOR_B(Ordering::AQRL, x31, x7, x15);
    REQUIRE(value == 0x26778FAF);
}

TEST_CASE("AMOCAS.H", "[Zabha]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.AMOCAS_H(Ordering::None, x31, x7, x15);
    REQUIRE(value == 0x28779FAF);

    as.RewindBuffer();

    as.AMOCAS_H(Ordering::AQ, x31, x7, x15);
    REQUIRE(value == 0x2C779FAF);

    as.RewindBuffer();

    as.AMOCAS_H(Ordering::RL, x31, x7, x15);
    REQUIRE(value == 0x2A779FAF);

    as.RewindBuffer();

    as.AMOCAS_H(Ordering::AQRL, x31, x7, x15);
    REQUIRE(value == 0x2E779FAF);
}

TEST_CASE("AMOCAS.B", "[Zabha]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.AMOCAS_B(Ordering::None, x31, x7, x15);
    REQUIRE(value == 0x28778FAF);

    as.RewindBuffer();

    as.AMOCAS_B(Ordering::AQ, x31, x7, x15);
    REQUIRE(value == 0x2C778FAF);

    as.RewindBuffer();

    as.AMOCAS_B(Ordering::RL, x31, x7, x15);
    REQUIRE(value == 0x2A778FAF);

    as.RewindBuffer();

    as.AMOCAS_B(Ordering::AQRL, x31, x7, x15);
    REQUIRE(value == 0x2E778FAF);
}
