#include <catch/catch.hpp>

#include <array>
#include <biscuit/assembler.hpp>

#include "assembler_test_utils.hpp"

using namespace biscuit;

TEST_CASE("ADDIW", "[rv64i]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.ADDIW(x31, x15, 1024);
    REQUIRE(value == 0x40078F9B);

    as.RewindBuffer();

    as.ADDIW(x31, x15, 2048);
    REQUIRE(value == 0x80078F9B);

    as.RewindBuffer();

    as.ADDIW(x31, x15, 4095);
    REQUIRE(value == 0xFFF78F9B);
}

TEST_CASE("ADDW", "[rv64i]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.ADDW(x7, x15, x31);
    REQUIRE(value == 0x01F783BB);

    as.RewindBuffer();

    as.ADDW(x31, x31, x31);
    REQUIRE(value == 0x01FF8FBB);

    as.RewindBuffer();

    as.ADDW(x0, x0, x0);
    REQUIRE(value == 0x0000003B);
}

TEST_CASE("LWU", "[rv64i]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.LWU(x15, 1024, x31);
    REQUIRE(value == 0x400FE783);

    as.RewindBuffer();

    as.LWU(x15, 1536, x31);
    REQUIRE(value == 0x600FE783);

    as.RewindBuffer();

    as.LWU(x15, -1, x31);
    REQUIRE(value == 0xFFFFE783);
}

TEST_CASE("LD", "[rv64i]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.LD(x15, 1024, x31);
    REQUIRE(value == 0x400FB783);

    as.RewindBuffer();

    as.LD(x15, 1536, x31);
    REQUIRE(value == 0x600FB783);

    as.RewindBuffer();

    as.LD(x15, -1, x31);
    REQUIRE(value == 0xFFFFB783);
}

TEST_CASE("LI (RV64)", "[rv64i]") {
    // Up to 8 instructions can be generated
    constexpr size_t MAX_INSTS = 8;
    std::array<uint32_t, MAX_INSTS> vals{};
    auto as = MakeAssembler64(vals);

    const auto compare_vals = [&vals]<typename... Args>(const Args&... args) {
        static_assert(sizeof...(args) <= MAX_INSTS);

        size_t i = 0;
        for (const auto arg : {args...}) {
            REQUIRE(vals[i] == arg);
            i++;
        }
    };

    ///////// Single ADDIW cases

    as.LI(x1, 0);
    // addiw x1, x0, 0
    compare_vals(0x0000009BU, 0x00000000U);
    as.RewindBuffer();
    vals = {};

    as.LI(x1, uint64_t(-1));
    // addiw x1, x0, -1
    compare_vals(0xFFF0009BU, 0x00000000U);
    as.RewindBuffer();
    vals = {};

    as.LI(x1, 42);
    // addiw x1, x0, 42
    compare_vals(0x02A0009BU, 0x000000000U);
    as.RewindBuffer();
    vals = {};

    as.LI(x1, 0x7ff);
    // addiw x1, x0, 2047
    compare_vals(0x7FF0009BU, 0x00000000U);
    as.RewindBuffer();
    vals = {};

    ///////// Single LUI cases

    as.LI(x1, 0x2A000);
    // lui x1, 42
    compare_vals(0x0002A0B7U, 0x00000000U);
    as.RewindBuffer();
    vals = {};

    as.LI(x1, ~0xFFFULL);
    // lui x1, -1
    compare_vals(0xFFFFF0B7U, 0x00000000U);
    as.RewindBuffer();
    vals = {};

    as.LI(x1, uint64_t(INT32_MIN));
    // lui x1, -524288
    compare_vals(0x800000B7U, 0x00000000U);
    as.RewindBuffer();
    vals = {};

    ///////// LUI+ADDIW cases

    as.LI(x1, 0x11111111);
    // lui x1, 69905
    // addiw x1, x1, 273
    compare_vals(0x111110B7U, 0x1110809BU, 0x00000000U);
    as.RewindBuffer();
    vals = {};

    as.LI(x1, INT32_MAX);
    // lui x1, -524288
    // addiw x1, x1, -1
    compare_vals(0x800000B7U, 0xFFF0809BU, 0x00000000U);
    as.RewindBuffer();
    vals = {};

    ///////// ADDIW+SLLI cases

    as.LI(x1, 0x7FF0000000ULL);
    // addiw x1, x0, 2047
    // slli x1, x1, 28
    compare_vals(0x7FF0009BU, 0x01C09093U, 0x000000000U);
    as.RewindBuffer();
    vals = {};

    as.LI(x1, 0xABC00000ULL);
    // addiw x1, x0, 687
    // slli x1, x1, 22
    compare_vals(0x2AF0009BU, 0x01609093U, 0x000000000U);
    as.RewindBuffer();
    vals = {};

    ///////// LUI+ADDIW+SLLI cases

    as.LI(x1, 0x7FFFFFFF0000ULL);
    // lui x1, -524288
    // addiw x1, x1, -1
    // slli x1, x1, 16
    compare_vals(0x800000B7U, 0xFFF0809BU, 0x01009093U, 0x000000000U);
    as.RewindBuffer();
    vals = {};

    ///////// LUI+ADDIW+SLLI+ADDI cases

    as.LI(x1, 0x7FFFFFFF0123);
    // lui x1, -524288
    // addiw x1, x1, -1
    // slli x1, x1, 16
    // addi x1, x1, 291
    compare_vals(0x800000B7U, 0xfff0809BU, 0x01009093U, 0x12308093U,
                 0x000000000U);
    as.RewindBuffer();
    vals = {};

    ///////// ADDIW+SLLI+ADDI+SLLI+ADDI cases

    as.LI(x1, 0x8000000080000001ULL);
    // addiw x1, x0, -1
    // slli x1, x1, 32
    // addi x1, x1, 1
    // slli x1, x1, 31
    // addi x1, x1, 1
    compare_vals(0xFFF0009BU, 0x02009093U, 0x00108093U, 0x01F09093U,
                 0x00108093U, 0x000000000U);
    as.RewindBuffer();
    vals = {};

    ///////// Full LUI+ADDIW+SLLI+ADDI+SLLI+ADDI+SLLI+ADDI cases

    as.LI(x1, 0x80808000808080F1ULL);
    // lui x1, -16
    // addiw x1, x1, 257
    // slli x1, x1, 16
    // addi x1, x1, 1
    // slli x1, x1, 16
    // addi x1, x1, 257
    // slli x1, x1, 15
    // addi x1, x1, 241
    compare_vals(0xFFFF00B7U, 0x1010809BU, 0x01009093U, 0x00108093U,
                 0x01009093U, 0x10108093U, 0x00F09093U, 0x0F108093U);
}

TEST_CASE("SD", "[rv64i]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.SD(x15, 1024, x31);
    REQUIRE(value == 0x40FFB023);

    as.RewindBuffer();

    as.SD(x15, 1536, x31);
    REQUIRE(value == 0x60FFB023);

    as.RewindBuffer();

    as.SD(x15, -1, x31);
    REQUIRE(value == 0xFEFFBFA3);
}

TEST_CASE("SLLI (RV64)", "[rv64i]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.SLLI(x31, x15, 10);
    REQUIRE(value == 0x00A79F93);

    as.RewindBuffer();

    as.SLLI(x31, x15, 20);
    REQUIRE(value == 0x01479F93);

    as.RewindBuffer();

    as.SLLI(x31, x15, 31);
    REQUIRE(value == 0x01F79F93);

    as.RewindBuffer();

    as.SLLI(x31, x15, 63);
    REQUIRE(value == 0x03F79F93);
}

TEST_CASE("SLLIW", "[rv64i]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.SLLIW(x31, x15, 10);
    REQUIRE(value == 0x00A79F9B);

    as.RewindBuffer();

    as.SLLIW(x31, x15, 20);
    REQUIRE(value == 0x01479F9B);

    as.RewindBuffer();

    as.SLLIW(x31, x15, 31);
    REQUIRE(value == 0x01F79F9B);
}

TEST_CASE("SLLW", "[rv64i]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.SLLW(x7, x15, x31);
    REQUIRE(value == 0x01F793BB);

    as.RewindBuffer();

    as.SLLW(x31, x31, x31);
    REQUIRE(value == 0x01FF9FBB);

    as.RewindBuffer();

    as.SLLW(x0, x0, x0);
    REQUIRE(value == 0x0000103B);
}

TEST_CASE("SRAI (RV64)", "[rv64i]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.SRAI(x31, x15, 10);
    REQUIRE(value == 0x40A7DF93);

    as.RewindBuffer();

    as.SRAI(x31, x15, 20);
    REQUIRE(value == 0x4147DF93);

    as.RewindBuffer();

    as.SRAI(x31, x15, 31);
    REQUIRE(value == 0x41F7DF93);

    as.RewindBuffer();

    as.SRAI(x31, x15, 63);
    REQUIRE(value == 0x43F7DF93);
}

TEST_CASE("SRAIW", "[rv64i]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.SRAIW(x31, x15, 10);
    REQUIRE(value == 0x40A7DF9B);

    as.RewindBuffer();

    as.SRAIW(x31, x15, 20);
    REQUIRE(value == 0x4147DF9B);

    as.RewindBuffer();

    as.SRAIW(x31, x15, 31);
    REQUIRE(value == 0x41F7DF9B);
}

TEST_CASE("SRAW", "[rv64i]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.SRAW(x7, x15, x31);
    REQUIRE(value == 0x41F7D3BB);

    as.RewindBuffer();

    as.SRAW(x31, x31, x31);
    REQUIRE(value == 0x41FFDFBB);

    as.RewindBuffer();

    as.SRAW(x0, x0, x0);
    REQUIRE(value == 0x4000503B);
}

TEST_CASE("SRLI (RV64)", "[rv64i]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.SRLI(x31, x15, 10);
    REQUIRE(value == 0x00A7DF93);

    as.RewindBuffer();

    as.SRLI(x31, x15, 20);
    REQUIRE(value == 0x0147DF93);

    as.RewindBuffer();

    as.SRLI(x31, x15, 31);
    REQUIRE(value == 0x01F7DF93);

    as.RewindBuffer();

    as.SRLI(x31, x15, 63);
    REQUIRE(value == 0x03F7DF93);
}

TEST_CASE("SRLIW", "[rv64i]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.SRLIW(x31, x15, 10);
    REQUIRE(value == 0x00A7DF9B);

    as.RewindBuffer();

    as.SRLIW(x31, x15, 20);
    REQUIRE(value == 0x0147DF9B);

    as.RewindBuffer();

    as.SRLIW(x31, x15, 31);
    REQUIRE(value == 0x01F7DF9B);
}

TEST_CASE("SRLW", "[rv64i]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.SRLW(x7, x15, x31);
    REQUIRE(value == 0x01F7D3BB);

    as.RewindBuffer();

    as.SRLW(x31, x31, x31);
    REQUIRE(value == 0x01FFDFBB);

    as.RewindBuffer();

    as.SRLW(x0, x0, x0);
    REQUIRE(value == 0x0000503B);
}

TEST_CASE("SUBW", "[rv64i]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.SUBW(x7, x15, x31);
    REQUIRE(value == 0x41F783BB);

    as.RewindBuffer();

    as.SUBW(x31, x31, x31);
    REQUIRE(value == 0x41FF8FBB);

    as.RewindBuffer();

    as.SUBW(x0, x0, x0);
    REQUIRE(value == 0x4000003B);
}

TEST_CASE("LI literal", "[rv64i]") {
    uint32_t instructions[10];
    auto as = MakeAssembler64(instructions);
    Literal<uint64_t> value(0x1234567890ABCDEF);
    as.LILiteral(x7, &value);
    as.NOP();
    as.NOP();
    as.NOP();
    as.NOP();
    as.Place(&value);

    // Should emit AUIPC+ADDI with offset == 24
    uint32_t expected_instructions[2];
    {
        auto as = MakeAssembler64(expected_instructions);
        as.AUIPC(x7, 0);
        as.ADDI(x7, x7, 24);
    }

    REQUIRE(instructions[0] == expected_instructions[0]);
    REQUIRE(instructions[1] == expected_instructions[1]);
}

TEST_CASE("LI literal backward", "[rv64i]") {
    uint32_t instructions[10];
    auto as = MakeAssembler64(instructions);
    Literal<uint64_t> value(0x1234567890ABCDEF);
    as.Place(&value);
    as.NOP();
    as.NOP();
    as.NOP();
    as.NOP();
    as.NOP();
    as.LILiteral(x7, &value);

    // Should emit AUIPC+ADDI with offset == -28
    uint32_t expected_instructions[2];
    {
        auto as = MakeAssembler64(expected_instructions);
        as.AUIPC(x7, 0);
        as.ADDI(x7, x7, -28);
    }

    REQUIRE(instructions[7] == expected_instructions[0]);
    REQUIRE(instructions[8] == expected_instructions[1]);
}

TEST_CASE("LI literal backward big type", "[rv64i]") {
    struct Data {
        uint32_t data[8];
    };

    Data my_data;
    uint32_t instructions[20];
    auto as = MakeAssembler64(instructions);
    Literal<Data> value(my_data);
    as.Place(&value);
    as.NOP();
    as.NOP();
    as.NOP();
    as.NOP();
    as.NOP();
    as.LILiteral(x7, &value);

    // Should emit AUIPC+ADDI with offset == -52
    uint32_t expected_instructions[2];
    {
        auto as = MakeAssembler64(expected_instructions);
        as.AUIPC(x7, 0);
        as.ADDI(x7, x7, -52);
    }

    REQUIRE(instructions[13] == expected_instructions[0]);
    REQUIRE(instructions[14] == expected_instructions[1]);
}