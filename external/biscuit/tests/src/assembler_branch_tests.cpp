#include <catch/catch.hpp>

#include <array>
#include <biscuit/assembler.hpp>

#include "assembler_test_utils.hpp"

using namespace biscuit;

TEST_CASE("Branch to Self", "[branch]") {
    uint32_t data;
    auto as = MakeAssembler32(data);

    // Simple branch to self with a jump instruction.
    {
        Label label;
        as.Bind(&label);
        as.J(&label);
        REQUIRE(data == 0x0000006F);
    }

    as.RewindBuffer();

    // Simple branch to self with a compressed jump instruction.
    {
        Label label;
        as.Bind(&label);
        as.C_J(&label);
        REQUIRE((data & 0xFFFF) == 0xA001);
    }

    as.RewindBuffer();

    // Simple branch to self with a conditional branch instruction.
    {
        Label label;
        as.Bind(&label);
        as.BNE(x3, x4, &label);
        REQUIRE(data == 0x00419063);
    }

    as.RewindBuffer();

    // Simple branch to self with a compressed branch instruction.
    {
        Label label;
        as.Bind(&label);
        as.C_BNEZ(x15, &label);
        REQUIRE((data & 0xFFFF) == 0xE381);
    }
}

TEST_CASE("Branch with Instructions Between", "[branch]") {
    std::array<uint32_t, 20> data{};
    auto as = MakeAssembler32(data);

    // Simple branch backward
    {
        Label label;
        as.Bind(&label);
        as.ADD(x1, x2, x3);
        as.SUB(x2, x4, x3);
        as.J(&label);
        REQUIRE(data[2] == 0xFF9FF06F);
    }

    as.RewindBuffer();
    data.fill(0);

    // Simple branch forward
    {
        Label label;
        as.J(&label);
        as.ADD(x1, x2, x3);
        as.SUB(x2, x4, x3);
        as.Bind(&label);
        REQUIRE(data[0] == 0x00C0006F);
    }

    as.RewindBuffer();
    data.fill(0);

    // Simple branch backward (compressed)
    {
        Label label;
        as.Bind(&label);
        as.ADD(x1, x2, x3);
        as.SUB(x2, x4, x3);
        as.C_J(&label);
        REQUIRE((data[2] & 0xFFFF) == 0xBFE5);
    }

    as.RewindBuffer();
    data.fill(0);

    // Simple branch forward (compressed)
    {
        Label label;
        as.C_J(&label);
        as.ADD(x1, x2, x3);
        as.SUB(x2, x4, x3);
        as.Bind(&label);
        REQUIRE((data[0] & 0xFFFF) == 0xA029);
    }
}

TEST_CASE("LI label forward", "[label]") {
    {
        std::array<uint32_t, 20> data{};
        auto as = MakeAssembler32(data);
        Label label;
        as.LILabel(t0, &label);
        // Should emit AUIPC+ADDI with offset == 24
        as.NOP();
        as.NOP();
        as.NOP();
        as.NOP();
        as.Bind(&label);

        std::array<uint32_t, 2> expected_data{};
        auto tas = MakeAssembler32(expected_data);
        tas.AUIPC(t0, 0);
        tas.ADDI(t0, t0, 24);
        REQUIRE(data[0] == expected_data[0]);
        REQUIRE(data[1] == expected_data[1]);
    }

    {
        std::vector<uint32_t> data;
        data.resize(0x10000);
        auto as = Assembler(reinterpret_cast<uint8_t*>(data.data()), data.size(), ArchFeature::RV32);
        Label label;
        as.LILabel(t0, &label);
        // Bind it far away
        as.SetCursorPointer(as.GetCursorPointer() + 0xF000);
        as.Bind(&label);

        std::array<uint32_t, 2> expected_data{};
        auto tas = MakeAssembler32(expected_data);
        tas.AUIPC(t0, 0xF);
        tas.ADDI(t0, t0, 0x8);
        REQUIRE(data[0] == expected_data[0]);
        REQUIRE(data[1] == expected_data[1]);
    }
}

TEST_CASE("LI label backward", "[label]") {
    {
        std::array<uint32_t, 20> data{};
        auto as = MakeAssembler32(data);

        Label label;
        as.Bind(&label);
        as.NOP();
        as.NOP();
        as.NOP();
        as.NOP();
        as.NOP();
        as.LILabel(t0, &label);
        // Should emit AUIPC+ADDI with offset == -20

        std::array<uint32_t, 2> expected_data{};
        auto tas = MakeAssembler32(expected_data);
        tas.AUIPC(t0, 0);
        tas.ADDI(t0, t0, -20);
        REQUIRE(data[5] == expected_data[0]);
        REQUIRE(data[6] == expected_data[1]);
    }

    {
        std::vector<uint32_t> data;
        data.resize(0x10000);
        auto as = Assembler(reinterpret_cast<uint8_t*>(data.data()), data.size(), ArchFeature::RV32);
        // Bind it far away
        Label label;
        as.Bind(&label);
        as.SetCursorPointer(as.GetCursorPointer() + 0xF004);
        uint32_t* instructions = reinterpret_cast<uint32_t*>(as.GetCursorPointer());
        as.LILabel(t0, &label);

        std::array<uint32_t, 2> expected_data{};
        auto tas = MakeAssembler32(expected_data);
        tas.AUIPC(t0, -15);
        tas.ADDI(t0, t0, -4);
        REQUIRE(instructions[0] == expected_data[0]);
        REQUIRE(instructions[1] == expected_data[1]);
    }
}