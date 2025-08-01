#include <catch/catch.hpp>

#include <biscuit/assembler.hpp>

#include "assembler_test_utils.hpp"

using namespace biscuit;

TEST_CASE("ADD to C.ADD", "[autocompress]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);
    as.EnableOptimization(Optimization::AutoCompress);

    as.ADD(x15, x15, x8);
    REQUIRE(value == 0x97A2);

    as.RewindBuffer();

    as.ADD(x15, x8, x15);
    REQUIRE(value == 0x97A2);
}

TEST_CASE("ADDW to C.ADDW", "[autocompress]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);
    as.EnableOptimization(Optimization::AutoCompress);

    as.ADDW(x15, x15, x8);
    REQUIRE(value == 0x9FA1);

    as.RewindBuffer();

    as.ADDW(x15, x8, x15);
    REQUIRE(value == 0x9FA1);
}

TEST_CASE("AND to C.AND", "[autocompress]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);
    as.EnableOptimization(Optimization::AutoCompress);

    as.AND(x15, x15, x8);
    REQUIRE(value == 0x8FE1);

    as.RewindBuffer();

    as.AND(x15, x8, x15);
    REQUIRE(value == 0x8FE1);
}

TEST_CASE("ANDI to C.ANDI", "[autocompress]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);
    as.EnableOptimization(Optimization::AutoCompress);

    as.ANDI(x15, x15, 0xFFFF'FFFE);
    REQUIRE(value == 0x9BF9);

    as.RewindBuffer();

    as.ANDI(x8, x8, 1);
    REQUIRE(value == 0x8805);
}

TEST_CASE("ADDI to C.ADDI", "[autocompress]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);
    as.EnableOptimization(Optimization::AutoCompress);

    as.ADDI(x8, x8, 31);
    REQUIRE(value == 0x047D);

    as.RewindBuffer();

    as.ADDI(x7, x7, -32);
    REQUIRE(value == 0x1381);
}

TEST_CASE("ADDI to C.ADDI16SP", "[autocompress]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);
    as.EnableOptimization(Optimization::AutoCompress);

    as.ADDI(x2, x2, 496);
    REQUIRE(value == 0x617D);

    as.RewindBuffer();

    as.ADDI(x2, x2, -512);
    REQUIRE(value == 0x7101);
}

TEST_CASE("ADDI to C.ADDI4SPN", "[autocompress]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);
    as.EnableOptimization(Optimization::AutoCompress);

    as.ADDI(x8, x2, 4);
    REQUIRE(value == 0x0040);

    as.RewindBuffer();

    as.ADDI(x15, x2, 1020);
    REQUIRE(value == 0x1FFC);
}

TEST_CASE("ADDI to C.LI", "[autocompress]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);
    as.EnableOptimization(Optimization::AutoCompress);

    as.ADDI(x7, x0, 31);
    REQUIRE(value == 0x43FD);

    as.RewindBuffer();

    as.ADDI(x7, x0, -32);
    REQUIRE(value == 0x5381);
}

TEST_CASE("ADDIW to C.ADDIW", "[autocompress]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);
    as.EnableOptimization(Optimization::AutoCompress);

    as.ADDIW(x8, x8, 31);
    REQUIRE(value == 0x247D);

    as.RewindBuffer();

    as.ADDIW(x7, x7, -32);
    REQUIRE(value == 0x3381);
}

TEST_CASE("BEQZ to C.BEQZ", "[autocompress]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);
    as.EnableOptimization(Optimization::AutoCompress);

    as.BEQ(x8, x0, -256);
    REQUIRE(value == 0xD001);

    as.RewindBuffer();

    as.BEQ(x0, x15, 254);
    REQUIRE(value == 0xCFFD);
}

TEST_CASE("BNEZ to C.BNEZ", "[autocompress]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);
    as.EnableOptimization(Optimization::AutoCompress);

    as.BNE(x8, x0, -256);
    REQUIRE(value == 0xF001);

    as.RewindBuffer();

    as.BNE(x0, x15, 254);
    REQUIRE(value == 0xEFFD);
}

TEST_CASE("FLD to C.FLD", "[autocompress]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);
    as.EnableOptimization(Optimization::AutoCompress);

    as.FLD(f8, 248, x9);
    REQUIRE(value == 0x3CE0);

    as.RewindBuffer();

    as.FLD(f8, 0, x9);
    REQUIRE(value == 0x2080);
}

TEST_CASE("FLD to C.FLDSP", "[autocompress]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);
    as.EnableOptimization(Optimization::AutoCompress);

    as.FLD(f8, 504, sp);
    REQUIRE(value == 0x347E);

    as.RewindBuffer();

    as.FLD(f7, 0, sp);
    REQUIRE(value == 0x2382);
}

TEST_CASE("FLW to C.FLW", "[autocompress]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);
    as.EnableOptimization(Optimization::AutoCompress);

    as.FLW(f8, 124, x9);
    REQUIRE(value == 0x7CE0);

    as.RewindBuffer();

    as.FLW(f8, 0, x9);
    REQUIRE(value == 0x6080);
}

TEST_CASE("FLW to C.FLWSP", "[autocompress]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);
    as.EnableOptimization(Optimization::AutoCompress);

    as.FLW(f8, 252, sp);
    REQUIRE(value == 0x747E);

    as.RewindBuffer();

    as.FLW(f7, 0, sp);
    REQUIRE(value == 0x6382);
}

TEST_CASE("FSD to C.FSD", "[autocompress]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);
    as.EnableOptimization(Optimization::AutoCompress);

    as.FSD(f8, 248, x9);
    REQUIRE(value == 0xBCE0);

    as.RewindBuffer();

    as.FSD(f8, 0, x9);
    REQUIRE(value == 0xA080);
}

TEST_CASE("FSD to C.FSDSP", "[autocompress]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);
    as.EnableOptimization(Optimization::AutoCompress);

    as.FSD(f8, 504, sp);
    REQUIRE(value == 0xBFA2);

    as.RewindBuffer();

    as.FSD(f7, 0, sp);
    REQUIRE(value == 0xA01E);
}

TEST_CASE("FSW to C.FSW", "[autocompress]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);
    as.EnableOptimization(Optimization::AutoCompress);

    as.FSW(f8, 124, x9);
    REQUIRE(value == 0xFCE0);

    as.RewindBuffer();

    as.FSW(f8, 0, x9);
    REQUIRE(value == 0xE080);
}

TEST_CASE("FSW to C.FSWSP", "[autocompress]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);
    as.EnableOptimization(Optimization::AutoCompress);

    as.FSW(f8, 252, sp);
    REQUIRE(value == 0xFFA2);

    as.RewindBuffer();

    as.FSW(f7, 0, sp);
    REQUIRE(value == 0xE01E);
}

TEST_CASE("J to C.J", "[autocompress]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);
    as.EnableOptimization(Optimization::AutoCompress);

    as.J(2044);
    REQUIRE(value == 0xAFF5);

    as.RewindBuffer();

    as.J(-2048);
    REQUIRE(value == 0xB001);
}

TEST_CASE("JAL to C.JAL", "[autocompress]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);
    as.EnableOptimization(Optimization::AutoCompress);

    as.JAL(2044);
    REQUIRE(value == 0x2FF5);

    as.RewindBuffer();

    as.JAL(-2048);
    REQUIRE(value == 0x3001);
}

TEST_CASE("JR to C.JR", "[autocompress]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);
    as.EnableOptimization(Optimization::AutoCompress);

    as.JR(x7);
    REQUIRE(value == 0x8382);

    as.RewindBuffer();

    as.JR(x15);
    REQUIRE(value == 0x8782);
}

TEST_CASE("JALR to C.JALR", "[autocompress]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);
    as.EnableOptimization(Optimization::AutoCompress);

    as.JALR(x7);
    REQUIRE(value == 0x9382);

    as.RewindBuffer();

    as.JALR(x15);
    REQUIRE(value == 0x9782);
}

TEST_CASE("LD to C.LD", "[autocompress]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);
    as.EnableOptimization(Optimization::AutoCompress);

    as.LD(x8, 248, x9);
    REQUIRE(value == 0x7CE0);

    as.RewindBuffer();

    as.LD(x8, 0, x9);
    REQUIRE(value == 0x6080);
}

TEST_CASE("LD to C.LDSP", "[autocompress]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);
    as.EnableOptimization(Optimization::AutoCompress);

    as.LD(x8, 504, sp);
    REQUIRE(value == 0x747E);

    as.RewindBuffer();

    as.LD(x7, 0, sp);
    REQUIRE(value == 0x6382);
}

TEST_CASE("LW to C.LW", "[autocompress]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);
    as.EnableOptimization(Optimization::AutoCompress);

    as.LW(x8, 124, x9);
    REQUIRE(value == 0x5CE0);

    as.RewindBuffer();

    as.LW(x8, 0, x9);
    REQUIRE(value == 0x4080);
}

TEST_CASE("LW to C.LWSP", "[autocompress]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);
    as.EnableOptimization(Optimization::AutoCompress);

    as.LW(x8, 252, sp);
    REQUIRE(value == 0x547E);

    as.RewindBuffer();

    as.LW(x7, 0, sp);
    REQUIRE(value == 0x4382);
}

TEST_CASE("LUI to C.LUI", "[autocompress]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);
    as.EnableOptimization(Optimization::AutoCompress);

    as.LUI(x7, -1);
    REQUIRE(value == 0x73FD);

    as.RewindBuffer();

    as.LUI(x7, 1);
    REQUIRE(value == 0x6385);
}

TEST_CASE("MV to C.MV", "[autocompress]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);
    as.EnableOptimization(Optimization::AutoCompress);

    as.MV(x15, x8);
    REQUIRE(value == 0x87A2);

    as.RewindBuffer();

    as.MV(x2, x1);
    REQUIRE(value == 0x8106);
}

TEST_CASE("OR to C.OR", "[autocompress]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);
    as.EnableOptimization(Optimization::AutoCompress);

    as.OR(x15, x15, x8);
    REQUIRE(value == 0x8FC1);

    as.RewindBuffer();

    as.OR(x15, x8, x15);
    REQUIRE(value == 0x8FC1);
}

TEST_CASE("SD to C.SD", "[autocompress]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);
    as.EnableOptimization(Optimization::AutoCompress);

    as.SD(x8, 248, x9);
    REQUIRE(value == 0xFCE0);

    as.RewindBuffer();

    as.SD(x8, 0, x9);
    REQUIRE(value == 0xE080);
}

TEST_CASE("SD to C.SDSP", "[autocompress]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);
    as.EnableOptimization(Optimization::AutoCompress);

    as.SD(x8, 504, sp);
    REQUIRE(value == 0xFFA2);

    as.RewindBuffer();

    as.SD(x7, 0, sp);
    REQUIRE(value == 0xE01E);
}

TEST_CASE("SLLI to C.SLLI", "[autocompress]") {
    {
        uint32_t value = 0;
        auto as = MakeAssembler32(value);
        as.EnableOptimization(Optimization::AutoCompress);

        as.SLLI(x7, x7, 31);
        REQUIRE(value == 0x03FE);
    }

    {
        uint32_t value = 0;
        auto as = MakeAssembler64(value);
        as.EnableOptimization(Optimization::AutoCompress);

        as.SLLI(x7, x7, 63);
        REQUIRE(value == 0x13FE);
    }
}

TEST_CASE("SRAI to C.SRAI", "[autocompress]") {
    {
        uint32_t value = 0;
        auto as = MakeAssembler32(value);
        as.EnableOptimization(Optimization::AutoCompress);

        as.SRAI(x8, x8, 31);
        REQUIRE(value == 0x847D);
    }

    {
        uint32_t value = 0;
        auto as = MakeAssembler64(value);
        as.EnableOptimization(Optimization::AutoCompress);

        as.SRAI(x8, x8, 63);
        REQUIRE(value == 0x947D);
    }
}

TEST_CASE("SRLI to C.SRLI", "[autocompress]") {
    {
        uint32_t value = 0;
        auto as = MakeAssembler32(value);
        as.EnableOptimization(Optimization::AutoCompress);

        as.SRLI(x8, x8, 31);
        REQUIRE(value == 0x807D);
    }

    {
        uint32_t value = 0;
        auto as = MakeAssembler64(value);
        as.EnableOptimization(Optimization::AutoCompress);

        as.SRLI(x8, x8, 63);
        REQUIRE(value == 0x907D);
    }
}

TEST_CASE("SUB to C.SUB", "[autocompress]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);
    as.EnableOptimization(Optimization::AutoCompress);

    as.SUB(x15, x15, x8);
    REQUIRE(value == 0x8F81);
}

TEST_CASE("SUBW to C.SUBW", "[autocompress]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);
    as.EnableOptimization(Optimization::AutoCompress);

    as.SUBW(x15, x15, x8);
    REQUIRE(value == 0x9F81);
}

TEST_CASE("SW to C.SW", "[autocompress]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);
    as.EnableOptimization(Optimization::AutoCompress);

    as.SW(x8, 124, x9);
    REQUIRE(value == 0xDCE0);

    as.RewindBuffer();

    as.SW(x8, 0, x9);
    REQUIRE(value == 0xC080);
}

TEST_CASE("SW to C.SWSP", "[autocompress]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);
    as.EnableOptimization(Optimization::AutoCompress);

    as.SW(x8, 252, sp);
    REQUIRE(value == 0xDFA2);

    as.RewindBuffer();

    as.SW(x7, 0, sp);
    REQUIRE(value == 0xC01E);
}

TEST_CASE("XOR to C.XOR", "[autocompress]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);
    as.EnableOptimization(Optimization::AutoCompress);

    as.XOR(x15, x15, x8);
    REQUIRE(value == 0x8FA1);

    as.RewindBuffer();

    as.XOR(x15, x8, x15);
    REQUIRE(value == 0x8FA1);
}