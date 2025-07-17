#include <biscuit/assembler.hpp>
#include <biscuit/assert.hpp>

#include <array>
#include <bit>
#include <cstring>
#include <utility>

#include "assembler_util.hpp"

namespace biscuit {

Assembler::Assembler(size_t capacity) : m_buffer(capacity) {}

Assembler::Assembler(uint8_t* buffer, size_t capacity, ArchFeature features) : m_buffer(buffer, capacity), m_features{features} {}

Assembler::~Assembler() = default;

CodeBuffer& Assembler::GetCodeBuffer() {
    return m_buffer;
}

CodeBuffer Assembler::SwapCodeBuffer(CodeBuffer&& buffer) noexcept {
    return std::exchange(m_buffer, std::move(buffer));
}

void Assembler::Bind(Label* label) {
    BindToOffset(label, m_buffer.GetCursorOffset());
}

void Assembler::ADD(GPR rd, GPR lhs, GPR rhs) noexcept {
    EmitRType(m_buffer, 0b0000000, rhs, lhs, 0b000, rd, 0b0110011);
}

void Assembler::ADDI(GPR rd, GPR rs, int32_t imm) noexcept {
    EmitIType(m_buffer, static_cast<uint32_t>(imm), rs, 0b000, rd, 0b0010011);
}

void Assembler::AND(GPR rd, GPR lhs, GPR rhs) noexcept {
    EmitRType(m_buffer, 0b0000000, rhs, lhs, 0b111, rd, 0b0110011);
}

void Assembler::ANDI(GPR rd, GPR rs, uint32_t imm) noexcept {
    EmitIType(m_buffer, imm, rs, 0b111, rd, 0b0010011);
}

void Assembler::AUIPC(GPR rd, int32_t imm) noexcept {
    EmitUType(m_buffer, static_cast<uint32_t>(imm), rd, 0b0010111);
}

void Assembler::BEQ(GPR rs1, GPR rs2, Label* label) noexcept {
    const auto address = LinkAndGetOffset(label);
    BEQ(rs1, rs2, static_cast<int32_t>(address));
}

void Assembler::BEQZ(GPR rs, Label* label) noexcept {
    const auto address = LinkAndGetOffset(label);
    BEQZ(rs, static_cast<int32_t>(address));
}

void Assembler::BGE(GPR rs1, GPR rs2, Label* label) noexcept {
    const auto address = LinkAndGetOffset(label);
    BGE(rs1, rs2, static_cast<int32_t>(address));
}

void Assembler::BGEU(GPR rs1, GPR rs2, Label* label) noexcept {
    const auto address = LinkAndGetOffset(label);
    BGEU(rs1, rs2, static_cast<int32_t>(address));
}

void Assembler::BGEZ(GPR rs, Label* label) noexcept {
    const auto address = LinkAndGetOffset(label);
    BGEZ(rs, static_cast<int32_t>(address));
}

void Assembler::BGT(GPR rs, GPR rt, Label* label) noexcept {
    const auto address = LinkAndGetOffset(label);
    BGT(rs, rt, static_cast<int32_t>(address));
}

void Assembler::BGTU(GPR rs, GPR rt, Label* label) noexcept {
    const auto address = LinkAndGetOffset(label);
    BGTU(rs, rt, static_cast<int32_t>(address));
}

void Assembler::BGTZ(GPR rs, Label* label) noexcept {
    const auto address = LinkAndGetOffset(label);
    BGTZ(rs, static_cast<int32_t>(address));
}

void Assembler::BLE(GPR rs, GPR rt, Label* label) noexcept {
    const auto address = LinkAndGetOffset(label);
    BLE(rs, rt, static_cast<int32_t>(address));
}

void Assembler::BLEU(GPR rs, GPR rt, Label* label) noexcept {
    const auto address = LinkAndGetOffset(label);
    BLEU(rs, rt, static_cast<int32_t>(address));
}

void Assembler::BLEZ(GPR rs, Label* label) noexcept {
    const auto address = LinkAndGetOffset(label);
    BLEZ(rs, static_cast<int32_t>(address));
}

void Assembler::BLT(GPR rs1, GPR rs2, Label* label) noexcept {
    const auto address = LinkAndGetOffset(label);
    BLT(rs1, rs2, static_cast<int32_t>(address));
}

void Assembler::BLTU(GPR rs1, GPR rs2, Label* label) noexcept {
    const auto address = LinkAndGetOffset(label);
    BLTU(rs1, rs2, static_cast<int32_t>(address));
}

void Assembler::BLTZ(GPR rs, Label* label) noexcept {
    const auto address = LinkAndGetOffset(label);
    BLTZ(rs, static_cast<int32_t>(address));
}

void Assembler::BNE(GPR rs1, GPR rs2, Label* label) noexcept {
    const auto address = LinkAndGetOffset(label);
    BNE(rs1, rs2, static_cast<int32_t>(address));
}

void Assembler::BNEZ(GPR rs, Label* label) noexcept {
    const auto address = LinkAndGetOffset(label);
    BNEZ(rs, static_cast<int32_t>(address));
}

void Assembler::BEQ(GPR rs1, GPR rs2, int32_t imm) noexcept {
    BISCUIT_ASSERT(IsValidBTypeImm(imm));
    EmitBType(m_buffer, static_cast<uint32_t>(imm), rs2, rs1, 0b000, 0b1100011);
}

void Assembler::BEQZ(GPR rs, int32_t imm) noexcept {
    BEQ(rs, x0, imm);
}

void Assembler::BGE(GPR rs1, GPR rs2, int32_t imm) noexcept {
    BISCUIT_ASSERT(IsValidBTypeImm(imm));
    EmitBType(m_buffer, static_cast<uint32_t>(imm), rs2, rs1, 0b101, 0b1100011);
}

void Assembler::BGEU(GPR rs1, GPR rs2, int32_t imm) noexcept {
    BISCUIT_ASSERT(IsValidBTypeImm(imm));
    EmitBType(m_buffer, static_cast<uint32_t>(imm), rs2, rs1, 0b111, 0b1100011);
}

void Assembler::BGEZ(GPR rs, int32_t imm) noexcept {
    BGE(rs, x0, imm);
}

void Assembler::BGT(GPR rs, GPR rt, int32_t imm) noexcept {
    BLT(rt, rs, imm);
}

void Assembler::BGTU(GPR rs, GPR rt, int32_t imm) noexcept {
    BLTU(rt, rs, imm);
}

void Assembler::BGTZ(GPR rs, int32_t imm) noexcept {
    BLT(x0, rs, imm);
}

void Assembler::BLE(GPR rs, GPR rt, int32_t imm) noexcept {
    BGE(rt, rs, imm);
}

void Assembler::BLEU(GPR rs, GPR rt, int32_t imm) noexcept {
    BGEU(rt, rs, imm);
}

void Assembler::BLEZ(GPR rs, int32_t imm) noexcept {
    BGE(x0, rs, imm);
}

void Assembler::BLT(GPR rs1, GPR rs2, int32_t imm) noexcept {
    BISCUIT_ASSERT(IsValidBTypeImm(imm));
    EmitBType(m_buffer, static_cast<uint32_t>(imm), rs2, rs1, 0b100, 0b1100011);
}

void Assembler::BLTU(GPR rs1, GPR rs2, int32_t imm) noexcept {
    BISCUIT_ASSERT(IsValidBTypeImm(imm));
    EmitBType(m_buffer, static_cast<uint32_t>(imm), rs2, rs1, 0b110, 0b1100011);
}

void Assembler::BLTZ(GPR rs, int32_t imm) noexcept {
    BLT(rs, x0, imm);
}

void Assembler::BNE(GPR rs1, GPR rs2, int32_t imm) noexcept {
    BISCUIT_ASSERT(IsValidBTypeImm(imm));
    EmitBType(m_buffer, static_cast<uint32_t>(imm), rs2, rs1, 0b001, 0b1100011);
}

void Assembler::BNEZ(GPR rs, int32_t imm) noexcept {
    BNE(x0, rs, imm);
}

void Assembler::CALL(int32_t offset) noexcept {
    const auto uimm = static_cast<uint32_t>(offset);
    const auto lower = uimm & 0xFFF;
    const auto upper = (uimm & 0xFFFFF000) >> 12;
    const auto needs_increment = (uimm & 0x800) != 0;

    // Sign-extend the lower portion if the MSB of it is set.
    const auto new_lower = needs_increment ? static_cast<int32_t>(lower << 20) >> 20 : static_cast<int32_t>(lower);
    const auto new_upper = needs_increment ? upper + 1 : upper;

    AUIPC(x1, static_cast<int32_t>(new_upper));
    JALR(x1, new_lower, x1);
}

void Assembler::EBREAK() noexcept {
    m_buffer.Emit32(0x00100073);
}

void Assembler::ECALL() noexcept {
    m_buffer.Emit32(0x00000073);
}

void Assembler::FENCE() noexcept {
    FENCE(FenceOrder::IORW, FenceOrder::IORW);
}

void Assembler::FENCE(FenceOrder pred, FenceOrder succ) noexcept {
    EmitFENCE(m_buffer, 0b0000, pred, succ, x0, 0b000, x0, 0b0001111);
}

void Assembler::FENCEI(GPR rd, GPR rs, uint32_t imm) noexcept {
    m_buffer.Emit32(((imm & 0xFFF) << 20) | (rs.Index() << 15) | 0x1000U | (rd.Index() << 7) | 0b0001111);
}

void Assembler::FENCETSO() noexcept {
    EmitFENCE(m_buffer, 0b1000, FenceOrder::RW, FenceOrder::RW, x0, 0b000, x0, 0b0001111);
}

void Assembler::J(Label* label) noexcept {
    const auto address = LinkAndGetOffset(label);
    BISCUIT_ASSERT(IsValidJTypeImm(address));
    J(static_cast<int32_t>(address));
}

void Assembler::JAL(Label* label) noexcept {
    const auto address = LinkAndGetOffset(label);
    BISCUIT_ASSERT(IsValidJTypeImm(address));
    JAL(static_cast<int32_t>(address));
}

void Assembler::JAL(GPR rd, Label* label) noexcept {
    const auto address = LinkAndGetOffset(label);
    BISCUIT_ASSERT(IsValidJTypeImm(address));
    JAL(rd, static_cast<int32_t>(address));
}

void Assembler::J(int32_t imm) noexcept {
    BISCUIT_ASSERT(IsValidJTypeImm(imm));
    JAL(x0, imm);
}

void Assembler::JAL(int32_t imm) noexcept {
    BISCUIT_ASSERT(IsValidJTypeImm(imm));
    EmitJType(m_buffer, static_cast<uint32_t>(imm), x1, 0b1101111);
}

void Assembler::JAL(GPR rd, int32_t imm) noexcept {
    BISCUIT_ASSERT(IsValidJTypeImm(imm));
    EmitJType(m_buffer, static_cast<uint32_t>(imm), rd, 0b1101111);
}

void Assembler::JALR(GPR rs) noexcept {
    JALR(x1, 0, rs);
}

void Assembler::JALR(GPR rd, int32_t imm, GPR rs1) noexcept {
    BISCUIT_ASSERT(IsValidSigned12BitImm(imm));
    EmitIType(m_buffer, static_cast<uint32_t>(imm), rs1, 0b000, rd, 0b1100111);
}

void Assembler::JR(GPR rs) noexcept {
    JALR(x0, 0, rs);
}

void Assembler::JR(GPR rs, int32_t imm) noexcept {
    JALR(x0, imm, rs);
}

void Assembler::LB(GPR rd, int32_t imm, GPR rs) noexcept {
    BISCUIT_ASSERT(IsValidSigned12BitImm(imm));
    EmitIType(m_buffer, static_cast<uint32_t>(imm), rs, 0b000, rd, 0b0000011);
}

void Assembler::LBU(GPR rd, int32_t imm, GPR rs) noexcept {
    BISCUIT_ASSERT(IsValidSigned12BitImm(imm));
    EmitIType(m_buffer, static_cast<uint32_t>(imm), rs, 0b100, rd, 0b0000011);
}

void Assembler::LH(GPR rd, int32_t imm, GPR rs) noexcept {
    BISCUIT_ASSERT(IsValidSigned12BitImm(imm));
    EmitIType(m_buffer, static_cast<uint32_t>(imm), rs, 0b001, rd, 0b0000011);
}

void Assembler::LHU(GPR rd, int32_t imm, GPR rs) noexcept {
    BISCUIT_ASSERT(IsValidSigned12BitImm(imm));
    EmitIType(m_buffer, static_cast<uint32_t>(imm), rs, 0b101, rd, 0b0000011);
}

void Assembler::LI(GPR rd, uint64_t imm) noexcept {
    if (IsRV32(m_features)) {
        // Depending on imm, the following instructions are emitted.
        // hi20 == 0              -> ADDI
        // lo12 == 0 && hi20 != 0 -> LUI
        // otherwise              -> LUI+ADDI

        // Add 0x800 to cancel out the signed extension of ADDI.
        const auto uimm32 = static_cast<uint32_t>(imm);
        const auto hi20 = (uimm32 + 0x800) >> 12 & 0xFFFFF;
        const auto lo12 = static_cast<int32_t>(uimm32) & 0xFFF;
        GPR rs1 = zero;

        if (hi20 != 0) {
            LUI(rd, hi20);
            rs1 = rd;
        }

        if (lo12 != 0 || hi20 == 0) {
            ADDI(rd, rs1, lo12);
        }
    } else {
        // For 64-bit imm, a sequence of up to 8 instructions (i.e. LUI+ADDIW+SLLI+
        // ADDI+SLLI+ADDI+SLLI+ADDI) is emitted.
        // In the following, imm is processed from LSB to MSB while instruction emission
        // is performed from MSB to LSB by calling LI() recursively. In each recursion,
        // the lowest 12 bits are removed from imm and the optimal shift amount is
        // calculated. Then, the remaining part of imm is processed recursively and
        // LI() get called as soon as it fits into 32 bits.

        if (static_cast<uint64_t>(static_cast<int64_t>(imm << 32) >> 32) == imm) {
            // Depending on imm, the following instructions are emitted.
            // hi20 == 0              -> ADDIW
            // lo12 == 0 && hi20 != 0 -> LUI
            // otherwise              -> LUI+ADDIW

            // Add 0x800 to cancel out the signed extension of ADDIW.
            const auto hi20 = (static_cast<uint32_t>(imm) + 0x800) >> 12 & 0xFFFFF;
            const auto lo12 = static_cast<int32_t>(imm) & 0xFFF;
            GPR rs1 = zero;

            if (hi20 != 0) {
                LUI(rd, hi20);
                rs1 = rd;
            }

            if (lo12 != 0 || hi20 == 0) {
                ADDIW(rd, rs1, lo12);
            }
            return;
        }

        const auto lo12 = static_cast<int32_t>(static_cast<int64_t>(imm << 52) >> 52);
        // Add 0x800 to cancel out the signed extension of ADDI.
        uint64_t hi52 = (imm + 0x800) >> 12;
        const uint32_t shift = 12 + static_cast<uint32_t>(std::countr_zero(hi52));
        hi52 = static_cast<uint64_t>((static_cast<int64_t>(hi52 >> (shift - 12)) << shift) >> shift);
        LI(rd, hi52);
        SLLI(rd, rd, shift);
        if (lo12 != 0) {
            ADDI(rd, rd, lo12);
        }
    }
}

void Assembler::LUI(GPR rd, uint32_t imm) noexcept {
    EmitUType(m_buffer, imm, rd, 0b0110111);
}

void Assembler::LW(GPR rd, int32_t imm, GPR rs) noexcept {
    BISCUIT_ASSERT(IsValidSigned12BitImm(imm));
    EmitIType(m_buffer, static_cast<uint32_t>(imm), rs, 0b010, rd, 0b0000011);
}

void Assembler::MV(GPR rd, GPR rs) noexcept {
    ADDI(rd, rs, 0);
}

void Assembler::NEG(GPR rd, GPR rs) noexcept {
    SUB(rd, x0, rs);
}

void Assembler::NOP() noexcept {
    ADDI(x0, x0, 0);
}

void Assembler::NOT(GPR rd, GPR rs) noexcept {
    XORI(rd, rs, UINT32_MAX);
}

void Assembler::OR(GPR rd, GPR lhs, GPR rhs) noexcept {
    EmitRType(m_buffer, 0b0000000, rhs, lhs, 0b110, rd, 0b0110011);
}

void Assembler::ORI(GPR rd, GPR rs, uint32_t imm) noexcept {
    EmitIType(m_buffer, imm, rs, 0b110, rd, 0b0010011);
}

void Assembler::PAUSE() noexcept {
    m_buffer.Emit32(0x0100000F);
}

void Assembler::RET() noexcept {
    JALR(x0, 0, x1);
}

void Assembler::SB(GPR rs2, int32_t imm, GPR rs1) noexcept {
    BISCUIT_ASSERT(IsValidSigned12BitImm(imm));
    EmitSType(m_buffer, static_cast<uint32_t>(imm), rs2, rs1, 0b000, 0b0100011);
}

void Assembler::SEQZ(GPR rd, GPR rs) noexcept {
    SLTIU(rd, rs, 1);
}

void Assembler::SGT(GPR rd, GPR lhs, GPR rhs) noexcept {
    SLT(rd, rhs, lhs);
}

void Assembler::SGTU(GPR rd, GPR lhs, GPR rhs) noexcept {
    SLTU(rd, rhs, lhs);
}

void Assembler::SGTZ(GPR rd, GPR rs) noexcept {
    SLT(rd, x0, rs);
}

void Assembler::SH(GPR rs2, int32_t imm, GPR rs1) noexcept {
    BISCUIT_ASSERT(IsValidSigned12BitImm(imm));
    EmitSType(m_buffer, static_cast<uint32_t>(imm), rs2, rs1, 0b001, 0b0100011);
}

void Assembler::SLL(GPR rd, GPR lhs, GPR rhs) noexcept {
    EmitRType(m_buffer, 0b0000000, rhs, lhs, 0b001, rd, 0b0110011);
}

void Assembler::SLLI(GPR rd, GPR rs, uint32_t shift) noexcept {
    if (IsRV32(m_features)) {
        BISCUIT_ASSERT(shift <= 31);
        EmitIType(m_buffer, shift & 0x1F, rs, 0b001, rd, 0b0010011);
    } else {
        BISCUIT_ASSERT(shift <= 63);
        EmitIType(m_buffer, shift & 0x3F, rs, 0b001, rd, 0b0010011);
    }
}

void Assembler::SLT(GPR rd, GPR lhs, GPR rhs) noexcept {
    EmitRType(m_buffer, 0b0000000, rhs, lhs, 0b010, rd, 0b0110011);
}

void Assembler::SLTI(GPR rd, GPR rs, int32_t imm) noexcept {
    BISCUIT_ASSERT(IsValidSigned12BitImm(imm));
    EmitIType(m_buffer, static_cast<uint32_t>(imm), rs, 0b010, rd, 0b0010011);
}

void Assembler::SLTIU(GPR rd, GPR rs, int32_t imm) noexcept {
    BISCUIT_ASSERT(IsValidSigned12BitImm(imm));
    EmitIType(m_buffer, static_cast<uint32_t>(imm), rs, 0b011, rd, 0b0010011);
}

void Assembler::SLTU(GPR rd, GPR lhs, GPR rhs) noexcept {
    EmitRType(m_buffer, 0b0000000, rhs, lhs, 0b011, rd, 0b0110011);
}

void Assembler::SLTZ(GPR rd, GPR rs) noexcept {
    SLT(rd, rs, x0);
}

void Assembler::SNEZ(GPR rd, GPR rs) noexcept {
    SLTU(rd, x0, rs);
}

void Assembler::SRA(GPR rd, GPR lhs, GPR rhs) noexcept {
    EmitRType(m_buffer, 0b0100000, rhs, lhs, 0b101, rd, 0b0110011);
}

void Assembler::SRAI(GPR rd, GPR rs, uint32_t shift) noexcept {
    if (IsRV32(m_features)) {
        BISCUIT_ASSERT(shift <= 31);
        EmitIType(m_buffer, (0b0100000 << 5) | (shift & 0x1F), rs, 0b101, rd, 0b0010011);
    } else {
        BISCUIT_ASSERT(shift <= 63);
        EmitIType(m_buffer, (0b0100000 << 5) | (shift & 0x3F), rs, 0b101, rd, 0b0010011);
    }
}

void Assembler::SRL(GPR rd, GPR lhs, GPR rhs) noexcept {
    EmitRType(m_buffer, 0b0000000, rhs, lhs, 0b101, rd, 0b0110011);
}

void Assembler::SRLI(GPR rd, GPR rs, uint32_t shift) noexcept {
    if (IsRV32(m_features)) {
        BISCUIT_ASSERT(shift <= 31);
        EmitIType(m_buffer, shift & 0x1F, rs, 0b101, rd, 0b0010011);
    } else {
        BISCUIT_ASSERT(shift <= 63);
        EmitIType(m_buffer, shift & 0x3F, rs, 0b101, rd, 0b0010011);
    }
}

void Assembler::SUB(GPR rd, GPR lhs, GPR rhs) noexcept {
    EmitRType(m_buffer, 0b0100000, rhs, lhs, 0b000, rd, 0b0110011);
}

void Assembler::SW(GPR rs2, int32_t imm, GPR rs1) noexcept {
    BISCUIT_ASSERT(IsValidSigned12BitImm(imm));
    EmitSType(m_buffer, static_cast<uint32_t>(imm), rs2, rs1, 0b010, 0b0100011);
}

void Assembler::XOR(GPR rd, GPR lhs, GPR rhs) noexcept {
    EmitRType(m_buffer, 0b0000000, rhs, lhs, 0b100, rd, 0b0110011);
}

void Assembler::XORI(GPR rd, GPR rs, uint32_t imm) noexcept {
    EmitIType(m_buffer, imm, rs, 0b100, rd, 0b0010011);
}

// RV64I Instructions

void Assembler::ADDIW(GPR rd, GPR rs, int32_t imm) noexcept {
    BISCUIT_ASSERT(IsRV64(m_features));
    EmitIType(m_buffer, static_cast<uint32_t>(imm), rs, 0b000, rd, 0b0011011);
}

void Assembler::ADDW(GPR rd, GPR lhs, GPR rhs) noexcept {
    BISCUIT_ASSERT(IsRV64(m_features));
    EmitRType(m_buffer, 0b0000000, rhs, lhs, 0b000, rd, 0b0111011);
}

void Assembler::LD(GPR rd, int32_t imm, GPR rs) noexcept {
    BISCUIT_ASSERT(IsRV32OrRV64(m_features));
    BISCUIT_ASSERT(IsValidSigned12BitImm(imm));
    EmitIType(m_buffer, static_cast<uint32_t>(imm), rs, 0b011, rd, 0b0000011);
}

void Assembler::LWU(GPR rd, int32_t imm, GPR rs) noexcept {
    BISCUIT_ASSERT(IsRV64(m_features));
    BISCUIT_ASSERT(IsValidSigned12BitImm(imm));
    EmitIType(m_buffer, static_cast<uint32_t>(imm), rs, 0b110, rd, 0b0000011);
}

void Assembler::SD(GPR rs2, int32_t imm, GPR rs1) noexcept {
    BISCUIT_ASSERT(IsRV32OrRV64(m_features));
    BISCUIT_ASSERT(IsValidSigned12BitImm(imm));
    EmitSType(m_buffer, static_cast<uint32_t>(imm), rs2, rs1, 0b011, 0b0100011);
}

void Assembler::SLLIW(GPR rd, GPR rs, uint32_t shift) noexcept {
    BISCUIT_ASSERT(IsRV64(m_features));
    BISCUIT_ASSERT(shift <= 31);
    EmitIType(m_buffer, shift & 0x1F, rs, 0b001, rd, 0b0011011);
}
void Assembler::SRAIW(GPR rd, GPR rs, uint32_t shift) noexcept {
    BISCUIT_ASSERT(IsRV64(m_features));
    BISCUIT_ASSERT(shift <= 31);
    EmitIType(m_buffer, (0b0100000 << 5) | (shift & 0x1F), rs, 0b101, rd, 0b0011011);
}
void Assembler::SRLIW(GPR rd, GPR rs, uint32_t shift) noexcept {
    BISCUIT_ASSERT(IsRV64(m_features));
    BISCUIT_ASSERT(shift <= 31);
    EmitIType(m_buffer, shift & 0x1F, rs, 0b101, rd, 0b0011011);
}

void Assembler::SLLW(GPR rd, GPR lhs, GPR rhs) noexcept {
    BISCUIT_ASSERT(IsRV64(m_features));
    EmitRType(m_buffer, 0b0000000, rhs, lhs, 0b001, rd, 0b0111011);
}
void Assembler::SRAW(GPR rd, GPR lhs, GPR rhs) noexcept {
    BISCUIT_ASSERT(IsRV64(m_features));
    EmitRType(m_buffer, 0b0100000, rhs, lhs, 0b101, rd, 0b0111011);
}
void Assembler::SRLW(GPR rd, GPR lhs, GPR rhs) noexcept {
    BISCUIT_ASSERT(IsRV64(m_features));
    EmitRType(m_buffer, 0b0000000, rhs, lhs, 0b101, rd, 0b0111011);
}

void Assembler::SUBW(GPR rd, GPR lhs, GPR rhs) noexcept {
    BISCUIT_ASSERT(IsRV64(m_features));
    EmitRType(m_buffer, 0b0100000, rhs, lhs, 0b000, rd, 0b0111011);
}

// Zawrs Extension Instructions

void Assembler::WRS_NTO() noexcept {
    EmitIType(m_buffer, 0b01101, x0, 0, x0, 0b1110011);
}
void Assembler::WRS_STO() noexcept {
    EmitIType(m_buffer, 0b11101, x0, 0, x0, 0b1110011);
}

// Zacas Extension Instructions

void Assembler::AMOCAS_D(Ordering ordering, GPR rd, GPR rs2, GPR rs1) noexcept {
    if (IsRV32(m_features)) {
        BISCUIT_ASSERT((rd.Index() % 2) == 0);
        BISCUIT_ASSERT((rs2.Index() % 2) == 0);
    }
    EmitAtomic(m_buffer, 0b00101, ordering, rs2, rs1, 0b011, rd, 0b0101111);
}
void Assembler::AMOCAS_Q(Ordering ordering, GPR rd, GPR rs2, GPR rs1) noexcept {
    BISCUIT_ASSERT(IsRV64(m_features));

    // Both rd and rs2 indicate a register pair, so they need to be even-numbered.
    BISCUIT_ASSERT((rd.Index() % 2) == 0);
    BISCUIT_ASSERT((rs2.Index() % 2) == 0);
    EmitAtomic(m_buffer, 0b00101, ordering, rs2, rs1, 0b100, rd, 0b0101111);
}
void Assembler::AMOCAS_W(Ordering ordering, GPR rd, GPR rs2, GPR rs1) noexcept {
    EmitAtomic(m_buffer, 0b00101, ordering, rs2, rs1, 0b010, rd, 0b0101111);
}

// Zabha Extension Instructions

void Assembler::AMOADD_B(Ordering ordering, GPR rd, GPR rs2, GPR rs1) noexcept {
    EmitAtomic(m_buffer, 0b00000, ordering, rs2, rs1, 0b000, rd, 0b0101111);
}
void Assembler::AMOAND_B(Ordering ordering, GPR rd, GPR rs2, GPR rs1) noexcept {
    EmitAtomic(m_buffer, 0b01100, ordering, rs2, rs1, 0b000, rd, 0b0101111);
}
void Assembler::AMOMAX_B(Ordering ordering, GPR rd, GPR rs2, GPR rs1) noexcept {
    EmitAtomic(m_buffer, 0b10100, ordering, rs2, rs1, 0b000, rd, 0b0101111);
}
void Assembler::AMOMAXU_B(Ordering ordering, GPR rd, GPR rs2, GPR rs1) noexcept {
    EmitAtomic(m_buffer, 0b11100, ordering, rs2, rs1, 0b000, rd, 0b0101111);
}
void Assembler::AMOMIN_B(Ordering ordering, GPR rd, GPR rs2, GPR rs1) noexcept {
    EmitAtomic(m_buffer, 0b10000, ordering, rs2, rs1, 0b000, rd, 0b0101111);
}
void Assembler::AMOMINU_B(Ordering ordering, GPR rd, GPR rs2, GPR rs1) noexcept {
    EmitAtomic(m_buffer, 0b11000, ordering, rs2, rs1, 0b000, rd, 0b0101111);
}
void Assembler::AMOOR_B(Ordering ordering, GPR rd, GPR rs2, GPR rs1) noexcept {
    EmitAtomic(m_buffer, 0b01000, ordering, rs2, rs1, 0b000, rd, 0b0101111);
}
void Assembler::AMOSWAP_B(Ordering ordering, GPR rd, GPR rs2, GPR rs1) noexcept {
    EmitAtomic(m_buffer, 0b00001, ordering, rs2, rs1, 0b000, rd, 0b0101111);
}
void Assembler::AMOXOR_B(Ordering ordering, GPR rd, GPR rs2, GPR rs1) noexcept {
    EmitAtomic(m_buffer, 0b00100, ordering, rs2, rs1, 0b000, rd, 0b0101111);
}
void Assembler::AMOCAS_B(Ordering ordering, GPR rd, GPR rs2, GPR rs1) noexcept {
    EmitAtomic(m_buffer, 0b00101, ordering, rs2, rs1, 0b000, rd, 0b0101111);
}

void Assembler::AMOADD_H(Ordering ordering, GPR rd, GPR rs2, GPR rs1) noexcept {
    EmitAtomic(m_buffer, 0b00000, ordering, rs2, rs1, 0b001, rd, 0b0101111);
}
void Assembler::AMOAND_H(Ordering ordering, GPR rd, GPR rs2, GPR rs1) noexcept {
    EmitAtomic(m_buffer, 0b01100, ordering, rs2, rs1, 0b001, rd, 0b0101111);
}
void Assembler::AMOMAX_H(Ordering ordering, GPR rd, GPR rs2, GPR rs1) noexcept {
    EmitAtomic(m_buffer, 0b10100, ordering, rs2, rs1, 0b001, rd, 0b0101111);
}
void Assembler::AMOMAXU_H(Ordering ordering, GPR rd, GPR rs2, GPR rs1) noexcept {
    EmitAtomic(m_buffer, 0b11100, ordering, rs2, rs1, 0b001, rd, 0b0101111);
}
void Assembler::AMOMIN_H(Ordering ordering, GPR rd, GPR rs2, GPR rs1) noexcept {
    EmitAtomic(m_buffer, 0b10000, ordering, rs2, rs1, 0b001, rd, 0b0101111);
}
void Assembler::AMOMINU_H(Ordering ordering, GPR rd, GPR rs2, GPR rs1) noexcept {
    EmitAtomic(m_buffer, 0b11000, ordering, rs2, rs1, 0b001, rd, 0b0101111);
}
void Assembler::AMOOR_H(Ordering ordering, GPR rd, GPR rs2, GPR rs1) noexcept {
    EmitAtomic(m_buffer, 0b01000, ordering, rs2, rs1, 0b001, rd, 0b0101111);
}
void Assembler::AMOSWAP_H(Ordering ordering, GPR rd, GPR rs2, GPR rs1) noexcept {
    EmitAtomic(m_buffer, 0b00001, ordering, rs2, rs1, 0b001, rd, 0b0101111);
}
void Assembler::AMOXOR_H(Ordering ordering, GPR rd, GPR rs2, GPR rs1) noexcept {
    EmitAtomic(m_buffer, 0b00100, ordering, rs2, rs1, 0b001, rd, 0b0101111);
}
void Assembler::AMOCAS_H(Ordering ordering, GPR rd, GPR rs2, GPR rs1) noexcept {
    EmitAtomic(m_buffer, 0b00101, ordering, rs2, rs1, 0b001, rd, 0b0101111);
}

// Zicond Extension Instructions

void Assembler::CZERO_EQZ(GPR rd, GPR value, GPR condition) noexcept {
    EmitRType(m_buffer, 0b0000111, condition, value, 0b101, rd, 0b0110011);
}
void Assembler::CZERO_NEZ(GPR rd, GPR value, GPR condition) noexcept {
    EmitRType(m_buffer, 0b0000111, condition, value, 0b111, rd, 0b0110011);
}

// XTheadCondMov Extension Instructions

void Assembler::TH_MVEQZ(GPR rd, GPR value, GPR condition) noexcept {
    EmitRType(m_buffer, 0b0100000, condition, value, 0b001, rd, 0b0001011);
}

void Assembler::TH_MVNEZ(GPR rd, GPR value, GPR condition) noexcept {
    EmitRType(m_buffer, 0b0100001, condition, value, 0b001, rd, 0b0001011);
}

// XTheadBa Extension Instructions

void Assembler::TH_ADDSL(GPR rd, GPR rs1, GPR rs2, uint32_t shift) noexcept {
    BISCUIT_ASSERT(shift <= 3);
    EmitRType(m_buffer, 0b0000000 | shift, rs2, rs1, 0b001, rd, 0b0001011);
}

// Zicsr Extension Instructions

void Assembler::CSRRC(GPR rd, CSR csr, GPR rs) noexcept {
    EmitIType(m_buffer, static_cast<uint32_t>(csr), rs, 0b011, rd, 0b1110011);
}
void Assembler::CSRRCI(GPR rd, CSR csr, uint32_t imm) noexcept {
    BISCUIT_ASSERT(imm <= 0x1F);
    EmitIType(m_buffer, static_cast<uint32_t>(csr), GPR{imm & 0x1F}, 0b111, rd, 0b1110011);
}
void Assembler::CSRRS(GPR rd, CSR csr, GPR rs) noexcept {
    EmitIType(m_buffer, static_cast<uint32_t>(csr), rs, 0b010, rd, 0b1110011);
}
void Assembler::CSRRSI(GPR rd, CSR csr, uint32_t imm) noexcept {
    BISCUIT_ASSERT(imm <= 0x1F);
    EmitIType(m_buffer, static_cast<uint32_t>(csr), GPR{imm & 0x1F}, 0b110, rd, 0b1110011);
}
void Assembler::CSRRW(GPR rd, CSR csr, GPR rs) noexcept {
    EmitIType(m_buffer, static_cast<uint32_t>(csr), rs, 0b001, rd, 0b1110011);
}
void Assembler::CSRRWI(GPR rd, CSR csr, uint32_t imm) noexcept {
    BISCUIT_ASSERT(imm <= 0x1F);
    EmitIType(m_buffer, static_cast<uint32_t>(csr), GPR{imm & 0x1F}, 0b101, rd, 0b1110011);
}

void Assembler::CSRR(GPR rd, CSR csr) noexcept {
    CSRRS(rd, csr, x0);
}
void Assembler::CSWR(CSR csr, GPR rs) noexcept {
    CSRRW(x0, csr, rs);
}

void Assembler::CSRS(CSR csr, GPR rs) noexcept {
    CSRRS(x0, csr, rs);
}
void Assembler::CSRC(CSR csr, GPR rs) noexcept {
    CSRRC(x0, csr, rs);
}

void Assembler::CSRCI(CSR csr, uint32_t imm) noexcept {
    CSRRCI(x0, csr, imm);
}
void Assembler::CSRSI(CSR csr, uint32_t imm) noexcept {
    CSRRSI(x0, csr, imm);
}
void Assembler::CSRWI(CSR csr, uint32_t imm) noexcept {
    CSRRWI(x0, csr, imm);
}

void Assembler::FRCSR(GPR rd) noexcept {
    CSRRS(rd, CSR::FCSR, x0);
}
void Assembler::FSCSR(GPR rd, GPR rs) noexcept {
    CSRRW(rd, CSR::FCSR, rs);
}
void Assembler::FSCSR(GPR rs) noexcept {
    CSRRW(x0, CSR::FCSR, rs);
}

void Assembler::FRRM(GPR rd) noexcept {
    CSRRS(rd, CSR::FRM, x0);
}
void Assembler::FSRM(GPR rd, GPR rs) noexcept {
    CSRRW(rd, CSR::FRM, rs);
}
void Assembler::FSRM(GPR rs) noexcept {
    CSRRW(x0, CSR::FRM, rs);
}

void Assembler::FSRMI(GPR rd, uint32_t imm) noexcept {
    CSRRWI(rd, CSR::FRM, imm);
}
void Assembler::FSRMI(uint32_t imm) noexcept {
    CSRRWI(x0, CSR::FRM, imm);
}

void Assembler::FRFLAGS(GPR rd) noexcept {
    CSRRS(rd, CSR::FFlags, x0);
}
void Assembler::FSFLAGS(GPR rd, GPR rs) noexcept {
    CSRRW(rd, CSR::FFlags, rs);
}
void Assembler::FSFLAGS(GPR rs) noexcept {
    CSRRW(x0, CSR::FFlags, rs);
}

void Assembler::FSFLAGSI(GPR rd, uint32_t imm) noexcept {
    CSRRWI(rd, CSR::FFlags, imm);
}
void Assembler::FSFLAGSI(uint32_t imm) noexcept {
    CSRRWI(x0, CSR::FFlags, imm);
}

void Assembler::RDCYCLE(GPR rd) noexcept {
    CSRRS(rd, CSR::Cycle, x0);
}
void Assembler::RDCYCLEH(GPR rd) noexcept {
    CSRRS(rd, CSR::CycleH, x0);
}

void Assembler::RDINSTRET(GPR rd) noexcept {
    CSRRS(rd, CSR::InstRet, x0);
}
void Assembler::RDINSTRETH(GPR rd) noexcept {
    CSRRS(rd, CSR::InstRetH, x0);
}

void Assembler::RDTIME(GPR rd) noexcept {
    CSRRS(rd, CSR::Time, x0);
}
void Assembler::RDTIMEH(GPR rd) noexcept {
    CSRRS(rd, CSR::TimeH, x0);
}

// Zihintntl Extension Instructions

void Assembler::C_NTL_ALL() noexcept {
    C_ADD(x0, x5);
}
void Assembler::C_NTL_S1() noexcept {
    C_ADD(x0, x4);
}
void Assembler::C_NTL_P1() noexcept {
    C_ADD(x0, x2);
}
void Assembler::C_NTL_PALL() noexcept {
    C_ADD(x0, x3);
}
void Assembler::NTL_ALL() noexcept {
    ADD(x0, x0, x5);
}
void Assembler::NTL_S1() noexcept {
    ADD(x0, x0, x4);
}
void Assembler::NTL_P1() noexcept {
    ADD(x0, x0, x2);
}
void Assembler::NTL_PALL() noexcept {
    ADD(x0, x0, x3);
}

// RV32M Extension Instructions

void Assembler::DIV(GPR rd, GPR rs1, GPR rs2) noexcept {
    EmitRType(m_buffer, 0b0000001, rs2, rs1, 0b100, rd, 0b0110011);
}
void Assembler::DIVU(GPR rd, GPR rs1, GPR rs2) noexcept {
    EmitRType(m_buffer, 0b0000001, rs2, rs1, 0b101, rd, 0b0110011);
}
void Assembler::MUL(GPR rd, GPR rs1, GPR rs2) noexcept {
    EmitRType(m_buffer, 0b0000001, rs2, rs1, 0b000, rd, 0b0110011);
}
void Assembler::MULH(GPR rd, GPR rs1, GPR rs2) noexcept {
    EmitRType(m_buffer, 0b0000001, rs2, rs1, 0b001, rd, 0b0110011);
}
void Assembler::MULHSU(GPR rd, GPR rs1, GPR rs2) noexcept {
    EmitRType(m_buffer, 0b0000001, rs2, rs1, 0b010, rd, 0b0110011);
}
void Assembler::MULHU(GPR rd, GPR rs1, GPR rs2) noexcept {
    EmitRType(m_buffer, 0b0000001, rs2, rs1, 0b011, rd, 0b0110011);
}
void Assembler::REM(GPR rd, GPR rs1, GPR rs2) noexcept {
    EmitRType(m_buffer, 0b0000001, rs2, rs1, 0b110, rd, 0b0110011);
}
void Assembler::REMU(GPR rd, GPR rs1, GPR rs2) noexcept {
    EmitRType(m_buffer, 0b0000001, rs2, rs1, 0b111, rd, 0b0110011);
}

// RV64M Extension Instructions

void Assembler::DIVW(GPR rd, GPR rs1, GPR rs2) noexcept {
    EmitRType(m_buffer, 0b0000001, rs2, rs1, 0b100, rd, 0b0111011);
}
void Assembler::DIVUW(GPR rd, GPR rs1, GPR rs2) noexcept {
    EmitRType(m_buffer, 0b0000001, rs2, rs1, 0b101, rd, 0b0111011);
}
void Assembler::MULW(GPR rd, GPR rs1, GPR rs2) noexcept {
    EmitRType(m_buffer, 0b0000001, rs2, rs1, 0b000, rd, 0b0111011);
}
void Assembler::REMW(GPR rd, GPR rs1, GPR rs2) noexcept {
    EmitRType(m_buffer, 0b0000001, rs2, rs1, 0b110, rd, 0b0111011);
}
void Assembler::REMUW(GPR rd, GPR rs1, GPR rs2) noexcept {
    EmitRType(m_buffer, 0b0000001, rs2, rs1, 0b111, rd, 0b0111011);
}

// RV32A Extension Instructions

void Assembler::AMOADD_W(Ordering ordering, GPR rd, GPR rs2, GPR rs1) noexcept {
    EmitAtomic(m_buffer, 0b00000, ordering, rs2, rs1, 0b010, rd, 0b0101111);
}
void Assembler::AMOAND_W(Ordering ordering, GPR rd, GPR rs2, GPR rs1) noexcept {
    EmitAtomic(m_buffer, 0b01100, ordering, rs2, rs1, 0b010, rd, 0b0101111);
}
void Assembler::AMOMAX_W(Ordering ordering, GPR rd, GPR rs2, GPR rs1) noexcept {
    EmitAtomic(m_buffer, 0b10100, ordering, rs2, rs1, 0b010, rd, 0b0101111);
}
void Assembler::AMOMAXU_W(Ordering ordering, GPR rd, GPR rs2, GPR rs1) noexcept {
    EmitAtomic(m_buffer, 0b11100, ordering, rs2, rs1, 0b010, rd, 0b0101111);
}
void Assembler::AMOMIN_W(Ordering ordering, GPR rd, GPR rs2, GPR rs1) noexcept {
    EmitAtomic(m_buffer, 0b10000, ordering, rs2, rs1, 0b010, rd, 0b0101111);
}
void Assembler::AMOMINU_W(Ordering ordering, GPR rd, GPR rs2, GPR rs1) noexcept {
    EmitAtomic(m_buffer, 0b11000, ordering, rs2, rs1, 0b010, rd, 0b0101111);
}
void Assembler::AMOOR_W(Ordering ordering, GPR rd, GPR rs2, GPR rs1) noexcept {
    EmitAtomic(m_buffer, 0b01000, ordering, rs2, rs1, 0b010, rd, 0b0101111);
}
void Assembler::AMOSWAP_W(Ordering ordering, GPR rd, GPR rs2, GPR rs1) noexcept {
    EmitAtomic(m_buffer, 0b00001, ordering, rs2, rs1, 0b010, rd, 0b0101111);
}
void Assembler::AMOXOR_W(Ordering ordering, GPR rd, GPR rs2, GPR rs1) noexcept {
    EmitAtomic(m_buffer, 0b00100, ordering, rs2, rs1, 0b010, rd, 0b0101111);
}
void Assembler::LR_W(Ordering ordering, GPR rd, GPR rs) noexcept {
    EmitAtomic(m_buffer, 0b00010, ordering, x0, rs, 0b010, rd, 0b0101111);
}
void Assembler::SC_W(Ordering ordering, GPR rd, GPR rs2, GPR rs1) noexcept {
    EmitAtomic(m_buffer, 0b00011, ordering, rs2, rs1, 0b010, rd, 0b0101111);
}

// RV64A Extension Instructions

void Assembler::AMOADD_D(Ordering ordering, GPR rd, GPR rs2, GPR rs1) noexcept {
    BISCUIT_ASSERT(IsRV64(m_features));
    EmitAtomic(m_buffer, 0b00000, ordering, rs2, rs1, 0b011, rd, 0b0101111);
}
void Assembler::AMOAND_D(Ordering ordering, GPR rd, GPR rs2, GPR rs1) noexcept {
    BISCUIT_ASSERT(IsRV64(m_features));
    EmitAtomic(m_buffer, 0b01100, ordering, rs2, rs1, 0b011, rd, 0b0101111);
}
void Assembler::AMOMAX_D(Ordering ordering, GPR rd, GPR rs2, GPR rs1) noexcept {
    BISCUIT_ASSERT(IsRV64(m_features));
    EmitAtomic(m_buffer, 0b10100, ordering, rs2, rs1, 0b011, rd, 0b0101111);
}
void Assembler::AMOMAXU_D(Ordering ordering, GPR rd, GPR rs2, GPR rs1) noexcept {
    BISCUIT_ASSERT(IsRV64(m_features));
    EmitAtomic(m_buffer, 0b11100, ordering, rs2, rs1, 0b011, rd, 0b0101111);
}
void Assembler::AMOMIN_D(Ordering ordering, GPR rd, GPR rs2, GPR rs1) noexcept {
    BISCUIT_ASSERT(IsRV64(m_features));
    EmitAtomic(m_buffer, 0b10000, ordering, rs2, rs1, 0b011, rd, 0b0101111);
}
void Assembler::AMOMINU_D(Ordering ordering, GPR rd, GPR rs2, GPR rs1) noexcept {
    BISCUIT_ASSERT(IsRV64(m_features));
    EmitAtomic(m_buffer, 0b11000, ordering, rs2, rs1, 0b011, rd, 0b0101111);
}
void Assembler::AMOOR_D(Ordering ordering, GPR rd, GPR rs2, GPR rs1) noexcept {
    BISCUIT_ASSERT(IsRV64(m_features));
    EmitAtomic(m_buffer, 0b01000, ordering, rs2, rs1, 0b011, rd, 0b0101111);
}
void Assembler::AMOSWAP_D(Ordering ordering, GPR rd, GPR rs2, GPR rs1) noexcept {
    BISCUIT_ASSERT(IsRV64(m_features));
    EmitAtomic(m_buffer, 0b00001, ordering, rs2, rs1, 0b011, rd, 0b0101111);
}
void Assembler::AMOXOR_D(Ordering ordering, GPR rd, GPR rs2, GPR rs1) noexcept {
    BISCUIT_ASSERT(IsRV64(m_features));
    EmitAtomic(m_buffer, 0b00100, ordering, rs2, rs1, 0b011, rd, 0b0101111);
}
void Assembler::LR_D(Ordering ordering, GPR rd, GPR rs) noexcept {
    BISCUIT_ASSERT(IsRV64(m_features));
    EmitAtomic(m_buffer, 0b00010, ordering, x0, rs, 0b011, rd, 0b0101111);
}
void Assembler::SC_D(Ordering ordering, GPR rd, GPR rs2, GPR rs1) noexcept {
    BISCUIT_ASSERT(IsRV64(m_features));
    EmitAtomic(m_buffer, 0b00011, ordering, rs2, rs1, 0b011, rd, 0b0101111);
}

// RVB Extension Instructions

void Assembler::ADDUW(GPR rd, GPR rs1, GPR rs2) noexcept {
    BISCUIT_ASSERT(IsRV64(m_features));
    EmitRType(m_buffer, 0b0000100, rs2, rs1, 0b000, rd, 0b0111011);
}

void Assembler::ANDN(GPR rd, GPR rs1, GPR rs2) noexcept {
    EmitRType(m_buffer, 0b0100000, rs2, rs1, 0b111, rd, 0b0110011);
}

void Assembler::BCLR(GPR rd, GPR rs1, GPR rs2) noexcept {
    EmitRType(m_buffer, 0b0100100, rs2, rs1, 0b001, rd, 0b0110011);
}

void Assembler::BCLRI(GPR rd, GPR rs, uint32_t bit) noexcept {
    if (IsRV32(m_features)) {
        BISCUIT_ASSERT(bit <= 31);
    } else {
        BISCUIT_ASSERT(bit <= 63);
    }

    const auto imm = (0b010010U << 6) | bit;
    EmitIType(m_buffer, imm, rs, 0b001, rd, 0b0010011);
}

void Assembler::BEXT(GPR rd, GPR rs1, GPR rs2) noexcept {
    EmitRType(m_buffer, 0b0100100, rs2, rs1, 0b101, rd, 0b0110011);
}

void Assembler::BEXTI(GPR rd, GPR rs, uint32_t bit) noexcept {
    if (IsRV32(m_features)) {
        BISCUIT_ASSERT(bit <= 31);
    } else {
        BISCUIT_ASSERT(bit <= 63);
    }

    const auto imm = (0b010010U << 6) | bit;
    EmitIType(m_buffer, imm, rs, 0b101, rd, 0b0010011);
}

void Assembler::BINV(GPR rd, GPR rs1, GPR rs2) noexcept {
    EmitRType(m_buffer, 0b0110100, rs2, rs1, 0b001, rd, 0b0110011);
}

void Assembler::BINVI(GPR rd, GPR rs, uint32_t bit) noexcept {
    if (IsRV32(m_features)) {
        BISCUIT_ASSERT(bit <= 31);
    } else {
        BISCUIT_ASSERT(bit <= 63);
    }

    const auto imm = (0b011010U << 6) | bit;
    EmitIType(m_buffer, imm, rs, 0b001, rd, 0b0010011);
}

void Assembler::BREV8(GPR rd, GPR rs) noexcept {
    EmitIType(m_buffer, 0b011010000111, rs, 0b101, rd, 0b0010011);
}

void Assembler::BSET(GPR rd, GPR rs1, GPR rs2) noexcept {
    EmitRType(m_buffer, 0b0010100, rs2, rs1, 0b001, rd, 0b0110011);
}

void Assembler::BSETI(GPR rd, GPR rs, uint32_t bit) noexcept {
    if (IsRV32(m_features)) {
        BISCUIT_ASSERT(bit <= 31);
    } else {
        BISCUIT_ASSERT(bit <= 63);
    }

    const auto imm = (0b001010U << 6) | bit;
    EmitIType(m_buffer, imm, rs, 0b001, rd, 0b0110011);
}

void Assembler::CLMUL(GPR rd, GPR rs1, GPR rs2) noexcept {
    EmitRType(m_buffer, 0b0000101, rs2, rs1, 0b001, rd, 0b0110011);
}

void Assembler::CLMULH(GPR rd, GPR rs1, GPR rs2) noexcept {
    EmitRType(m_buffer, 0b0000101, rs2, rs1, 0b011, rd, 0b0110011);
}

void Assembler::CLMULR(GPR rd, GPR rs1, GPR rs2) noexcept {
    EmitRType(m_buffer, 0b0000101, rs2, rs1, 0b010, rd, 0b0110011);
}

void Assembler::CLZ(GPR rd, GPR rs) noexcept {
    EmitIType(m_buffer, 0b011000000000, rs, 0b001, rd, 0b0010011);
}

void Assembler::CLZW(GPR rd, GPR rs) noexcept {
    BISCUIT_ASSERT(IsRV64(m_features));
    EmitIType(m_buffer, 0b011000000000, rs, 0b001, rd, 0b0011011);
}

void Assembler::CPOP(GPR rd, GPR rs) noexcept {
    EmitIType(m_buffer, 0b011000000010, rs, 0b001, rd, 0b0010011);
}

void Assembler::CPOPW(GPR rd, GPR rs) noexcept {
    BISCUIT_ASSERT(IsRV64(m_features));
    EmitIType(m_buffer, 0b011000000010, rs, 0b001, rd, 0b0011011);
}

void Assembler::CTZ(GPR rd, GPR rs) noexcept {
    EmitIType(m_buffer, 0b011000000001, rs, 0b001, rd, 0b0010011);
}

void Assembler::CTZW(GPR rd, GPR rs) noexcept {
    BISCUIT_ASSERT(IsRV64(m_features));
    EmitIType(m_buffer, 0b011000000001, rs, 0b001, rd, 0b0011011);
}

void Assembler::MAX(GPR rd, GPR rs1, GPR rs2) noexcept {
    EmitRType(m_buffer, 0b0000101, rs2, rs1, 0b110, rd, 0b0110011);
}

void Assembler::MAXU(GPR rd, GPR rs1, GPR rs2) noexcept {
    EmitRType(m_buffer, 0b0000101, rs2, rs1, 0b111, rd, 0b0110011);
}

void Assembler::MIN(GPR rd, GPR rs1, GPR rs2) noexcept {
    EmitRType(m_buffer, 0b0000101, rs2, rs1, 0b100, rd, 0b0110011);
}

void Assembler::MINU(GPR rd, GPR rs1, GPR rs2) noexcept {
    EmitRType(m_buffer, 0b0000101, rs2, rs1, 0b101, rd, 0b0110011);
}

void Assembler::ORCB(GPR rd, GPR rs) noexcept {
    EmitIType(m_buffer, 0b001010000111, rs, 0b101, rd, 0b0010011);
}

void Assembler::ORN(GPR rd, GPR rs1, GPR rs2) noexcept {
    EmitRType(m_buffer, 0b0100000, rs2, rs1, 0b110, rd, 0b0110011);
}

void Assembler::PACK(GPR rd, GPR rs1, GPR rs2) noexcept {
    EmitRType(m_buffer, 0b0000100, rs2, rs1, 0b100, rd, 0b0110011);
}

void Assembler::PACKH(GPR rd, GPR rs1, GPR rs2) noexcept {
    EmitRType(m_buffer, 0b0000100, rs2, rs1, 0b111, rd, 0b0110011);
}

void Assembler::PACKW(GPR rd, GPR rs1, GPR rs2) noexcept {
    BISCUIT_ASSERT(IsRV64(m_features));
    EmitRType(m_buffer, 0b0000100, rs2, rs1, 0b100, rd, 0b0111011);
}

void Assembler::REV8(GPR rd, GPR rs) noexcept {
    if (IsRV32(m_features)) {
        EmitIType(m_buffer, 0b011010011000, rs, 0b101, rd, 0b0010011);
    } else {
        EmitIType(m_buffer, 0b011010111000, rs, 0b101, rd, 0b0010011);
    }
}

void Assembler::ROL(GPR rd, GPR rs1, GPR rs2) noexcept {
    EmitRType(m_buffer, 0b0110000, rs2, rs1, 0b001, rd, 0b0110011);
}

void Assembler::ROLW(GPR rd, GPR rs1, GPR rs2) noexcept {
    BISCUIT_ASSERT(IsRV64(m_features));
    EmitRType(m_buffer, 0b0110000, rs2, rs1, 0b001, rd, 0b0111011);
}

void Assembler::ROR(GPR rd, GPR rs1, GPR rs2) noexcept {
    EmitRType(m_buffer, 0b0110000, rs2, rs1, 0b101, rd, 0b0110011);
}

void Assembler::RORI(GPR rd, GPR rs, uint32_t rotate_amount) noexcept {
    BISCUIT_ASSERT(rotate_amount <= 63);
    const auto imm = (0b011000U << 6) | rotate_amount;
    EmitIType(m_buffer, imm, rs, 0b101, rd, 0b0010011);
}

void Assembler::RORIW(GPR rd, GPR rs, uint32_t rotate_amount) noexcept {
    BISCUIT_ASSERT(IsRV64(m_features));
    BISCUIT_ASSERT(rotate_amount <= 31);
    const auto imm = (0b011000U << 6) | rotate_amount;
    EmitIType(m_buffer, imm, rs, 0b101, rd, 0b0011011);
}

void Assembler::RORW(GPR rd, GPR rs1, GPR rs2) noexcept {
    BISCUIT_ASSERT(IsRV64(m_features));
    EmitRType(m_buffer, 0b0110000, rs2, rs1, 0b101, rd, 0b0111011);
}

void Assembler::SEXTB(GPR rd, GPR rs) noexcept {
    EmitIType(m_buffer, 0b011000000100, rs, 0b001, rd, 0b0010011);
}

void Assembler::SEXTH(GPR rd, GPR rs) noexcept {
    EmitIType(m_buffer, 0b011000000101, rs, 0b001, rd, 0b0010011);
}

void Assembler::SH1ADD(GPR rd, GPR rs1, GPR rs2) noexcept {
    EmitRType(m_buffer, 0b0010000, rs2, rs1, 0b010, rd, 0b0110011);
}

void Assembler::SH1ADDUW(GPR rd, GPR rs1, GPR rs2) noexcept {
    BISCUIT_ASSERT(IsRV64(m_features));
    EmitRType(m_buffer, 0b0010000, rs2, rs1, 0b010, rd, 0b0111011);
}

void Assembler::SH2ADD(GPR rd, GPR rs1, GPR rs2) noexcept {
    EmitRType(m_buffer, 0b0010000, rs2, rs1, 0b100, rd, 0b0110011);
}

void Assembler::SH2ADDUW(GPR rd, GPR rs1, GPR rs2) noexcept {
    BISCUIT_ASSERT(IsRV64(m_features));
    EmitRType(m_buffer, 0b0010000, rs2, rs1, 0b100, rd, 0b0111011);
}

void Assembler::SH3ADD(GPR rd, GPR rs1, GPR rs2) noexcept {
    EmitRType(m_buffer, 0b0010000, rs2, rs1, 0b110, rd, 0b0110011);
}

void Assembler::SH3ADDUW(GPR rd, GPR rs1, GPR rs2) noexcept {
    BISCUIT_ASSERT(IsRV64(m_features));
    EmitRType(m_buffer, 0b0010000, rs2, rs1, 0b110, rd, 0b0111011);
}

void Assembler::SLLIUW(GPR rd, GPR rs, uint32_t shift_amount) noexcept {
    BISCUIT_ASSERT(IsRV64(m_features));
    BISCUIT_ASSERT(shift_amount <= 63);
    const auto imm = (0b000010U << 6) | shift_amount;
    EmitIType(m_buffer, imm, rs, 0b001, rd, 0b0011011);
}

void Assembler::UNZIP(GPR rd, GPR rs) noexcept {
    BISCUIT_ASSERT(IsRV32(m_features));
    EmitIType(m_buffer, 0b000010011111, rs, 0b101, rd, 0b0010011);
}

void Assembler::XNOR(GPR rd, GPR rs1, GPR rs2) noexcept {
    EmitRType(m_buffer, 0b0100000, rs2, rs1, 0b100, rd, 0b0110011);
}

void Assembler::XPERM4(GPR rd, GPR rs1, GPR rs2) noexcept {
    EmitRType(m_buffer, 0b0010100, rs2, rs1, 0b010, rd, 0b0110011);
}

void Assembler::XPERM8(GPR rd, GPR rs1, GPR rs2) noexcept {
    EmitRType(m_buffer, 0b0010100, rs2, rs1, 0b100, rd, 0b0110011);
}

void Assembler::ZEXTH(GPR rd, GPR rs) noexcept {
    if (IsRV32(m_features)) {
        EmitIType(m_buffer, 0b000010000000, rs, 0b100, rd, 0b0110011);
    } else {
        EmitIType(m_buffer, 0b000010000000, rs, 0b100, rd, 0b0111011);
    }
}

void Assembler::ZEXTW(GPR rd, GPR rs) noexcept {
    ADDUW(rd, rs, x0);
}

void Assembler::ZIP(GPR rd, GPR rs) noexcept {
    BISCUIT_ASSERT(IsRV32(m_features));
    EmitIType(m_buffer, 0b000010011110, rs, 0b001, rd, 0b0010011);
}

// Cache Management Operation Extension Instructions

void Assembler::CBO_CLEAN(GPR rs) noexcept {
    EmitRType(m_buffer, 0b0000000, x1, rs, 0b010, x0, 0b0001111);
}

void Assembler::CBO_FLUSH(GPR rs) noexcept {
    EmitRType(m_buffer, 0b0000000, x2, rs, 0b010, x0, 0b0001111);
}

void Assembler::CBO_INVAL(GPR rs) noexcept {
    EmitRType(m_buffer, 0b0000000, x0, rs, 0b010, x0, 0b0001111);
}

void Assembler::CBO_ZERO(GPR rs) noexcept {
    EmitRType(m_buffer, 0b0000000, x4, rs, 0b010, x0, 0b0001111);
}

void Assembler::PREFETCH_I(GPR rs, int32_t offset) noexcept {
    // Offset must be able to fit in a 12-bit signed immediate and be
    // cleanly divisible by 32 since the bottom 5 bits are encoded as zero.
    BISCUIT_ASSERT(IsValidSigned12BitImm(offset));
    BISCUIT_ASSERT(offset % 32 == 0);
    EmitIType(m_buffer, static_cast<uint32_t>(offset), rs, 0b110, x0, 0b0010011);
}

void Assembler::PREFETCH_R(GPR rs, int32_t offset) noexcept {
    // Offset must be able to fit in a 12-bit signed immediate and be
    // cleanly divisible by 32 since the bottom 5 bits are encoded as zero.
    BISCUIT_ASSERT(IsValidSigned12BitImm(offset));
    BISCUIT_ASSERT(offset % 32 == 0);
    EmitIType(m_buffer, static_cast<uint32_t>(offset) | 0b01, rs, 0b110, x0, 0b0010011);
}

void Assembler::PREFETCH_W(GPR rs, int32_t offset) noexcept {
    // Offset must be able to fit in a 12-bit signed immediate and be
    // cleanly divisible by 32 since the bottom 5 bits are encoded as zero.
    BISCUIT_ASSERT(IsValidSigned12BitImm(offset));
    BISCUIT_ASSERT(offset % 32 == 0);
    EmitIType(m_buffer, static_cast<uint32_t>(offset) | 0b11, rs, 0b110, x0, 0b0010011);
}

// Control flow integrity instructions

void Assembler::SSAMOSWAP_D(Ordering ordering, GPR rd, GPR rs2, GPR rs1) noexcept {
    BISCUIT_ASSERT(IsRV64(m_features));
    EmitAtomic(m_buffer, 0b01001, ordering, rs2, rs1, 0b011, rd, 0b0101111);
}
void Assembler::SSAMOSWAP_W(Ordering ordering, GPR rd, GPR rs2, GPR rs1) noexcept {
    EmitAtomic(m_buffer, 0b01001, ordering, rs2, rs1, 0b010, rd, 0b0101111);
}
void Assembler::SSRDP(GPR rd) noexcept {
    BISCUIT_ASSERT(rd != x0);
    EmitMOP_R(m_buffer, 28, rd, x0);
}
void Assembler::SSPOPCHK(GPR rs) noexcept {
    BISCUIT_ASSERT(rs == x1 || rs == x5);
    EmitMOP_R(m_buffer, 28, x0, rs);
}
void Assembler::SSPUSH(GPR rs) noexcept {
    BISCUIT_ASSERT(rs == x1 || rs == x5);
    EmitMOP_RR(m_buffer, 7, x0, x0, rs);
}
void Assembler::LPAD(int32_t imm) noexcept {
    EmitUType(m_buffer, static_cast<uint32_t>(imm), x0, 0b0010111);
}

// Privileged Instructions

void Assembler::HFENCE_GVMA(GPR rs1, GPR rs2) noexcept {
    EmitRType(m_buffer, 0b0110001, rs2, rs1, 0b000, x0, 0b1110011);
}

void Assembler::HFENCE_VVMA(GPR rs1, GPR rs2) noexcept {
    EmitRType(m_buffer, 0b0010001, rs2, rs1, 0b000, x0, 0b1110011);
}

void Assembler::HINVAL_GVMA(GPR rs1, GPR rs2) noexcept {
    EmitRType(m_buffer, 0b0110011, rs2, rs1, 0b000, x0, 0b1110011);
}

void Assembler::HINVAL_VVMA(GPR rs1, GPR rs2) noexcept {
    EmitRType(m_buffer, 0b0010011, rs2, rs1, 0b000, x0, 0b1110011);
}

void Assembler::HLV_B(GPR rd, GPR rs) noexcept {
    EmitRType(m_buffer, 0b0110000, x0, rs, 0b100, rd, 0b1110011);
}

void Assembler::HLV_BU(GPR rd, GPR rs) noexcept {
    EmitRType(m_buffer, 0b0110000, x1, rs, 0b100, rd, 0b1110011);
}

void Assembler::HLV_D(GPR rd, GPR rs) noexcept {
    BISCUIT_ASSERT(IsRV64(m_features));
    EmitRType(m_buffer, 0b0110110, x0, rs, 0b100, rd, 0b1110011);
}

void Assembler::HLV_H(GPR rd, GPR rs) noexcept {
    EmitRType(m_buffer, 0b0110010, x0, rs, 0b100, rd, 0b1110011);
}

void Assembler::HLV_HU(GPR rd, GPR rs) noexcept {
    EmitRType(m_buffer, 0b0110010, x1, rs, 0b100, rd, 0b1110011);
}

void Assembler::HLV_W(GPR rd, GPR rs) noexcept {
    EmitRType(m_buffer, 0b0110100, x0, rs, 0b100, rd, 0b1110011);
}

void Assembler::HLV_WU(GPR rd, GPR rs) noexcept {
    BISCUIT_ASSERT(IsRV64(m_features));
    EmitRType(m_buffer, 0b0110100, x1, rs, 0b100, rd, 0b1110011);
}

void Assembler::HLVX_HU(GPR rd, GPR rs) noexcept {
    EmitRType(m_buffer, 0b0110010, x3, rs, 0b100, rd, 0b1110011);
}

void Assembler::HLVX_WU(GPR rd, GPR rs) noexcept {
    EmitRType(m_buffer, 0b0110100, x3, rs, 0b100, rd, 0b1110011);
}

void Assembler::HSV_B(GPR rs2, GPR rs1) noexcept {
    EmitRType(m_buffer, 0b0110001, rs2, rs1, 0b100, x0, 0b1110011);
}

void Assembler::HSV_D(GPR rs2, GPR rs1) noexcept {
    BISCUIT_ASSERT(IsRV64(m_features));
    EmitRType(m_buffer, 0b0110111, rs2, rs1, 0b100, x0, 0b1110011);
}

void Assembler::HSV_H(GPR rs2, GPR rs1) noexcept {
    EmitRType(m_buffer, 0b0110011, rs2, rs1, 0b100, x0, 0b1110011);
}

void Assembler::HSV_W(GPR rs2, GPR rs1) noexcept {
    EmitRType(m_buffer, 0b0110101, rs2, rs1, 0b100, x0, 0b1110011);
}

void Assembler::MRET() noexcept {
    m_buffer.Emit32(0x30200073);
}

void Assembler::SCTRCLR() noexcept {
    m_buffer.Emit32(0x10400073);
}

void Assembler::SFENCE_INVAL_IR() noexcept {
    m_buffer.Emit32(0x18100073U);
}

void Assembler::SFENCE_VMA(GPR rs1, GPR rs2) noexcept {
    EmitRType(m_buffer, 0b0001001, rs2, rs1, 0b000, x0, 0b1110011);
}

void Assembler::SFENCE_W_INVAL() noexcept {
    m_buffer.Emit32(0x18000073U);
}

void Assembler::SINVAL_VMA(GPR rs1, GPR rs2) noexcept {
    EmitRType(m_buffer, 0b0001011, rs2, rs1, 0b000, x0, 0b1110011);
}

void Assembler::SRET() noexcept {
    m_buffer.Emit32(0x10200073);
}

void Assembler::URET() noexcept {
    m_buffer.Emit32(0x00200073);
}

void Assembler::WFI() noexcept {
    m_buffer.Emit32(0x10500073);
}

void Assembler::BindToOffset(Label* label, Label::LocationOffset offset) {
    BISCUIT_ASSERT(label != nullptr);
    BISCUIT_ASSERT(offset >= 0 && offset <= m_buffer.GetCursorOffset());

    label->Bind(offset);
    ResolveLabelOffsets(label);
    label->ClearOffsets();
}

ptrdiff_t Assembler::LinkAndGetOffset(Label* label) {
    BISCUIT_ASSERT(label != nullptr);

    // If we have a bound label, then it's straightforward to calculate
    // the offsets.
    if (label->IsBound()) {
        const auto cursor_address = m_buffer.GetCursorAddress();
        const auto label_offset = m_buffer.GetOffsetAddress(*label->GetLocation());
        return static_cast<ptrdiff_t>(label_offset - cursor_address);
    }

    // If we don't have a bound location, we return an offset of zero.
    // While the emitter will emit a bogus branch instruction initially,
    // the offset will be patched over once the label has been properly
    // bound to a location.
    label->AddOffset(m_buffer.GetCursorOffset());
    return 0;
}

void Assembler::ResolveLabelOffsets(Label* label) {
    // Conditional branch instructions make use of the B-type immediate encoding for offsets.
    const auto is_b_type = [](uint32_t instruction) { return (instruction & 0x7F) == 0b1100011; };
    // JAL makes use of the J-type immediate encoding for offsets.
    const auto is_j_type = [](uint32_t instruction) { return (instruction & 0x7F) == 0b1101111; };
    // C.BEQZ and C.BNEZ make use of this encoding type.
    const auto is_cb_type = [](uint32_t instruction) {
        const auto op = instruction & 0b11;
        const auto funct3 = instruction & 0xE000;
        return op == 0b01 && funct3 >= 0xC000;
    };
    // C.JAL and C.J make use of this encoding type.
    const auto is_cj_type = [](uint32_t instruction) {
        const auto op = instruction & 0b11;
        const auto funct3 = instruction & 0xE000;
        return op == 0b01 && (funct3 == 0x2000 || funct3 == 0xA000);
    };
    // If we know an instruction is a compressed branch, then it's a 16-bit instruction
    // Otherwise it's a regular-sized 32-bit instruction.
    const auto determine_inst_size = [&](uint32_t instruction) -> size_t {
        if (is_cj_type(instruction) || is_cb_type(instruction)) {
            return 2;
        } else {
            return 4;
        }
    };

    const auto label_location = *label->GetLocation();

    for (const auto offset : label->m_offsets) {
        const auto address = m_buffer.GetOffsetAddress(offset);
        auto* const ptr = reinterpret_cast<uint8_t*>(address);
        const auto inst_size = determine_inst_size(uint32_t{*ptr} | (uint32_t{*(ptr + 1)} << 8));

        uint32_t instruction = 0;
        std::memcpy(&instruction, ptr, inst_size);

        // Given all branch instructions we need to patch have 0 encoded as
        // their branch offset, we don't need to worry about any masking work.
        //
        // It's enough to verify that the immediate is going to be valid
        // and then OR it into the instruction.

        const auto encoded_offset = label_location - offset;

        if (inst_size == sizeof(uint32_t)) {
            if (is_b_type(instruction)) {
                BISCUIT_ASSERT(IsValidBTypeImm(encoded_offset));
                instruction |= TransformToBTypeImm(static_cast<uint32_t>(encoded_offset));
            } else if (is_j_type(instruction)) {
                BISCUIT_ASSERT(IsValidJTypeImm(encoded_offset));
                instruction |= TransformToJTypeImm(static_cast<uint32_t>(encoded_offset));
            }
        } else {
            if (is_cb_type(instruction)) {
                BISCUIT_ASSERT(IsValidCBTypeImm(encoded_offset));
                instruction |= TransformToCBTypeImm(static_cast<uint32_t>(encoded_offset));
            } else if (is_cj_type(instruction)) {
                BISCUIT_ASSERT(IsValidCJTypeImm(encoded_offset));
                instruction |= TransformToCJTypeImm(static_cast<uint32_t>(encoded_offset));
            }
        }

        std::memcpy(ptr, &instruction, inst_size);
    }
}

void Assembler::ResolveLiteralOffsetsRaw(ptrdiff_t location, const std::set<ptrdiff_t>& offsets) {
    [[maybe_unused]] const auto is_auipc_type = [](uint32_t instruction) { return (instruction & 0x7F) == 0b0010111; };

    const auto is_gpr_load_type = [](uint32_t instruction) { return (instruction & 0x7F) == 0b0000011; };

    for (const auto offset : offsets) {
        const auto address = m_buffer.GetOffsetAddress(offset);
        auto* const ptr = reinterpret_cast<uint8_t*>(address);

        std::array<uint32_t, 2> instructions{};
        std::memcpy(&instructions[0], ptr, sizeof(uint32_t));
        std::memcpy(&instructions[1], ptr + sizeof(uint32_t), sizeof(uint32_t));

        // Given all load instructions we need to patch have 0 encoded as
        // their load offset, we don't need to worry about any masking work.
        //
        // It's enough to verify that the immediate is going to be valid
        // and then OR it into the instruction.

        const auto encoded_offset = location - offset;

        BISCUIT_ASSERT(is_auipc_type(instructions[0]));

        // Make sure the distance is within the bounds of a 32-bit signed integer.
        BISCUIT_ASSERT((static_cast<int64_t>(encoded_offset << 32) >> 32) == encoded_offset);

        if (is_gpr_load_type(instructions[1])) {
            const auto high20 = static_cast<uint32_t>(encoded_offset & 0xFFFFF000);
            const auto low12 = static_cast<uint32_t>(encoded_offset & 0xFFF);
            instructions[0] |= high20;
            instructions[1] |= low12 << 20;
        } else {
            BISCUIT_ASSERT(false);
        }

        std::memcpy(ptr, &instructions[0], sizeof(uint32_t));
        std::memcpy(ptr + sizeof(uint32_t), &instructions[1], sizeof(uint32_t));
    }
}

} // namespace biscuit
