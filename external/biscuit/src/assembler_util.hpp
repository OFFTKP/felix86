#pragma once

#include <biscuit/assert.hpp>
#include <biscuit/code_buffer.hpp>
#include <biscuit/registers.hpp>

#include <cstddef>
#include <cstdint>

// Generic internal utility header for various helper functions related
// to encoding instructions.

namespace biscuit {
// Determines if a value lies within the range of a 6-bit immediate.
[[nodiscard]] constexpr bool IsValidSigned6BitImm(ptrdiff_t value) {
    return value >= -32 && value <= 31;
}

// S-type and I-type immediates are 12 bits in size
[[nodiscard]] constexpr bool IsValidSigned12BitImm(ptrdiff_t value) {
    return value >= -2048 && value <= 2047;
}

// B-type immediates only provide -4KiB to +4KiB range branches.
[[nodiscard]] constexpr bool IsValidBTypeImm(ptrdiff_t value) {
    return value >= -4096 && value <= 4095;
}

// J-type immediates only provide -1MiB to +1MiB range branches.
[[nodiscard]] constexpr bool IsValidJTypeImm(ptrdiff_t value) {
    return value >= -0x80000 && value <= 0x7FFFF;
}

// CB-type immediates only provide -256B to +256B range branches.
[[nodiscard]] constexpr bool IsValidCBTypeImm(ptrdiff_t value) {
    return value >= -256 && value <= 255;
}

// CJ-type immediates only provide -2KiB to +2KiB range branches.
[[nodiscard]] constexpr bool IsValidCJTypeImm(ptrdiff_t value) {
    return IsValidSigned12BitImm(value);
}

// Determines whether or not the register fits in 3-bit compressed encoding.
[[nodiscard]] constexpr bool IsValid3BitCompressedReg(Register reg) {
    const auto index = reg.Index();
    return index >= 8 && index <= 15;
}

// Determines whether or not the given shift amount is valid for a compressed shift instruction
[[nodiscard]] constexpr bool IsValidCompressedShiftAmount(uint32_t shift) {
    return shift > 0 && shift <= 64;
}

// Turns a compressed register into its encoding.
[[nodiscard]] constexpr uint32_t CompressedRegTo3BitEncoding(Register reg) {
    return reg.Index() - 8;
}

// Transforms a regular value into an immediate encoded in a B-type instruction.
[[nodiscard]] constexpr uint32_t TransformToBTypeImm(uint32_t imm) {
    // clang-format off
    return ((imm & 0x07E0) << 20) |
           ((imm & 0x1000) << 19) |
           ((imm & 0x001E) << 7) |
           ((imm & 0x0800) >> 4);
    // clang-format on
}

// Transforms a regular value into an immediate encoded in a J-type instruction.
[[nodiscard]] constexpr uint32_t TransformToJTypeImm(uint32_t imm) {
    // clang-format off
    return ((imm & 0x0FF000) >> 0) |
           ((imm & 0x000800) << 9) |
           ((imm & 0x0007FE) << 20) |
           ((imm & 0x100000) << 11);
    // clang-format on
}

// Transforms a regular value into an immediate encoded in a CB-type instruction.
[[nodiscard]] constexpr uint32_t TransformToCBTypeImm(uint32_t imm) {
    // clang-format off
    return ((imm & 0x0C0) >> 1) |
           ((imm & 0x006) << 2) |
           ((imm & 0x020) >> 3) |
           ((imm & 0x018) << 7) |
           ((imm & 0x100) << 4);
    // clang-format on
}

// Transforms a regular value into an immediate encoded in a CJ-type instruction.
[[nodiscard]] constexpr uint32_t TransformToCJTypeImm(uint32_t imm) {
    // clang-format off
    return ((imm & 0x800) << 1) |
           ((imm & 0x010) << 7) |
           ((imm & 0x300) << 1) |
           ((imm & 0x400) >> 2) |
           ((imm & 0x040) << 1) |
           ((imm & 0x080) >> 1) |
           ((imm & 0x00E) << 2) |
           ((imm & 0x020) >> 3);
    // clang-format on
}

// Emits a B type RISC-V instruction. These consist of:
// imm[12|10:5] | rs2 | rs1 | funct3 | imm[4:1] | imm[11] | opcode
inline void EmitBType(CodeBuffer& buffer, uint32_t imm, GPR rs2, GPR rs1,
                      uint32_t funct3, uint32_t opcode) {
    imm &= 0x1FFE;

    buffer.Emit32(TransformToBTypeImm(imm) | (rs2.Index() << 20) | (rs1.Index() << 15) |
                  ((funct3 & 0b111) << 12) | (opcode & 0x7F));
}

// Emits a I type RISC-V instruction. These consist of:
// imm[11:0] | rs1 | funct3 | rd | opcode
inline void EmitIType(CodeBuffer& buffer, uint32_t imm, Register rs1, uint32_t funct3,
                      Register rd, uint32_t opcode) {
    imm &= 0xFFF;

    buffer.Emit32((imm << 20) | (rs1.Index() << 15) | ((funct3 & 0b111) << 12) |
                  (rd.Index() << 7) | (opcode & 0x7F));
}

// Emits a J type RISC-V instruction. These consist of:
// imm[20|10:1|11|19:12] | rd | opcode
inline void EmitJType(CodeBuffer& buffer, uint32_t imm, GPR rd, uint32_t opcode) {
    imm &= 0x1FFFFE;

    buffer.Emit32(TransformToJTypeImm(imm) | rd.Index() << 7 | (opcode & 0x7F));
}

// Emits a R type RISC instruction. These consist of:
// funct7 | rs2 | rs1 | funct3 | rd | opcode
inline void EmitRType(CodeBuffer& buffer, uint32_t funct7, Register rs2, Register rs1,
                      uint32_t funct3, Register rd, uint32_t opcode) {
    // clang-format off
    const auto value = ((funct7 & 0xFF) << 25) |
                       (rs2.Index() << 20) |
                       (rs1.Index() << 15) |
                       ((funct3 & 0b111) << 12) |
                       (rd.Index() << 7) |
                       (opcode & 0x7F);
    // clang-format off

    buffer.Emit32(value);
}

// Emits a R type RISC instruction. These consist of:
// funct7 | rs2 | rs1 | funct3 | rd | opcode
inline void EmitRType(CodeBuffer& buffer, uint32_t funct7, FPR rs2, FPR rs1, RMode funct3,
                      FPR rd, uint32_t opcode) {
    EmitRType(buffer, funct7, rs2, rs1, static_cast<uint32_t>(funct3), rd, opcode);
}

// Emits a R4 type RISC instruction. These consist of:
// rs3 | funct2 | rs2 | rs1 | funct3 | rd | opcode
inline void EmitR4Type(CodeBuffer& buffer, FPR rs3, uint32_t funct2, FPR rs2, FPR rs1,
                       RMode funct3, FPR rd, uint32_t opcode) {
    const auto reg_bits = (rs3.Index() << 27) | (rs2.Index() << 20) | (rs1.Index() << 15) | (rd.Index() << 7);
    const auto funct_bits = ((funct2 & 0b11) << 25) | (static_cast<uint32_t>(funct3) << 12);
    buffer.Emit32(reg_bits | funct_bits | (opcode & 0x7F));
}

// Emits a S type RISC-V instruction. These consist of:
// imm[11:5] | rs2 | rs1 | funct3 | imm[4:0] | opcode
inline void EmitSType(CodeBuffer& buffer, uint32_t imm, Register rs2, GPR rs1,
                      uint32_t funct3, uint32_t opcode) {
    imm &= 0xFFF;

    // clang-format off
    const auto new_imm = ((imm & 0x01F) << 7) |
                         ((imm & 0xFE0) << 20);
    // clang-format on

    buffer.Emit32(new_imm | (rs2.Index() << 20) | (rs1.Index() << 15) |
                  ((funct3 & 0b111) << 12) | (opcode & 0x7F));
}

// Emits a U type RISC-V instruction. These consist of:
// imm[31:12] | rd | opcode
inline void EmitUType(CodeBuffer& buffer, uint32_t imm, GPR rd, uint32_t opcode) {
    buffer.Emit32((imm & 0x000FFFFF) << 12 | rd.Index() << 7 | (opcode & 0x7F));
}

// Emits an atomic instruction.
inline void EmitAtomic(CodeBuffer& buffer, uint32_t funct5, Ordering ordering, GPR rs2, GPR rs1,
                       uint32_t funct3, GPR rd, uint32_t opcode) noexcept {
    const auto funct7 = (funct5 << 2) | static_cast<uint32_t>(ordering);
    EmitRType(buffer, funct7, rs2, rs1, funct3, rd, opcode);
}

// Emits a fence instruction
inline void EmitFENCE(CodeBuffer& buffer, uint32_t fm, FenceOrder pred, FenceOrder succ,
                      GPR rs, uint32_t funct3, GPR rd, uint32_t opcode) noexcept {
    // clang-format off
    buffer.Emit32(((fm & 0b1111) << 28) |
                  (static_cast<uint32_t>(pred) << 24) |
                  (static_cast<uint32_t>(succ) << 20) |
                  (rs.Index() << 15) |
                  ((funct3 & 0b111) << 12) |
                  (rd.Index() << 7) |
                  (opcode & 0x7F));
    // clang-format on
}

// Maybe OPs (see Zcmop/Zimop extension)
inline void EmitMOP_R(CodeBuffer& buffer, uint32_t op, GPR rd, GPR rs) {
    constexpr auto base = 0x81C04073U;
    const auto encoded_op = ((op & 0b11) << 20) | ((op & 0b1100) << 24) | (op & 0b10000) << 26;

    buffer.Emit32(base | encoded_op | (rd.Index() << 7) | (rs.Index() << 15));
}
inline void EmitMOP_RR(CodeBuffer& buffer, uint32_t op, GPR rd, GPR rs1, GPR rs2) {
    constexpr auto base = 0x82004073U;
    const auto encoded_op = ((op & 0b11) << 26) | ((op & 0b100) << 28);

    buffer.Emit32(base | encoded_op | (rd.Index() << 7) | (rs1.Index() << 15) | (rs2.Index() << 20));
}
inline void EmitCMOP(CodeBuffer& buffer, uint32_t op) {
    constexpr auto base = 0x6081U;
    const auto encoded_op = (op & 0b1110) << 7;

    buffer.Emit16(base | encoded_op);
}

// Internal helpers for siloing away particular comparisons for behavior.
constexpr bool IsRV32(ArchFeature feature) {
    return feature == ArchFeature::RV32;
}
constexpr bool IsRV64(ArchFeature feature) {
    return feature == ArchFeature::RV64;
}
constexpr bool IsRV128(ArchFeature feature) {
    return feature == ArchFeature::RV128;
}
constexpr bool IsRV32OrRV64(ArchFeature feature) {
    return IsRV32(feature) || IsRV64(feature);
}
constexpr bool IsRV64OrRV128(ArchFeature feature) {
    return IsRV64(feature) || IsRV128(feature);
}

} // namespace biscuit
