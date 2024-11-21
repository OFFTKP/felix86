#include <catch/catch.hpp>

#include <array>
#include <biscuit/assembler.hpp>

#include "assembler_test_utils.hpp"

using namespace biscuit;

TEST_CASE("ADD", "[rv32i]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.ADD(x7, x15, x31);
    REQUIRE(value == 0x01F783B3);

    as.RewindBuffer();

    as.ADD(x31, x31, x31);
    REQUIRE(value == 0x01FF8FB3);

    as.RewindBuffer();

    as.ADD(x0, x0, x0);
    REQUIRE(value == 0x00000033);
}

TEST_CASE("ADDI", "[rv32i]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.ADDI(x15, x31, 1024);
    REQUIRE(value == 0x400F8793);

    as.RewindBuffer();

    as.ADDI(x15, x31, 2048);
    REQUIRE(value == 0x800F8793);

    as.RewindBuffer();

    as.ADDI(x15, x31, 4095);
    REQUIRE(value == 0xFFFF8793);
}

TEST_CASE("AND", "[rv32i]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.AND(x7, x15, x31);
    REQUIRE(value == 0x01F7F3B3);

    as.RewindBuffer();

    as.AND(x31, x31, x31);
    REQUIRE(value == 0x01FFFFB3);

    as.RewindBuffer();

    as.AND(x0, x0, x0);
    REQUIRE(value == 0x00007033);
}

TEST_CASE("ANDI", "[rv32i]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.ANDI(x15, x31, 1024);
    REQUIRE(value == 0x400FF793);

    as.RewindBuffer();

    as.ANDI(x15, x31, 2048);
    REQUIRE(value == 0x800FF793);

    as.RewindBuffer();

    as.ANDI(x15, x31, 4095);
    REQUIRE(value == 0xFFFFF793);
}

TEST_CASE("AUIPC", "[rv32i]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.AUIPC(x31, -1);
    REQUIRE(value == 0xFFFFFF97);

    as.RewindBuffer();

    as.AUIPC(x31, 0);
    REQUIRE(value == 0x00000F97);

    as.RewindBuffer();

    as.AUIPC(x31, 0x00FF00FF);
    REQUIRE(value == 0xF00FFF97);
}

TEST_CASE("BEQ", "[rv32i]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.BEQ(x15, x31, 2000);
    REQUIRE(value == 0x7DF78863);

    as.RewindBuffer();

    as.BEQ(x15, x31, -2);
    REQUIRE(value == 0xFFF78FE3);
}

TEST_CASE("BGE", "[rv32i]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.BGE(x15, x31, 2000);
    REQUIRE(value == 0x7DF7D863);

    as.RewindBuffer();

    as.BGE(x15, x31, -2);
    REQUIRE(value == 0xFFF7DFE3);
}

TEST_CASE("BGEU", "[rv32i]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.BGEU(x15, x31, 2000);
    REQUIRE(value == 0x7DF7F863);

    as.RewindBuffer();

    as.BGEU(x15, x31, -2);
    REQUIRE(value == 0xFFF7FFE3);
}

TEST_CASE("BNE", "[rv32i]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.BNE(x15, x31, 2000);
    REQUIRE(value == 0x7DF79863);

    as.RewindBuffer();

    as.BNE(x15, x31, -2);
    REQUIRE(value == 0xFFF79FE3);
}

TEST_CASE("BLT", "[rv32i]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.BLT(x15, x31, 2000);
    REQUIRE(value == 0x7DF7C863);

    as.RewindBuffer();

    as.BLT(x15, x31, -2);
    REQUIRE(value == 0xFFF7CFE3);
}

TEST_CASE("BLTU", "[rv32i]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.BLTU(x15, x31, 2000);
    REQUIRE(value == 0x7DF7E863);

    as.RewindBuffer();

    as.BLTU(x15, x31, -2);
    REQUIRE(value == 0xFFF7EFE3);
}

TEST_CASE("CALL", "[rv32i]") {
    std::array<uint32_t, 2> vals{};
    auto as = MakeAssembler32(vals);

    const auto compare_vals = [&vals](uint32_t val_1, uint32_t val_2) {
        REQUIRE(vals[0] == val_1);
        REQUIRE(vals[1] == val_2);
    };

    as.CALL(-1);
    compare_vals(0x00000097, 0xFFF080E7);
}

TEST_CASE("EBREAK", "[rv32i]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.EBREAK();
    REQUIRE(value == 0x00100073);
}

TEST_CASE("ECALL", "[rv32i]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.ECALL();
    REQUIRE(value == 0x00000073);
}

TEST_CASE("FENCE", "[rv32i]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.FENCE(FenceOrder::IORW, FenceOrder::IORW);
    REQUIRE(value == 0x0FF0000F);

    as.RewindBuffer();

    as.FENCETSO();
    REQUIRE(value == 0x8330000F);

    as.RewindBuffer();

    as.FENCEI();
    REQUIRE(value == 0x0000100F);
}

TEST_CASE("JAL", "[rv32i]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.JAL(x31, 0xFFFFFFFF);
    REQUIRE(value == 0xFFFFFFEF);

    as.RewindBuffer();

    as.JAL(x31, 2000);
    REQUIRE(value == 0x7D000FEF);

    as.RewindBuffer();

    as.JAL(x31, 100000);
    REQUIRE(value == 0x6A018FEF);
}

TEST_CASE("JALR", "[rv32i]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.JALR(x15, 1024, x31);
    REQUIRE(value == 0x400F87E7);

    as.RewindBuffer();

    as.JALR(x15, 1536, x31);
    REQUIRE(value == 0x600F87E7);

    as.RewindBuffer();

    as.JALR(x15, -1, x31);
    REQUIRE(value == 0xFFFF87E7);
}

TEST_CASE("LB", "[rv32i]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.LB(x15, 1024, x31);
    REQUIRE(value == 0x400F8783);

    as.RewindBuffer();

    as.LB(x15, 1536, x31);
    REQUIRE(value == 0x600F8783);

    as.RewindBuffer();

    as.LB(x15, -1, x31);
    REQUIRE(value == 0xFFFF8783);
}

TEST_CASE("LBU", "[rv32i]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.LBU(x15, 1024, x31);
    REQUIRE(value == 0x400FC783);

    as.RewindBuffer();

    as.LBU(x15, 1536, x31);
    REQUIRE(value == 0x600FC783);

    as.RewindBuffer();

    as.LBU(x15, -1, x31);
    REQUIRE(value == 0xFFFFC783);
}

TEST_CASE("LH", "[rv32i]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.LH(x15, 1024, x31);
    REQUIRE(value == 0x400F9783);

    as.RewindBuffer();

    as.LH(x15, 1536, x31);
    REQUIRE(value == 0x600F9783);

    as.RewindBuffer();

    as.LH(x15, -1, x31);
    REQUIRE(value == 0xFFFF9783);
}

TEST_CASE("LHU", "[rv32i]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.LHU(x15, 1024, x31);
    REQUIRE(value == 0x400FD783);

    as.RewindBuffer();

    as.LHU(x15, 1536, x31);
    REQUIRE(value == 0x600FD783);

    as.RewindBuffer();

    as.LHU(x15, -1, x31);
    REQUIRE(value == 0xFFFFD783);
}

TEST_CASE("LI", "[rv32i]") {
    std::array<uint32_t, 2> vals{};
    auto as = MakeAssembler32(vals);

    const auto compare_vals = [&vals](uint32_t val_1, uint32_t val_2) {
        REQUIRE(vals[0] == val_1);
        REQUIRE(vals[1] == val_2);
    };

    ///////// Single ADDI cases

    as.LI(x1, 0);
    // addi x1, x0, 0
    compare_vals(0x00000093, 0x00000000);
    as.RewindBuffer();
    vals = {};

    as.LI(x1, uint64_t(-1));
    // addi x1, x0, -1
    compare_vals(0xFFF00093, 0x00000000);
    as.RewindBuffer();
    vals = {};

    as.LI(x1, 42);
    // addi x1, x0, 42
    compare_vals(0x02A00093, 0x000000000);
    as.RewindBuffer();
    vals = {};

    as.LI(x1, 0x7ff);
    // addi x1, x0, 2047
    compare_vals(0x7FF00093, 0x00000000);
    as.RewindBuffer();
    vals = {};

    ///////// Single LUI cases

    as.LI(x1, 0x2A000);
    // lui x1, 42
    compare_vals(0x0002A0B7, 0x00000000);
    as.RewindBuffer();
    vals = {};

    as.LI(x1, ~0xFFFULL);
    // lui x1, -1
    compare_vals(0xFFFFF0B7, 0x00000000);
    as.RewindBuffer();
    vals = {};

    as.LI(x1, uint64_t(INT32_MIN));
    // lui x1, -524288
    compare_vals(0x800000B7, 0x00000000);
    as.RewindBuffer();
    vals = {};

    ///////// Full LUI+ADDI cases

    as.LI(x1, 0x11111111);
    // lui x1, 69905
    // addi x1, x1, 273
    compare_vals(0x111110B7, 0x11108093);
    as.RewindBuffer();
    vals = {};

    as.LI(x1, INT32_MAX);
    // lui x1, -524288
    // addi x1, x1, -1
    compare_vals(0x800000B7, 0xFFF08093);
}

TEST_CASE("LUI", "[rv32i]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.LUI(x10, 0xFFFFFFFF);
    REQUIRE(value == 0xFFFFF537);

    as.RewindBuffer();

    as.LUI(x10, 0xFFF7FFFF);
    REQUIRE(value == 0x7FFFF537);

    as.RewindBuffer();

    as.LUI(x31, 0xFFFFFFFF);
    REQUIRE(value == 0xFFFFFFB7);
}

TEST_CASE("LW", "[rv32i]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.LW(x15, 1024, x31);
    REQUIRE(value == 0x400FA783);

    as.RewindBuffer();

    as.LW(x15, 1536, x31);
    REQUIRE(value == 0x600FA783);

    as.RewindBuffer();

    as.LW(x15, -1, x31);
    REQUIRE(value == 0xFFFFA783);
}

TEST_CASE("OR", "[rv32i]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.OR(x7, x15, x31);
    REQUIRE(value == 0x01F7E3B3);

    as.RewindBuffer();

    as.OR(x31, x31, x31);
    REQUIRE(value == 0x01FFEFB3);

    as.RewindBuffer();

    as.OR(x0, x0, x0);
    REQUIRE(value == 0x00006033);
}

TEST_CASE("ORI", "[rv32i]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.ORI(x15, x31, 1024);
    REQUIRE(value == 0x400FE793);

    as.RewindBuffer();

    as.ORI(x15, x31, 2048);
    REQUIRE(value == 0x800FE793);

    as.RewindBuffer();

    as.ORI(x15, x31, 4095);
    REQUIRE(value == 0xFFFFE793);
}

TEST_CASE("PAUSE", "[rv32i]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.PAUSE();
    REQUIRE(value == 0x0100000F);
}

TEST_CASE("SB", "[rv32i]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.SB(x31, 1024, x15);
    REQUIRE(value == 0x41F78023);

    as.RewindBuffer();

    as.SB(x31, 1536, x15);
    REQUIRE(value == 0x61F78023);

    as.RewindBuffer();

    as.SB(x31, -1, x15);
    REQUIRE(value == 0xFFF78FA3);
}

TEST_CASE("SH", "[rv32i]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.SH(x31, 1024, x15);
    REQUIRE(value == 0x41F79023);

    as.RewindBuffer();

    as.SH(x31, 1536, x15);
    REQUIRE(value == 0x61F79023);

    as.RewindBuffer();

    as.SH(x31, -1, x15);
    REQUIRE(value == 0xFFF79FA3);
}

TEST_CASE("SLL", "[rv32i]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.SLL(x7, x15, x31);
    REQUIRE(value == 0x01F793B3);

    as.RewindBuffer();

    as.SLL(x31, x31, x31);
    REQUIRE(value == 0x01FF9FB3);

    as.RewindBuffer();

    as.SLL(x0, x0, x0);
    REQUIRE(value == 0x00001033);
}

TEST_CASE("SLLI", "[rv32i]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.SLLI(x31, x15, 10);
    REQUIRE(value == 0x00A79F93);

    as.RewindBuffer();

    as.SLLI(x31, x15, 20);
    REQUIRE(value == 0x01479F93);

    as.RewindBuffer();

    as.SLLI(x31, x15, 31);
    REQUIRE(value == 0x01F79F93);
}

TEST_CASE("SLT", "[rv32i]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.SLT(x7, x15, x31);
    REQUIRE(value == 0x01F7A3B3);

    as.RewindBuffer();

    as.SLT(x31, x31, x31);
    REQUIRE(value == 0x01FFAFB3);

    as.RewindBuffer();

    as.SLT(x0, x0, x0);
    REQUIRE(value == 0x00002033);
}

TEST_CASE("SLTI", "[rv32i]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.SLTI(x15, x31, 1024);
    REQUIRE(value == 0x400FA793);

    as.RewindBuffer();

    as.SLTI(x15, x31, -2048);
    REQUIRE(value == 0x800FA793);

    as.RewindBuffer();

    as.SLTI(x15, x31, -1);
    REQUIRE(value == 0xFFFFA793);
}

TEST_CASE("SLTIU", "[rv32i]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.SLTIU(x15, x31, 1024);
    REQUIRE(value == 0x400FB793);

    as.RewindBuffer();

    as.SLTIU(x15, x31, -2048);
    REQUIRE(value == 0x800FB793);

    as.RewindBuffer();

    as.SLTIU(x15, x31, -1);
    REQUIRE(value == 0xFFFFB793);
}

TEST_CASE("SLTU", "[rv32i]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.SLTU(x7, x15, x31);
    REQUIRE(value == 0x01F7B3B3);

    as.RewindBuffer();

    as.SLTU(x31, x31, x31);
    REQUIRE(value == 0x01FFBFB3);

    as.RewindBuffer();

    as.SLTU(x0, x0, x0);
    REQUIRE(value == 0x00003033);
}

TEST_CASE("SRA", "[rv32i]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.SRA(x7, x15, x31);
    REQUIRE(value == 0x41F7D3B3);

    as.RewindBuffer();

    as.SRA(x31, x31, x31);
    REQUIRE(value == 0x41FFDFB3);

    as.RewindBuffer();

    as.SRA(x0, x0, x0);
    REQUIRE(value == 0x40005033);
}

TEST_CASE("SRAI", "[rv32i]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.SRAI(x31, x15, 10);
    REQUIRE(value == 0x40A7DF93);

    as.RewindBuffer();

    as.SRAI(x31, x15, 20);
    REQUIRE(value == 0x4147DF93);

    as.RewindBuffer();

    as.SRAI(x31, x15, 31);
    REQUIRE(value == 0x41F7DF93);
}

TEST_CASE("SRL", "[rv32i]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.SRL(x7, x15, x31);
    REQUIRE(value == 0x01F7D3B3);

    as.RewindBuffer();

    as.SRL(x31, x31, x31);
    REQUIRE(value == 0x01FFDFB3);

    as.RewindBuffer();

    as.SRL(x0, x0, x0);
    REQUIRE(value == 0x00005033);
}

TEST_CASE("SRLI", "[rv32i]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.SRLI(x31, x15, 10);
    REQUIRE(value == 0x00A7DF93);

    as.RewindBuffer();

    as.SRLI(x31, x15, 20);
    REQUIRE(value == 0x0147DF93);

    as.RewindBuffer();

    as.SRLI(x31, x15, 31);
    REQUIRE(value == 0x01F7DF93);
}

TEST_CASE("SUB", "[rv32i]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.SUB(x7, x15, x31);
    REQUIRE(value == 0x41F783B3);

    as.RewindBuffer();

    as.SUB(x31, x31, x31);
    REQUIRE(value == 0x41FF8FB3);

    as.RewindBuffer();

    as.SUB(x0, x0, x0);
    REQUIRE(value == 0x40000033);
}

TEST_CASE("SW", "[rv32i]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.SW(x31, 1024, x15);
    REQUIRE(value == 0x41F7A023);

    as.RewindBuffer();

    as.SW(x31, 1536, x15);
    REQUIRE(value == 0x61F7A023);

    as.RewindBuffer();

    as.SW(x31, -1, x15);
    REQUIRE(value == 0xFFF7AFA3);
}

TEST_CASE("XOR", "[rv32i]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.XOR(x7, x15, x31);
    REQUIRE(value == 0x01F7C3B3);

    as.RewindBuffer();

    as.XOR(x31, x31, x31);
    REQUIRE(value == 0x01FFCFB3);

    as.RewindBuffer();

    as.XOR(x0, x0, x0);
    REQUIRE(value == 0x00004033);
}

TEST_CASE("XORI", "[rv32i]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.XORI(x15, x31, 1024);
    REQUIRE(value == 0x400FC793);

    as.RewindBuffer();

    as.XORI(x15, x31, 2048);
    REQUIRE(value == 0x800FC793);

    as.RewindBuffer();

    as.XORI(x15, x31, 4095);
    REQUIRE(value == 0xFFFFC793);
}
