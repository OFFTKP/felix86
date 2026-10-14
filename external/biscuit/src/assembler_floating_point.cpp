#include <biscuit/assert.hpp>
#include <biscuit/assembler.hpp>

#include <algorithm>
#include <array>
#include <cstring>
#include <iterator>

#include "assembler_util.hpp"

// Various floating-point-based extension instructions.

namespace biscuit {
namespace {
[[nodiscard]] uint32_t PrecisionFunct7(uint32_t base, Precision precision) noexcept {
    return base | static_cast<uint32_t>(precision);
}

[[nodiscard]] uint32_t PrecisionWidth(Precision precision) noexcept {
    switch (precision) {
    case Precision::H:
        return 0b001;
    case Precision::S:
        return 0b010;
    case Precision::D:
        return 0b011;
    case Precision::Q:
        return 0b100;
    }

    BISCUIT_ASSERT(false);
    return 0;
}
} // namespace

// RV32F Extension Instructions

void Assembler::FADD_S(FPR rd, FPR rs1, FPR rs2, RMode rmode) noexcept {
    FADD(rd, rs1, rs2, Precision::S, rmode);
}
void Assembler::FCLASS_S(GPR rd, FPR rs1) noexcept {
    FCLASS(rd, rs1, Precision::S);
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
    FDIV(rd, rs1, rs2, Precision::S, rmode);
}
void Assembler::FEQ_S(GPR rd, FPR rs1, FPR rs2) noexcept {
    FEQ(rd, rs1, rs2, Precision::S);
}
void Assembler::FLE_S(GPR rd, FPR rs1, FPR rs2) noexcept {
    FLE(rd, rs1, rs2, Precision::S);
}
void Assembler::FLT_S(GPR rd, FPR rs1, FPR rs2) noexcept {
    FLT(rd, rs1, rs2, Precision::S);
}
void Assembler::FLW(FPR rd, int32_t offset, GPR rs) noexcept {
    FL(rd, offset, rs, Precision::S);
}
void Assembler::FMADD_S(FPR rd, FPR rs1, FPR rs2, FPR rs3, RMode rmode) noexcept {
    FMADD(rd, rs1, rs2, rs3, Precision::S, rmode);
}
void Assembler::FMAX_S(FPR rd, FPR rs1, FPR rs2) noexcept {
    FMAX(rd, rs1, rs2, Precision::S);
}
void Assembler::FMIN_S(FPR rd, FPR rs1, FPR rs2) noexcept {
    FMIN(rd, rs1, rs2, Precision::S);
}
void Assembler::FMSUB_S(FPR rd, FPR rs1, FPR rs2, FPR rs3, RMode rmode) noexcept {
    FMSUB(rd, rs1, rs2, rs3, Precision::S, rmode);
}
void Assembler::FMUL_S(FPR rd, FPR rs1, FPR rs2, RMode rmode) noexcept {
    FMUL(rd, rs1, rs2, Precision::S, rmode);
}
void Assembler::FMV_W_X(FPR rd, GPR rs1) noexcept {
    EmitRType(m_buffer, 0b1111000, f0, rs1, 0b000, rd, 0b1010011);
}
void Assembler::FMV_X_W(GPR rd, FPR rs1) noexcept {
    EmitRType(m_buffer, 0b1110000, f0, rs1, 0b000, rd, 0b1010011);
}
void Assembler::FNMADD_S(FPR rd, FPR rs1, FPR rs2, FPR rs3, RMode rmode) noexcept {
    FNMADD(rd, rs1, rs2, rs3, Precision::S, rmode);
}
void Assembler::FNMSUB_S(FPR rd, FPR rs1, FPR rs2, FPR rs3, RMode rmode) noexcept {
    FNMSUB(rd, rs1, rs2, rs3, Precision::S, rmode);
}
void Assembler::FSGNJ_S(FPR rd, FPR rs1, FPR rs2) noexcept {
    FSGNJ(rd, rs1, rs2, Precision::S);
}
void Assembler::FSGNJN_S(FPR rd, FPR rs1, FPR rs2) noexcept {
    FSGNJN(rd, rs1, rs2, Precision::S);
}
void Assembler::FSGNJX_S(FPR rd, FPR rs1, FPR rs2) noexcept {
    FSGNJX(rd, rs1, rs2, Precision::S);
}
void Assembler::FSQRT_S(FPR rd, FPR rs1, RMode rmode) noexcept {
    FSQRT(rd, rs1, Precision::S, rmode);
}
void Assembler::FSUB_S(FPR rd, FPR rs1, FPR rs2, RMode rmode) noexcept {
    FSUB(rd, rs1, rs2, Precision::S, rmode);
}
void Assembler::FSW(FPR rs2, int32_t offset, GPR rs1) noexcept {
    FS(rs2, offset, rs1, Precision::S);
}

void Assembler::FABS_S(FPR rd, FPR rs) noexcept {
    FABS(rd, rs, Precision::S);
}
void Assembler::FMV_S(FPR rd, FPR rs) noexcept {
    FMV(rd, rs, Precision::S);
}
void Assembler::FNEG_S(FPR rd, FPR rs) noexcept {
    FNEG(rd, rs, Precision::S);
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
    FADD(rd, rs1, rs2, Precision::D, rmode);
}
void Assembler::FCLASS_D(GPR rd, FPR rs1) noexcept {
    FCLASS(rd, rs1, Precision::D);
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
    FDIV(rd, rs1, rs2, Precision::D, rmode);
}
void Assembler::FEQ_D(GPR rd, FPR rs1, FPR rs2) noexcept {
    FEQ(rd, rs1, rs2, Precision::D);
}
void Assembler::FLE_D(GPR rd, FPR rs1, FPR rs2) noexcept {
    FLE(rd, rs1, rs2, Precision::D);
}
void Assembler::FLT_D(GPR rd, FPR rs1, FPR rs2) noexcept {
    FLT(rd, rs1, rs2, Precision::D);
}
void Assembler::FLD(FPR rd, int32_t offset, GPR rs) noexcept {
    FL(rd, offset, rs, Precision::D);
}
void Assembler::FMADD_D(FPR rd, FPR rs1, FPR rs2, FPR rs3, RMode rmode) noexcept {
    FMADD(rd, rs1, rs2, rs3, Precision::D, rmode);
}
void Assembler::FMAX_D(FPR rd, FPR rs1, FPR rs2) noexcept {
    FMAX(rd, rs1, rs2, Precision::D);
}
void Assembler::FMIN_D(FPR rd, FPR rs1, FPR rs2) noexcept {
    FMIN(rd, rs1, rs2, Precision::D);
}
void Assembler::FMSUB_D(FPR rd, FPR rs1, FPR rs2, FPR rs3, RMode rmode) noexcept {
    FMSUB(rd, rs1, rs2, rs3, Precision::D, rmode);
}
void Assembler::FMUL_D(FPR rd, FPR rs1, FPR rs2, RMode rmode) noexcept {
    FMUL(rd, rs1, rs2, Precision::D, rmode);
}
void Assembler::FNMADD_D(FPR rd, FPR rs1, FPR rs2, FPR rs3, RMode rmode) noexcept {
    FNMADD(rd, rs1, rs2, rs3, Precision::D, rmode);
}
void Assembler::FNMSUB_D(FPR rd, FPR rs1, FPR rs2, FPR rs3, RMode rmode) noexcept {
    FNMSUB(rd, rs1, rs2, rs3, Precision::D, rmode);
}
void Assembler::FSGNJ_D(FPR rd, FPR rs1, FPR rs2) noexcept {
    FSGNJ(rd, rs1, rs2, Precision::D);
}
void Assembler::FSGNJN_D(FPR rd, FPR rs1, FPR rs2) noexcept {
    FSGNJN(rd, rs1, rs2, Precision::D);
}
void Assembler::FSGNJX_D(FPR rd, FPR rs1, FPR rs2) noexcept {
    FSGNJX(rd, rs1, rs2, Precision::D);
}
void Assembler::FSQRT_D(FPR rd, FPR rs1, RMode rmode) noexcept {
    FSQRT(rd, rs1, Precision::D, rmode);
}
void Assembler::FSUB_D(FPR rd, FPR rs1, FPR rs2, RMode rmode) noexcept {
    FSUB(rd, rs1, rs2, Precision::D, rmode);
}
void Assembler::FSD(FPR rs2, int32_t offset, GPR rs1) noexcept {
    FS(rs2, offset, rs1, Precision::D);
}

void Assembler::FABS_D(FPR rd, FPR rs) noexcept {
    FABS(rd, rs, Precision::D);
}
void Assembler::FMV_D(FPR rd, FPR rs) noexcept {
    FMV(rd, rs, Precision::D);
}
void Assembler::FNEG_D(FPR rd, FPR rs) noexcept {
    FNEG(rd, rs, Precision::D);
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
    FADD(rd, rs1, rs2, Precision::Q, rmode);
}
void Assembler::FCLASS_Q(GPR rd, FPR rs1) noexcept {
    FCLASS(rd, rs1, Precision::Q);
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
    FDIV(rd, rs1, rs2, Precision::Q, rmode);
}
void Assembler::FEQ_Q(GPR rd, FPR rs1, FPR rs2) noexcept {
    FEQ(rd, rs1, rs2, Precision::Q);
}
void Assembler::FLE_Q(GPR rd, FPR rs1, FPR rs2) noexcept {
    FLE(rd, rs1, rs2, Precision::Q);
}
void Assembler::FLT_Q(GPR rd, FPR rs1, FPR rs2) noexcept {
    FLT(rd, rs1, rs2, Precision::Q);
}
void Assembler::FLQ(FPR rd, int32_t offset, GPR rs) noexcept {
    FL(rd, offset, rs, Precision::Q);
}
void Assembler::FMADD_Q(FPR rd, FPR rs1, FPR rs2, FPR rs3, RMode rmode) noexcept {
    FMADD(rd, rs1, rs2, rs3, Precision::Q, rmode);
}
void Assembler::FMAX_Q(FPR rd, FPR rs1, FPR rs2) noexcept {
    FMAX(rd, rs1, rs2, Precision::Q);
}
void Assembler::FMIN_Q(FPR rd, FPR rs1, FPR rs2) noexcept {
    FMIN(rd, rs1, rs2, Precision::Q);
}
void Assembler::FMSUB_Q(FPR rd, FPR rs1, FPR rs2, FPR rs3, RMode rmode) noexcept {
    FMSUB(rd, rs1, rs2, rs3, Precision::Q, rmode);
}
void Assembler::FMUL_Q(FPR rd, FPR rs1, FPR rs2, RMode rmode) noexcept {
    FMUL(rd, rs1, rs2, Precision::Q, rmode);
}
void Assembler::FNMADD_Q(FPR rd, FPR rs1, FPR rs2, FPR rs3, RMode rmode) noexcept {
    FNMADD(rd, rs1, rs2, rs3, Precision::Q, rmode);
}
void Assembler::FNMSUB_Q(FPR rd, FPR rs1, FPR rs2, FPR rs3, RMode rmode) noexcept {
    FNMSUB(rd, rs1, rs2, rs3, Precision::Q, rmode);
}
void Assembler::FSGNJ_Q(FPR rd, FPR rs1, FPR rs2) noexcept {
    FSGNJ(rd, rs1, rs2, Precision::Q);
}
void Assembler::FSGNJN_Q(FPR rd, FPR rs1, FPR rs2) noexcept {
    FSGNJN(rd, rs1, rs2, Precision::Q);
}
void Assembler::FSGNJX_Q(FPR rd, FPR rs1, FPR rs2) noexcept {
    FSGNJX(rd, rs1, rs2, Precision::Q);
}
void Assembler::FSQRT_Q(FPR rd, FPR rs1, RMode rmode) noexcept {
    FSQRT(rd, rs1, Precision::Q, rmode);
}
void Assembler::FSUB_Q(FPR rd, FPR rs1, FPR rs2, RMode rmode) noexcept {
    FSUB(rd, rs1, rs2, Precision::Q, rmode);
}
void Assembler::FSQ(FPR rs2, int32_t offset, GPR rs1) noexcept {
    FS(rs2, offset, rs1, Precision::Q);
}

void Assembler::FABS_Q(FPR rd, FPR rs) noexcept {
    FABS(rd, rs, Precision::Q);
}
void Assembler::FMV_Q(FPR rd, FPR rs) noexcept {
    FMV(rd, rs, Precision::Q);
}
void Assembler::FNEG_Q(FPR rd, FPR rs) noexcept {
    FNEG(rd, rs, Precision::Q);
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
    FADD(rd, rs1, rs2, Precision::H, rmode);
}
void Assembler::FCLASS_H(GPR rd, FPR rs1) noexcept {
    FCLASS(rd, rs1, Precision::H);
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
    FDIV(rd, rs1, rs2, Precision::H, rmode);
}
void Assembler::FEQ_H(GPR rd, FPR rs1, FPR rs2) noexcept {
    FEQ(rd, rs1, rs2, Precision::H);
}
void Assembler::FLE_H(GPR rd, FPR rs1, FPR rs2) noexcept {
    FLE(rd, rs1, rs2, Precision::H);
}
void Assembler::FLH(FPR rd, int32_t offset, GPR rs) noexcept {
    FL(rd, offset, rs, Precision::H);
}
void Assembler::FLT_H(GPR rd, FPR rs1, FPR rs2) noexcept {
    FLT(rd, rs1, rs2, Precision::H);
}
void Assembler::FMADD_H(FPR rd, FPR rs1, FPR rs2, FPR rs3, RMode rmode) noexcept {
    FMADD(rd, rs1, rs2, rs3, Precision::H, rmode);
}
void Assembler::FMAX_H(FPR rd, FPR rs1, FPR rs2) noexcept {
    FMAX(rd, rs1, rs2, Precision::H);
}
void Assembler::FMIN_H(FPR rd, FPR rs1, FPR rs2) noexcept {
    FMIN(rd, rs1, rs2, Precision::H);
}
void Assembler::FMSUB_H(FPR rd, FPR rs1, FPR rs2, FPR rs3, RMode rmode) noexcept {
    FMSUB(rd, rs1, rs2, rs3, Precision::H, rmode);
}
void Assembler::FMUL_H(FPR rd, FPR rs1, FPR rs2, RMode rmode) noexcept {
    FMUL(rd, rs1, rs2, Precision::H, rmode);
}
void Assembler::FMV_H_X(FPR rd, GPR rs1) noexcept {
    EmitRType(m_buffer, 0b1111010, f0, rs1, 0b000, rd, 0b1010011);
}
void Assembler::FMV_X_H(GPR rd, FPR rs1) noexcept {
    EmitRType(m_buffer, 0b1110010, f0, rs1, 0b000, rd, 0b1010011);
}
void Assembler::FNMADD_H(FPR rd, FPR rs1, FPR rs2, FPR rs3, RMode rmode) noexcept {
    FNMADD(rd, rs1, rs2, rs3, Precision::H, rmode);
}
void Assembler::FNMSUB_H(FPR rd, FPR rs1, FPR rs2, FPR rs3, RMode rmode) noexcept {
    FNMSUB(rd, rs1, rs2, rs3, Precision::H, rmode);
}
void Assembler::FSGNJ_H(FPR rd, FPR rs1, FPR rs2) noexcept {
    FSGNJ(rd, rs1, rs2, Precision::H);
}
void Assembler::FSGNJN_H(FPR rd, FPR rs1, FPR rs2) noexcept {
    FSGNJN(rd, rs1, rs2, Precision::H);
}
void Assembler::FSGNJX_H(FPR rd, FPR rs1, FPR rs2) noexcept {
    FSGNJX(rd, rs1, rs2, Precision::H);
}
void Assembler::FSH(FPR rs2, int32_t offset, GPR rs1) noexcept {
    FS(rs2, offset, rs1, Precision::H);
}
void Assembler::FSQRT_H(FPR rd, FPR rs1, RMode rmode) noexcept {
    FSQRT(rd, rs1, Precision::H, rmode);
}
void Assembler::FSUB_H(FPR rd, FPR rs1, FPR rs2, RMode rmode) noexcept {
    FSUB(rd, rs1, rs2, Precision::H, rmode);
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
    FLI(rd, value, Precision::D);
}
void Assembler::FLI_H(FPR rd, double value) noexcept {
    FLI(rd, value, Precision::H);
}
void Assembler::FLI_S(FPR rd, double value) noexcept {
    FLI(rd, value, Precision::S);
}

void Assembler::FMINM_D(FPR rd, FPR rs1, FPR rs2) noexcept {
    FMINM(rd, rs1, rs2, Precision::D);
}
void Assembler::FMINM_H(FPR rd, FPR rs1, FPR rs2) noexcept {
    FMINM(rd, rs1, rs2, Precision::H);
}
void Assembler::FMINM_Q(FPR rd, FPR rs1, FPR rs2) noexcept {
    FMINM(rd, rs1, rs2, Precision::Q);
}
void Assembler::FMINM_S(FPR rd, FPR rs1, FPR rs2) noexcept {
    FMINM(rd, rs1, rs2, Precision::S);
}

void Assembler::FMAXM_D(FPR rd, FPR rs1, FPR rs2) noexcept {
    FMAXM(rd, rs1, rs2, Precision::D);
}
void Assembler::FMAXM_H(FPR rd, FPR rs1, FPR rs2) noexcept {
    FMAXM(rd, rs1, rs2, Precision::H);
}
void Assembler::FMAXM_Q(FPR rd, FPR rs1, FPR rs2) noexcept {
    FMAXM(rd, rs1, rs2, Precision::Q);
}
void Assembler::FMAXM_S(FPR rd, FPR rs1, FPR rs2) noexcept {
    FMAXM(rd, rs1, rs2, Precision::S);
}

void Assembler::FROUND_D(FPR rd, FPR rs1, RMode rmode) noexcept {
    FROUND(rd, rs1, Precision::D, rmode);
}
void Assembler::FROUND_H(FPR rd, FPR rs1, RMode rmode) noexcept {
    FROUND(rd, rs1, Precision::H, rmode);
}
void Assembler::FROUND_Q(FPR rd, FPR rs1, RMode rmode) noexcept {
    FROUND(rd, rs1, Precision::Q, rmode);
}
void Assembler::FROUND_S(FPR rd, FPR rs1, RMode rmode) noexcept {
    FROUND(rd, rs1, Precision::S, rmode);
}

void Assembler::FROUNDNX_D(FPR rd, FPR rs1, RMode rmode) noexcept {
    FROUNDNX(rd, rs1, Precision::D, rmode);
}
void Assembler::FROUNDNX_H(FPR rd, FPR rs1, RMode rmode) noexcept {
    FROUNDNX(rd, rs1, Precision::H, rmode);
}
void Assembler::FROUNDNX_Q(FPR rd, FPR rs1, RMode rmode) noexcept {
    FROUNDNX(rd, rs1, Precision::Q, rmode);
}
void Assembler::FROUNDNX_S(FPR rd, FPR rs1, RMode rmode) noexcept {
    FROUNDNX(rd, rs1, Precision::S, rmode);
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
    FLEQ(rd, rs1, rs2, Precision::D);
}
void Assembler::FLTQ_D(GPR rd, FPR rs1, FPR rs2) noexcept {
    FLTQ(rd, rs1, rs2, Precision::D);
}

void Assembler::FLEQ_H(GPR rd, FPR rs1, FPR rs2) noexcept {
    FLEQ(rd, rs1, rs2, Precision::H);
}
void Assembler::FLTQ_H(GPR rd, FPR rs1, FPR rs2) noexcept {
    FLTQ(rd, rs1, rs2, Precision::H);
}

void Assembler::FLEQ_Q(GPR rd, FPR rs1, FPR rs2) noexcept {
    FLEQ(rd, rs1, rs2, Precision::Q);
}
void Assembler::FLTQ_Q(GPR rd, FPR rs1, FPR rs2) noexcept {
    FLTQ(rd, rs1, rs2, Precision::Q);
}

void Assembler::FLEQ_S(GPR rd, FPR rs1, FPR rs2) noexcept {
    FLEQ(rd, rs1, rs2, Precision::S);
}
void Assembler::FLTQ_S(GPR rd, FPR rs1, FPR rs2) noexcept {
    FLTQ(rd, rs1, rs2, Precision::S);
}

// Zfbfmin, Zvfbfmin, Zvfbfwma Extension Instructions

void Assembler::FCVT_BF16_S(FPR rd, FPR rs, RMode rmode) noexcept {
    EmitRType(m_buffer, 0b0100010, f8, rs, static_cast<uint32_t>(rmode), rd, 0b1010011);
}

void Assembler::FCVT_S_BF16(FPR rd, FPR rs, RMode rmode) noexcept {
    EmitRType(m_buffer, 0b0100000, f6, rs, static_cast<uint32_t>(rmode), rd, 0b1010011);
}

// Generic form of floating-point instructions

void Assembler::FADD(FPR rd, FPR rs1, FPR rs2, Precision prec, RMode rmode) noexcept {
    EmitRType(m_buffer, PrecisionFunct7(0b0000000, prec), rs2, rs1, rmode, rd, 0b1010011);
}
void Assembler::FSUB(FPR rd, FPR rs1, FPR rs2, Precision prec, RMode rmode) noexcept {
    EmitRType(m_buffer, PrecisionFunct7(0b0000100, prec), rs2, rs1, rmode, rd, 0b1010011);
}
void Assembler::FMUL(FPR rd, FPR rs1, FPR rs2, Precision prec, RMode rmode) noexcept {
    EmitRType(m_buffer, PrecisionFunct7(0b0001000, prec), rs2, rs1, rmode, rd, 0b1010011);
}
void Assembler::FDIV(FPR rd, FPR rs1, FPR rs2, Precision prec, RMode rmode) noexcept {
    EmitRType(m_buffer, PrecisionFunct7(0b0001100, prec), rs2, rs1, rmode, rd, 0b1010011);
}
void Assembler::FSQRT(FPR rd, FPR rs1, Precision prec, RMode rmode) noexcept {
    EmitRType(m_buffer, PrecisionFunct7(0b0101100, prec), f0, rs1, rmode, rd, 0b1010011);
}

void Assembler::FMADD(FPR rd, FPR rs1, FPR rs2, FPR rs3, Precision prec, RMode rmode) noexcept {
    EmitR4Type(m_buffer, rs3, static_cast<uint32_t>(prec), rs2, rs1, rmode, rd, 0b1000011);
}
void Assembler::FMSUB(FPR rd, FPR rs1, FPR rs2, FPR rs3, Precision prec, RMode rmode) noexcept {
    EmitR4Type(m_buffer, rs3, static_cast<uint32_t>(prec), rs2, rs1, rmode, rd, 0b1000111);
}
void Assembler::FNMADD(FPR rd, FPR rs1, FPR rs2, FPR rs3, Precision prec, RMode rmode) noexcept {
    EmitR4Type(m_buffer, rs3, static_cast<uint32_t>(prec), rs2, rs1, rmode, rd, 0b1001111);
}
void Assembler::FNMSUB(FPR rd, FPR rs1, FPR rs2, FPR rs3, Precision prec, RMode rmode) noexcept {
    EmitR4Type(m_buffer, rs3, static_cast<uint32_t>(prec), rs2, rs1, rmode, rd, 0b1001011);
}

void Assembler::FSGNJ(FPR rd, FPR rs1, FPR rs2, Precision prec) noexcept {
    EmitRType(m_buffer, PrecisionFunct7(0b0010000, prec), rs2, rs1, 0b000, rd, 0b1010011);
}
void Assembler::FSGNJN(FPR rd, FPR rs1, FPR rs2, Precision prec) noexcept {
    EmitRType(m_buffer, PrecisionFunct7(0b0010000, prec), rs2, rs1, 0b001, rd, 0b1010011);
}
void Assembler::FSGNJX(FPR rd, FPR rs1, FPR rs2, Precision prec) noexcept {
    EmitRType(m_buffer, PrecisionFunct7(0b0010000, prec), rs2, rs1, 0b010, rd, 0b1010011);
}

void Assembler::FMIN(FPR rd, FPR rs1, FPR rs2, Precision prec) noexcept {
    EmitRType(m_buffer, PrecisionFunct7(0b0010100, prec), rs2, rs1, 0b000, rd, 0b1010011);
}
void Assembler::FMAX(FPR rd, FPR rs1, FPR rs2, Precision prec) noexcept {
    EmitRType(m_buffer, PrecisionFunct7(0b0010100, prec), rs2, rs1, 0b001, rd, 0b1010011);
}
void Assembler::FMINM(FPR rd, FPR rs1, FPR rs2, Precision prec) noexcept {
    EmitRType(m_buffer, PrecisionFunct7(0b0010100, prec), rs2, rs1, 0b010, rd, 0b1010011);
}
void Assembler::FMAXM(FPR rd, FPR rs1, FPR rs2, Precision prec) noexcept {
    EmitRType(m_buffer, PrecisionFunct7(0b0010100, prec), rs2, rs1, 0b011, rd, 0b1010011);
}

void Assembler::FLE(GPR rd, FPR rs1, FPR rs2, Precision prec) noexcept {
    EmitRType(m_buffer, PrecisionFunct7(0b1010000, prec), rs2, rs1, 0b000, rd, 0b1010011);
}
void Assembler::FLT(GPR rd, FPR rs1, FPR rs2, Precision prec) noexcept {
    EmitRType(m_buffer, PrecisionFunct7(0b1010000, prec), rs2, rs1, 0b001, rd, 0b1010011);
}
void Assembler::FEQ(GPR rd, FPR rs1, FPR rs2, Precision prec) noexcept {
    EmitRType(m_buffer, PrecisionFunct7(0b1010000, prec), rs2, rs1, 0b010, rd, 0b1010011);
}
void Assembler::FLEQ(GPR rd, FPR rs1, FPR rs2, Precision prec) noexcept {
    EmitRType(m_buffer, PrecisionFunct7(0b1010000, prec), rs2, rs1, 0b100, rd, 0b1010011);
}
void Assembler::FLTQ(GPR rd, FPR rs1, FPR rs2, Precision prec) noexcept {
    EmitRType(m_buffer, PrecisionFunct7(0b1010000, prec), rs2, rs1, 0b101, rd, 0b1010011);
}

void Assembler::FCLASS(GPR rd, FPR rs1, Precision prec) noexcept {
    EmitRType(m_buffer, PrecisionFunct7(0b1110000, prec), f0, rs1, 0b001, rd, 0b1010011);
}

void Assembler::FROUND(FPR rd, FPR rs1, Precision prec, RMode rmode) noexcept {
    EmitRType(m_buffer, PrecisionFunct7(0b0100000, prec), f4, rs1, static_cast<uint32_t>(rmode), rd, 0b1010011);
}
void Assembler::FROUNDNX(FPR rd, FPR rs1, Precision prec, RMode rmode) noexcept {
    EmitRType(m_buffer, PrecisionFunct7(0b0100000, prec), f5, rs1, static_cast<uint32_t>(rmode), rd, 0b1010011);
}

void Assembler::FLI(FPR rd, double value, Precision prec) noexcept {
    BISCUIT_ASSERT(prec != Precision::Q);
    FLIImpl(m_buffer, PrecisionFunct7(0b1111000, prec), rd, value);
}


void Assembler::FL(FPR rd, int32_t offset, GPR rs, Precision prec) noexcept {
    BISCUIT_ASSERT(IsValidSigned12BitImm(offset));

    if (IsOptimizationEnabled(Optimization::AutoCompress)) {
        if (prec == Precision::S && IsRV32(m_features)) {
            if (rs == sp && offset >= 0 && offset <= 252 && (offset & 0b11) == 0) {
                C_FLWSP(rd, static_cast<uint32_t>(offset));
                return;
            } else if (offset >= 0 && offset <= 124 && (offset & 0b11) == 0 && IsValid3BitCompressedReg(rd) && IsValid3BitCompressedReg(rs)) {
                C_FLW(rd, static_cast<uint32_t>(offset), rs);
                return;
            }
        } else if (prec == Precision::D && IsRV64(m_features)) {
            if (rs == sp && offset >= 0 && offset <= 504 && (offset & 0b111) == 0) {
                C_FLDSP(rd, static_cast<uint32_t>(offset));
                return;
            } else if (offset >= 0 && offset <= 248 && (offset & 0b111) == 0 && IsValid3BitCompressedReg(rd) && IsValid3BitCompressedReg(rs)) {
                C_FLD(rd, static_cast<uint32_t>(offset), rs);
                return;
            }
        }
    }

    EmitIType(m_buffer, static_cast<uint32_t>(offset), rs, PrecisionWidth(prec), rd, 0b0000111);
}
void Assembler::FS(FPR rs2, int32_t offset, GPR rs1, Precision prec) noexcept {
    BISCUIT_ASSERT(IsValidSigned12BitImm(offset));

    if (IsOptimizationEnabled(Optimization::AutoCompress)) {
        if (prec == Precision::S && IsRV32(m_features)) {
            if (rs1 == sp && offset >= 0 && offset <= 252 && (offset & 0b11) == 0) {
                C_FSWSP(rs2, static_cast<uint32_t>(offset));
                return;
            } else if (offset >= 0 && offset <= 124 && (offset & 0b11) == 0 && IsValid3BitCompressedReg(rs2) && IsValid3BitCompressedReg(rs1)) {
                C_FSW(rs2, static_cast<uint32_t>(offset), rs1);
                return;
            }
        } else if (prec == Precision::D && IsRV64(m_features)) {
            if (rs1 == sp && offset >= 0 && offset <= 504 && (offset & 0b111) == 0) {
                C_FSDSP(rs2, static_cast<uint32_t>(offset));
                return;
            } else if (offset >= 0 && offset <= 248 && (offset & 0b111) == 0 && IsValid3BitCompressedReg(rs2) && IsValid3BitCompressedReg(rs1)) {
                C_FSD(rs2, static_cast<uint32_t>(offset), rs1);
                return;
            }
        }
    }

    EmitSType(m_buffer, static_cast<uint32_t>(offset), rs2, rs1, PrecisionWidth(prec), 0b0100111);
}

void Assembler::FABS(FPR rd, FPR rs, Precision prec) noexcept {
    FSGNJX(rd, rs, rs, prec);
}
void Assembler::FMV(FPR rd, FPR rs, Precision prec) noexcept {
    FSGNJ(rd, rs, rs, prec);
}
void Assembler::FNEG(FPR rd, FPR rs, Precision prec) noexcept {
    FSGNJN(rd, rs, rs, prec);
}

} // namespace biscuit
