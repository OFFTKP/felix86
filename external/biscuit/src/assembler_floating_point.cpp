#include <biscuit/assert.hpp>
#include <biscuit/assembler.hpp>

#include <algorithm>
#include <array>
#include <cstring>
#include <iterator>

#include "assembler_util.hpp"

// Various floating-point-based extension instructions.

namespace biscuit {

// RV32F Extension Instructions

void Assembler::FADD_S(FPR rd, FPR rs1, FPR rs2, RMode rmode) noexcept {
    EmitRType(m_buffer, 0b0000000, rs2, rs1, rmode, rd, 0b1010011);
}
void Assembler::FCLASS_S(GPR rd, FPR rs1) noexcept {
    EmitRType(m_buffer, 0b1110000, f0, rs1, 0b001, rd, 0b1010011);
}
void Assembler::FCVT_S_W(FPR rd, GPR rs1, RMode rmode) noexcept {
    EmitRType(m_buffer, 0b1101000, f0, rs1, static_cast<uint32_t>(rmode), rd, 0b1010011);
}
void Assembler::FCVT_S_WU(FPR rd, GPR rs1, RMode rmode) noexcept {
    EmitRType(m_buffer, 0b1101000, f1, rs1, static_cast<uint32_t>(rmode), rd, 0b1010011);
}
void Assembler::FCVT_W_S(GPR rd, FPR rs1, RMode rmode) noexcept {
    EmitRType(m_buffer, 0b1100000, f0, rs1, static_cast<uint32_t>(rmode), rd, 0b1010011);
}
void Assembler::FCVT_WU_S(GPR rd, FPR rs1, RMode rmode) noexcept {
    EmitRType(m_buffer, 0b1100000, f1, rs1, static_cast<uint32_t>(rmode), rd, 0b1010011);
}
void Assembler::FDIV_S(FPR rd, FPR rs1, FPR rs2, RMode rmode) noexcept {
    EmitRType(m_buffer, 0b0001100, rs2, rs1, rmode, rd, 0b1010011);
}
void Assembler::FEQ_S(GPR rd, FPR rs1, FPR rs2) noexcept {
    EmitRType(m_buffer, 0b1010000, rs2, rs1, 0b010, rd, 0b1010011);
}
void Assembler::FLE_S(GPR rd, FPR rs1, FPR rs2) noexcept {
    EmitRType(m_buffer, 0b1010000, rs2, rs1, 0b000, rd, 0b1010011);
}
void Assembler::FLT_S(GPR rd, FPR rs1, FPR rs2) noexcept {
    EmitRType(m_buffer, 0b1010000, rs2, rs1, 0b001, rd, 0b1010011);
}
void Assembler::FLW(FPR rd, int32_t offset, GPR rs) noexcept {
    BISCUIT_ASSERT(IsValidSigned12BitImm(offset));

    if (IsOptimizationEnabled(Optimization::AutoCompress) && IsRV32(m_features)) {
        if (rs == sp && offset >= 0 && offset <= 252 && (offset & 0b11) == 0) {
            C_FLWSP(rd, static_cast<uint32_t>(offset));
            return;
        } else if (offset >= 0 && offset <= 124 && (offset & 0b11) == 0 && IsValid3BitCompressedReg(rd) && IsValid3BitCompressedReg(rs)) {
            C_FLW(rd, static_cast<uint32_t>(offset), rs);
            return;
        }
    }

    EmitIType(m_buffer, static_cast<uint32_t>(offset), rs, 0b010, rd, 0b0000111);
}
void Assembler::FMADD_S(FPR rd, FPR rs1, FPR rs2, FPR rs3, RMode rmode) noexcept {
    EmitR4Type(m_buffer, rs3, 0b00, rs2, rs1, rmode, rd, 0b1000011);
}
void Assembler::FMAX_S(FPR rd, FPR rs1, FPR rs2) noexcept {
    EmitRType(m_buffer, 0b0010100, rs2, rs1, 0b001, rd, 0b1010011);
}
void Assembler::FMIN_S(FPR rd, FPR rs1, FPR rs2) noexcept {
    EmitRType(m_buffer, 0b0010100, rs2, rs1, 0b000, rd, 0b1010011);
}
void Assembler::FMSUB_S(FPR rd, FPR rs1, FPR rs2, FPR rs3, RMode rmode) noexcept {
    EmitR4Type(m_buffer, rs3, 0b00, rs2, rs1, rmode, rd, 0b1000111);
}
void Assembler::FMUL_S(FPR rd, FPR rs1, FPR rs2, RMode rmode) noexcept {
    EmitRType(m_buffer, 0b0001000, rs2, rs1, rmode, rd, 0b1010011);
}
void Assembler::FMV_W_X(FPR rd, GPR rs1) noexcept {
    EmitRType(m_buffer, 0b1111000, f0, rs1, 0b000, rd, 0b1010011);
}
void Assembler::FMV_X_W(GPR rd, FPR rs1) noexcept {
    EmitRType(m_buffer, 0b1110000, f0, rs1, 0b000, rd, 0b1010011);
}
void Assembler::FNMADD_S(FPR rd, FPR rs1, FPR rs2, FPR rs3, RMode rmode) noexcept {
    EmitR4Type(m_buffer, rs3, 0b00, rs2, rs1, rmode, rd, 0b1001111);
}
void Assembler::FNMSUB_S(FPR rd, FPR rs1, FPR rs2, FPR rs3, RMode rmode) noexcept {
    EmitR4Type(m_buffer, rs3, 0b00, rs2, rs1, rmode, rd, 0b1001011);
}
void Assembler::FSGNJ_S(FPR rd, FPR rs1, FPR rs2) noexcept {
    EmitRType(m_buffer, 0b0010000, rs2, rs1, 0b000, rd, 0b1010011);
}
void Assembler::FSGNJN_S(FPR rd, FPR rs1, FPR rs2) noexcept {
    EmitRType(m_buffer, 0b0010000, rs2, rs1, 0b001, rd, 0b1010011);
}
void Assembler::FSGNJX_S(FPR rd, FPR rs1, FPR rs2) noexcept {
    EmitRType(m_buffer, 0b0010000, rs2, rs1, 0b010, rd, 0b1010011);
}
void Assembler::FSQRT_S(FPR rd, FPR rs1, RMode rmode) noexcept {
    EmitRType(m_buffer, 0b0101100, f0, rs1, rmode, rd, 0b1010011);
}
void Assembler::FSUB_S(FPR rd, FPR rs1, FPR rs2, RMode rmode) noexcept {
    EmitRType(m_buffer, 0b0000100, rs2, rs1, rmode, rd, 0b1010011);
}
void Assembler::FSW(FPR rs2, int32_t offset, GPR rs1) noexcept {
    BISCUIT_ASSERT(IsValidSigned12BitImm(offset));

    if (IsOptimizationEnabled(Optimization::AutoCompress) && IsRV32(m_features)) {
        if (rs1 == sp && offset >= 0 && offset <= 252 && (offset & 0b11) == 0) {
            C_FSWSP(rs2, static_cast<uint32_t>(offset));
            return;
        } else if (offset >= 0 && offset <= 124 && (offset & 0b11) == 0 && IsValid3BitCompressedReg(rs2) && IsValid3BitCompressedReg(rs1)) {
            C_FSW(rs2, static_cast<uint32_t>(offset), rs1);
            return;
        }
    }

    EmitSType(m_buffer, static_cast<uint32_t>(offset), rs2, rs1, 0b010, 0b0100111);
}

void Assembler::FABS_S(FPR rd, FPR rs) noexcept {
    FSGNJX_S(rd, rs, rs);
}
void Assembler::FMV_S(FPR rd, FPR rs) noexcept {
    FSGNJ_S(rd, rs, rs);
}
void Assembler::FNEG_S(FPR rd, FPR rs) noexcept {
    FSGNJN_S(rd, rs, rs);
}

// RV64F Extension Instructions

void Assembler::FCVT_L_S(GPR rd, FPR rs1, RMode rmode) noexcept {
    BISCUIT_ASSERT(IsRV64(m_features));
    EmitRType(m_buffer, 0b1100000, f2, rs1, static_cast<uint32_t>(rmode), rd, 0b1010011);
}
void Assembler::FCVT_LU_S(GPR rd, FPR rs1, RMode rmode) noexcept {
    BISCUIT_ASSERT(IsRV64(m_features));
    EmitRType(m_buffer, 0b1100000, f3, rs1, static_cast<uint32_t>(rmode), rd, 0b1010011);
}
void Assembler::FCVT_S_L(FPR rd, GPR rs1, RMode rmode) noexcept {
    BISCUIT_ASSERT(IsRV64(m_features));
    EmitRType(m_buffer, 0b1101000, f2, rs1, static_cast<uint32_t>(rmode), rd, 0b1010011);
}
void Assembler::FCVT_S_LU(FPR rd, GPR rs1, RMode rmode) noexcept {
    BISCUIT_ASSERT(IsRV64(m_features));
    EmitRType(m_buffer, 0b1101000, f3, rs1, static_cast<uint32_t>(rmode), rd, 0b1010011);
}

// RV32D Extension Instructions

void Assembler::FADD_D(FPR rd, FPR rs1, FPR rs2, RMode rmode) noexcept {
    EmitRType(m_buffer, 0b0000001, rs2, rs1, rmode, rd, 0b1010011);
}
void Assembler::FCLASS_D(GPR rd, FPR rs1) noexcept {
    EmitRType(m_buffer, 0b1110001, f0, rs1, 0b001, rd, 0b1010011);
}
void Assembler::FCVT_D_W(FPR rd, GPR rs1, RMode rmode) noexcept {
    EmitRType(m_buffer, 0b1101001, f0, rs1, static_cast<uint32_t>(rmode), rd, 0b1010011);
}
void Assembler::FCVT_D_WU(FPR rd, GPR rs1, RMode rmode) noexcept {
    EmitRType(m_buffer, 0b1101001, f1, rs1, static_cast<uint32_t>(rmode), rd, 0b1010011);
}
void Assembler::FCVT_W_D(GPR rd, FPR rs1, RMode rmode) noexcept {
    EmitRType(m_buffer, 0b1100001, f0, rs1, static_cast<uint32_t>(rmode), rd, 0b1010011);
}
void Assembler::FCVT_WU_D(GPR rd, FPR rs1, RMode rmode) noexcept {
    EmitRType(m_buffer, 0b1100001, f1, rs1, static_cast<uint32_t>(rmode), rd, 0b1010011);
}
void Assembler::FCVT_D_S(FPR rd, FPR rs1, RMode rmode) noexcept {
    EmitRType(m_buffer, 0b0100001, f0, rs1, static_cast<uint32_t>(rmode), rd, 0b1010011);
}
void Assembler::FCVT_S_D(FPR rd, FPR rs1, RMode rmode) noexcept {
    EmitRType(m_buffer, 0b0100000, f1, rs1, static_cast<uint32_t>(rmode), rd, 0b1010011);
}
void Assembler::FDIV_D(FPR rd, FPR rs1, FPR rs2, RMode rmode) noexcept {
    EmitRType(m_buffer, 0b0001101, rs2, rs1, rmode, rd, 0b1010011);
}
void Assembler::FEQ_D(GPR rd, FPR rs1, FPR rs2) noexcept {
    EmitRType(m_buffer, 0b1010001, rs2, rs1, 0b010, rd, 0b1010011);
}
void Assembler::FLE_D(GPR rd, FPR rs1, FPR rs2) noexcept {
    EmitRType(m_buffer, 0b1010001, rs2, rs1, 0b000, rd, 0b1010011);
}
void Assembler::FLT_D(GPR rd, FPR rs1, FPR rs2) noexcept {
    EmitRType(m_buffer, 0b1010001, rs2, rs1, 0b001, rd, 0b1010011);
}
void Assembler::FLD(FPR rd, int32_t offset, GPR rs) noexcept {
    BISCUIT_ASSERT(IsValidSigned12BitImm(offset));

    if (IsOptimizationEnabled(Optimization::AutoCompress) && IsRV64(m_features)) {
        if (rs == sp && offset >= 0 && offset <= 504 && (offset & 0b111) == 0) {
            C_FLDSP(rd, static_cast<uint32_t>(offset));
            return;
        } else if (offset >= 0 && offset <= 248 && (offset & 0b111) == 0 && IsValid3BitCompressedReg(rd) && IsValid3BitCompressedReg(rs)) {
            C_FLD(rd, static_cast<uint32_t>(offset), rs);
            return;
        }
    }

    EmitIType(m_buffer, static_cast<uint32_t>(offset), rs, 0b011, rd, 0b0000111);
}
void Assembler::FMADD_D(FPR rd, FPR rs1, FPR rs2, FPR rs3, RMode rmode) noexcept {
    EmitR4Type(m_buffer, rs3, 0b01, rs2, rs1, rmode, rd, 0b1000011);
}
void Assembler::FMAX_D(FPR rd, FPR rs1, FPR rs2) noexcept {
    EmitRType(m_buffer, 0b0010101, rs2, rs1, 0b001, rd, 0b1010011);
}
void Assembler::FMIN_D(FPR rd, FPR rs1, FPR rs2) noexcept {
    EmitRType(m_buffer, 0b0010101, rs2, rs1, 0b000, rd, 0b1010011);
}
void Assembler::FMSUB_D(FPR rd, FPR rs1, FPR rs2, FPR rs3, RMode rmode) noexcept {
    EmitR4Type(m_buffer, rs3, 0b01, rs2, rs1, rmode, rd, 0b1000111);
}
void Assembler::FMUL_D(FPR rd, FPR rs1, FPR rs2, RMode rmode) noexcept {
    EmitRType(m_buffer, 0b0001001, rs2, rs1, rmode, rd, 0b1010011);
}
void Assembler::FNMADD_D(FPR rd, FPR rs1, FPR rs2, FPR rs3, RMode rmode) noexcept {
    EmitR4Type(m_buffer, rs3, 0b01, rs2, rs1, rmode, rd, 0b1001111);
}
void Assembler::FNMSUB_D(FPR rd, FPR rs1, FPR rs2, FPR rs3, RMode rmode) noexcept {
    EmitR4Type(m_buffer, rs3, 0b01, rs2, rs1, rmode, rd, 0b1001011);
}
void Assembler::FSGNJ_D(FPR rd, FPR rs1, FPR rs2) noexcept {
    EmitRType(m_buffer, 0b0010001, rs2, rs1, 0b000, rd, 0b1010011);
}
void Assembler::FSGNJN_D(FPR rd, FPR rs1, FPR rs2) noexcept {
    EmitRType(m_buffer, 0b0010001, rs2, rs1, 0b001, rd, 0b1010011);
}
void Assembler::FSGNJX_D(FPR rd, FPR rs1, FPR rs2) noexcept {
    EmitRType(m_buffer, 0b0010001, rs2, rs1, 0b010, rd, 0b1010011);
}
void Assembler::FSQRT_D(FPR rd, FPR rs1, RMode rmode) noexcept {
    EmitRType(m_buffer, 0b0101101, f0, rs1, rmode, rd, 0b1010011);
}
void Assembler::FSUB_D(FPR rd, FPR rs1, FPR rs2, RMode rmode) noexcept {
    EmitRType(m_buffer, 0b0000101, rs2, rs1, rmode, rd, 0b1010011);
}
void Assembler::FSD(FPR rs2, int32_t offset, GPR rs1) noexcept {
    BISCUIT_ASSERT(IsValidSigned12BitImm(offset));

    if (IsOptimizationEnabled(Optimization::AutoCompress) && IsRV64(m_features)) {
        if (rs1 == sp && offset >= 0 && offset <= 504 && (offset & 0b111) == 0) {
            C_FSDSP(rs2, static_cast<uint32_t>(offset));
            return;
        } else if (offset >= 0 && offset <= 248 && (offset & 0b111) == 0 && IsValid3BitCompressedReg(rs2) && IsValid3BitCompressedReg(rs1)) {
            C_FSD(rs2, static_cast<uint32_t>(offset), rs1);
            return;
        }
    }

    EmitSType(m_buffer, static_cast<uint32_t>(offset), rs2, rs1, 0b011, 0b0100111);
}

void Assembler::FABS_D(FPR rd, FPR rs) noexcept {
    FSGNJX_D(rd, rs, rs);
}
void Assembler::FMV_D(FPR rd, FPR rs) noexcept {
    FSGNJ_D(rd, rs, rs);
}
void Assembler::FNEG_D(FPR rd, FPR rs) noexcept {
    FSGNJN_D(rd, rs, rs);
}

// RV64D Extension Instructions

void Assembler::FCVT_L_D(GPR rd, FPR rs1, RMode rmode) noexcept {
    BISCUIT_ASSERT(IsRV64(m_features));
    EmitRType(m_buffer, 0b1100001, f2, rs1, static_cast<uint32_t>(rmode), rd, 0b1010011);
}
void Assembler::FCVT_LU_D(GPR rd, FPR rs1, RMode rmode) noexcept {
    BISCUIT_ASSERT(IsRV64(m_features));
    EmitRType(m_buffer, 0b1100001, f3, rs1, static_cast<uint32_t>(rmode), rd, 0b1010011);
}
void Assembler::FCVT_D_L(FPR rd, GPR rs1, RMode rmode) noexcept {
    BISCUIT_ASSERT(IsRV64(m_features));
    EmitRType(m_buffer, 0b1101001, f2, rs1, static_cast<uint32_t>(rmode), rd, 0b1010011);
}
void Assembler::FCVT_D_LU(FPR rd, GPR rs1, RMode rmode) noexcept {
    BISCUIT_ASSERT(IsRV64(m_features));
    EmitRType(m_buffer, 0b1101001, f3, rs1, static_cast<uint32_t>(rmode), rd, 0b1010011);
}
void Assembler::FMV_D_X(FPR rd, GPR rs1) noexcept {
    BISCUIT_ASSERT(IsRV64OrRV128(m_features));
    EmitRType(m_buffer, 0b1111001, f0, rs1, 0b000, rd, 0b1010011);
}
void Assembler::FMV_X_D(GPR rd, FPR rs1) noexcept {
    BISCUIT_ASSERT(IsRV64OrRV128(m_features));
    EmitRType(m_buffer, 0b1110001, f0, rs1, 0b000, rd, 0b1010011);
}

// RV32Q Extension Instructions

void Assembler::FADD_Q(FPR rd, FPR rs1, FPR rs2, RMode rmode) noexcept {
    EmitRType(m_buffer, 0b0000011, rs2, rs1, rmode, rd, 0b1010011);
}
void Assembler::FCLASS_Q(GPR rd, FPR rs1) noexcept {
    EmitRType(m_buffer, 0b1110011, f0, rs1, 0b001, rd, 0b1010011);
}
void Assembler::FCVT_Q_W(FPR rd, GPR rs1, RMode rmode) noexcept {
    EmitRType(m_buffer, 0b1101011, f0, rs1, static_cast<uint32_t>(rmode), rd, 0b1010011);
}
void Assembler::FCVT_Q_WU(FPR rd, GPR rs1, RMode rmode) noexcept {
    EmitRType(m_buffer, 0b1101011, f1, rs1, static_cast<uint32_t>(rmode), rd, 0b1010011);
}
void Assembler::FCVT_W_Q(GPR rd, FPR rs1, RMode rmode) noexcept {
    EmitRType(m_buffer, 0b1100011, f0, rs1, static_cast<uint32_t>(rmode), rd, 0b1010011);
}
void Assembler::FCVT_WU_Q(GPR rd, FPR rs1, RMode rmode) noexcept {
    EmitRType(m_buffer, 0b1100011, f1, rs1, static_cast<uint32_t>(rmode), rd, 0b1010011);
}
void Assembler::FCVT_Q_D(FPR rd, FPR rs1, RMode rmode) noexcept {
    EmitRType(m_buffer, 0b0100011, f1, rs1, static_cast<uint32_t>(rmode), rd, 0b1010011);
}
void Assembler::FCVT_D_Q(FPR rd, FPR rs1, RMode rmode) noexcept {
    EmitRType(m_buffer, 0b0100001, f3, rs1, static_cast<uint32_t>(rmode), rd, 0b1010011);
}
void Assembler::FCVT_Q_S(FPR rd, FPR rs1, RMode rmode) noexcept {
    EmitRType(m_buffer, 0b0100011, f0, rs1, static_cast<uint32_t>(rmode), rd, 0b1010011);
}
void Assembler::FCVT_S_Q(FPR rd, FPR rs1, RMode rmode) noexcept {
    EmitRType(m_buffer, 0b0100000, f3, rs1, static_cast<uint32_t>(rmode), rd, 0b1010011);
}
void Assembler::FDIV_Q(FPR rd, FPR rs1, FPR rs2, RMode rmode) noexcept {
    EmitRType(m_buffer, 0b0001111, rs2, rs1, rmode, rd, 0b1010011);
}
void Assembler::FEQ_Q(GPR rd, FPR rs1, FPR rs2) noexcept {
    EmitRType(m_buffer, 0b1010011, rs2, rs1, 0b010, rd, 0b1010011);
}
void Assembler::FLE_Q(GPR rd, FPR rs1, FPR rs2) noexcept {
    EmitRType(m_buffer, 0b1010011, rs2, rs1, 0b000, rd, 0b1010011);
}
void Assembler::FLT_Q(GPR rd, FPR rs1, FPR rs2) noexcept {
    EmitRType(m_buffer, 0b1010011, rs2, rs1, 0b001, rd, 0b1010011);
}
void Assembler::FLQ(FPR rd, int32_t offset, GPR rs) noexcept {
    BISCUIT_ASSERT(IsValidSigned12BitImm(offset));
    EmitIType(m_buffer, static_cast<uint32_t>(offset), rs, 0b100, rd, 0b0000111);
}
void Assembler::FMADD_Q(FPR rd, FPR rs1, FPR rs2, FPR rs3, RMode rmode) noexcept {
    EmitR4Type(m_buffer, rs3, 0b11, rs2, rs1, rmode, rd, 0b1000011);
}
void Assembler::FMAX_Q(FPR rd, FPR rs1, FPR rs2) noexcept {
    EmitRType(m_buffer, 0b0010111, rs2, rs1, 0b001, rd, 0b1010011);
}
void Assembler::FMIN_Q(FPR rd, FPR rs1, FPR rs2) noexcept {
    EmitRType(m_buffer, 0b0010111, rs2, rs1, 0b000, rd, 0b1010011);
}
void Assembler::FMSUB_Q(FPR rd, FPR rs1, FPR rs2, FPR rs3, RMode rmode) noexcept {
    EmitR4Type(m_buffer, rs3, 0b11, rs2, rs1, rmode, rd, 0b1000111);
}
void Assembler::FMUL_Q(FPR rd, FPR rs1, FPR rs2, RMode rmode) noexcept {
    EmitRType(m_buffer, 0b0001011, rs2, rs1, rmode, rd, 0b1010011);
}
void Assembler::FNMADD_Q(FPR rd, FPR rs1, FPR rs2, FPR rs3, RMode rmode) noexcept {
    EmitR4Type(m_buffer, rs3, 0b11, rs2, rs1, rmode, rd, 0b1001111);
}
void Assembler::FNMSUB_Q(FPR rd, FPR rs1, FPR rs2, FPR rs3, RMode rmode) noexcept {
    EmitR4Type(m_buffer, rs3, 0b11, rs2, rs1, rmode, rd, 0b1001011);
}
void Assembler::FSGNJ_Q(FPR rd, FPR rs1, FPR rs2) noexcept {
    EmitRType(m_buffer, 0b0010011, rs2, rs1, 0b000, rd, 0b1010011);
}
void Assembler::FSGNJN_Q(FPR rd, FPR rs1, FPR rs2) noexcept {
    EmitRType(m_buffer, 0b0010011, rs2, rs1, 0b001, rd, 0b1010011);
}
void Assembler::FSGNJX_Q(FPR rd, FPR rs1, FPR rs2) noexcept {
    EmitRType(m_buffer, 0b0010011, rs2, rs1, 0b010, rd, 0b1010011);
}
void Assembler::FSQRT_Q(FPR rd, FPR rs1, RMode rmode) noexcept {
    EmitRType(m_buffer, 0b0101111, f0, rs1, rmode, rd, 0b1010011);
}
void Assembler::FSUB_Q(FPR rd, FPR rs1, FPR rs2, RMode rmode) noexcept {
    EmitRType(m_buffer, 0b0000111, rs2, rs1, rmode, rd, 0b1010011);
}
void Assembler::FSQ(FPR rs2, int32_t offset, GPR rs1) noexcept {
    BISCUIT_ASSERT(IsValidSigned12BitImm(offset));
    EmitSType(m_buffer, static_cast<uint32_t>(offset), rs2, rs1, 0b100, 0b0100111);
}

void Assembler::FABS_Q(FPR rd, FPR rs) noexcept {
    FSGNJX_Q(rd, rs, rs);
}
void Assembler::FMV_Q(FPR rd, FPR rs) noexcept {
    FSGNJ_Q(rd, rs, rs);
}
void Assembler::FNEG_Q(FPR rd, FPR rs) noexcept {
    FSGNJN_Q(rd, rs, rs);
}

// RV64Q Extension Instructions

void Assembler::FCVT_L_Q(GPR rd, FPR rs1, RMode rmode) noexcept {
    BISCUIT_ASSERT(IsRV64(m_features));
    EmitRType(m_buffer, 0b1100011, f2, rs1, static_cast<uint32_t>(rmode), rd, 0b1010011);
}
void Assembler::FCVT_LU_Q(GPR rd, FPR rs1, RMode rmode) noexcept {
    BISCUIT_ASSERT(IsRV64(m_features));
    EmitRType(m_buffer, 0b1100011, f3, rs1, static_cast<uint32_t>(rmode), rd, 0b1010011);
}
void Assembler::FCVT_Q_L(FPR rd, GPR rs1, RMode rmode) noexcept {
    BISCUIT_ASSERT(IsRV64(m_features));
    EmitRType(m_buffer, 0b1101011, f2, rs1, static_cast<uint32_t>(rmode), rd, 0b1010011);
}
void Assembler::FCVT_Q_LU(FPR rd, GPR rs1, RMode rmode) noexcept {
    BISCUIT_ASSERT(IsRV64(m_features));
    EmitRType(m_buffer, 0b1101011, f3, rs1, static_cast<uint32_t>(rmode), rd, 0b1010011);
}

// RV32Zfh Extension Instructions

void Assembler::FADD_H(FPR rd, FPR rs1, FPR rs2, RMode rmode) noexcept {
    EmitRType(m_buffer, 0b0000010, rs2, rs1, rmode, rd, 0b1010011);
}
void Assembler::FCLASS_H(GPR rd, FPR rs1) noexcept {
    EmitRType(m_buffer, 0b1110010, f0, rs1, 0b001, rd, 0b1010011);
}
void Assembler::FCVT_D_H(FPR rd, FPR rs1, RMode rmode) noexcept {
    EmitRType(m_buffer, 0b0100001, f2, rs1, static_cast<uint32_t>(rmode), rd, 0b1010011);
}
void Assembler::FCVT_H_D(FPR rd, FPR rs1, RMode rmode) noexcept {
    EmitRType(m_buffer, 0b0100010, f1, rs1, static_cast<uint32_t>(rmode), rd, 0b1010011);
}
void Assembler::FCVT_H_Q(FPR rd, FPR rs1, RMode rmode) noexcept {
    EmitRType(m_buffer, 0b0100010, f3, rs1, static_cast<uint32_t>(rmode), rd, 0b1010011);
}
void Assembler::FCVT_H_S(FPR rd, FPR rs1, RMode rmode) noexcept {
    EmitRType(m_buffer, 0b0100010, f0, rs1, static_cast<uint32_t>(rmode), rd, 0b1010011);
}
void Assembler::FCVT_H_W(FPR rd, GPR rs1, RMode rmode) noexcept {
    EmitRType(m_buffer, 0b1101010, f0, rs1, static_cast<uint32_t>(rmode), rd, 0b1010011);
}
void Assembler::FCVT_H_WU(FPR rd, GPR rs1, RMode rmode) noexcept {
    EmitRType(m_buffer, 0b1101010, f1, rs1, static_cast<uint32_t>(rmode), rd, 0b1010011);
}
void Assembler::FCVT_Q_H(FPR rd, FPR rs1, RMode rmode) noexcept {
    EmitRType(m_buffer, 0b0100011, f2, rs1, static_cast<uint32_t>(rmode), rd, 0b1010011);
}
void Assembler::FCVT_S_H(FPR rd, FPR rs1, RMode rmode) noexcept {
    EmitRType(m_buffer, 0b0100000, f2, rs1, static_cast<uint32_t>(rmode), rd, 0b1010011);
}
void Assembler::FCVT_W_H(GPR rd, FPR rs1, RMode rmode) noexcept {
    EmitRType(m_buffer, 0b1100010, f0, rs1, static_cast<uint32_t>(rmode), rd, 0b1010011);
}
void Assembler::FCVT_WU_H(GPR rd, FPR rs1, RMode rmode) noexcept {
    EmitRType(m_buffer, 0b1100010, f1, rs1, static_cast<uint32_t>(rmode), rd, 0b1010011);
}
void Assembler::FDIV_H(FPR rd, FPR rs1, FPR rs2, RMode rmode) noexcept {
    EmitRType(m_buffer, 0b0001110, rs2, rs1, rmode, rd, 0b1010011);
}
void Assembler::FEQ_H(GPR rd, FPR rs1, FPR rs2) noexcept {
    EmitRType(m_buffer, 0b1010010, rs2, rs1, 0b010, rd, 0b1010011);
}
void Assembler::FLE_H(GPR rd, FPR rs1, FPR rs2) noexcept {
    EmitRType(m_buffer, 0b1010010, rs2, rs1, 0b000, rd, 0b1010011);
}
void Assembler::FLH(FPR rd, int32_t offset, GPR rs) noexcept {
    BISCUIT_ASSERT(IsValidSigned12BitImm(offset));
    EmitIType(m_buffer, static_cast<uint32_t>(offset), rs, 0b001, rd, 0b0000111);
}
void Assembler::FLT_H(GPR rd, FPR rs1, FPR rs2) noexcept {
    EmitRType(m_buffer, 0b1010010, rs2, rs1, 0b001, rd, 0b1010011);
}
void Assembler::FMADD_H(FPR rd, FPR rs1, FPR rs2, FPR rs3, RMode rmode) noexcept {
    EmitR4Type(m_buffer, rs3, 0b10, rs2, rs1, rmode, rd, 0b1000011);
}
void Assembler::FMAX_H(FPR rd, FPR rs1, FPR rs2) noexcept {
    EmitRType(m_buffer, 0b0010110, rs2, rs1, 0b001, rd, 0b1010011);
}
void Assembler::FMIN_H(FPR rd, FPR rs1, FPR rs2) noexcept {
    EmitRType(m_buffer, 0b0010110, rs2, rs1, 0b000, rd, 0b1010011);
}
void Assembler::FMSUB_H(FPR rd, FPR rs1, FPR rs2, FPR rs3, RMode rmode) noexcept {
    EmitR4Type(m_buffer, rs3, 0b10, rs2, rs1, rmode, rd, 0b1000111);
}
void Assembler::FMUL_H(FPR rd, FPR rs1, FPR rs2, RMode rmode) noexcept {
    EmitRType(m_buffer, 0b0001010, rs2, rs1, rmode, rd, 0b1010011);
}
void Assembler::FMV_H_X(FPR rd, GPR rs1) noexcept {
    EmitRType(m_buffer, 0b1111010, f0, rs1, 0b000, rd, 0b1010011);
}
void Assembler::FMV_X_H(GPR rd, FPR rs1) noexcept {
    EmitRType(m_buffer, 0b1110010, f0, rs1, 0b000, rd, 0b1010011);
}
void Assembler::FNMADD_H(FPR rd, FPR rs1, FPR rs2, FPR rs3, RMode rmode) noexcept {
    EmitR4Type(m_buffer, rs3, 0b10, rs2, rs1, rmode, rd, 0b1001111);
}
void Assembler::FNMSUB_H(FPR rd, FPR rs1, FPR rs2, FPR rs3, RMode rmode) noexcept {
    EmitR4Type(m_buffer, rs3, 0b10, rs2, rs1, rmode, rd, 0b1001011);
}
void Assembler::FSGNJ_H(FPR rd, FPR rs1, FPR rs2) noexcept {
    EmitRType(m_buffer, 0b0010010, rs2, rs1, 0b000, rd, 0b1010011);
}
void Assembler::FSGNJN_H(FPR rd, FPR rs1, FPR rs2) noexcept {
    EmitRType(m_buffer, 0b0010010, rs2, rs1, 0b001, rd, 0b1010011);
}
void Assembler::FSGNJX_H(FPR rd, FPR rs1, FPR rs2) noexcept {
    EmitRType(m_buffer, 0b0010010, rs2, rs1, 0b010, rd, 0b1010011);
}
void Assembler::FSH(FPR rs2, int32_t offset, GPR rs1) noexcept {
    BISCUIT_ASSERT(IsValidSigned12BitImm(offset));
    EmitSType(m_buffer, static_cast<uint32_t>(offset), rs2, rs1, 0b001, 0b0100111);
}
void Assembler::FSQRT_H(FPR rd, FPR rs1, RMode rmode) noexcept {
    EmitRType(m_buffer, 0b0101110, f0, rs1, rmode, rd, 0b1010011);
}
void Assembler::FSUB_H(FPR rd, FPR rs1, FPR rs2, RMode rmode) noexcept {
    EmitRType(m_buffer, 0b0000110, rs2, rs1, rmode, rd, 0b1010011);
}

// RV64Zfh Extension Instructions

void Assembler::FCVT_L_H(GPR rd, FPR rs1, RMode rmode) noexcept {
    EmitRType(m_buffer, 0b1100010, f2, rs1, static_cast<uint32_t>(rmode), rd, 0b1010011);
}
void Assembler::FCVT_LU_H(GPR rd, FPR rs1, RMode rmode) noexcept {
    EmitRType(m_buffer, 0b1100010, f3, rs1, static_cast<uint32_t>(rmode), rd, 0b1010011);
}
void Assembler::FCVT_H_L(FPR rd, GPR rs1, RMode rmode) noexcept {
    EmitRType(m_buffer, 0b1101010, f2, rs1, static_cast<uint32_t>(rmode), rd, 0b1010011);
}
void Assembler::FCVT_H_LU(FPR rd, GPR rs1, RMode rmode) noexcept {
    EmitRType(m_buffer, 0b1101010, f3, rs1, static_cast<uint32_t>(rmode), rd, 0b1010011);
}

// Zfa Extension Instructions

static void FLIImpl(CodeBuffer& buffer, uint32_t funct7, FPR rd, double value) noexcept {
    static constexpr std::array fli_table{
        0xBFF0000000000000ULL, // -1.0
        0x0010000000000000ULL, // Minimum positive normal
        0x3EF0000000000000ULL, // 1.0 * 2^-16
        0x3F00000000000000ULL, // 1.0 * 2^-15
        0x3F70000000000000ULL, // 1.0 * 2^-8
        0x3F80000000000000ULL, // 1.0 * 2^-7
        0x3FB0000000000000ULL, // 1.0 * 2^-4
        0x3FC0000000000000ULL, // 1.0 * 2^-3
        0x3FD0000000000000ULL, // 0.25
        0x3FD4000000000000ULL, // 0.3125
        0x3FD8000000000000ULL, // 0.375
        0x3FDC000000000000ULL, // 0.4375
        0x3FE0000000000000ULL, // 0.5
        0x3FE4000000000000ULL, // 0.625
        0x3FE8000000000000ULL, // 0.75
        0x3FEC000000000000ULL, // 0.875
        0x3FF0000000000000ULL, // 1.0
        0x3FF4000000000000ULL, // 1.25
        0x3FF8000000000000ULL, // 1.5
        0x3FFC000000000000ULL, // 1.75
        0x4000000000000000ULL, // 2.0
        0x4004000000000000ULL, // 2.5
        0x4008000000000000ULL, // 3
        0x4010000000000000ULL, // 4
        0x4020000000000000ULL, // 8
        0x4030000000000000ULL, // 16
        0x4060000000000000ULL, // 2^7
        0x4070000000000000ULL, // 2^8
        0x40E0000000000000ULL, // 2^15
        0x40F0000000000000ULL, // 2^16
        0x7FF0000000000000ULL, // +inf
        0x7FF8000000000000ULL, // Canonical NaN
    };

    uint64_t ivalue{};
    std::memcpy(&ivalue, &value, sizeof(uint64_t));

    const auto iter = std::find_if(fli_table.cbegin(), fli_table.cend(), [ivalue](uint64_t entry) {
        return entry == ivalue;
    });
    BISCUIT_ASSERT(iter != fli_table.cend());

    const auto index = static_cast<uint32_t>(std::distance(fli_table.cbegin(), iter));
    EmitRType(buffer, funct7, f1, GPR{index}, 0b000, rd, 0b1010011);
}

void Assembler::FLI_D(FPR rd, double value) noexcept {
    FLIImpl(m_buffer, 0b1111001, rd, value);
}
void Assembler::FLI_H(FPR rd, double value) noexcept {
    FLIImpl(m_buffer, 0b1111010, rd, value);
}
void Assembler::FLI_S(FPR rd, double value) noexcept {
    FLIImpl(m_buffer, 0b1111000, rd, value);
}

void Assembler::FMINM_D(FPR rd, FPR rs1, FPR rs2) noexcept {
    EmitRType(m_buffer, 0b0010101, rs2, rs1, 0b010, rd, 0b1010011);
}
void Assembler::FMINM_H(FPR rd, FPR rs1, FPR rs2) noexcept {
    EmitRType(m_buffer, 0b0010110, rs2, rs1, 0b010, rd, 0b1010011);
}
void Assembler::FMINM_Q(FPR rd, FPR rs1, FPR rs2) noexcept {
    EmitRType(m_buffer, 0b0010111, rs2, rs1, 0b010, rd, 0b1010011);
}
void Assembler::FMINM_S(FPR rd, FPR rs1, FPR rs2) noexcept {
    EmitRType(m_buffer, 0b0010100, rs2, rs1, 0b010, rd, 0b1010011);
}

void Assembler::FMAXM_D(FPR rd, FPR rs1, FPR rs2) noexcept {
    EmitRType(m_buffer, 0b0010101, rs2, rs1, 0b011, rd, 0b1010011);
}
void Assembler::FMAXM_H(FPR rd, FPR rs1, FPR rs2) noexcept {
    EmitRType(m_buffer, 0b0010110, rs2, rs1, 0b011, rd, 0b1010011);
}
void Assembler::FMAXM_Q(FPR rd, FPR rs1, FPR rs2) noexcept {
    EmitRType(m_buffer, 0b0010111, rs2, rs1, 0b011, rd, 0b1010011);
}
void Assembler::FMAXM_S(FPR rd, FPR rs1, FPR rs2) noexcept {
    EmitRType(m_buffer, 0b0010100, rs2, rs1, 0b011, rd, 0b1010011);
}

void Assembler::FROUND_D(FPR rd, FPR rs1, RMode rmode) noexcept {
    EmitRType(m_buffer, 0b0100001, f4, rs1, static_cast<uint32_t>(rmode), rd, 0b1010011);
}
void Assembler::FROUND_H(FPR rd, FPR rs1, RMode rmode) noexcept {
    EmitRType(m_buffer, 0b0100010, f4, rs1, static_cast<uint32_t>(rmode), rd, 0b1010011);
}
void Assembler::FROUND_Q(FPR rd, FPR rs1, RMode rmode) noexcept {
    EmitRType(m_buffer, 0b0100011, f4, rs1, static_cast<uint32_t>(rmode), rd, 0b1010011);
}
void Assembler::FROUND_S(FPR rd, FPR rs1, RMode rmode) noexcept {
    EmitRType(m_buffer, 0b0100000, f4, rs1, static_cast<uint32_t>(rmode), rd, 0b1010011);
}

void Assembler::FROUNDNX_D(FPR rd, FPR rs1, RMode rmode) noexcept {
    EmitRType(m_buffer, 0b0100001, f5, rs1, static_cast<uint32_t>(rmode), rd, 0b1010011);
}
void Assembler::FROUNDNX_H(FPR rd, FPR rs1, RMode rmode) noexcept {
    EmitRType(m_buffer, 0b0100010, f5, rs1, static_cast<uint32_t>(rmode), rd, 0b1010011);
}
void Assembler::FROUNDNX_Q(FPR rd, FPR rs1, RMode rmode) noexcept {
    EmitRType(m_buffer, 0b0100011, f5, rs1, static_cast<uint32_t>(rmode), rd, 0b1010011);
}
void Assembler::FROUNDNX_S(FPR rd, FPR rs1, RMode rmode) noexcept {
    EmitRType(m_buffer, 0b0100000, f5, rs1, static_cast<uint32_t>(rmode), rd, 0b1010011);
}

void Assembler::FCVTMOD_W_D(GPR rd, FPR rs1) noexcept {
    EmitRType(m_buffer, 0b1100001, f8, rs1, static_cast<uint32_t>(RMode::RTZ), rd, 0b1010011);
}

void Assembler::FMVH_X_D(GPR rd, FPR rs1) noexcept {
    EmitRType(m_buffer, 0b1110001, f1, rs1, 0b000, rd, 0b1010011);
}
void Assembler::FMVH_X_Q(GPR rd, FPR rs1) noexcept {
    EmitRType(m_buffer, 0b1110011, f1, rs1, 0b000, rd, 0b1010011);
}
void Assembler::FMVP_D_X(FPR rd, GPR rs1, GPR rs2) noexcept {
    EmitRType(m_buffer, 0b1011001, rs2, rs1, 0b000, rd, 0b1010011);
}
void Assembler::FMVP_Q_X(FPR rd, GPR rs1, GPR rs2) noexcept {
    EmitRType(m_buffer, 0b1011011, rs2, rs1, 0b000, rd, 0b1010011);
}

void Assembler::FLEQ_D(GPR rd, FPR rs1, FPR rs2) noexcept {
    EmitRType(m_buffer, 0b1010001, rs2, rs1, 0b100, rd, 0b1010011);
}
void Assembler::FLTQ_D(GPR rd, FPR rs1, FPR rs2) noexcept {
    EmitRType(m_buffer, 0b1010001, rs2, rs1, 0b101, rd, 0b1010011);
}

void Assembler::FLEQ_H(GPR rd, FPR rs1, FPR rs2) noexcept {
    EmitRType(m_buffer, 0b1010010, rs2, rs1, 0b100, rd, 0b1010011);
}
void Assembler::FLTQ_H(GPR rd, FPR rs1, FPR rs2) noexcept {
    EmitRType(m_buffer, 0b1010010, rs2, rs1, 0b101, rd, 0b1010011);
}

void Assembler::FLEQ_Q(GPR rd, FPR rs1, FPR rs2) noexcept {
    EmitRType(m_buffer, 0b1010011, rs2, rs1, 0b100, rd, 0b1010011);
}
void Assembler::FLTQ_Q(GPR rd, FPR rs1, FPR rs2) noexcept {
    EmitRType(m_buffer, 0b1010011, rs2, rs1, 0b101, rd, 0b1010011);
}

void Assembler::FLEQ_S(GPR rd, FPR rs1, FPR rs2) noexcept {
    EmitRType(m_buffer, 0b1010000, rs2, rs1, 0b100, rd, 0b1010011);
}
void Assembler::FLTQ_S(GPR rd, FPR rs1, FPR rs2) noexcept {
    EmitRType(m_buffer, 0b1010000, rs2, rs1, 0b101, rd, 0b1010011);
}

// Zfbfmin, Zvfbfmin, Zvfbfwma Extension Instructions

void Assembler::FCVT_BF16_S(FPR rd, FPR rs, RMode rmode) noexcept {
    EmitRType(m_buffer, 0b0100010, f8, rs, static_cast<uint32_t>(rmode), rd, 0b1010011);
}

void Assembler::FCVT_S_BF16(FPR rd, FPR rs, RMode rmode) noexcept {
    EmitRType(m_buffer, 0b0100000, f6, rs, static_cast<uint32_t>(rmode), rd, 0b1010011);
}

} // namespace biscuit
