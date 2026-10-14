// riscv-opcodes commit 5f869fc6fead9a58a05ac2715accb8e7635e6315
#include "decoder.h"
#define BX(v, b) (((v) >> b) & 1)
#define FX(v, high, low) (((v) >> low) & ((1U << ((high) - (low) + 1)) - 1))
RiscvMnemonic riscv_get_mnemonic(uint32_t data)
{
    if ((data & 0xfff0707fu) == 0x60701013u)
        return RISCV_ABS;
    if ((data & 0xfff0707fu) == 0x6070101bu)
        return RISCV_ABSW;
    if ((data & 0xfe00707fu) == 0x33u)
        return RISCV_ADD;
    if ((data & 0xfe00707fu) == 0x800003bu)
        return RISCV_ADD_UW;
    if ((data & 0xfe10f0ffu) == 0x8600601bu)
        return RISCV_ADDD;
    if ((data & 0x707fu) == 0x13u)
        return RISCV_ADDI;
    if ((data & 0x707fu) == 0x1bu)
        return RISCV_ADDIW;
    if ((data & 0xfe00707fu) == 0x3bu)
        return RISCV_ADDW;
    if ((data & 0x3e00707fu) == 0x2a000033u)
        return RISCV_AES32DSI;
    if ((data & 0x3e00707fu) == 0x2e000033u)
        return RISCV_AES32DSMI;
    if ((data & 0x3e00707fu) == 0x22000033u)
        return RISCV_AES32ESI;
    if ((data & 0x3e00707fu) == 0x26000033u)
        return RISCV_AES32ESMI;
    if ((data & 0xfe00707fu) == 0x3a000033u)
        return RISCV_AES64DS;
    if ((data & 0xfe00707fu) == 0x3e000033u)
        return RISCV_AES64DSM;
    if ((data & 0xfe00707fu) == 0x32000033u)
        return RISCV_AES64ES;
    if ((data & 0xfe00707fu) == 0x36000033u)
        return RISCV_AES64ESM;
    if ((data & 0xfff0707fu) == 0x30001013u)
        return RISCV_AES64IM;
    if ((data & 0xff00707fu) == 0x31001013u)
        return RISCV_AES64KS1I;
    if ((data & 0xfe00707fu) == 0x7e000033u)
        return RISCV_AES64KS2;
    if ((data & 0xf800707fu) == 0x2fu)
        return RISCV_AMOADD_B;
    if ((data & 0xf800707fu) == 0x302fu)
        return RISCV_AMOADD_D;
    if ((data & 0xf800707fu) == 0x102fu)
        return RISCV_AMOADD_H;
    if ((data & 0xf800707fu) == 0x202fu)
        return RISCV_AMOADD_W;
    if ((data & 0xf800707fu) == 0x6000002fu)
        return RISCV_AMOAND_B;
    if ((data & 0xf800707fu) == 0x6000302fu)
        return RISCV_AMOAND_D;
    if ((data & 0xf800707fu) == 0x6000102fu)
        return RISCV_AMOAND_H;
    if ((data & 0xf800707fu) == 0x6000202fu)
        return RISCV_AMOAND_W;
    if ((data & 0xf800707fu) == 0x2800002fu)
        return RISCV_AMOCAS_B;
    if ((data & 0xf800707fu) == 0x2800302fu)
        return RISCV_AMOCAS_D;
    if ((data & 0xf800707fu) == 0x2800102fu)
        return RISCV_AMOCAS_H;
    if ((data & 0xf800707fu) == 0x2800402fu)
        return RISCV_AMOCAS_Q;
    if ((data & 0xf800707fu) == 0x2800202fu)
        return RISCV_AMOCAS_W;
    if ((data & 0xf800707fu) == 0xa000002fu)
        return RISCV_AMOMAX_B;
    if ((data & 0xf800707fu) == 0xa000302fu)
        return RISCV_AMOMAX_D;
    if ((data & 0xf800707fu) == 0xa000102fu)
        return RISCV_AMOMAX_H;
    if ((data & 0xf800707fu) == 0xa000202fu)
        return RISCV_AMOMAX_W;
    if ((data & 0xf800707fu) == 0xe000002fu)
        return RISCV_AMOMAXU_B;
    if ((data & 0xf800707fu) == 0xe000302fu)
        return RISCV_AMOMAXU_D;
    if ((data & 0xf800707fu) == 0xe000102fu)
        return RISCV_AMOMAXU_H;
    if ((data & 0xf800707fu) == 0xe000202fu)
        return RISCV_AMOMAXU_W;
    if ((data & 0xf800707fu) == 0x8000002fu)
        return RISCV_AMOMIN_B;
    if ((data & 0xf800707fu) == 0x8000302fu)
        return RISCV_AMOMIN_D;
    if ((data & 0xf800707fu) == 0x8000102fu)
        return RISCV_AMOMIN_H;
    if ((data & 0xf800707fu) == 0x8000202fu)
        return RISCV_AMOMIN_W;
    if ((data & 0xf800707fu) == 0xc000002fu)
        return RISCV_AMOMINU_B;
    if ((data & 0xf800707fu) == 0xc000302fu)
        return RISCV_AMOMINU_D;
    if ((data & 0xf800707fu) == 0xc000102fu)
        return RISCV_AMOMINU_H;
    if ((data & 0xf800707fu) == 0xc000202fu)
        return RISCV_AMOMINU_W;
    if ((data & 0xf800707fu) == 0x4000002fu)
        return RISCV_AMOOR_B;
    if ((data & 0xf800707fu) == 0x4000302fu)
        return RISCV_AMOOR_D;
    if ((data & 0xf800707fu) == 0x4000102fu)
        return RISCV_AMOOR_H;
    if ((data & 0xf800707fu) == 0x4000202fu)
        return RISCV_AMOOR_W;
    if ((data & 0xf800707fu) == 0x800002fu)
        return RISCV_AMOSWAP_B;
    if ((data & 0xf800707fu) == 0x800302fu)
        return RISCV_AMOSWAP_D;
    if ((data & 0xf800707fu) == 0x800102fu)
        return RISCV_AMOSWAP_H;
    if ((data & 0xf800707fu) == 0x800202fu)
        return RISCV_AMOSWAP_W;
    if ((data & 0xf800707fu) == 0x2000002fu)
        return RISCV_AMOXOR_B;
    if ((data & 0xf800707fu) == 0x2000302fu)
        return RISCV_AMOXOR_D;
    if ((data & 0xf800707fu) == 0x2000102fu)
        return RISCV_AMOXOR_H;
    if ((data & 0xf800707fu) == 0x2000202fu)
        return RISCV_AMOXOR_W;
    if ((data & 0xfe00707fu) == 0x7033u)
        return RISCV_AND;
    if ((data & 0x707fu) == 0x7013u)
        return RISCV_ANDI;
    if ((data & 0xfe00707fu) == 0x40007033u)
        return RISCV_ANDN;
    if ((data & 0x7fu) == 0x17u)
        return RISCV_AUIPC;
    if ((data & 0xfe00707fu) == 0x48001033u)
        return RISCV_BCLR;
    if ((data & 0xfc00707fu) == 0x48001013u)
        return RISCV_BCLRI;
    if ((data & 0x707fu) == 0x63u)
        return RISCV_BEQ;
    if ((data & 0x707fu) == 0x2063u)
        return RISCV_BEQI;
    if ((data & 0xfe00707fu) == 0x48005033u)
        return RISCV_BEXT;
    if ((data & 0xfc00707fu) == 0x48005013u)
        return RISCV_BEXTI;
    if ((data & 0x707fu) == 0x5063u)
        return RISCV_BGE;
    if ((data & 0x707fu) == 0x7063u)
        return RISCV_BGEU;
    if ((data & 0xfe00707fu) == 0x68001033u)
        return RISCV_BINV;
    if ((data & 0xfc00707fu) == 0x68001013u)
        return RISCV_BINVI;
    if ((data & 0x707fu) == 0x4063u)
        return RISCV_BLT;
    if ((data & 0x707fu) == 0x6063u)
        return RISCV_BLTU;
    if ((data & 0x707fu) == 0x1063u)
        return RISCV_BNE;
    if ((data & 0x707fu) == 0x3063u)
        return RISCV_BNEI;
    if ((data & 0xfff0707fu) == 0x68705013u)
        return RISCV_BREV8;
    if ((data & 0xfe00707fu) == 0x28001033u)
        return RISCV_BSET;
    if ((data & 0xfc00707fu) == 0x28001013u)
        return RISCV_BSETI;
    if ((data & 0xffffu) == 0x9002u)
        return RISCV_C_EBREAK;
    if ((data & 0xf07fu) == 0x9002u)
        return RISCV_C_JALR;
    if ((data & 0xf003u) == 0x9002u)
        return RISCV_C_ADD;
    if ((data & 0xef83u) == 0x1u)
        return RISCV_C_NOP;
    if ((data & 0xe003u) == 0x1u)
        return RISCV_C_ADDI;
    if ((data & 0xef83u) == 0x6101u)
        return RISCV_C_ADDI16SP;
    if ((data & 0xe003u) == 0x0u && !(((FX(data, 10, 7) << 6) | (FX(data, 12, 11) << 4) | (BX(data, 5) << 3) | (BX(data, 6) << 2)) == 0))
        return RISCV_C_ADDI4SPN;
    if ((data & 0xe003u) == 0x2001u)
        return RISCV_C_ADDIW;
    if ((data & 0xfc63u) == 0x9c21u)
        return RISCV_C_ADDW;
    if ((data & 0xfc63u) == 0x8c61u)
        return RISCV_C_AND;
    if ((data & 0xec03u) == 0x8801u)
        return RISCV_C_ANDI;
    if ((data & 0xe003u) == 0xc001u)
        return RISCV_C_BEQZ;
    if ((data & 0xe003u) == 0xe001u)
        return RISCV_C_BNEZ;
    if ((data & 0xe003u) == 0x2000u)
        return RISCV_C_FLD;
    if ((data & 0xe003u) == 0x2002u)
        return RISCV_C_FLDSP;
    if ((data & 0xe003u) == 0x6000u)
        return RISCV_C_FLW;
    if ((data & 0xe003u) == 0x6002u)
        return RISCV_C_FLWSP;
    if ((data & 0xe003u) == 0xa000u)
        return RISCV_C_FSD;
    if ((data & 0xe003u) == 0xa002u)
        return RISCV_C_FSDSP;
    if ((data & 0xe003u) == 0xe000u)
        return RISCV_C_FSW;
    if ((data & 0xe003u) == 0xe002u)
        return RISCV_C_FSWSP;
    if ((data & 0xe003u) == 0xa001u)
        return RISCV_C_J;
    if ((data & 0xe003u) == 0x2001u)
        return RISCV_C_JAL;
    if ((data & 0xf07fu) == 0x8002u && !(FX(data, 11, 7) == 0))
        return RISCV_C_JR;
    if ((data & 0xfc03u) == 0x8000u)
        return RISCV_C_LBU;
    if ((data & 0xe003u) == 0x6000u)
        return RISCV_C_LD;
    if ((data & 0xe003u) == 0x6002u)
        return RISCV_C_LDSP;
    if ((data & 0xfc43u) == 0x8440u)
        return RISCV_C_LH;
    if ((data & 0xfc43u) == 0x8400u)
        return RISCV_C_LHU;
    if ((data & 0xe003u) == 0x4001u)
        return RISCV_C_LI;
    if ((data & 0xf0ffu) == 0x6081u && !(FX(data, 11, 7) > 15))
        return RISCV_C_MOP_N;
    if ((data & 0xe003u) == 0x6001u)
        return RISCV_C_LUI;
    if ((data & 0xe003u) == 0x4000u)
        return RISCV_C_LW;
    if ((data & 0xe003u) == 0x4002u)
        return RISCV_C_LWSP;
    if ((data & 0xfc63u) == 0x9c41u)
        return RISCV_C_MUL;
    if ((data & 0xf003u) == 0x8002u)
        return RISCV_C_MV;
    if ((data & 0xfc7fu) == 0x9c75u)
        return RISCV_C_NOT;
    if ((data & 0xfc63u) == 0x8c41u)
        return RISCV_C_OR;
    if ((data & 0xfc03u) == 0x8800u)
        return RISCV_C_SB;
    if ((data & 0xe003u) == 0xe000u)
        return RISCV_C_SD;
    if ((data & 0xe003u) == 0xe002u)
        return RISCV_C_SDSP;
    if ((data & 0xfc7fu) == 0x9c65u)
        return RISCV_C_SEXT_B;
    if ((data & 0xfc7fu) == 0x9c6du)
        return RISCV_C_SEXT_H;
    if ((data & 0xfc43u) == 0x8c00u)
        return RISCV_C_SH;
    if ((data & 0xe003u) == 0x2u)
        return RISCV_C_SLLI;
    if ((data & 0xec03u) == 0x8401u)
        return RISCV_C_SRAI;
    if ((data & 0xec03u) == 0x8001u)
        return RISCV_C_SRLI;
    if ((data & 0xfc63u) == 0x8c01u)
        return RISCV_C_SUB;
    if ((data & 0xfc63u) == 0x9c01u)
        return RISCV_C_SUBW;
    if ((data & 0xe003u) == 0xc000u)
        return RISCV_C_SW;
    if ((data & 0xe003u) == 0xc002u)
        return RISCV_C_SWSP;
    if ((data & 0xfc63u) == 0x8c21u)
        return RISCV_C_XOR;
    if ((data & 0xfc7fu) == 0x9c61u)
        return RISCV_C_ZEXT_B;
    if ((data & 0xfc7fu) == 0x9c69u)
        return RISCV_C_ZEXT_H;
    if ((data & 0xfc7fu) == 0x9c71u)
        return RISCV_C_ZEXT_W;
    if ((data & 0xfff07fffu) == 0x10200fu)
        return RISCV_CBO_CLEAN;
    if ((data & 0xfff07fffu) == 0x20200fu)
        return RISCV_CBO_FLUSH;
    if ((data & 0xfff07fffu) == 0x200fu)
        return RISCV_CBO_INVAL;
    if ((data & 0xfff07fffu) == 0x40200fu)
        return RISCV_CBO_ZERO;
    if ((data & 0xfe00707fu) == 0xa001033u)
        return RISCV_CLMUL;
    if ((data & 0xfe00707fu) == 0xa003033u)
        return RISCV_CLMULH;
    if ((data & 0xfe00707fu) == 0xa002033u)
        return RISCV_CLMULR;
    if ((data & 0xfff0707fu) == 0x60301013u)
        return RISCV_CLS;
    if ((data & 0xfff0707fu) == 0x6030101bu)
        return RISCV_CLSW;
    if ((data & 0xfff0707fu) == 0x60001013u)
        return RISCV_CLZ;
    if ((data & 0xfff0707fu) == 0x6000101bu)
        return RISCV_CLZW;
    if ((data & 0xfff0707fu) == 0x60201013u)
        return RISCV_CPOP;
    if ((data & 0xfff0707fu) == 0x6020101bu)
        return RISCV_CPOPW;
    if ((data & 0x707fu) == 0x3073u)
        return RISCV_CSRRC;
    if ((data & 0x707fu) == 0x7073u)
        return RISCV_CSRRCI;
    if ((data & 0x707fu) == 0x2073u)
        return RISCV_CSRRS;
    if ((data & 0x707fu) == 0x6073u)
        return RISCV_CSRRSI;
    if ((data & 0x707fu) == 0x1073u)
        return RISCV_CSRRW;
    if ((data & 0x707fu) == 0x5073u)
        return RISCV_CSRRWI;
    if ((data & 0xfff0707fu) == 0x60101013u)
        return RISCV_CTZ;
    if ((data & 0xfff0707fu) == 0x6010101bu)
        return RISCV_CTZW;
    if ((data & 0xfe00707fu) == 0xe005033u)
        return RISCV_CZERO_EQZ;
    if ((data & 0xfe00707fu) == 0xe007033u)
        return RISCV_CZERO_NEZ;
    if ((data & 0xfe00707fu) == 0x2004033u)
        return RISCV_DIV;
    if ((data & 0xfe00707fu) == 0x2005033u)
        return RISCV_DIVU;
    if ((data & 0xfe00707fu) == 0x200503bu)
        return RISCV_DIVUW;
    if ((data & 0xfe00707fu) == 0x200403bu)
        return RISCV_DIVW;
    if ((data & 0xffffffffu) == 0x7b200073u)
        return RISCV_DRET;
    if ((data & 0xffffffffu) == 0x100073u)
        return RISCV_EBREAK;
    if ((data & 0xffffffffu) == 0x73u)
        return RISCV_ECALL;
    if ((data & 0xfe00007fu) == 0x2000053u)
        return RISCV_FADD_D;
    if ((data & 0xfe00007fu) == 0x4000053u)
        return RISCV_FADD_H;
    if ((data & 0xfe00007fu) == 0x6000053u)
        return RISCV_FADD_Q;
    if ((data & 0xfe00007fu) == 0x53u)
        return RISCV_FADD_S;
    if ((data & 0xfff0707fu) == 0xe2001053u)
        return RISCV_FCLASS_D;
    if ((data & 0xfff0707fu) == 0xe4001053u)
        return RISCV_FCLASS_H;
    if ((data & 0xfff0707fu) == 0xe6001053u)
        return RISCV_FCLASS_Q;
    if ((data & 0xfff0707fu) == 0xe0001053u)
        return RISCV_FCLASS_S;
    if ((data & 0xfff0007fu) == 0x44800053u)
        return RISCV_FCVT_BF16_S;
    if ((data & 0xfff0007fu) == 0x42200053u)
        return RISCV_FCVT_D_H;
    if ((data & 0xfff0007fu) == 0xd2200053u)
        return RISCV_FCVT_D_L;
    if ((data & 0xfff0007fu) == 0xd2300053u)
        return RISCV_FCVT_D_LU;
    if ((data & 0xfff0007fu) == 0x42300053u)
        return RISCV_FCVT_D_Q;
    if ((data & 0xfff0007fu) == 0x42000053u)
        return RISCV_FCVT_D_S;
    if ((data & 0xfff0007fu) == 0xd2000053u)
        return RISCV_FCVT_D_W;
    if ((data & 0xfff0007fu) == 0xd2100053u)
        return RISCV_FCVT_D_WU;
    if ((data & 0xfff0007fu) == 0x44100053u)
        return RISCV_FCVT_H_D;
    if ((data & 0xfff0007fu) == 0xd4200053u)
        return RISCV_FCVT_H_L;
    if ((data & 0xfff0007fu) == 0xd4300053u)
        return RISCV_FCVT_H_LU;
    if ((data & 0xfff0007fu) == 0x44300053u)
        return RISCV_FCVT_H_Q;
    if ((data & 0xfff0007fu) == 0x44000053u)
        return RISCV_FCVT_H_S;
    if ((data & 0xfff0007fu) == 0xd4000053u)
        return RISCV_FCVT_H_W;
    if ((data & 0xfff0007fu) == 0xd4100053u)
        return RISCV_FCVT_H_WU;
    if ((data & 0xfff0007fu) == 0xc2200053u)
        return RISCV_FCVT_L_D;
    if ((data & 0xfff0007fu) == 0xc4200053u)
        return RISCV_FCVT_L_H;
    if ((data & 0xfff0007fu) == 0xc6200053u)
        return RISCV_FCVT_L_Q;
    if ((data & 0xfff0007fu) == 0xc0200053u)
        return RISCV_FCVT_L_S;
    if ((data & 0xfff0007fu) == 0xc2300053u)
        return RISCV_FCVT_LU_D;
    if ((data & 0xfff0007fu) == 0xc4300053u)
        return RISCV_FCVT_LU_H;
    if ((data & 0xfff0007fu) == 0xc6300053u)
        return RISCV_FCVT_LU_Q;
    if ((data & 0xfff0007fu) == 0xc0300053u)
        return RISCV_FCVT_LU_S;
    if ((data & 0xfff0007fu) == 0x46100053u)
        return RISCV_FCVT_Q_D;
    if ((data & 0xfff0007fu) == 0x46200053u)
        return RISCV_FCVT_Q_H;
    if ((data & 0xfff0007fu) == 0xd6200053u)
        return RISCV_FCVT_Q_L;
    if ((data & 0xfff0007fu) == 0xd6300053u)
        return RISCV_FCVT_Q_LU;
    if ((data & 0xfff0007fu) == 0x46000053u)
        return RISCV_FCVT_Q_S;
    if ((data & 0xfff0007fu) == 0xd6000053u)
        return RISCV_FCVT_Q_W;
    if ((data & 0xfff0007fu) == 0xd6100053u)
        return RISCV_FCVT_Q_WU;
    if ((data & 0xfff0007fu) == 0x40600053u)
        return RISCV_FCVT_S_BF16;
    if ((data & 0xfff0007fu) == 0x40100053u)
        return RISCV_FCVT_S_D;
    if ((data & 0xfff0007fu) == 0x40200053u)
        return RISCV_FCVT_S_H;
    if ((data & 0xfff0007fu) == 0xd0200053u)
        return RISCV_FCVT_S_L;
    if ((data & 0xfff0007fu) == 0xd0300053u)
        return RISCV_FCVT_S_LU;
    if ((data & 0xfff0007fu) == 0x40300053u)
        return RISCV_FCVT_S_Q;
    if ((data & 0xfff0007fu) == 0xd0000053u)
        return RISCV_FCVT_S_W;
    if ((data & 0xfff0007fu) == 0xd0100053u)
        return RISCV_FCVT_S_WU;
    if ((data & 0xfff0007fu) == 0xc2000053u)
        return RISCV_FCVT_W_D;
    if ((data & 0xfff0007fu) == 0xc4000053u)
        return RISCV_FCVT_W_H;
    if ((data & 0xfff0007fu) == 0xc6000053u)
        return RISCV_FCVT_W_Q;
    if ((data & 0xfff0007fu) == 0xc0000053u)
        return RISCV_FCVT_W_S;
    if ((data & 0xfff0007fu) == 0xc2100053u)
        return RISCV_FCVT_WU_D;
    if ((data & 0xfff0007fu) == 0xc4100053u)
        return RISCV_FCVT_WU_H;
    if ((data & 0xfff0007fu) == 0xc6100053u)
        return RISCV_FCVT_WU_Q;
    if ((data & 0xfff0007fu) == 0xc0100053u)
        return RISCV_FCVT_WU_S;
    if ((data & 0xfff0707fu) == 0xc2801053u)
        return RISCV_FCVTMOD_W_D;
    if ((data & 0xfe00007fu) == 0x1a000053u)
        return RISCV_FDIV_D;
    if ((data & 0xfe00007fu) == 0x1c000053u)
        return RISCV_FDIV_H;
    if ((data & 0xfe00007fu) == 0x1e000053u)
        return RISCV_FDIV_Q;
    if ((data & 0xfe00007fu) == 0x18000053u)
        return RISCV_FDIV_S;
    if ((data & 0x707fu) == 0xfu)
        return RISCV_FENCE;
    if ((data & 0x707fu) == 0x100fu)
        return RISCV_FENCE_I;
    if ((data & 0xfe00707fu) == 0xa2002053u)
        return RISCV_FEQ_D;
    if ((data & 0xfe00707fu) == 0xa4002053u)
        return RISCV_FEQ_H;
    if ((data & 0xfe00707fu) == 0xa6002053u)
        return RISCV_FEQ_Q;
    if ((data & 0xfe00707fu) == 0xa0002053u)
        return RISCV_FEQ_S;
    if ((data & 0x707fu) == 0x3007u)
        return RISCV_FLD;
    if ((data & 0xfe00707fu) == 0xa2000053u)
        return RISCV_FLE_D;
    if ((data & 0xfe00707fu) == 0xa4000053u)
        return RISCV_FLE_H;
    if ((data & 0xfe00707fu) == 0xa6000053u)
        return RISCV_FLE_Q;
    if ((data & 0xfe00707fu) == 0xa0000053u)
        return RISCV_FLE_S;
    if ((data & 0xfe00707fu) == 0xa2004053u)
        return RISCV_FLEQ_D;
    if ((data & 0xfe00707fu) == 0xa4004053u)
        return RISCV_FLEQ_H;
    if ((data & 0xfe00707fu) == 0xa6004053u)
        return RISCV_FLEQ_Q;
    if ((data & 0xfe00707fu) == 0xa0004053u)
        return RISCV_FLEQ_S;
    if ((data & 0x707fu) == 0x1007u)
        return RISCV_FLH;
    if ((data & 0xfff0707fu) == 0xf2100053u)
        return RISCV_FLI_D;
    if ((data & 0xfff0707fu) == 0xf4100053u)
        return RISCV_FLI_H;
    if ((data & 0xfff0707fu) == 0xf6100053u)
        return RISCV_FLI_Q;
    if ((data & 0xfff0707fu) == 0xf0100053u)
        return RISCV_FLI_S;
    if ((data & 0x707fu) == 0x4007u)
        return RISCV_FLQ;
    if ((data & 0xfe00707fu) == 0xa2001053u)
        return RISCV_FLT_D;
    if ((data & 0xfe00707fu) == 0xa4001053u)
        return RISCV_FLT_H;
    if ((data & 0xfe00707fu) == 0xa6001053u)
        return RISCV_FLT_Q;
    if ((data & 0xfe00707fu) == 0xa0001053u)
        return RISCV_FLT_S;
    if ((data & 0xfe00707fu) == 0xa2005053u)
        return RISCV_FLTQ_D;
    if ((data & 0xfe00707fu) == 0xa4005053u)
        return RISCV_FLTQ_H;
    if ((data & 0xfe00707fu) == 0xa6005053u)
        return RISCV_FLTQ_Q;
    if ((data & 0xfe00707fu) == 0xa0005053u)
        return RISCV_FLTQ_S;
    if ((data & 0x707fu) == 0x2007u)
        return RISCV_FLW;
    if ((data & 0x600007fu) == 0x2000043u)
        return RISCV_FMADD_D;
    if ((data & 0x600007fu) == 0x4000043u)
        return RISCV_FMADD_H;
    if ((data & 0x600007fu) == 0x6000043u)
        return RISCV_FMADD_Q;
    if ((data & 0x600007fu) == 0x43u)
        return RISCV_FMADD_S;
    if ((data & 0xfe00707fu) == 0x2a001053u)
        return RISCV_FMAX_D;
    if ((data & 0xfe00707fu) == 0x2c001053u)
        return RISCV_FMAX_H;
    if ((data & 0xfe00707fu) == 0x2e001053u)
        return RISCV_FMAX_Q;
    if ((data & 0xfe00707fu) == 0x28001053u)
        return RISCV_FMAX_S;
    if ((data & 0xfe00707fu) == 0x2a003053u)
        return RISCV_FMAXM_D;
    if ((data & 0xfe00707fu) == 0x2c003053u)
        return RISCV_FMAXM_H;
    if ((data & 0xfe00707fu) == 0x2e003053u)
        return RISCV_FMAXM_Q;
    if ((data & 0xfe00707fu) == 0x28003053u)
        return RISCV_FMAXM_S;
    if ((data & 0xfe00707fu) == 0x2a000053u)
        return RISCV_FMIN_D;
    if ((data & 0xfe00707fu) == 0x2c000053u)
        return RISCV_FMIN_H;
    if ((data & 0xfe00707fu) == 0x2e000053u)
        return RISCV_FMIN_Q;
    if ((data & 0xfe00707fu) == 0x28000053u)
        return RISCV_FMIN_S;
    if ((data & 0xfe00707fu) == 0x2a002053u)
        return RISCV_FMINM_D;
    if ((data & 0xfe00707fu) == 0x2c002053u)
        return RISCV_FMINM_H;
    if ((data & 0xfe00707fu) == 0x2e002053u)
        return RISCV_FMINM_Q;
    if ((data & 0xfe00707fu) == 0x28002053u)
        return RISCV_FMINM_S;
    if ((data & 0x600007fu) == 0x2000047u)
        return RISCV_FMSUB_D;
    if ((data & 0x600007fu) == 0x4000047u)
        return RISCV_FMSUB_H;
    if ((data & 0x600007fu) == 0x6000047u)
        return RISCV_FMSUB_Q;
    if ((data & 0x600007fu) == 0x47u)
        return RISCV_FMSUB_S;
    if ((data & 0xfe00007fu) == 0x12000053u)
        return RISCV_FMUL_D;
    if ((data & 0xfe00007fu) == 0x14000053u)
        return RISCV_FMUL_H;
    if ((data & 0xfe00007fu) == 0x16000053u)
        return RISCV_FMUL_Q;
    if ((data & 0xfe00007fu) == 0x10000053u)
        return RISCV_FMUL_S;
    if ((data & 0xfff0707fu) == 0xf2000053u)
        return RISCV_FMV_D_X;
    if ((data & 0xfff0707fu) == 0xf4000053u)
        return RISCV_FMV_H_X;
    if ((data & 0xfff0707fu) == 0xf0000053u)
        return RISCV_FMV_W_X;
    if ((data & 0xfff0707fu) == 0xe2000053u)
        return RISCV_FMV_X_D;
    if ((data & 0xfff0707fu) == 0xe4000053u)
        return RISCV_FMV_X_H;
    if ((data & 0xfff0707fu) == 0xe0000053u)
        return RISCV_FMV_X_W;
    if ((data & 0xfff0707fu) == 0xe2100053u)
        return RISCV_FMVH_X_D;
    if ((data & 0xfff0707fu) == 0xe6100053u)
        return RISCV_FMVH_X_Q;
    if ((data & 0xfe00707fu) == 0xb2000053u)
        return RISCV_FMVP_D_X;
    if ((data & 0xfe00707fu) == 0xb6000053u)
        return RISCV_FMVP_Q_X;
    if ((data & 0x600007fu) == 0x200004fu)
        return RISCV_FNMADD_D;
    if ((data & 0x600007fu) == 0x400004fu)
        return RISCV_FNMADD_H;
    if ((data & 0x600007fu) == 0x600004fu)
        return RISCV_FNMADD_Q;
    if ((data & 0x600007fu) == 0x4fu)
        return RISCV_FNMADD_S;
    if ((data & 0x600007fu) == 0x200004bu)
        return RISCV_FNMSUB_D;
    if ((data & 0x600007fu) == 0x400004bu)
        return RISCV_FNMSUB_H;
    if ((data & 0x600007fu) == 0x600004bu)
        return RISCV_FNMSUB_Q;
    if ((data & 0x600007fu) == 0x4bu)
        return RISCV_FNMSUB_S;
    if ((data & 0xfff0007fu) == 0x42400053u)
        return RISCV_FROUND_D;
    if ((data & 0xfff0007fu) == 0x44400053u)
        return RISCV_FROUND_H;
    if ((data & 0xfff0007fu) == 0x46400053u)
        return RISCV_FROUND_Q;
    if ((data & 0xfff0007fu) == 0x40400053u)
        return RISCV_FROUND_S;
    if ((data & 0xfff0007fu) == 0x42500053u)
        return RISCV_FROUNDNX_D;
    if ((data & 0xfff0007fu) == 0x44500053u)
        return RISCV_FROUNDNX_H;
    if ((data & 0xfff0007fu) == 0x46500053u)
        return RISCV_FROUNDNX_Q;
    if ((data & 0xfff0007fu) == 0x40500053u)
        return RISCV_FROUNDNX_S;
    if ((data & 0x707fu) == 0x3027u)
        return RISCV_FSD;
    if ((data & 0xfe00707fu) == 0x22000053u)
        return RISCV_FSGNJ_D;
    if ((data & 0xfe00707fu) == 0x24000053u)
        return RISCV_FSGNJ_H;
    if ((data & 0xfe00707fu) == 0x26000053u)
        return RISCV_FSGNJ_Q;
    if ((data & 0xfe00707fu) == 0x20000053u)
        return RISCV_FSGNJ_S;
    if ((data & 0xfe00707fu) == 0x22001053u)
        return RISCV_FSGNJN_D;
    if ((data & 0xfe00707fu) == 0x24001053u)
        return RISCV_FSGNJN_H;
    if ((data & 0xfe00707fu) == 0x26001053u)
        return RISCV_FSGNJN_Q;
    if ((data & 0xfe00707fu) == 0x20001053u)
        return RISCV_FSGNJN_S;
    if ((data & 0xfe00707fu) == 0x22002053u)
        return RISCV_FSGNJX_D;
    if ((data & 0xfe00707fu) == 0x24002053u)
        return RISCV_FSGNJX_H;
    if ((data & 0xfe00707fu) == 0x26002053u)
        return RISCV_FSGNJX_Q;
    if ((data & 0xfe00707fu) == 0x20002053u)
        return RISCV_FSGNJX_S;
    if ((data & 0x707fu) == 0x1027u)
        return RISCV_FSH;
    if ((data & 0x707fu) == 0x4027u)
        return RISCV_FSQ;
    if ((data & 0xfff0007fu) == 0x5a000053u)
        return RISCV_FSQRT_D;
    if ((data & 0xfff0007fu) == 0x5c000053u)
        return RISCV_FSQRT_H;
    if ((data & 0xfff0007fu) == 0x5e000053u)
        return RISCV_FSQRT_Q;
    if ((data & 0xfff0007fu) == 0x58000053u)
        return RISCV_FSQRT_S;
    if ((data & 0xfe00007fu) == 0xa000053u)
        return RISCV_FSUB_D;
    if ((data & 0xfe00007fu) == 0xc000053u)
        return RISCV_FSUB_H;
    if ((data & 0xfe00007fu) == 0xe000053u)
        return RISCV_FSUB_Q;
    if ((data & 0xfe00007fu) == 0x8000053u)
        return RISCV_FSUB_S;
    if ((data & 0x707fu) == 0x2027u)
        return RISCV_FSW;
    if ((data & 0xfe007fffu) == 0x62000073u)
        return RISCV_HFENCE_GVMA;
    if ((data & 0xfe007fffu) == 0x22000073u)
        return RISCV_HFENCE_VVMA;
    if ((data & 0xfe007fffu) == 0x66000073u)
        return RISCV_HINVAL_GVMA;
    if ((data & 0xfe007fffu) == 0x26000073u)
        return RISCV_HINVAL_VVMA;
    if ((data & 0xfff0707fu) == 0x60004073u)
        return RISCV_HLV_B;
    if ((data & 0xfff0707fu) == 0x60104073u)
        return RISCV_HLV_BU;
    if ((data & 0xfff0707fu) == 0x6c004073u)
        return RISCV_HLV_D;
    if ((data & 0xfff0707fu) == 0x64004073u)
        return RISCV_HLV_H;
    if ((data & 0xfff0707fu) == 0x64104073u)
        return RISCV_HLV_HU;
    if ((data & 0xfff0707fu) == 0x68004073u)
        return RISCV_HLV_W;
    if ((data & 0xfff0707fu) == 0x68104073u)
        return RISCV_HLV_WU;
    if ((data & 0xfff0707fu) == 0x64304073u)
        return RISCV_HLVX_HU;
    if ((data & 0xfff0707fu) == 0x68304073u)
        return RISCV_HLVX_WU;
    if ((data & 0xfe007fffu) == 0x62004073u)
        return RISCV_HSV_B;
    if ((data & 0xfe007fffu) == 0x6e004073u)
        return RISCV_HSV_D;
    if ((data & 0xfe007fffu) == 0x66004073u)
        return RISCV_HSV_H;
    if ((data & 0xfe007fffu) == 0x6a004073u)
        return RISCV_HSV_W;
    if ((data & 0x7fu) == 0x6fu)
        return RISCV_JAL;
    if ((data & 0x707fu) == 0x67u)
        return RISCV_JALR;
    if ((data & 0x707fu) == 0x3u)
        return RISCV_LB;
    if ((data & 0xfdf0707fu) == 0x3400002fu)
        return RISCV_LB_AQ;
    if ((data & 0x707fu) == 0x4003u)
        return RISCV_LBU;
    if ((data & 0x707fu) == 0x3003u)
        return RISCV_LD;
    if ((data & 0xfdf0707fu) == 0x3400302fu)
        return RISCV_LD_AQ;
    if ((data & 0x707fu) == 0x1003u)
        return RISCV_LH;
    if ((data & 0xfdf0707fu) == 0x3400102fu)
        return RISCV_LH_AQ;
    if ((data & 0x707fu) == 0x5003u)
        return RISCV_LHU;
    if ((data & 0xf9f0707fu) == 0x1000002fu)
        return RISCV_LR_B;
    if ((data & 0xf9f0707fu) == 0x1000302fu)
        return RISCV_LR_D;
    if ((data & 0xf9f0707fu) == 0x1000102fu)
        return RISCV_LR_H;
    if ((data & 0xf9f0707fu) == 0x1000202fu)
        return RISCV_LR_W;
    if ((data & 0x7fu) == 0x37u)
        return RISCV_LUI;
    if ((data & 0x707fu) == 0x2003u)
        return RISCV_LW;
    if ((data & 0xfdf0707fu) == 0x3400202fu)
        return RISCV_LW_AQ;
    if ((data & 0x707fu) == 0x6003u)
        return RISCV_LWU;
    if ((data & 0xfe00707fu) == 0x9000302fu)
        return RISCV_LXD;
    if ((data & 0xfe00707fu) == 0x9000102fu)
        return RISCV_LXH;
    if ((data & 0xfe00707fu) == 0x9000502fu)
        return RISCV_LXHU;
    if ((data & 0xfe00707fu) == 0xd000002fu)
        return RISCV_LXSB;
    if ((data & 0xfe00707fu) == 0xd000402fu)
        return RISCV_LXSBU;
    if ((data & 0xfe00707fu) == 0xd000302fu)
        return RISCV_LXSD;
    if ((data & 0xfe00707fu) == 0xd000102fu)
        return RISCV_LXSH;
    if ((data & 0xfe00707fu) == 0xd000502fu)
        return RISCV_LXSHU;
    if ((data & 0xfe00707fu) == 0xf000002fu)
        return RISCV_LXSUWB;
    if ((data & 0xfe00707fu) == 0xf000402fu)
        return RISCV_LXSUWBU;
    if ((data & 0xfe00707fu) == 0xf000302fu)
        return RISCV_LXSUWD;
    if ((data & 0xfe00707fu) == 0xf000102fu)
        return RISCV_LXSUWH;
    if ((data & 0xfe00707fu) == 0xf000502fu)
        return RISCV_LXSUWHU;
    if ((data & 0xfe00707fu) == 0xf000202fu)
        return RISCV_LXSUWW;
    if ((data & 0xfe00707fu) == 0xf000602fu)
        return RISCV_LXSUWWU;
    if ((data & 0xfe00707fu) == 0xd000202fu)
        return RISCV_LXSW;
    if ((data & 0xfe00707fu) == 0xd000602fu)
        return RISCV_LXSWU;
    if ((data & 0xfe00707fu) == 0x9000202fu)
        return RISCV_LXW;
    if ((data & 0xfe00707fu) == 0x9000602fu)
        return RISCV_LXWU;
    if ((data & 0xfe00707fu) == 0x8e00303bu)
        return RISCV_MACC_W00;
    if ((data & 0xfe00707fu) == 0x9e00103bu)
        return RISCV_MACC_W01;
    if ((data & 0xfe00707fu) == 0x9e00303bu)
        return RISCV_MACC_W11;
    if ((data & 0xfe00707fu) == 0xee00303bu)
        return RISCV_MACCSU_W00;
    if ((data & 0xfe00707fu) == 0xfe00303bu)
        return RISCV_MACCSU_W11;
    if ((data & 0xfe00707fu) == 0xae00303bu)
        return RISCV_MACCU_W00;
    if ((data & 0xfe00707fu) == 0xbe00103bu)
        return RISCV_MACCU_W01;
    if ((data & 0xfe00707fu) == 0xbe00303bu)
        return RISCV_MACCU_W11;
    if ((data & 0xfe00707fu) == 0xa006033u)
        return RISCV_MAX;
    if ((data & 0xfe00707fu) == 0xa007033u)
        return RISCV_MAXU;
    if ((data & 0xfe00707fu) == 0xac00103bu)
        return RISCV_MERGE;
    if ((data & 0xfe00707fu) == 0xa004033u)
        return RISCV_MIN;
    if ((data & 0xfe00707fu) == 0xa005033u)
        return RISCV_MINU;
    if ((data & 0xffffffffu) == 0x70200073u)
        return RISCV_MNRET;
    if ((data & 0xb3c0707fu) == 0x81c04073u)
        return RISCV_MOP_R_N;
    if ((data & 0xb200707fu) == 0x82004073u)
        return RISCV_MOP_RR_N;
    if ((data & 0xfe00707fu) == 0xea00703bu)
        return RISCV_MQACC_W00;
    if ((data & 0xfe00707fu) == 0xfa00503bu)
        return RISCV_MQACC_W01;
    if ((data & 0xfe00707fu) == 0xfa00703bu)
        return RISCV_MQACC_W11;
    if ((data & 0xfe00707fu) == 0xee00703bu)
        return RISCV_MQRACC_W00;
    if ((data & 0xfe00707fu) == 0xfe00503bu)
        return RISCV_MQRACC_W01;
    if ((data & 0xfe00707fu) == 0xfe00703bu)
        return RISCV_MQRACC_W11;
    if ((data & 0xfe0070ffu) == 0x7e00209bu)
        return RISCV_MQRWACC;
    if ((data & 0xfe0070ffu) == 0x7a00209bu)
        return RISCV_MQWACC;
    if ((data & 0xffffffffu) == 0x30200073u)
        return RISCV_MRET;
    if ((data & 0xfe00707fu) == 0x2000033u)
        return RISCV_MUL;
    if ((data & 0xfe00707fu) == 0x8600303bu)
        return RISCV_MUL_W00;
    if ((data & 0xfe00707fu) == 0x9600103bu)
        return RISCV_MUL_W01;
    if ((data & 0xfe00707fu) == 0x9600303bu)
        return RISCV_MUL_W11;
    if ((data & 0xfe00707fu) == 0x2001033u)
        return RISCV_MULH;
    if ((data & 0xfe00707fu) == 0x2002033u)
        return RISCV_MULHSU;
    if ((data & 0xfe00707fu) == 0x2003033u)
        return RISCV_MULHU;
    if ((data & 0xfe00707fu) == 0xe600303bu)
        return RISCV_MULSU_W00;
    if ((data & 0xfe00707fu) == 0xf600303bu)
        return RISCV_MULSU_W11;
    if ((data & 0xfe00707fu) == 0xa600303bu)
        return RISCV_MULU_W00;
    if ((data & 0xfe00707fu) == 0xb600103bu)
        return RISCV_MULU_W01;
    if ((data & 0xfe00707fu) == 0xb600303bu)
        return RISCV_MULU_W11;
    if ((data & 0xfe00707fu) == 0x200003bu)
        return RISCV_MULW;
    if ((data & 0xfe00707fu) == 0xa800103bu)
        return RISCV_MVM;
    if ((data & 0xfe00707fu) == 0xaa00103bu)
        return RISCV_MVMN;
    if ((data & 0xfe00f07fu) == 0x6e00c01bu)
        return RISCV_NCLIP;
    if ((data & 0xfc00f07fu) == 0x6400c01bu)
        return RISCV_NCLIPI;
    if ((data & 0xfc00f07fu) == 0x2400c01bu)
        return RISCV_NCLIPIU;
    if ((data & 0xfe00f07fu) == 0x7e00c01bu)
        return RISCV_NCLIPR;
    if ((data & 0xfc00f07fu) == 0x7400c01bu)
        return RISCV_NCLIPRI;
    if ((data & 0xfc00f07fu) == 0x3400c01bu)
        return RISCV_NCLIPRIU;
    if ((data & 0xfe00f07fu) == 0x3e00c01bu)
        return RISCV_NCLIPRU;
    if ((data & 0xfe00f07fu) == 0x2e00c01bu)
        return RISCV_NCLIPU;
    if ((data & 0xfe00f07fu) == 0x4e00c01bu)
        return RISCV_NSRA;
    if ((data & 0xfc00f07fu) == 0x4400c01bu)
        return RISCV_NSRAI;
    if ((data & 0xfe00f07fu) == 0x5e00c01bu)
        return RISCV_NSRAR;
    if ((data & 0xfc00f07fu) == 0x5400c01bu)
        return RISCV_NSRARI;
    if ((data & 0xfe00f07fu) == 0xe00c01bu)
        return RISCV_NSRL;
    if ((data & 0xfc00f07fu) == 0x400c01bu)
        return RISCV_NSRLI;
    if ((data & 0xfe00707fu) == 0x6033u)
        return RISCV_OR;
    if ((data & 0xfff0707fu) == 0x28705013u)
        return RISCV_ORC_B;
    if ((data & 0x707fu) == 0x6013u)
        return RISCV_ORI;
    if ((data & 0xfe00707fu) == 0x40006033u)
        return RISCV_ORN;
    if ((data & 0xfe00707fu) == 0x9c00003bu)
        return RISCV_PAADD_B;
    if ((data & 0xfe10f0ffu) == 0x9c00601bu)
        return RISCV_PAADD_DB;
    if ((data & 0xfe10f0ffu) == 0x9800601bu)
        return RISCV_PAADD_DH;
    if ((data & 0xfe10f0ffu) == 0x9a00601bu)
        return RISCV_PAADD_DW;
    if ((data & 0xfe00707fu) == 0x9800003bu)
        return RISCV_PAADD_H;
    if ((data & 0xfe00707fu) == 0x9a00003bu)
        return RISCV_PAADD_W;
    if ((data & 0xfe00707fu) == 0xbc00003bu)
        return RISCV_PAADDU_B;
    if ((data & 0xfe10f0ffu) == 0xbc00601bu)
        return RISCV_PAADDU_DB;
    if ((data & 0xfe10f0ffu) == 0xb800601bu)
        return RISCV_PAADDU_DH;
    if ((data & 0xfe10f0ffu) == 0xba00601bu)
        return RISCV_PAADDU_DW;
    if ((data & 0xfe00707fu) == 0xb800003bu)
        return RISCV_PAADDU_H;
    if ((data & 0xfe00707fu) == 0xba00003bu)
        return RISCV_PAADDU_W;
    if ((data & 0xfe10f0ffu) == 0x9810e01bu)
        return RISCV_PAAS_DHX;
    if ((data & 0xfe00707fu) == 0x9800603bu)
        return RISCV_PAAS_HX;
    if ((data & 0xfe00707fu) == 0x9a00603bu)
        return RISCV_PAAS_WX;
    if ((data & 0xfe00707fu) == 0xcc00003bu)
        return RISCV_PABD_B;
    if ((data & 0xfe10f0ffu) == 0xcc00601bu)
        return RISCV_PABD_DB;
    if ((data & 0xfe10f0ffu) == 0xc800601bu)
        return RISCV_PABD_DH;
    if ((data & 0xfe00707fu) == 0xc800003bu)
        return RISCV_PABD_H;
    if ((data & 0xfe00707fu) == 0xbc00103bu)
        return RISCV_PABDSUMAU_B;
    if ((data & 0xfe00707fu) == 0xb400103bu)
        return RISCV_PABDSUMU_B;
    if ((data & 0xfe00707fu) == 0xec00003bu)
        return RISCV_PABDU_B;
    if ((data & 0xfe10f0ffu) == 0xec00601bu)
        return RISCV_PABDU_DB;
    if ((data & 0xfe10f0ffu) == 0xe800601bu)
        return RISCV_PABDU_DH;
    if ((data & 0xfe00707fu) == 0xe800003bu)
        return RISCV_PABDU_H;
    if ((data & 0xfff0707fu) == 0x8004033u)
        return RISCV_ZEXT_H_RV32;
    if ((data & 0xfe00707fu) == 0x8004033u)
        return RISCV_PACK;
    if ((data & 0xfe00707fu) == 0x8007033u)
        return RISCV_PACKH;
    if ((data & 0xfff0707fu) == 0x800403bu)
        return RISCV_ZEXT_H;
    if ((data & 0xfe00707fu) == 0x800403bu)
        return RISCV_PACKW;
    if ((data & 0xfe00707fu) == 0x8400003bu)
        return RISCV_PADD_B;
    if ((data & 0xfe00707fu) == 0x9c00201bu)
        return RISCV_PADD_BS;
    if ((data & 0xfe10f0ffu) == 0x8400601bu)
        return RISCV_PADD_DB;
    if ((data & 0xfe00f0ffu) == 0x1c00601bu)
        return RISCV_PADD_DBS;
    if ((data & 0xfe10f0ffu) == 0x8000601bu)
        return RISCV_PADD_DH;
    if ((data & 0xfe00f0ffu) == 0x1800601bu)
        return RISCV_PADD_DHS;
    if ((data & 0xfe10f0ffu) == 0x8200601bu)
        return RISCV_PADD_DW;
    if ((data & 0xfe00f0ffu) == 0x1a00601bu)
        return RISCV_PADD_DWS;
    if ((data & 0xfe00707fu) == 0x8000003bu)
        return RISCV_PADD_H;
    if ((data & 0xfe00707fu) == 0x9800201bu)
        return RISCV_PADD_HS;
    if ((data & 0xfe00707fu) == 0x8200003bu)
        return RISCV_PADD_W;
    if ((data & 0xfe00707fu) == 0x9a00201bu)
        return RISCV_PADD_WS;
    if ((data & 0xfe10f0ffu) == 0x8010e01bu)
        return RISCV_PAS_DHX;
    if ((data & 0xfe00707fu) == 0x8000603bu)
        return RISCV_PAS_HX;
    if ((data & 0xfe00707fu) == 0x8200603bu)
        return RISCV_PAS_WX;
    if ((data & 0xfe10f0ffu) == 0x9c10e01bu)
        return RISCV_PASA_DHX;
    if ((data & 0xfe00707fu) == 0x9c00603bu)
        return RISCV_PASA_HX;
    if ((data & 0xfe00707fu) == 0x9e00603bu)
        return RISCV_PASA_WX;
    if ((data & 0xfe00707fu) == 0xdc00003bu)
        return RISCV_PASUB_B;
    if ((data & 0xfe10f0ffu) == 0xdc00601bu)
        return RISCV_PASUB_DB;
    if ((data & 0xfe10f0ffu) == 0xd800601bu)
        return RISCV_PASUB_DH;
    if ((data & 0xfe10f0ffu) == 0xda00601bu)
        return RISCV_PASUB_DW;
    if ((data & 0xfe00707fu) == 0xd800003bu)
        return RISCV_PASUB_H;
    if ((data & 0xfe00707fu) == 0xda00003bu)
        return RISCV_PASUB_W;
    if ((data & 0xfe00707fu) == 0xfc00003bu)
        return RISCV_PASUBU_B;
    if ((data & 0xfe10f0ffu) == 0xfc00601bu)
        return RISCV_PASUBU_DB;
    if ((data & 0xfe10f0ffu) == 0xf800601bu)
        return RISCV_PASUBU_DH;
    if ((data & 0xfe10f0ffu) == 0xfa00601bu)
        return RISCV_PASUBU_DW;
    if ((data & 0xfe00707fu) == 0xf800003bu)
        return RISCV_PASUBU_H;
    if ((data & 0xfe00707fu) == 0xfa00003bu)
        return RISCV_PASUBU_W;
    if ((data & 0xff00f07fu) == 0xb400201bu)
        return RISCV_PLI_B;
    if ((data & 0xff00f0ffu) == 0x3400201bu)
        return RISCV_PLI_DB;
    if ((data & 0xfe0070ffu) == 0x3000201bu)
        return RISCV_PLI_DH;
    if ((data & 0xfe00707fu) == 0xb000201bu)
        return RISCV_PLI_H;
    if ((data & 0xfe00707fu) == 0xb200201bu)
        return RISCV_PLI_W;
    if ((data & 0xfe0070ffu) == 0x7000201bu)
        return RISCV_PLUI_DH;
    if ((data & 0xfe00707fu) == 0xf000201bu)
        return RISCV_PLUI_H;
    if ((data & 0xfe00707fu) == 0xf200201bu)
        return RISCV_PLUI_W;
    if ((data & 0xfe00707fu) == 0x8000503bu)
        return RISCV_PM2ADD_H;
    if ((data & 0xfe00707fu) == 0x9000503bu)
        return RISCV_PM2ADD_HX;
    if ((data & 0xfe00707fu) == 0x8200503bu)
        return RISCV_PM2ADD_W;
    if ((data & 0xfe00707fu) == 0x9200503bu)
        return RISCV_PM2ADD_WX;
    if ((data & 0xfe00707fu) == 0x8800503bu)
        return RISCV_PM2ADDA_H;
    if ((data & 0xfe00707fu) == 0x9800503bu)
        return RISCV_PM2ADDA_HX;
    if ((data & 0xfe00707fu) == 0x8a00503bu)
        return RISCV_PM2ADDA_W;
    if ((data & 0xfe00707fu) == 0x9a00503bu)
        return RISCV_PM2ADDA_WX;
    if ((data & 0xfe00707fu) == 0xe800503bu)
        return RISCV_PM2ADDASU_H;
    if ((data & 0xfe00707fu) == 0xea00503bu)
        return RISCV_PM2ADDASU_W;
    if ((data & 0xfe00707fu) == 0xa800503bu)
        return RISCV_PM2ADDAU_H;
    if ((data & 0xfe00707fu) == 0xaa00503bu)
        return RISCV_PM2ADDAU_W;
    if ((data & 0xfe00707fu) == 0xe000503bu)
        return RISCV_PM2ADDSU_H;
    if ((data & 0xfe00707fu) == 0xe200503bu)
        return RISCV_PM2ADDSU_W;
    if ((data & 0xfe00707fu) == 0xa000503bu)
        return RISCV_PM2ADDU_H;
    if ((data & 0xfe00707fu) == 0xa200503bu)
        return RISCV_PM2ADDU_W;
    if ((data & 0xfe00707fu) == 0xc400503bu)
        return RISCV_PM2SADD_H;
    if ((data & 0xfe00707fu) == 0xd400503bu)
        return RISCV_PM2SADD_HX;
    if ((data & 0xfe00707fu) == 0xc000503bu)
        return RISCV_PM2SUB_H;
    if ((data & 0xfe00707fu) == 0xd000503bu)
        return RISCV_PM2SUB_HX;
    if ((data & 0xfe00707fu) == 0xc200503bu)
        return RISCV_PM2SUB_W;
    if ((data & 0xfe00707fu) == 0xd200503bu)
        return RISCV_PM2SUB_WX;
    if ((data & 0xfe00707fu) == 0xc800503bu)
        return RISCV_PM2SUBA_H;
    if ((data & 0xfe00707fu) == 0xd800503bu)
        return RISCV_PM2SUBA_HX;
    if ((data & 0xfe00707fu) == 0xca00503bu)
        return RISCV_PM2SUBA_W;
    if ((data & 0xfe00707fu) == 0xda00503bu)
        return RISCV_PM2SUBA_WX;
    if ((data & 0xfe0070ffu) == 0x600209bu)
        return RISCV_PM2WADD_H;
    if ((data & 0xfe0070ffu) == 0x1600209bu)
        return RISCV_PM2WADD_HX;
    if ((data & 0xfe0070ffu) == 0xe00209bu)
        return RISCV_PM2WADDA_H;
    if ((data & 0xfe0070ffu) == 0x1e00209bu)
        return RISCV_PM2WADDA_HX;
    if ((data & 0xfe0070ffu) == 0x6e00209bu)
        return RISCV_PM2WADDASU_H;
    if ((data & 0xfe0070ffu) == 0x2e00209bu)
        return RISCV_PM2WADDAU_H;
    if ((data & 0xfe0070ffu) == 0x6600209bu)
        return RISCV_PM2WADDSU_H;
    if ((data & 0xfe0070ffu) == 0x2600209bu)
        return RISCV_PM2WADDU_H;
    if ((data & 0xfe0070ffu) == 0x4600209bu)
        return RISCV_PM2WSUB_H;
    if ((data & 0xfe0070ffu) == 0x5600209bu)
        return RISCV_PM2WSUB_HX;
    if ((data & 0xfe0070ffu) == 0x4e00209bu)
        return RISCV_PM2WSUBA_H;
    if ((data & 0xfe0070ffu) == 0x5e00209bu)
        return RISCV_PM2WSUBA_HX;
    if ((data & 0xfe00707fu) == 0x8400503bu)
        return RISCV_PM4ADD_B;
    if ((data & 0xfe00707fu) == 0x8600503bu)
        return RISCV_PM4ADD_H;
    if ((data & 0xfe00707fu) == 0x8c00503bu)
        return RISCV_PM4ADDA_B;
    if ((data & 0xfe00707fu) == 0x8e00503bu)
        return RISCV_PM4ADDA_H;
    if ((data & 0xfe00707fu) == 0xec00503bu)
        return RISCV_PM4ADDASU_B;
    if ((data & 0xfe00707fu) == 0xee00503bu)
        return RISCV_PM4ADDASU_H;
    if ((data & 0xfe00707fu) == 0xac00503bu)
        return RISCV_PM4ADDAU_B;
    if ((data & 0xfe00707fu) == 0xae00503bu)
        return RISCV_PM4ADDAU_H;
    if ((data & 0xfe00707fu) == 0xe400503bu)
        return RISCV_PM4ADDSU_B;
    if ((data & 0xfe00707fu) == 0xe600503bu)
        return RISCV_PM4ADDSU_H;
    if ((data & 0xfe00707fu) == 0xa400503bu)
        return RISCV_PM4ADDU_B;
    if ((data & 0xfe00707fu) == 0xa600503bu)
        return RISCV_PM4ADDU_H;
    if ((data & 0xfe00707fu) == 0x8a00303bu)
        return RISCV_PMACC_W_H00;
    if ((data & 0xfe00707fu) == 0x9a00103bu)
        return RISCV_PMACC_W_H01;
    if ((data & 0xfe00707fu) == 0x9a00303bu)
        return RISCV_PMACC_W_H11;
    if ((data & 0xfe00707fu) == 0xea00303bu)
        return RISCV_PMACCSU_W_H00;
    if ((data & 0xfe00707fu) == 0xfa00303bu)
        return RISCV_PMACCSU_W_H11;
    if ((data & 0xfe00707fu) == 0xaa00303bu)
        return RISCV_PMACCU_W_H00;
    if ((data & 0xfe00707fu) == 0xba00103bu)
        return RISCV_PMACCU_W_H01;
    if ((data & 0xfe00707fu) == 0xba00303bu)
        return RISCV_PMACCU_W_H11;
    if ((data & 0xfe00707fu) == 0xf400603bu)
        return RISCV_PMAX_B;
    if ((data & 0xfe10f0ffu) == 0xf410e01bu)
        return RISCV_PMAX_DB;
    if ((data & 0xfe10f0ffu) == 0xf010e01bu)
        return RISCV_PMAX_DH;
    if ((data & 0xfe10f0ffu) == 0xf210e01bu)
        return RISCV_PMAX_DW;
    if ((data & 0xfe00707fu) == 0xf000603bu)
        return RISCV_PMAX_H;
    if ((data & 0xfe00707fu) == 0xf200603bu)
        return RISCV_PMAX_W;
    if ((data & 0xfe00707fu) == 0xfc00603bu)
        return RISCV_PMAXU_B;
    if ((data & 0xfe10f0ffu) == 0xfc10e01bu)
        return RISCV_PMAXU_DB;
    if ((data & 0xfe10f0ffu) == 0xf810e01bu)
        return RISCV_PMAXU_DH;
    if ((data & 0xfe10f0ffu) == 0xfa10e01bu)
        return RISCV_PMAXU_DW;
    if ((data & 0xfe00707fu) == 0xf800603bu)
        return RISCV_PMAXU_H;
    if ((data & 0xfe00707fu) == 0xfa00603bu)
        return RISCV_PMAXU_W;
    if ((data & 0xfe00707fu) == 0x8800703bu)
        return RISCV_PMHACC_H;
    if ((data & 0xfe00707fu) == 0xa800703bu)
        return RISCV_PMHACC_H_B0;
    if ((data & 0xfe00707fu) == 0xb800703bu)
        return RISCV_PMHACC_H_B1;
    if ((data & 0xfe00707fu) == 0x8a00703bu)
        return RISCV_PMHACC_W;
    if ((data & 0xfe00707fu) == 0xaa00703bu)
        return RISCV_PMHACC_W_H0;
    if ((data & 0xfe00707fu) == 0xba00703bu)
        return RISCV_PMHACC_W_H1;
    if ((data & 0xfe00707fu) == 0xc800703bu)
        return RISCV_PMHACCSU_H;
    if ((data & 0xfe00707fu) == 0xac00703bu)
        return RISCV_PMHACCSU_H_B0;
    if ((data & 0xfe00707fu) == 0xbc00703bu)
        return RISCV_PMHACCSU_H_B1;
    if ((data & 0xfe00707fu) == 0xca00703bu)
        return RISCV_PMHACCSU_W;
    if ((data & 0xfe00707fu) == 0xae00703bu)
        return RISCV_PMHACCSU_W_H0;
    if ((data & 0xfe00707fu) == 0xbe00703bu)
        return RISCV_PMHACCSU_W_H1;
    if ((data & 0xfe00707fu) == 0x9800703bu)
        return RISCV_PMHACCU_H;
    if ((data & 0xfe00707fu) == 0x9a00703bu)
        return RISCV_PMHACCU_W;
    if ((data & 0xfe00707fu) == 0x8c00703bu)
        return RISCV_PMHRACC_H;
    if ((data & 0xfe00707fu) == 0x8e00703bu)
        return RISCV_PMHRACC_W;
    if ((data & 0xfe00707fu) == 0xcc00703bu)
        return RISCV_PMHRACCSU_H;
    if ((data & 0xfe00707fu) == 0xce00703bu)
        return RISCV_PMHRACCSU_W;
    if ((data & 0xfe00707fu) == 0x9c00703bu)
        return RISCV_PMHRACCU_H;
    if ((data & 0xfe00707fu) == 0x9e00703bu)
        return RISCV_PMHRACCU_W;
    if ((data & 0xfe00707fu) == 0xe400603bu)
        return RISCV_PMIN_B;
    if ((data & 0xfe10f0ffu) == 0xe410e01bu)
        return RISCV_PMIN_DB;
    if ((data & 0xfe10f0ffu) == 0xe010e01bu)
        return RISCV_PMIN_DH;
    if ((data & 0xfe10f0ffu) == 0xe210e01bu)
        return RISCV_PMIN_DW;
    if ((data & 0xfe00707fu) == 0xe000603bu)
        return RISCV_PMIN_H;
    if ((data & 0xfe00707fu) == 0xe200603bu)
        return RISCV_PMIN_W;
    if ((data & 0xfe00707fu) == 0xec00603bu)
        return RISCV_PMINU_B;
    if ((data & 0xfe10f0ffu) == 0xec10e01bu)
        return RISCV_PMINU_DB;
    if ((data & 0xfe10f0ffu) == 0xe810e01bu)
        return RISCV_PMINU_DH;
    if ((data & 0xfe10f0ffu) == 0xea10e01bu)
        return RISCV_PMINU_DW;
    if ((data & 0xfe00707fu) == 0xe800603bu)
        return RISCV_PMINU_H;
    if ((data & 0xfe00707fu) == 0xea00603bu)
        return RISCV_PMINU_W;
    if ((data & 0xfe00707fu) == 0xb000503bu)
        return RISCV_PMQ2ADD_H;
    if ((data & 0xfe00707fu) == 0xb200503bu)
        return RISCV_PMQ2ADD_W;
    if ((data & 0xfe00707fu) == 0xb800503bu)
        return RISCV_PMQ2ADDA_H;
    if ((data & 0xfe00707fu) == 0xba00503bu)
        return RISCV_PMQ2ADDA_W;
    if ((data & 0xfe00707fu) == 0xe800703bu)
        return RISCV_PMQACC_W_H00;
    if ((data & 0xfe00707fu) == 0xf800503bu)
        return RISCV_PMQACC_W_H01;
    if ((data & 0xfe00707fu) == 0xf800703bu)
        return RISCV_PMQACC_W_H11;
    if ((data & 0xfe00707fu) == 0xb400503bu)
        return RISCV_PMQR2ADD_H;
    if ((data & 0xfe00707fu) == 0xb600503bu)
        return RISCV_PMQR2ADD_W;
    if ((data & 0xfe00707fu) == 0xbc00503bu)
        return RISCV_PMQR2ADDA_H;
    if ((data & 0xfe00707fu) == 0xbe00503bu)
        return RISCV_PMQR2ADDA_W;
    if ((data & 0xfe00707fu) == 0xec00703bu)
        return RISCV_PMQRACC_W_H00;
    if ((data & 0xfe00707fu) == 0xfc00503bu)
        return RISCV_PMQRACC_W_H01;
    if ((data & 0xfe00707fu) == 0xfc00703bu)
        return RISCV_PMQRACC_W_H11;
    if ((data & 0xfe0070ffu) == 0x7c00209bu)
        return RISCV_PMQRWACC_H;
    if ((data & 0xfe0070ffu) == 0x7800209bu)
        return RISCV_PMQWACC_H;
    if ((data & 0xfe00707fu) == 0xc400603bu)
        return RISCV_PMSEQ_B;
    if ((data & 0xfe10f0ffu) == 0xc410e01bu)
        return RISCV_PMSEQ_DB;
    if ((data & 0xfe10f0ffu) == 0xc010e01bu)
        return RISCV_PMSEQ_DH;
    if ((data & 0xfe10f0ffu) == 0xc210e01bu)
        return RISCV_PMSEQ_DW;
    if ((data & 0xfe00707fu) == 0xc000603bu)
        return RISCV_PMSEQ_H;
    if ((data & 0xfe00707fu) == 0xc200603bu)
        return RISCV_PMSEQ_W;
    if ((data & 0xfe00707fu) == 0xd400603bu)
        return RISCV_PMSLT_B;
    if ((data & 0xfe10f0ffu) == 0xd410e01bu)
        return RISCV_PMSLT_DB;
    if ((data & 0xfe10f0ffu) == 0xd010e01bu)
        return RISCV_PMSLT_DH;
    if ((data & 0xfe10f0ffu) == 0xd210e01bu)
        return RISCV_PMSLT_DW;
    if ((data & 0xfe00707fu) == 0xd000603bu)
        return RISCV_PMSLT_H;
    if ((data & 0xfe00707fu) == 0xd200603bu)
        return RISCV_PMSLT_W;
    if ((data & 0xfe00707fu) == 0xdc00603bu)
        return RISCV_PMSLTU_B;
    if ((data & 0xfe10f0ffu) == 0xdc10e01bu)
        return RISCV_PMSLTU_DB;
    if ((data & 0xfe10f0ffu) == 0xd810e01bu)
        return RISCV_PMSLTU_DH;
    if ((data & 0xfe10f0ffu) == 0xda10e01bu)
        return RISCV_PMSLTU_DW;
    if ((data & 0xfe00707fu) == 0xd800603bu)
        return RISCV_PMSLTU_H;
    if ((data & 0xfe00707fu) == 0xda00603bu)
        return RISCV_PMSLTU_W;
    if ((data & 0xfe00707fu) == 0x8000303bu)
        return RISCV_PMUL_H_B00;
    if ((data & 0xfe00707fu) == 0x9000103bu)
        return RISCV_PMUL_H_B01;
    if ((data & 0xfe00707fu) == 0x9000303bu)
        return RISCV_PMUL_H_B11;
    if ((data & 0xfe00707fu) == 0x8200303bu)
        return RISCV_PMUL_W_H00;
    if ((data & 0xfe00707fu) == 0x9200103bu)
        return RISCV_PMUL_W_H01;
    if ((data & 0xfe00707fu) == 0x9200303bu)
        return RISCV_PMUL_W_H11;
    if ((data & 0xfe00707fu) == 0x8000703bu)
        return RISCV_PMULH_H;
    if ((data & 0xfe00707fu) == 0xa000703bu)
        return RISCV_PMULH_H_B0;
    if ((data & 0xfe00707fu) == 0xb000703bu)
        return RISCV_PMULH_H_B1;
    if ((data & 0xfe00707fu) == 0x8200703bu)
        return RISCV_PMULH_W;
    if ((data & 0xfe00707fu) == 0xa200703bu)
        return RISCV_PMULH_W_H0;
    if ((data & 0xfe00707fu) == 0xb200703bu)
        return RISCV_PMULH_W_H1;
    if ((data & 0xfe00707fu) == 0x8400703bu)
        return RISCV_PMULHR_H;
    if ((data & 0xfe00707fu) == 0x8600703bu)
        return RISCV_PMULHR_W;
    if ((data & 0xfe00707fu) == 0xc400703bu)
        return RISCV_PMULHRSU_H;
    if ((data & 0xfe00707fu) == 0xc600703bu)
        return RISCV_PMULHRSU_W;
    if ((data & 0xfe00707fu) == 0x9400703bu)
        return RISCV_PMULHRU_H;
    if ((data & 0xfe00707fu) == 0x9600703bu)
        return RISCV_PMULHRU_W;
    if ((data & 0xfe00707fu) == 0xc000703bu)
        return RISCV_PMULHSU_H;
    if ((data & 0xfe00707fu) == 0xa400703bu)
        return RISCV_PMULHSU_H_B0;
    if ((data & 0xfe00707fu) == 0xb400703bu)
        return RISCV_PMULHSU_H_B1;
    if ((data & 0xfe00707fu) == 0xc200703bu)
        return RISCV_PMULHSU_W;
    if ((data & 0xfe00707fu) == 0xa600703bu)
        return RISCV_PMULHSU_W_H0;
    if ((data & 0xfe00707fu) == 0xb600703bu)
        return RISCV_PMULHSU_W_H1;
    if ((data & 0xfe00707fu) == 0x9000703bu)
        return RISCV_PMULHU_H;
    if ((data & 0xfe00707fu) == 0x9200703bu)
        return RISCV_PMULHU_W;
    if ((data & 0xfe00707fu) == 0xd000703bu)
        return RISCV_PMULQ_H;
    if ((data & 0xfe00707fu) == 0xd200703bu)
        return RISCV_PMULQ_W;
    if ((data & 0xfe00707fu) == 0xd400703bu)
        return RISCV_PMULQR_H;
    if ((data & 0xfe00707fu) == 0xd600703bu)
        return RISCV_PMULQR_W;
    if ((data & 0xfe00707fu) == 0xe000303bu)
        return RISCV_PMULSU_H_B00;
    if ((data & 0xfe00707fu) == 0xf000303bu)
        return RISCV_PMULSU_H_B11;
    if ((data & 0xfe00707fu) == 0xe200303bu)
        return RISCV_PMULSU_W_H00;
    if ((data & 0xfe00707fu) == 0xf200303bu)
        return RISCV_PMULSU_W_H11;
    if ((data & 0xfe00707fu) == 0xa000303bu)
        return RISCV_PMULU_H_B00;
    if ((data & 0xfe00707fu) == 0xb000103bu)
        return RISCV_PMULU_H_B01;
    if ((data & 0xfe00707fu) == 0xb000303bu)
        return RISCV_PMULU_H_B11;
    if ((data & 0xfe00707fu) == 0xa200303bu)
        return RISCV_PMULU_W_H00;
    if ((data & 0xfe00707fu) == 0xb200103bu)
        return RISCV_PMULU_W_H01;
    if ((data & 0xfe00707fu) == 0xb200303bu)
        return RISCV_PMULU_W_H11;
    if ((data & 0xfe00f07fu) == 0x6800c01bu)
        return RISCV_PNCLIP_BS;
    if ((data & 0xfe00f07fu) == 0x6a00c01bu)
        return RISCV_PNCLIP_HS;
    if ((data & 0xff00f07fu) == 0x6100c01bu)
        return RISCV_PNCLIPI_B;
    if ((data & 0xfe00f07fu) == 0x6200c01bu)
        return RISCV_PNCLIPI_H;
    if ((data & 0xff00f07fu) == 0x2100c01bu)
        return RISCV_PNCLIPIU_B;
    if ((data & 0xfe00f07fu) == 0x2200c01bu)
        return RISCV_PNCLIPIU_H;
    if ((data & 0xfe00707fu) == 0xc000203bu)
        return RISCV_PNCLIPP_B;
    if ((data & 0xfe00707fu) == 0xc200203bu)
        return RISCV_PNCLIPP_H;
    if ((data & 0xfe00707fu) == 0xc600203bu)
        return RISCV_PNCLIPP_W;
    if ((data & 0xfe00f07fu) == 0x7800c01bu)
        return RISCV_PNCLIPR_BS;
    if ((data & 0xfe00f07fu) == 0x7a00c01bu)
        return RISCV_PNCLIPR_HS;
    if ((data & 0xff00f07fu) == 0x7100c01bu)
        return RISCV_PNCLIPRI_B;
    if ((data & 0xfe00f07fu) == 0x7200c01bu)
        return RISCV_PNCLIPRI_H;
    if ((data & 0xff00f07fu) == 0x3100c01bu)
        return RISCV_PNCLIPRIU_B;
    if ((data & 0xfe00f07fu) == 0x3200c01bu)
        return RISCV_PNCLIPRIU_H;
    if ((data & 0xfe00f07fu) == 0x3800c01bu)
        return RISCV_PNCLIPRU_BS;
    if ((data & 0xfe00f07fu) == 0x3a00c01bu)
        return RISCV_PNCLIPRU_HS;
    if ((data & 0xfe00f07fu) == 0x2800c01bu)
        return RISCV_PNCLIPU_BS;
    if ((data & 0xfe00f07fu) == 0x2a00c01bu)
        return RISCV_PNCLIPU_HS;
    if ((data & 0xfe00707fu) == 0x8000203bu)
        return RISCV_PNCLIPUP_B;
    if ((data & 0xfe00707fu) == 0x8200203bu)
        return RISCV_PNCLIPUP_H;
    if ((data & 0xfe00707fu) == 0x8600203bu)
        return RISCV_PNCLIPUP_W;
    if ((data & 0xfe00f07fu) == 0x4800c01bu)
        return RISCV_PNSRA_BS;
    if ((data & 0xfe00f07fu) == 0x4a00c01bu)
        return RISCV_PNSRA_HS;
    if ((data & 0xff00f07fu) == 0x4100c01bu)
        return RISCV_PNSRAI_B;
    if ((data & 0xfe00f07fu) == 0x4200c01bu)
        return RISCV_PNSRAI_H;
    if ((data & 0xfe00f07fu) == 0x5800c01bu)
        return RISCV_PNSRAR_BS;
    if ((data & 0xfe00f07fu) == 0x5a00c01bu)
        return RISCV_PNSRAR_HS;
    if ((data & 0xff00f07fu) == 0x5100c01bu)
        return RISCV_PNSRARI_B;
    if ((data & 0xfe00f07fu) == 0x5200c01bu)
        return RISCV_PNSRARI_H;
    if ((data & 0xfe00f07fu) == 0x800c01bu)
        return RISCV_PNSRL_BS;
    if ((data & 0xfe00f07fu) == 0xa00c01bu)
        return RISCV_PNSRL_HS;
    if ((data & 0xff00f07fu) == 0x100c01bu)
        return RISCV_PNSRLI_B;
    if ((data & 0xfe00f07fu) == 0x200c01bu)
        return RISCV_PNSRLI_H;
    if ((data & 0xfe00707fu) == 0x8000403bu)
        return RISCV_PPAIRE_B;
    if ((data & 0xfe10f0ffu) == 0x8000e01bu)
        return RISCV_PPAIRE_DB;
    if ((data & 0xfe10f0ffu) == 0x8200e01bu)
        return RISCV_PPAIRE_DH;
    if ((data & 0xfe00707fu) == 0x8200403bu)
        return RISCV_PPAIRE_H;
    if ((data & 0xfe00707fu) == 0x9000403bu)
        return RISCV_PPAIREO_B;
    if ((data & 0xfe10f0ffu) == 0x9000e01bu)
        return RISCV_PPAIREO_DB;
    if ((data & 0xfe10f0ffu) == 0x9200e01bu)
        return RISCV_PPAIREO_DH;
    if ((data & 0xfe00707fu) == 0x9200403bu)
        return RISCV_PPAIREO_H;
    if ((data & 0xfe00707fu) == 0x9600403bu)
        return RISCV_PPAIREO_W;
    if ((data & 0xfe00707fu) == 0xb000403bu)
        return RISCV_PPAIRO_B;
    if ((data & 0xfe10f0ffu) == 0xb000e01bu)
        return RISCV_PPAIRO_DB;
    if ((data & 0xfe10f0ffu) == 0xb200e01bu)
        return RISCV_PPAIRO_DH;
    if ((data & 0xfe00707fu) == 0xb200403bu)
        return RISCV_PPAIRO_H;
    if ((data & 0xfe00707fu) == 0xb600403bu)
        return RISCV_PPAIRO_W;
    if ((data & 0xfe00707fu) == 0xa000403bu)
        return RISCV_PPAIROE_B;
    if ((data & 0xfe10f0ffu) == 0xa000e01bu)
        return RISCV_PPAIROE_DB;
    if ((data & 0xfe10f0ffu) == 0xa200e01bu)
        return RISCV_PPAIROE_DH;
    if ((data & 0xfe00707fu) == 0xa200403bu)
        return RISCV_PPAIROE_H;
    if ((data & 0xfe00707fu) == 0xa600403bu)
        return RISCV_PPAIROE_W;
    if ((data & 0xfe00707fu) == 0x9c00401bu)
        return RISCV_PREDSUM_BS;
    if ((data & 0xfe00f07fu) == 0x1c00401bu)
        return RISCV_PREDSUM_DBS;
    if ((data & 0xfe00f07fu) == 0x1800401bu)
        return RISCV_PREDSUM_DHS;
    if ((data & 0xfe00707fu) == 0x9800401bu)
        return RISCV_PREDSUM_HS;
    if ((data & 0xfe00707fu) == 0x9a00401bu)
        return RISCV_PREDSUM_WS;
    if ((data & 0xfe00707fu) == 0xbc00401bu)
        return RISCV_PREDSUMU_BS;
    if ((data & 0xfe00f07fu) == 0x3c00401bu)
        return RISCV_PREDSUMU_DBS;
    if ((data & 0xfe00f07fu) == 0x3800401bu)
        return RISCV_PREDSUMU_DHS;
    if ((data & 0xfe00707fu) == 0xb800401bu)
        return RISCV_PREDSUMU_HS;
    if ((data & 0xfe00707fu) == 0xba00401bu)
        return RISCV_PREDSUMU_WS;
    if ((data & 0xfe10f0ffu) == 0x8410e01bu)
        return RISCV_PSA_DHX;
    if ((data & 0xfe00707fu) == 0x8400603bu)
        return RISCV_PSA_HX;
    if ((data & 0xfe00707fu) == 0x8600603bu)
        return RISCV_PSA_WX;
    if ((data & 0xfff0707fu) == 0xe470201bu)
        return RISCV_PSABS_B;
    if ((data & 0xfff0f0ffu) == 0x6470601bu)
        return RISCV_PSABS_DB;
    if ((data & 0xfff0f0ffu) == 0x6070601bu)
        return RISCV_PSABS_DH;
    if ((data & 0xfff0707fu) == 0xe070201bu)
        return RISCV_PSABS_H;
    if ((data & 0xfe00707fu) == 0x9400003bu)
        return RISCV_PSADD_B;
    if ((data & 0xfe10f0ffu) == 0x9400601bu)
        return RISCV_PSADD_DB;
    if ((data & 0xfe10f0ffu) == 0x9000601bu)
        return RISCV_PSADD_DH;
    if ((data & 0xfe10f0ffu) == 0x9200601bu)
        return RISCV_PSADD_DW;
    if ((data & 0xfe00707fu) == 0x9000003bu)
        return RISCV_PSADD_H;
    if ((data & 0xfe00707fu) == 0x9200003bu)
        return RISCV_PSADD_W;
    if ((data & 0xfe00707fu) == 0xb400003bu)
        return RISCV_PSADDU_B;
    if ((data & 0xfe10f0ffu) == 0xb400601bu)
        return RISCV_PSADDU_DB;
    if ((data & 0xfe10f0ffu) == 0xb000601bu)
        return RISCV_PSADDU_DH;
    if ((data & 0xfe10f0ffu) == 0xb200601bu)
        return RISCV_PSADDU_DW;
    if ((data & 0xfe00707fu) == 0xb000003bu)
        return RISCV_PSADDU_H;
    if ((data & 0xfe00707fu) == 0xb200003bu)
        return RISCV_PSADDU_W;
    if ((data & 0xfe10f0ffu) == 0x9010e01bu)
        return RISCV_PSAS_DHX;
    if ((data & 0xfe00707fu) == 0x9000603bu)
        return RISCV_PSAS_HX;
    if ((data & 0xfe00707fu) == 0x9200603bu)
        return RISCV_PSAS_WX;
    if ((data & 0xff00f0ffu) == 0x6100e01bu)
        return RISCV_PSATI_DH;
    if ((data & 0xfe00f0ffu) == 0x6200e01bu)
        return RISCV_PSATI_DW;
    if ((data & 0xff00707fu) == 0xe100401bu)
        return RISCV_PSATI_H;
    if ((data & 0xfe00707fu) == 0xe200401bu)
        return RISCV_PSATI_W;
    if ((data & 0xfff0f0ffu) == 0x6040601bu)
        return RISCV_PSEXT_DH_B;
    if ((data & 0xfff0f0ffu) == 0x6240601bu)
        return RISCV_PSEXT_DW_B;
    if ((data & 0xfff0f0ffu) == 0x6250601bu)
        return RISCV_PSEXT_DW_H;
    if ((data & 0xfff0707fu) == 0xe040201bu)
        return RISCV_PSEXT_H_B;
    if ((data & 0xfff0707fu) == 0xe240201bu)
        return RISCV_PSEXT_W_B;
    if ((data & 0xfff0707fu) == 0xe250201bu)
        return RISCV_PSEXT_W_H;
    if ((data & 0xfe10f0ffu) == 0xa010601bu)
        return RISCV_PSH1ADD_DH;
    if ((data & 0xfe10f0ffu) == 0xa210601bu)
        return RISCV_PSH1ADD_DW;
    if ((data & 0xfe00707fu) == 0xa000203bu)
        return RISCV_PSH1ADD_H;
    if ((data & 0xfe00707fu) == 0xa200203bu)
        return RISCV_PSH1ADD_W;
    if ((data & 0xfe00707fu) == 0x8c00201bu)
        return RISCV_PSLL_BS;
    if ((data & 0xfe00f0ffu) == 0xc00601bu)
        return RISCV_PSLL_DBS;
    if ((data & 0xfe00f0ffu) == 0x800601bu)
        return RISCV_PSLL_DHS;
    if ((data & 0xfe00f0ffu) == 0xa00601bu)
        return RISCV_PSLL_DWS;
    if ((data & 0xfe00707fu) == 0x8800201bu)
        return RISCV_PSLL_HS;
    if ((data & 0xfe00707fu) == 0x8a00201bu)
        return RISCV_PSLL_WS;
    if ((data & 0xff80707fu) == 0x8080201bu)
        return RISCV_PSLLI_B;
    if ((data & 0xff80f0ffu) == 0x80601bu)
        return RISCV_PSLLI_DB;
    if ((data & 0xff00f0ffu) == 0x100601bu)
        return RISCV_PSLLI_DH;
    if ((data & 0xfe00f0ffu) == 0x200601bu)
        return RISCV_PSLLI_DW;
    if ((data & 0xff00707fu) == 0x8100201bu)
        return RISCV_PSLLI_H;
    if ((data & 0xfe00707fu) == 0x8200201bu)
        return RISCV_PSLLI_W;
    if ((data & 0xfe00707fu) == 0xcc00401bu)
        return RISCV_PSRA_BS;
    if ((data & 0xfe00f0ffu) == 0x4c00e01bu)
        return RISCV_PSRA_DBS;
    if ((data & 0xfe00f0ffu) == 0x4800e01bu)
        return RISCV_PSRA_DHS;
    if ((data & 0xfe00f0ffu) == 0x4a00e01bu)
        return RISCV_PSRA_DWS;
    if ((data & 0xfe00707fu) == 0xc800401bu)
        return RISCV_PSRA_HS;
    if ((data & 0xfe00707fu) == 0xca00401bu)
        return RISCV_PSRA_WS;
    if ((data & 0xff80707fu) == 0xc080401bu)
        return RISCV_PSRAI_B;
    if ((data & 0xff80f0ffu) == 0x4080e01bu)
        return RISCV_PSRAI_DB;
    if ((data & 0xff00f0ffu) == 0x4100e01bu)
        return RISCV_PSRAI_DH;
    if ((data & 0xfe00f0ffu) == 0x4200e01bu)
        return RISCV_PSRAI_DW;
    if ((data & 0xff00707fu) == 0xc100401bu)
        return RISCV_PSRAI_H;
    if ((data & 0xfe00707fu) == 0xc200401bu)
        return RISCV_PSRAI_W;
    if ((data & 0xff00f0ffu) == 0x5100e01bu)
        return RISCV_PSRARI_DH;
    if ((data & 0xfe00f0ffu) == 0x5200e01bu)
        return RISCV_PSRARI_DW;
    if ((data & 0xff00707fu) == 0xd100401bu)
        return RISCV_PSRARI_H;
    if ((data & 0xfe00707fu) == 0xd200401bu)
        return RISCV_PSRARI_W;
    if ((data & 0xfe00707fu) == 0x8c00401bu)
        return RISCV_PSRL_BS;
    if ((data & 0xfe00f0ffu) == 0xc00e01bu)
        return RISCV_PSRL_DBS;
    if ((data & 0xfe00f0ffu) == 0x800e01bu)
        return RISCV_PSRL_DHS;
    if ((data & 0xfe00f0ffu) == 0xa00e01bu)
        return RISCV_PSRL_DWS;
    if ((data & 0xfe00707fu) == 0x8800401bu)
        return RISCV_PSRL_HS;
    if ((data & 0xfe00707fu) == 0x8a00401bu)
        return RISCV_PSRL_WS;
    if ((data & 0xff80707fu) == 0x8080401bu)
        return RISCV_PSRLI_B;
    if ((data & 0xff80f0ffu) == 0x80e01bu)
        return RISCV_PSRLI_DB;
    if ((data & 0xff00f0ffu) == 0x100e01bu)
        return RISCV_PSRLI_DH;
    if ((data & 0xfe00f0ffu) == 0x200e01bu)
        return RISCV_PSRLI_DW;
    if ((data & 0xff00707fu) == 0x8100401bu)
        return RISCV_PSRLI_H;
    if ((data & 0xfe00707fu) == 0x8200401bu)
        return RISCV_PSRLI_W;
    if ((data & 0xfe10f0ffu) == 0x9410e01bu)
        return RISCV_PSSA_DHX;
    if ((data & 0xfe00707fu) == 0x9400603bu)
        return RISCV_PSSA_HX;
    if ((data & 0xfe00707fu) == 0x9600603bu)
        return RISCV_PSSA_WX;
    if ((data & 0xfe10f0ffu) == 0xb010601bu)
        return RISCV_PSSH1SADD_DH;
    if ((data & 0xfe10f0ffu) == 0xb210601bu)
        return RISCV_PSSH1SADD_DW;
    if ((data & 0xfe00707fu) == 0xb000203bu)
        return RISCV_PSSH1SADD_H;
    if ((data & 0xfe00707fu) == 0xb200203bu)
        return RISCV_PSSH1SADD_W;
    if ((data & 0xfe00f0ffu) == 0x6800601bu)
        return RISCV_PSSHA_DHS;
    if ((data & 0xfe00f0ffu) == 0x6a00601bu)
        return RISCV_PSSHA_DWS;
    if ((data & 0xfe00707fu) == 0xe800201bu)
        return RISCV_PSSHA_HS;
    if ((data & 0xfe00707fu) == 0xea00201bu)
        return RISCV_PSSHA_WS;
    if ((data & 0xfe00f0ffu) == 0x7800601bu)
        return RISCV_PSSHAR_DHS;
    if ((data & 0xfe00f0ffu) == 0x7a00601bu)
        return RISCV_PSSHAR_DWS;
    if ((data & 0xfe00707fu) == 0xf800201bu)
        return RISCV_PSSHAR_HS;
    if ((data & 0xfe00707fu) == 0xfa00201bu)
        return RISCV_PSSHAR_WS;
    if ((data & 0xfe00f0ffu) == 0x2800601bu)
        return RISCV_PSSHL_DHS;
    if ((data & 0xfe00f0ffu) == 0x2a00601bu)
        return RISCV_PSSHL_DWS;
    if ((data & 0xfe00707fu) == 0xa800201bu)
        return RISCV_PSSHL_HS;
    if ((data & 0xfe00707fu) == 0xaa00201bu)
        return RISCV_PSSHL_WS;
    if ((data & 0xfe00f0ffu) == 0x3800601bu)
        return RISCV_PSSHLR_DHS;
    if ((data & 0xfe00f0ffu) == 0x3a00601bu)
        return RISCV_PSSHLR_DWS;
    if ((data & 0xfe00707fu) == 0xb800201bu)
        return RISCV_PSSHLR_HS;
    if ((data & 0xfe00707fu) == 0xba00201bu)
        return RISCV_PSSHLR_WS;
    if ((data & 0xff00f0ffu) == 0x5100601bu)
        return RISCV_PSSLAI_DH;
    if ((data & 0xfe00f0ffu) == 0x5200601bu)
        return RISCV_PSSLAI_DW;
    if ((data & 0xff00707fu) == 0xd100201bu)
        return RISCV_PSSLAI_H;
    if ((data & 0xfe00707fu) == 0xd200201bu)
        return RISCV_PSSLAI_W;
    if ((data & 0xfe00707fu) == 0xd400003bu)
        return RISCV_PSSUB_B;
    if ((data & 0xfe10f0ffu) == 0xd400601bu)
        return RISCV_PSSUB_DB;
    if ((data & 0xfe10f0ffu) == 0xd000601bu)
        return RISCV_PSSUB_DH;
    if ((data & 0xfe10f0ffu) == 0xd200601bu)
        return RISCV_PSSUB_DW;
    if ((data & 0xfe00707fu) == 0xd000003bu)
        return RISCV_PSSUB_H;
    if ((data & 0xfe00707fu) == 0xd200003bu)
        return RISCV_PSSUB_W;
    if ((data & 0xfe00707fu) == 0xf400003bu)
        return RISCV_PSSUBU_B;
    if ((data & 0xfe10f0ffu) == 0xf400601bu)
        return RISCV_PSSUBU_DB;
    if ((data & 0xfe10f0ffu) == 0xf000601bu)
        return RISCV_PSSUBU_DH;
    if ((data & 0xfe10f0ffu) == 0xf200601bu)
        return RISCV_PSSUBU_DW;
    if ((data & 0xfe00707fu) == 0xf000003bu)
        return RISCV_PSSUBU_H;
    if ((data & 0xfe00707fu) == 0xf200003bu)
        return RISCV_PSSUBU_W;
    if ((data & 0xfe00707fu) == 0xc400003bu)
        return RISCV_PSUB_B;
    if ((data & 0xfe10f0ffu) == 0xc400601bu)
        return RISCV_PSUB_DB;
    if ((data & 0xfe10f0ffu) == 0xc000601bu)
        return RISCV_PSUB_DH;
    if ((data & 0xfe10f0ffu) == 0xc200601bu)
        return RISCV_PSUB_DW;
    if ((data & 0xfe00707fu) == 0xc000003bu)
        return RISCV_PSUB_H;
    if ((data & 0xfe00707fu) == 0xc200003bu)
        return RISCV_PSUB_W;
    if ((data & 0xff00f0ffu) == 0x2100e01bu)
        return RISCV_PUSATI_DH;
    if ((data & 0xfe00f0ffu) == 0x2200e01bu)
        return RISCV_PUSATI_DW;
    if ((data & 0xff00707fu) == 0xa100401bu)
        return RISCV_PUSATI_H;
    if ((data & 0xfe00707fu) == 0xa200401bu)
        return RISCV_PUSATI_W;
    if ((data & 0xfe0070ffu) == 0x400209bu)
        return RISCV_PWADD_B;
    if ((data & 0xfe0070ffu) == 0x209bu)
        return RISCV_PWADD_H;
    if ((data & 0xfe0070ffu) == 0xc00209bu)
        return RISCV_PWADDA_B;
    if ((data & 0xfe0070ffu) == 0x800209bu)
        return RISCV_PWADDA_H;
    if ((data & 0xfe0070ffu) == 0x1c00209bu)
        return RISCV_PWADDAU_B;
    if ((data & 0xfe0070ffu) == 0x1800209bu)
        return RISCV_PWADDAU_H;
    if ((data & 0xfe0070ffu) == 0x1400209bu)
        return RISCV_PWADDU_B;
    if ((data & 0xfe0070ffu) == 0x1000209bu)
        return RISCV_PWADDU_H;
    if ((data & 0xfe0070ffu) == 0x2800209bu)
        return RISCV_PWMACC_H;
    if ((data & 0xfe0070ffu) == 0x6800209bu)
        return RISCV_PWMACCSU_H;
    if ((data & 0xfe0070ffu) == 0x3800209bu)
        return RISCV_PWMACCU_H;
    if ((data & 0xfe0070ffu) == 0x2400209bu)
        return RISCV_PWMUL_B;
    if ((data & 0xfe0070ffu) == 0x2000209bu)
        return RISCV_PWMUL_H;
    if ((data & 0xfe0070ffu) == 0x6400209bu)
        return RISCV_PWMULSU_B;
    if ((data & 0xfe0070ffu) == 0x6000209bu)
        return RISCV_PWMULSU_H;
    if ((data & 0xfe0070ffu) == 0x3400209bu)
        return RISCV_PWMULU_B;
    if ((data & 0xfe0070ffu) == 0x3000209bu)
        return RISCV_PWMULU_H;
    if ((data & 0xfe0070ffu) == 0x4800201bu)
        return RISCV_PWSLA_BS;
    if ((data & 0xfe0070ffu) == 0x4a00201bu)
        return RISCV_PWSLA_HS;
    if ((data & 0xff0070ffu) == 0x4100201bu)
        return RISCV_PWSLAI_B;
    if ((data & 0xfe0070ffu) == 0x4200201bu)
        return RISCV_PWSLAI_H;
    if ((data & 0xfe0070ffu) == 0x800201bu)
        return RISCV_PWSLL_BS;
    if ((data & 0xfe0070ffu) == 0xa00201bu)
        return RISCV_PWSLL_HS;
    if ((data & 0xff0070ffu) == 0x100201bu)
        return RISCV_PWSLLI_B;
    if ((data & 0xfe0070ffu) == 0x200201bu)
        return RISCV_PWSLLI_H;
    if ((data & 0xfe0070ffu) == 0x4400209bu)
        return RISCV_PWSUB_B;
    if ((data & 0xfe0070ffu) == 0x4000209bu)
        return RISCV_PWSUB_H;
    if ((data & 0xfe0070ffu) == 0x4c00209bu)
        return RISCV_PWSUBA_B;
    if ((data & 0xfe0070ffu) == 0x4800209bu)
        return RISCV_PWSUBA_H;
    if ((data & 0xfe0070ffu) == 0x5c00209bu)
        return RISCV_PWSUBAU_B;
    if ((data & 0xfe0070ffu) == 0x5800209bu)
        return RISCV_PWSUBAU_H;
    if ((data & 0xfe0070ffu) == 0x5400209bu)
        return RISCV_PWSUBU_B;
    if ((data & 0xfe0070ffu) == 0x5000209bu)
        return RISCV_PWSUBU_H;
    if ((data & 0xfe00707fu) == 0x2006033u)
        return RISCV_REM;
    if ((data & 0xfe00707fu) == 0x2007033u)
        return RISCV_REMU;
    if ((data & 0xfe00707fu) == 0x200703bu)
        return RISCV_REMUW;
    if ((data & 0xfe00707fu) == 0x200603bu)
        return RISCV_REMW;
    if ((data & 0xfff0707fu) == 0x6bf05013u)
        return RISCV_REV;
    if ((data & 0xfff0707fu) == 0x69f05013u)
        return RISCV_REV_RV32;
    if ((data & 0xfff0707fu) == 0x6b005013u)
        return RISCV_REV16;
    if ((data & 0xfff0707fu) == 0x6b805013u)
        return RISCV_REV8;
    if ((data & 0xfff0707fu) == 0x69805013u)
        return RISCV_REV8_RV32;
    if ((data & 0xfe00707fu) == 0x60001033u)
        return RISCV_ROL;
    if ((data & 0xfe00707fu) == 0x6000103bu)
        return RISCV_ROLW;
    if ((data & 0xfe00707fu) == 0x60005033u)
        return RISCV_ROR;
    if ((data & 0xfc00707fu) == 0x60005013u)
        return RISCV_RORI;
    if ((data & 0xfe00707fu) == 0x6000501bu)
        return RISCV_RORIW;
    if ((data & 0xfe00707fu) == 0x6000503bu)
        return RISCV_RORW;
    if ((data & 0xfc00707fu) == 0xe400401bu)
        return RISCV_SATI;
    if ((data & 0x707fu) == 0x23u)
        return RISCV_SB;
    if ((data & 0xfa007fffu) == 0x3a00002fu)
        return RISCV_SB_RL;
    if ((data & 0xf800707fu) == 0x1800002fu)
        return RISCV_SC_B;
    if ((data & 0xf800707fu) == 0x1800302fu)
        return RISCV_SC_D;
    if ((data & 0xf800707fu) == 0x1800102fu)
        return RISCV_SC_H;
    if ((data & 0xf800707fu) == 0x1800202fu)
        return RISCV_SC_W;
    if ((data & 0xffffffffu) == 0x10400073u)
        return RISCV_SCTRCLR;
    if ((data & 0x707fu) == 0x3023u)
        return RISCV_SD;
    if ((data & 0xfa007fffu) == 0x3a00302fu)
        return RISCV_SD_RL;
    if ((data & 0xfff0707fu) == 0x60401013u)
        return RISCV_SEXT_B;
    if ((data & 0xfff0707fu) == 0x60501013u)
        return RISCV_SEXT_H;
    if ((data & 0xffffffffu) == 0x18100073u)
        return RISCV_SFENCE_INVAL_IR;
    if ((data & 0xfe007fffu) == 0x12000073u)
        return RISCV_SFENCE_VMA;
    if ((data & 0xffffffffu) == 0x18000073u)
        return RISCV_SFENCE_W_INVAL;
    if ((data & 0x707fu) == 0x1023u)
        return RISCV_SH;
    if ((data & 0xfa007fffu) == 0x3a00102fu)
        return RISCV_SH_RL;
    if ((data & 0xfe00707fu) == 0x20002033u)
        return RISCV_SH1ADD;
    if ((data & 0xfe00707fu) == 0x2000203bu)
        return RISCV_SH1ADD_UW;
    if ((data & 0xfe00707fu) == 0x20004033u)
        return RISCV_SH2ADD;
    if ((data & 0xfe00707fu) == 0x2000403bu)
        return RISCV_SH2ADD_UW;
    if ((data & 0xfe00707fu) == 0x20006033u)
        return RISCV_SH3ADD;
    if ((data & 0xfe00707fu) == 0x2000603bu)
        return RISCV_SH3ADD_UW;
    if ((data & 0xfe00707fu) == 0xee00201bu)
        return RISCV_SHA;
    if ((data & 0xfff0707fu) == 0x10201013u)
        return RISCV_SHA256SIG0;
    if ((data & 0xfff0707fu) == 0x10301013u)
        return RISCV_SHA256SIG1;
    if ((data & 0xfff0707fu) == 0x10001013u)
        return RISCV_SHA256SUM0;
    if ((data & 0xfff0707fu) == 0x10101013u)
        return RISCV_SHA256SUM1;
    if ((data & 0xfff0707fu) == 0x10601013u)
        return RISCV_SHA512SIG0;
    if ((data & 0xfe00707fu) == 0x5c000033u)
        return RISCV_SHA512SIG0H;
    if ((data & 0xfe00707fu) == 0x54000033u)
        return RISCV_SHA512SIG0L;
    if ((data & 0xfff0707fu) == 0x10701013u)
        return RISCV_SHA512SIG1;
    if ((data & 0xfe00707fu) == 0x5e000033u)
        return RISCV_SHA512SIG1H;
    if ((data & 0xfe00707fu) == 0x56000033u)
        return RISCV_SHA512SIG1L;
    if ((data & 0xfff0707fu) == 0x10401013u)
        return RISCV_SHA512SUM0;
    if ((data & 0xfe00707fu) == 0x50000033u)
        return RISCV_SHA512SUM0R;
    if ((data & 0xfff0707fu) == 0x10501013u)
        return RISCV_SHA512SUM1;
    if ((data & 0xfe00707fu) == 0x52000033u)
        return RISCV_SHA512SUM1R;
    if ((data & 0xfe00707fu) == 0xfe00201bu)
        return RISCV_SHAR;
    if ((data & 0xfe00707fu) == 0xae00201bu)
        return RISCV_SHL;
    if ((data & 0xfe00707fu) == 0xbe00201bu)
        return RISCV_SHLR;
    if ((data & 0xfe007fffu) == 0x16000073u)
        return RISCV_SINVAL_VMA;
    if ((data & 0xfe00707fu) == 0x1033u)
        return RISCV_SLL;
    if ((data & 0xfc00707fu) == 0x1013u)
        return RISCV_SLLI;
    if ((data & 0xfc00707fu) == 0x800101bu)
        return RISCV_SLLI_UW;
    if ((data & 0xfe00707fu) == 0x101bu)
        return RISCV_SLLIW;
    if ((data & 0xfe00707fu) == 0x103bu)
        return RISCV_SLLW;
    if ((data & 0xfe00707fu) == 0x2033u)
        return RISCV_SLT;
    if ((data & 0x707fu) == 0x2013u)
        return RISCV_SLTI;
    if ((data & 0x707fu) == 0x3013u)
        return RISCV_SLTIU;
    if ((data & 0xfe00707fu) == 0x3033u)
        return RISCV_SLTU;
    if ((data & 0xfe00707fu) == 0x8e00103bu)
        return RISCV_SLX;
    if ((data & 0xfff0707fu) == 0x10801013u)
        return RISCV_SM3P0;
    if ((data & 0xfff0707fu) == 0x10901013u)
        return RISCV_SM3P1;
    if ((data & 0x3e00707fu) == 0x30000033u)
        return RISCV_SM4ED;
    if ((data & 0x3e00707fu) == 0x34000033u)
        return RISCV_SM4KS;
    if ((data & 0xfe00707fu) == 0x40005033u)
        return RISCV_SRA;
    if ((data & 0xfc00707fu) == 0x40005013u)
        return RISCV_SRAI;
    if ((data & 0xfe00707fu) == 0x4000501bu)
        return RISCV_SRAIW;
    if ((data & 0xfc00707fu) == 0xd400401bu)
        return RISCV_SRARI;
    if ((data & 0xfe00707fu) == 0x4000503bu)
        return RISCV_SRAW;
    if ((data & 0xffffffffu) == 0x10200073u)
        return RISCV_SRET;
    if ((data & 0xfe00707fu) == 0x5033u)
        return RISCV_SRL;
    if ((data & 0xfc00707fu) == 0x5013u)
        return RISCV_SRLI;
    if ((data & 0xfe00707fu) == 0x501bu)
        return RISCV_SRLIW;
    if ((data & 0xfe00707fu) == 0x503bu)
        return RISCV_SRLW;
    if ((data & 0xfe00707fu) == 0xae00103bu)
        return RISCV_SRX;
    if ((data & 0xf800707fu) == 0x4800302fu)
        return RISCV_SSAMOSWAP_D;
    if ((data & 0xf800707fu) == 0x4800202fu)
        return RISCV_SSAMOSWAP_W;
    if ((data & 0xfe00707fu) == 0x40000033u)
        return RISCV_SUB;
    if ((data & 0xfe10f0ffu) == 0xc600601bu)
        return RISCV_SUBD;
    if ((data & 0xfe00707fu) == 0x4000003bu)
        return RISCV_SUBW;
    if ((data & 0x707fu) == 0x2023u)
        return RISCV_SW;
    if ((data & 0xfa007fffu) == 0x3a00202fu)
        return RISCV_SW_RL;
    if ((data & 0xfff0707fu) == 0x8f05013u)
        return RISCV_UNZIP;
    if ((data & 0xfe00707fu) == 0xe600203bu)
        return RISCV_UNZIP16HP;
    if ((data & 0xfe00707fu) == 0xe200203bu)
        return RISCV_UNZIP16P;
    if ((data & 0xfe00707fu) == 0xe400203bu)
        return RISCV_UNZIP8HP;
    if ((data & 0xfe00707fu) == 0xe000203bu)
        return RISCV_UNZIP8P;
    if ((data & 0xfc00707fu) == 0xa400401bu)
        return RISCV_USATI;
    if ((data & 0xfc00707fu) == 0x24002057u)
        return RISCV_VAADD_VV;
    if ((data & 0xfc00707fu) == 0x24006057u)
        return RISCV_VAADD_VX;
    if ((data & 0xfc00707fu) == 0x20002057u)
        return RISCV_VAADDU_VV;
    if ((data & 0xfc00707fu) == 0x20006057u)
        return RISCV_VAADDU_VX;
    if ((data & 0xfc00707fu) == 0x44002057u)
        return RISCV_VABD_VV;
    if ((data & 0xfc00707fu) == 0x4c002057u)
        return RISCV_VABDU_VV;
    if ((data & 0xfc0ff07fu) == 0x48082057u)
        return RISCV_VABS_V;
    if ((data & 0xfe00707fu) == 0x40003057u)
        return RISCV_VADC_VIM;
    if ((data & 0xfe00707fu) == 0x40000057u)
        return RISCV_VADC_VVM;
    if ((data & 0xfe00707fu) == 0x40004057u)
        return RISCV_VADC_VXM;
    if ((data & 0xfc00707fu) == 0x3057u)
        return RISCV_VADD_VI;
    if ((data & 0xfc00707fu) == 0x57u)
        return RISCV_VADD_VV;
    if ((data & 0xfc00707fu) == 0x4057u)
        return RISCV_VADD_VX;
    if ((data & 0xfe0ff07fu) == 0xa600a077u)
        return RISCV_VAESDF_VS;
    if ((data & 0xfe0ff07fu) == 0xa200a077u)
        return RISCV_VAESDF_VV;
    if ((data & 0xfe0ff07fu) == 0xa6002077u)
        return RISCV_VAESDM_VS;
    if ((data & 0xfe0ff07fu) == 0xa2002077u)
        return RISCV_VAESDM_VV;
    if ((data & 0xfe0ff07fu) == 0xa601a077u)
        return RISCV_VAESEF_VS;
    if ((data & 0xfe0ff07fu) == 0xa201a077u)
        return RISCV_VAESEF_VV;
    if ((data & 0xfe0ff07fu) == 0xa6012077u)
        return RISCV_VAESEM_VS;
    if ((data & 0xfe0ff07fu) == 0xa2012077u)
        return RISCV_VAESEM_VV;
    if ((data & 0xfe00707fu) == 0x8a002077u)
        return RISCV_VAESKF1_VI;
    if ((data & 0xfe00707fu) == 0xaa002077u)
        return RISCV_VAESKF2_VI;
    if ((data & 0xfe0ff07fu) == 0xa603a077u)
        return RISCV_VAESZ_VS;
    if ((data & 0xfc00707fu) == 0x24003057u)
        return RISCV_VAND_VI;
    if ((data & 0xfc00707fu) == 0x24000057u)
        return RISCV_VAND_VV;
    if ((data & 0xfc00707fu) == 0x24004057u)
        return RISCV_VAND_VX;
    if ((data & 0xfc00707fu) == 0x4000057u)
        return RISCV_VANDN_VV;
    if ((data & 0xfc00707fu) == 0x4004057u)
        return RISCV_VANDN_VX;
    if ((data & 0xfc00707fu) == 0x2c002057u)
        return RISCV_VASUB_VV;
    if ((data & 0xfc00707fu) == 0x2c006057u)
        return RISCV_VASUB_VX;
    if ((data & 0xfc00707fu) == 0x28002057u)
        return RISCV_VASUBU_VV;
    if ((data & 0xfc00707fu) == 0x28006057u)
        return RISCV_VASUBU_VX;
    if ((data & 0xfc0ff07fu) == 0x48052057u)
        return RISCV_VBREV_V;
    if ((data & 0xfc0ff07fu) == 0x48042057u)
        return RISCV_VBREV8_V;
    if ((data & 0xfc00707fu) == 0x30002057u)
        return RISCV_VCLMUL_VV;
    if ((data & 0xfc00707fu) == 0x30006057u)
        return RISCV_VCLMUL_VX;
    if ((data & 0xfc00707fu) == 0x34002057u)
        return RISCV_VCLMULH_VV;
    if ((data & 0xfc00707fu) == 0x34006057u)
        return RISCV_VCLMULH_VX;
    if ((data & 0xfc0ff07fu) == 0x48062057u)
        return RISCV_VCLZ_V;
    if ((data & 0xfe00707fu) == 0x5e002057u)
        return RISCV_VCOMPRESS_VM;
    if ((data & 0xfc0ff07fu) == 0x40082057u)
        return RISCV_VCPOP_M;
    if ((data & 0xfc0ff07fu) == 0x48072057u)
        return RISCV_VCPOP_V;
    if ((data & 0xfc0ff07fu) == 0x4806a057u)
        return RISCV_VCTZ_V;
    if ((data & 0xfc00707fu) == 0x84002057u)
        return RISCV_VDIV_VV;
    if ((data & 0xfc00707fu) == 0x84006057u)
        return RISCV_VDIV_VX;
    if ((data & 0xfc00707fu) == 0x80002057u)
        return RISCV_VDIVU_VV;
    if ((data & 0xfc00707fu) == 0x80006057u)
        return RISCV_VDIVU_VX;
    if ((data & 0xfc00707fu) == 0xb0002057u)
        return RISCV_VDOT4A_VV;
    if ((data & 0xfc00707fu) == 0xb0006057u)
        return RISCV_VDOT4A_VX;
    if ((data & 0xfc00707fu) == 0xa8002057u)
        return RISCV_VDOT4ASU_VV;
    if ((data & 0xfc00707fu) == 0xa8006057u)
        return RISCV_VDOT4ASU_VX;
    if ((data & 0xfc00707fu) == 0xa0002057u)
        return RISCV_VDOT4AU_VV;
    if ((data & 0xfc00707fu) == 0xa0006057u)
        return RISCV_VDOT4AU_VX;
    if ((data & 0xfc00707fu) == 0xb8006057u)
        return RISCV_VDOT4AUS_VX;
    if ((data & 0xfc00707fu) == 0x5057u)
        return RISCV_VFADD_VF;
    if ((data & 0xfc00707fu) == 0x1057u)
        return RISCV_VFADD_VV;
    if ((data & 0xfc00707fu) == 0xac001077u)
        return RISCV_VFBDOTA_VV;
    if ((data & 0xfc0ff07fu) == 0x4c081057u)
        return RISCV_VFCLASS_V;
    if ((data & 0xfc0ff07fu) == 0x48019057u)
        return RISCV_VFCVT_F_X_V;
    if ((data & 0xfc0ff07fu) == 0x48011057u)
        return RISCV_VFCVT_F_XU_V;
    if ((data & 0xfc0ff07fu) == 0x48039057u)
        return RISCV_VFCVT_RTZ_X_F_V;
    if ((data & 0xfc0ff07fu) == 0x48031057u)
        return RISCV_VFCVT_RTZ_XU_F_V;
    if ((data & 0xfc0ff07fu) == 0x48009057u)
        return RISCV_VFCVT_X_F_V;
    if ((data & 0xfc0ff07fu) == 0x48001057u)
        return RISCV_VFCVT_XU_F_V;
    if ((data & 0xfc00707fu) == 0x80005057u)
        return RISCV_VFDIV_VF;
    if ((data & 0xfc00707fu) == 0x80001057u)
        return RISCV_VFDIV_VV;
    if ((data & 0xfc0ff07fu) == 0x480b2057u)
        return RISCV_VFEXT_VF2;
    if ((data & 0xfc0ff07fu) == 0x4008a057u)
        return RISCV_VFIRST_M;
    if ((data & 0xfc00707fu) == 0xb0005057u)
        return RISCV_VFMACC_VF;
    if ((data & 0xfc00707fu) == 0xb0001057u)
        return RISCV_VFMACC_VV;
    if ((data & 0xfc00707fu) == 0xa0005057u)
        return RISCV_VFMADD_VF;
    if ((data & 0xfc00707fu) == 0xa0001057u)
        return RISCV_VFMADD_VV;
    if ((data & 0xfc00707fu) == 0x18005057u)
        return RISCV_VFMAX_VF;
    if ((data & 0xfc00707fu) == 0x18001057u)
        return RISCV_VFMAX_VV;
    if ((data & 0xfe00707fu) == 0x5c005057u)
        return RISCV_VFMERGE_VFM;
    if ((data & 0xfc00707fu) == 0x10005057u)
        return RISCV_VFMIN_VF;
    if ((data & 0xfc00707fu) == 0x10001057u)
        return RISCV_VFMIN_VV;
    if ((data & 0xfc00707fu) == 0xb8005057u)
        return RISCV_VFMSAC_VF;
    if ((data & 0xfc00707fu) == 0xb8001057u)
        return RISCV_VFMSAC_VV;
    if ((data & 0xfc00707fu) == 0xa8005057u)
        return RISCV_VFMSUB_VF;
    if ((data & 0xfc00707fu) == 0xa8001057u)
        return RISCV_VFMSUB_VV;
    if ((data & 0xfc00707fu) == 0x90005057u)
        return RISCV_VFMUL_VF;
    if ((data & 0xfc00707fu) == 0x90001057u)
        return RISCV_VFMUL_VV;
    if ((data & 0xfe0ff07fu) == 0x42001057u)
        return RISCV_VFMV_F_S;
    if ((data & 0xfff0707fu) == 0x42005057u)
        return RISCV_VFMV_S_F;
    if ((data & 0xfff0707fu) == 0x5e005057u)
        return RISCV_VFMV_V_F;
    if ((data & 0xfc0ff07fu) == 0x480c9057u)
        return RISCV_VFNCVT_F_F_Q;
    if ((data & 0xfc0ff07fu) == 0x480a1057u)
        return RISCV_VFNCVT_F_F_W;
    if ((data & 0xfc0ff07fu) == 0x48099057u)
        return RISCV_VFNCVT_F_X_W;
    if ((data & 0xfc0ff07fu) == 0x48091057u)
        return RISCV_VFNCVT_F_XU_W;
    if ((data & 0xfc0ff07fu) == 0x480a9057u)
        return RISCV_VFNCVT_ROD_F_F_W;
    if ((data & 0xfc0ff07fu) == 0x480b9057u)
        return RISCV_VFNCVT_RTZ_X_F_W;
    if ((data & 0xfc0ff07fu) == 0x480b1057u)
        return RISCV_VFNCVT_RTZ_XU_F_W;
    if ((data & 0xfc0ff07fu) == 0x480d9057u)
        return RISCV_VFNCVT_SAT_F_F_Q;
    if ((data & 0xfc0ff07fu) == 0x48089057u)
        return RISCV_VFNCVT_X_F_W;
    if ((data & 0xfc0ff07fu) == 0x48081057u)
        return RISCV_VFNCVT_XU_F_W;
    if ((data & 0xfc0ff07fu) == 0x480e9057u)
        return RISCV_VFNCVTBF16_F_F_W;
    if ((data & 0xfc0ff07fu) == 0x480f9057u)
        return RISCV_VFNCVTBF16_SAT_F_F_W;
    if ((data & 0xfc00707fu) == 0xb4005057u)
        return RISCV_VFNMACC_VF;
    if ((data & 0xfc00707fu) == 0xb4001057u)
        return RISCV_VFNMACC_VV;
    if ((data & 0xfc00707fu) == 0xa4005057u)
        return RISCV_VFNMADD_VF;
    if ((data & 0xfc00707fu) == 0xa4001057u)
        return RISCV_VFNMADD_VV;
    if ((data & 0xfc00707fu) == 0xbc005057u)
        return RISCV_VFNMSAC_VF;
    if ((data & 0xfc00707fu) == 0xbc001057u)
        return RISCV_VFNMSAC_VV;
    if ((data & 0xfc00707fu) == 0xac005057u)
        return RISCV_VFNMSUB_VF;
    if ((data & 0xfc00707fu) == 0xac001057u)
        return RISCV_VFNMSUB_VV;
    if ((data & 0xfc00707fu) == 0xbc001077u)
        return RISCV_VFQWBDOTA_ALT_VV;
    if ((data & 0xfc00707fu) == 0xb8001077u)
        return RISCV_VFQWBDOTA_VV;
    if ((data & 0xfc00707fu) == 0x9c001077u)
        return RISCV_VFQWDOTA_ALT_VV;
    if ((data & 0xfc00707fu) == 0x98001077u)
        return RISCV_VFQWDOTA_VV;
    if ((data & 0xfc00707fu) == 0x84005057u)
        return RISCV_VFRDIV_VF;
    if ((data & 0xfc0ff07fu) == 0x4c029057u)
        return RISCV_VFREC7_V;
    if ((data & 0xfc00707fu) == 0x1c001057u)
        return RISCV_VFREDMAX_VS;
    if ((data & 0xfc00707fu) == 0x14001057u)
        return RISCV_VFREDMIN_VS;
    if ((data & 0xfc00707fu) == 0xc001057u)
        return RISCV_VFREDOSUM_VS;
    if ((data & 0xfc00707fu) == 0x4001057u)
        return RISCV_VFREDUSUM_VS;
    if ((data & 0xfc0ff07fu) == 0x4c021057u)
        return RISCV_VFRSQRT7_V;
    if ((data & 0xfc00707fu) == 0x9c005057u)
        return RISCV_VFRSUB_VF;
    if ((data & 0xfc00707fu) == 0x20005057u)
        return RISCV_VFSGNJ_VF;
    if ((data & 0xfc00707fu) == 0x20001057u)
        return RISCV_VFSGNJ_VV;
    if ((data & 0xfc00707fu) == 0x24005057u)
        return RISCV_VFSGNJN_VF;
    if ((data & 0xfc00707fu) == 0x24001057u)
        return RISCV_VFSGNJN_VV;
    if ((data & 0xfc00707fu) == 0x28005057u)
        return RISCV_VFSGNJX_VF;
    if ((data & 0xfc00707fu) == 0x28001057u)
        return RISCV_VFSGNJX_VV;
    if ((data & 0xfc00707fu) == 0x3c005057u)
        return RISCV_VFSLIDE1DOWN_VF;
    if ((data & 0xfc00707fu) == 0x38005057u)
        return RISCV_VFSLIDE1UP_VF;
    if ((data & 0xfc0ff07fu) == 0x4c001057u)
        return RISCV_VFSQRT_V;
    if ((data & 0xfc00707fu) == 0x8005057u)
        return RISCV_VFSUB_VF;
    if ((data & 0xfc00707fu) == 0x8001057u)
        return RISCV_VFSUB_VV;
    if ((data & 0xfc00707fu) == 0xc0005057u)
        return RISCV_VFWADD_VF;
    if ((data & 0xfc00707fu) == 0xc0001057u)
        return RISCV_VFWADD_VV;
    if ((data & 0xfc00707fu) == 0xd0005057u)
        return RISCV_VFWADD_WF;
    if ((data & 0xfc00707fu) == 0xd0001057u)
        return RISCV_VFWADD_WV;
    if ((data & 0xfc00707fu) == 0xb0001077u)
        return RISCV_VFWBDOTA_VV;
    if ((data & 0xfc0ff07fu) == 0x48061057u)
        return RISCV_VFWCVT_F_F_V;
    if ((data & 0xfc0ff07fu) == 0x48059057u)
        return RISCV_VFWCVT_F_X_V;
    if ((data & 0xfc0ff07fu) == 0x48051057u)
        return RISCV_VFWCVT_F_XU_V;
    if ((data & 0xfc0ff07fu) == 0x48079057u)
        return RISCV_VFWCVT_RTZ_X_F_V;
    if ((data & 0xfc0ff07fu) == 0x48071057u)
        return RISCV_VFWCVT_RTZ_XU_F_V;
    if ((data & 0xfc0ff07fu) == 0x48049057u)
        return RISCV_VFWCVT_X_F_V;
    if ((data & 0xfc0ff07fu) == 0x48041057u)
        return RISCV_VFWCVT_XU_F_V;
    if ((data & 0xfc0ff07fu) == 0x48069057u)
        return RISCV_VFWCVTBF16_F_F_V;
    if ((data & 0xfc00707fu) == 0x90001077u)
        return RISCV_VFWDOTA_VV;
    if ((data & 0xfc00707fu) == 0xf0005057u)
        return RISCV_VFWMACC_VF;
    if ((data & 0xfc00707fu) == 0xf0001057u)
        return RISCV_VFWMACC_VV;
    if ((data & 0xfc00707fu) == 0xec005057u)
        return RISCV_VFWMACCBF16_VF;
    if ((data & 0xfc00707fu) == 0xec001057u)
        return RISCV_VFWMACCBF16_VV;
    if ((data & 0xfc00707fu) == 0xf8005057u)
        return RISCV_VFWMSAC_VF;
    if ((data & 0xfc00707fu) == 0xf8001057u)
        return RISCV_VFWMSAC_VV;
    if ((data & 0xfc00707fu) == 0xe0005057u)
        return RISCV_VFWMUL_VF;
    if ((data & 0xfc00707fu) == 0xe0001057u)
        return RISCV_VFWMUL_VV;
    if ((data & 0xfc00707fu) == 0xf4005057u)
        return RISCV_VFWNMACC_VF;
    if ((data & 0xfc00707fu) == 0xf4001057u)
        return RISCV_VFWNMACC_VV;
    if ((data & 0xfc00707fu) == 0xfc005057u)
        return RISCV_VFWNMSAC_VF;
    if ((data & 0xfc00707fu) == 0xfc001057u)
        return RISCV_VFWNMSAC_VV;
    if ((data & 0xfc00707fu) == 0xcc001057u)
        return RISCV_VFWREDOSUM_VS;
    if ((data & 0xfc00707fu) == 0xc4001057u)
        return RISCV_VFWREDUSUM_VS;
    if ((data & 0xfc00707fu) == 0xc8005057u)
        return RISCV_VFWSUB_VF;
    if ((data & 0xfc00707fu) == 0xc8001057u)
        return RISCV_VFWSUB_VV;
    if ((data & 0xfc00707fu) == 0xd8005057u)
        return RISCV_VFWSUB_WF;
    if ((data & 0xfc00707fu) == 0xd8001057u)
        return RISCV_VFWSUB_WV;
    if ((data & 0xfe00707fu) == 0xb2002077u)
        return RISCV_VGHSH_VV;
    if ((data & 0xfe0ff07fu) == 0xa208a077u)
        return RISCV_VGMUL_VV;
    if ((data & 0xfdfff07fu) == 0x5008a057u)
        return RISCV_VID_V;
    if ((data & 0xfc0ff07fu) == 0x50082057u)
        return RISCV_VIOTA_M;
    if ((data & 0xfff0707fu) == 0x2805007u)
        return RISCV_VL1RE16_V;
    if ((data & 0xfff0707fu) == 0x2806007u)
        return RISCV_VL1RE32_V;
    if ((data & 0xfff0707fu) == 0x2807007u)
        return RISCV_VL1RE64_V;
    if ((data & 0xfff0707fu) == 0x2800007u)
        return RISCV_VL1RE8_V;
    if ((data & 0xfff0707fu) == 0x22805007u)
        return RISCV_VL2RE16_V;
    if ((data & 0xfff0707fu) == 0x22806007u)
        return RISCV_VL2RE32_V;
    if ((data & 0xfff0707fu) == 0x22807007u)
        return RISCV_VL2RE64_V;
    if ((data & 0xfff0707fu) == 0x22800007u)
        return RISCV_VL2RE8_V;
    if ((data & 0xfff0707fu) == 0x62805007u)
        return RISCV_VL4RE16_V;
    if ((data & 0xfff0707fu) == 0x62806007u)
        return RISCV_VL4RE32_V;
    if ((data & 0xfff0707fu) == 0x62807007u)
        return RISCV_VL4RE64_V;
    if ((data & 0xfff0707fu) == 0x62800007u)
        return RISCV_VL4RE8_V;
    if ((data & 0xfff0707fu) == 0xe2805007u)
        return RISCV_VL8RE16_V;
    if ((data & 0xfff0707fu) == 0xe2806007u)
        return RISCV_VL8RE32_V;
    if ((data & 0xfff0707fu) == 0xe2807007u)
        return RISCV_VL8RE64_V;
    if ((data & 0xfff0707fu) == 0xe2800007u)
        return RISCV_VL8RE8_V;
    if ((data & 0x1df0707fu) == 0x5007u)
        return RISCV_VLE16_V;
    if ((data & 0x1df0707fu) == 0x1005007u)
        return RISCV_VLE16FF_V;
    if ((data & 0x1df0707fu) == 0x6007u)
        return RISCV_VLE32_V;
    if ((data & 0x1df0707fu) == 0x1006007u)
        return RISCV_VLE32FF_V;
    if ((data & 0x1df0707fu) == 0x7007u)
        return RISCV_VLE64_V;
    if ((data & 0x1df0707fu) == 0x1007007u)
        return RISCV_VLE64FF_V;
    if ((data & 0x1df0707fu) == 0x7u)
        return RISCV_VLE8_V;
    if ((data & 0x1df0707fu) == 0x1000007u)
        return RISCV_VLE8FF_V;
    if ((data & 0xfff0707fu) == 0x2b00007u)
        return RISCV_VLM_V;
    if ((data & 0x1c00707fu) == 0xc005007u)
        return RISCV_VLOXEI16_V;
    if ((data & 0x1c00707fu) == 0xc006007u)
        return RISCV_VLOXEI32_V;
    if ((data & 0x1c00707fu) == 0xc007007u)
        return RISCV_VLOXEI64_V;
    if ((data & 0x1c00707fu) == 0xc000007u)
        return RISCV_VLOXEI8_V;
    if ((data & 0x1c00707fu) == 0x8005007u)
        return RISCV_VLSE16_V;
    if ((data & 0x1c00707fu) == 0x8006007u)
        return RISCV_VLSE32_V;
    if ((data & 0x1c00707fu) == 0x8007007u)
        return RISCV_VLSE64_V;
    if ((data & 0x1c00707fu) == 0x8000007u)
        return RISCV_VLSE8_V;
    if ((data & 0x1c00707fu) == 0x4005007u)
        return RISCV_VLUXEI16_V;
    if ((data & 0x1c00707fu) == 0x4006007u)
        return RISCV_VLUXEI32_V;
    if ((data & 0x1c00707fu) == 0x4007007u)
        return RISCV_VLUXEI64_V;
    if ((data & 0x1c00707fu) == 0x4000007u)
        return RISCV_VLUXEI8_V;
    if ((data & 0xfc00707fu) == 0xb4002057u)
        return RISCV_VMACC_VV;
    if ((data & 0xfc00707fu) == 0xb4006057u)
        return RISCV_VMACC_VX;
    if ((data & 0xfe00707fu) == 0x46003057u)
        return RISCV_VMADC_VI;
    if ((data & 0xfe00707fu) == 0x44003057u)
        return RISCV_VMADC_VIM;
    if ((data & 0xfe00707fu) == 0x46000057u)
        return RISCV_VMADC_VV;
    if ((data & 0xfe00707fu) == 0x44000057u)
        return RISCV_VMADC_VVM;
    if ((data & 0xfe00707fu) == 0x46004057u)
        return RISCV_VMADC_VX;
    if ((data & 0xfe00707fu) == 0x44004057u)
        return RISCV_VMADC_VXM;
    if ((data & 0xfc00707fu) == 0xa4002057u)
        return RISCV_VMADD_VV;
    if ((data & 0xfc00707fu) == 0xa4006057u)
        return RISCV_VMADD_VX;
    if ((data & 0xfe00707fu) == 0x66002057u)
        return RISCV_VMAND_MM;
    if ((data & 0xfe00707fu) == 0x62002057u)
        return RISCV_VMANDN_MM;
    if ((data & 0xfc00707fu) == 0x1c000057u)
        return RISCV_VMAX_VV;
    if ((data & 0xfc00707fu) == 0x1c004057u)
        return RISCV_VMAX_VX;
    if ((data & 0xfc00707fu) == 0x18000057u)
        return RISCV_VMAXU_VV;
    if ((data & 0xfc00707fu) == 0x18004057u)
        return RISCV_VMAXU_VX;
    if ((data & 0xfe00707fu) == 0x5c003057u)
        return RISCV_VMERGE_VIM;
    if ((data & 0xfe00707fu) == 0x5c000057u)
        return RISCV_VMERGE_VVM;
    if ((data & 0xfe00707fu) == 0x5c004057u)
        return RISCV_VMERGE_VXM;
    if ((data & 0xfc00707fu) == 0x60005057u)
        return RISCV_VMFEQ_VF;
    if ((data & 0xfc00707fu) == 0x60001057u)
        return RISCV_VMFEQ_VV;
    if ((data & 0xfc00707fu) == 0x7c005057u)
        return RISCV_VMFGE_VF;
    if ((data & 0xfc00707fu) == 0x74005057u)
        return RISCV_VMFGT_VF;
    if ((data & 0xfc00707fu) == 0x64005057u)
        return RISCV_VMFLE_VF;
    if ((data & 0xfc00707fu) == 0x64001057u)
        return RISCV_VMFLE_VV;
    if ((data & 0xfc00707fu) == 0x6c005057u)
        return RISCV_VMFLT_VF;
    if ((data & 0xfc00707fu) == 0x6c001057u)
        return RISCV_VMFLT_VV;
    if ((data & 0xfc00707fu) == 0x70005057u)
        return RISCV_VMFNE_VF;
    if ((data & 0xfc00707fu) == 0x70001057u)
        return RISCV_VMFNE_VV;
    if ((data & 0xfc00707fu) == 0x14000057u)
        return RISCV_VMIN_VV;
    if ((data & 0xfc00707fu) == 0x14004057u)
        return RISCV_VMIN_VX;
    if ((data & 0xfc00707fu) == 0x10000057u)
        return RISCV_VMINU_VV;
    if ((data & 0xfc00707fu) == 0x10004057u)
        return RISCV_VMINU_VX;
    if ((data & 0xfe00707fu) == 0x76002057u)
        return RISCV_VMNAND_MM;
    if ((data & 0xfe00707fu) == 0x7a002057u)
        return RISCV_VMNOR_MM;
    if ((data & 0xfe00707fu) == 0x6a002057u)
        return RISCV_VMOR_MM;
    if ((data & 0xfe00707fu) == 0x72002057u)
        return RISCV_VMORN_MM;
    if ((data & 0xfe00707fu) == 0x4e000057u)
        return RISCV_VMSBC_VV;
    if ((data & 0xfe00707fu) == 0x4c000057u)
        return RISCV_VMSBC_VVM;
    if ((data & 0xfe00707fu) == 0x4e004057u)
        return RISCV_VMSBC_VX;
    if ((data & 0xfe00707fu) == 0x4c004057u)
        return RISCV_VMSBC_VXM;
    if ((data & 0xfc0ff07fu) == 0x5000a057u)
        return RISCV_VMSBF_M;
    if ((data & 0xfc00707fu) == 0x60003057u)
        return RISCV_VMSEQ_VI;
    if ((data & 0xfc00707fu) == 0x60000057u)
        return RISCV_VMSEQ_VV;
    if ((data & 0xfc00707fu) == 0x60004057u)
        return RISCV_VMSEQ_VX;
    if ((data & 0xfc00707fu) == 0x7c003057u)
        return RISCV_VMSGT_VI;
    if ((data & 0xfc00707fu) == 0x7c004057u)
        return RISCV_VMSGT_VX;
    if ((data & 0xfc00707fu) == 0x78003057u)
        return RISCV_VMSGTU_VI;
    if ((data & 0xfc00707fu) == 0x78004057u)
        return RISCV_VMSGTU_VX;
    if ((data & 0xfc0ff07fu) == 0x5001a057u)
        return RISCV_VMSIF_M;
    if ((data & 0xfc00707fu) == 0x74003057u)
        return RISCV_VMSLE_VI;
    if ((data & 0xfc00707fu) == 0x74000057u)
        return RISCV_VMSLE_VV;
    if ((data & 0xfc00707fu) == 0x74004057u)
        return RISCV_VMSLE_VX;
    if ((data & 0xfc00707fu) == 0x70003057u)
        return RISCV_VMSLEU_VI;
    if ((data & 0xfc00707fu) == 0x70000057u)
        return RISCV_VMSLEU_VV;
    if ((data & 0xfc00707fu) == 0x70004057u)
        return RISCV_VMSLEU_VX;
    if ((data & 0xfc00707fu) == 0x6c000057u)
        return RISCV_VMSLT_VV;
    if ((data & 0xfc00707fu) == 0x6c004057u)
        return RISCV_VMSLT_VX;
    if ((data & 0xfc00707fu) == 0x68000057u)
        return RISCV_VMSLTU_VV;
    if ((data & 0xfc00707fu) == 0x68004057u)
        return RISCV_VMSLTU_VX;
    if ((data & 0xfc00707fu) == 0x64003057u)
        return RISCV_VMSNE_VI;
    if ((data & 0xfc00707fu) == 0x64000057u)
        return RISCV_VMSNE_VV;
    if ((data & 0xfc00707fu) == 0x64004057u)
        return RISCV_VMSNE_VX;
    if ((data & 0xfc0ff07fu) == 0x50012057u)
        return RISCV_VMSOF_M;
    if ((data & 0xfc00707fu) == 0x94002057u)
        return RISCV_VMUL_VV;
    if ((data & 0xfc00707fu) == 0x94006057u)
        return RISCV_VMUL_VX;
    if ((data & 0xfc00707fu) == 0x9c002057u)
        return RISCV_VMULH_VV;
    if ((data & 0xfc00707fu) == 0x9c006057u)
        return RISCV_VMULH_VX;
    if ((data & 0xfc00707fu) == 0x98002057u)
        return RISCV_VMULHSU_VV;
    if ((data & 0xfc00707fu) == 0x98006057u)
        return RISCV_VMULHSU_VX;
    if ((data & 0xfc00707fu) == 0x90002057u)
        return RISCV_VMULHU_VV;
    if ((data & 0xfc00707fu) == 0x90006057u)
        return RISCV_VMULHU_VX;
    if ((data & 0xfff0707fu) == 0x42006057u)
        return RISCV_VMV_S_X;
    if ((data & 0xfff0707fu) == 0x5e003057u)
        return RISCV_VMV_V_I;
    if ((data & 0xfff0707fu) == 0x5e000057u)
        return RISCV_VMV_V_V;
    if ((data & 0xfff0707fu) == 0x5e004057u)
        return RISCV_VMV_V_X;
    if ((data & 0xfe0ff07fu) == 0x42002057u)
        return RISCV_VMV_X_S;
    if ((data & 0xfe0ff07fu) == 0x9e003057u)
        return RISCV_VMV1R_V;
    if ((data & 0xfe0ff07fu) == 0x9e00b057u)
        return RISCV_VMV2R_V;
    if ((data & 0xfe0ff07fu) == 0x9e01b057u)
        return RISCV_VMV4R_V;
    if ((data & 0xfe0ff07fu) == 0x9e03b057u)
        return RISCV_VMV8R_V;
    if ((data & 0xfe00707fu) == 0x7e002057u)
        return RISCV_VMXNOR_MM;
    if ((data & 0xfe00707fu) == 0x6e002057u)
        return RISCV_VMXOR_MM;
    if ((data & 0xfc00707fu) == 0xbc003057u)
        return RISCV_VNCLIP_WI;
    if ((data & 0xfc00707fu) == 0xbc000057u)
        return RISCV_VNCLIP_WV;
    if ((data & 0xfc00707fu) == 0xbc004057u)
        return RISCV_VNCLIP_WX;
    if ((data & 0xfc00707fu) == 0xb8003057u)
        return RISCV_VNCLIPU_WI;
    if ((data & 0xfc00707fu) == 0xb8000057u)
        return RISCV_VNCLIPU_WV;
    if ((data & 0xfc00707fu) == 0xb8004057u)
        return RISCV_VNCLIPU_WX;
    if ((data & 0xfc00707fu) == 0xbc002057u)
        return RISCV_VNMSAC_VV;
    if ((data & 0xfc00707fu) == 0xbc006057u)
        return RISCV_VNMSAC_VX;
    if ((data & 0xfc00707fu) == 0xac002057u)
        return RISCV_VNMSUB_VV;
    if ((data & 0xfc00707fu) == 0xac006057u)
        return RISCV_VNMSUB_VX;
    if ((data & 0xfc00707fu) == 0xb4003057u)
        return RISCV_VNSRA_WI;
    if ((data & 0xfc00707fu) == 0xb4000057u)
        return RISCV_VNSRA_WV;
    if ((data & 0xfc00707fu) == 0xb4004057u)
        return RISCV_VNSRA_WX;
    if ((data & 0xfc00707fu) == 0xb0003057u)
        return RISCV_VNSRL_WI;
    if ((data & 0xfc00707fu) == 0xb0000057u)
        return RISCV_VNSRL_WV;
    if ((data & 0xfc00707fu) == 0xb0004057u)
        return RISCV_VNSRL_WX;
    if ((data & 0xfc00707fu) == 0x28003057u)
        return RISCV_VOR_VI;
    if ((data & 0xfc00707fu) == 0x28000057u)
        return RISCV_VOR_VV;
    if ((data & 0xfc00707fu) == 0x28004057u)
        return RISCV_VOR_VX;
    if ((data & 0xfc00707fu) == 0x3c000057u)
        return RISCV_VPAIRE_VV;
    if ((data & 0xfc00707fu) == 0x3c002057u)
        return RISCV_VPAIRO_VV;
    if ((data & 0xfc00707fu) == 0xbc000077u)
        return RISCV_VQWBDOTAS_VV;
    if ((data & 0xfc00707fu) == 0xb8000077u)
        return RISCV_VQWBDOTAU_VV;
    if ((data & 0xfc00707fu) == 0x9c000077u)
        return RISCV_VQWDOTAS_VV;
    if ((data & 0xfc00707fu) == 0x98000077u)
        return RISCV_VQWDOTAU_VV;
    if ((data & 0xfc00707fu) == 0x4002057u)
        return RISCV_VREDAND_VS;
    if ((data & 0xfc00707fu) == 0x1c002057u)
        return RISCV_VREDMAX_VS;
    if ((data & 0xfc00707fu) == 0x18002057u)
        return RISCV_VREDMAXU_VS;
    if ((data & 0xfc00707fu) == 0x14002057u)
        return RISCV_VREDMIN_VS;
    if ((data & 0xfc00707fu) == 0x10002057u)
        return RISCV_VREDMINU_VS;
    if ((data & 0xfc00707fu) == 0x8002057u)
        return RISCV_VREDOR_VS;
    if ((data & 0xfc00707fu) == 0x2057u)
        return RISCV_VREDSUM_VS;
    if ((data & 0xfc00707fu) == 0xc002057u)
        return RISCV_VREDXOR_VS;
    if ((data & 0xfc00707fu) == 0x8c002057u)
        return RISCV_VREM_VV;
    if ((data & 0xfc00707fu) == 0x8c006057u)
        return RISCV_VREM_VX;
    if ((data & 0xfc00707fu) == 0x88002057u)
        return RISCV_VREMU_VV;
    if ((data & 0xfc00707fu) == 0x88006057u)
        return RISCV_VREMU_VX;
    if ((data & 0xfc0ff07fu) == 0x4804a057u)
        return RISCV_VREV8_V;
    if ((data & 0xfc00707fu) == 0x30003057u)
        return RISCV_VRGATHER_VI;
    if ((data & 0xfc00707fu) == 0x30000057u)
        return RISCV_VRGATHER_VV;
    if ((data & 0xfc00707fu) == 0x30004057u)
        return RISCV_VRGATHER_VX;
    if ((data & 0xfc00707fu) == 0x38000057u)
        return RISCV_VRGATHEREI16_VV;
    if ((data & 0xfc00707fu) == 0x54000057u)
        return RISCV_VROL_VV;
    if ((data & 0xfc00707fu) == 0x54004057u)
        return RISCV_VROL_VX;
    if ((data & 0xf800707fu) == 0x50003057u)
        return RISCV_VROR_VI;
    if ((data & 0xfc00707fu) == 0x50000057u)
        return RISCV_VROR_VV;
    if ((data & 0xfc00707fu) == 0x50004057u)
        return RISCV_VROR_VX;
    if ((data & 0xfc00707fu) == 0xc003057u)
        return RISCV_VRSUB_VI;
    if ((data & 0xfc00707fu) == 0xc004057u)
        return RISCV_VRSUB_VX;
    if ((data & 0xfff0707fu) == 0x2800027u)
        return RISCV_VS1R_V;
    if ((data & 0xfff0707fu) == 0x22800027u)
        return RISCV_VS2R_V;
    if ((data & 0xfff0707fu) == 0x62800027u)
        return RISCV_VS4R_V;
    if ((data & 0xfff0707fu) == 0xe2800027u)
        return RISCV_VS8R_V;
    if ((data & 0xfc00707fu) == 0x84003057u)
        return RISCV_VSADD_VI;
    if ((data & 0xfc00707fu) == 0x84000057u)
        return RISCV_VSADD_VV;
    if ((data & 0xfc00707fu) == 0x84004057u)
        return RISCV_VSADD_VX;
    if ((data & 0xfc00707fu) == 0x80003057u)
        return RISCV_VSADDU_VI;
    if ((data & 0xfc00707fu) == 0x80000057u)
        return RISCV_VSADDU_VV;
    if ((data & 0xfc00707fu) == 0x80004057u)
        return RISCV_VSADDU_VX;
    if ((data & 0xfe00707fu) == 0x48000057u)
        return RISCV_VSBC_VVM;
    if ((data & 0xfe00707fu) == 0x48004057u)
        return RISCV_VSBC_VXM;
    if ((data & 0x1df0707fu) == 0x5027u)
        return RISCV_VSE16_V;
    if ((data & 0x1df0707fu) == 0x6027u)
        return RISCV_VSE32_V;
    if ((data & 0x1df0707fu) == 0x7027u)
        return RISCV_VSE64_V;
    if ((data & 0x1df0707fu) == 0x27u)
        return RISCV_VSE8_V;
    if ((data & 0xc000707fu) == 0xc0007057u)
        return RISCV_VSETIVLI;
    if ((data & 0xfe00707fu) == 0x80007057u)
        return RISCV_VSETVL;
    if ((data & 0x8000707fu) == 0x7057u)
        return RISCV_VSETVLI;
    if ((data & 0xfc0ff07fu) == 0x4803a057u)
        return RISCV_VSEXT_VF2;
    if ((data & 0xfc0ff07fu) == 0x4802a057u)
        return RISCV_VSEXT_VF4;
    if ((data & 0xfc0ff07fu) == 0x4801a057u)
        return RISCV_VSEXT_VF8;
    if ((data & 0xfe00707fu) == 0xba002077u)
        return RISCV_VSHA2CH_VV;
    if ((data & 0xfe00707fu) == 0xbe002077u)
        return RISCV_VSHA2CL_VV;
    if ((data & 0xfe00707fu) == 0xb6002077u)
        return RISCV_VSHA2MS_VV;
    if ((data & 0xfc00707fu) == 0x3c006057u)
        return RISCV_VSLIDE1DOWN_VX;
    if ((data & 0xfc00707fu) == 0x38006057u)
        return RISCV_VSLIDE1UP_VX;
    if ((data & 0xfc00707fu) == 0x3c003057u)
        return RISCV_VSLIDEDOWN_VI;
    if ((data & 0xfc00707fu) == 0x3c004057u)
        return RISCV_VSLIDEDOWN_VX;
    if ((data & 0xfc00707fu) == 0x38003057u)
        return RISCV_VSLIDEUP_VI;
    if ((data & 0xfc00707fu) == 0x38004057u)
        return RISCV_VSLIDEUP_VX;
    if ((data & 0xfc00707fu) == 0x94003057u)
        return RISCV_VSLL_VI;
    if ((data & 0xfc00707fu) == 0x94000057u)
        return RISCV_VSLL_VV;
    if ((data & 0xfc00707fu) == 0x94004057u)
        return RISCV_VSLL_VX;
    if ((data & 0xfff0707fu) == 0x2b00027u)
        return RISCV_VSM_V;
    if ((data & 0xfe00707fu) == 0xae002077u)
        return RISCV_VSM3C_VI;
    if ((data & 0xfe00707fu) == 0x82002077u)
        return RISCV_VSM3ME_VV;
    if ((data & 0xfe00707fu) == 0x86002077u)
        return RISCV_VSM4K_VI;
    if ((data & 0xfe0ff07fu) == 0xa6082077u)
        return RISCV_VSM4R_VS;
    if ((data & 0xfe0ff07fu) == 0xa2082077u)
        return RISCV_VSM4R_VV;
    if ((data & 0xfc00707fu) == 0x9c000057u)
        return RISCV_VSMUL_VV;
    if ((data & 0xfc00707fu) == 0x9c004057u)
        return RISCV_VSMUL_VX;
    if ((data & 0x1c00707fu) == 0xc005027u)
        return RISCV_VSOXEI16_V;
    if ((data & 0x1c00707fu) == 0xc006027u)
        return RISCV_VSOXEI32_V;
    if ((data & 0x1c00707fu) == 0xc007027u)
        return RISCV_VSOXEI64_V;
    if ((data & 0x1c00707fu) == 0xc000027u)
        return RISCV_VSOXEI8_V;
    if ((data & 0xfc00707fu) == 0xa4003057u)
        return RISCV_VSRA_VI;
    if ((data & 0xfc00707fu) == 0xa4000057u)
        return RISCV_VSRA_VV;
    if ((data & 0xfc00707fu) == 0xa4004057u)
        return RISCV_VSRA_VX;
    if ((data & 0xfc00707fu) == 0xa0003057u)
        return RISCV_VSRL_VI;
    if ((data & 0xfc00707fu) == 0xa0000057u)
        return RISCV_VSRL_VV;
    if ((data & 0xfc00707fu) == 0xa0004057u)
        return RISCV_VSRL_VX;
    if ((data & 0x1c00707fu) == 0x8005027u)
        return RISCV_VSSE16_V;
    if ((data & 0x1c00707fu) == 0x8006027u)
        return RISCV_VSSE32_V;
    if ((data & 0x1c00707fu) == 0x8007027u)
        return RISCV_VSSE64_V;
    if ((data & 0x1c00707fu) == 0x8000027u)
        return RISCV_VSSE8_V;
    if ((data & 0xfc00707fu) == 0xac003057u)
        return RISCV_VSSRA_VI;
    if ((data & 0xfc00707fu) == 0xac000057u)
        return RISCV_VSSRA_VV;
    if ((data & 0xfc00707fu) == 0xac004057u)
        return RISCV_VSSRA_VX;
    if ((data & 0xfc00707fu) == 0xa8003057u)
        return RISCV_VSSRL_VI;
    if ((data & 0xfc00707fu) == 0xa8000057u)
        return RISCV_VSSRL_VV;
    if ((data & 0xfc00707fu) == 0xa8004057u)
        return RISCV_VSSRL_VX;
    if ((data & 0xfc00707fu) == 0x8c000057u)
        return RISCV_VSSUB_VV;
    if ((data & 0xfc00707fu) == 0x8c004057u)
        return RISCV_VSSUB_VX;
    if ((data & 0xfc00707fu) == 0x88000057u)
        return RISCV_VSSUBU_VV;
    if ((data & 0xfc00707fu) == 0x88004057u)
        return RISCV_VSSUBU_VX;
    if ((data & 0xfc00707fu) == 0x8000057u)
        return RISCV_VSUB_VV;
    if ((data & 0xfc00707fu) == 0x8004057u)
        return RISCV_VSUB_VX;
    if ((data & 0x1c00707fu) == 0x4005027u)
        return RISCV_VSUXEI16_V;
    if ((data & 0x1c00707fu) == 0x4006027u)
        return RISCV_VSUXEI32_V;
    if ((data & 0x1c00707fu) == 0x4007027u)
        return RISCV_VSUXEI64_V;
    if ((data & 0x1c00707fu) == 0x4000027u)
        return RISCV_VSUXEI8_V;
    if ((data & 0xfc0ff07fu) == 0x4805a057u)
        return RISCV_VUNZIPE_V;
    if ((data & 0xfc0ff07fu) == 0x4807a057u)
        return RISCV_VUNZIPO_V;
    if ((data & 0xfc00707fu) == 0x54002057u)
        return RISCV_VWABDA_VV;
    if ((data & 0xfc00707fu) == 0x58002057u)
        return RISCV_VWABDAU_VV;
    if ((data & 0xfc00707fu) == 0xc4002057u)
        return RISCV_VWADD_VV;
    if ((data & 0xfc00707fu) == 0xc4006057u)
        return RISCV_VWADD_VX;
    if ((data & 0xfc00707fu) == 0xd4002057u)
        return RISCV_VWADD_WV;
    if ((data & 0xfc00707fu) == 0xd4006057u)
        return RISCV_VWADD_WX;
    if ((data & 0xfc00707fu) == 0xc0002057u)
        return RISCV_VWADDU_VV;
    if ((data & 0xfc00707fu) == 0xc0006057u)
        return RISCV_VWADDU_VX;
    if ((data & 0xfc00707fu) == 0xd0002057u)
        return RISCV_VWADDU_WV;
    if ((data & 0xfc00707fu) == 0xd0006057u)
        return RISCV_VWADDU_WX;
    if ((data & 0xfc00707fu) == 0xf4002057u)
        return RISCV_VWMACC_VV;
    if ((data & 0xfc00707fu) == 0xf4006057u)
        return RISCV_VWMACC_VX;
    if ((data & 0xfc00707fu) == 0xfc002057u)
        return RISCV_VWMACCSU_VV;
    if ((data & 0xfc00707fu) == 0xfc006057u)
        return RISCV_VWMACCSU_VX;
    if ((data & 0xfc00707fu) == 0xf0002057u)
        return RISCV_VWMACCU_VV;
    if ((data & 0xfc00707fu) == 0xf0006057u)
        return RISCV_VWMACCU_VX;
    if ((data & 0xfc00707fu) == 0xf8006057u)
        return RISCV_VWMACCUS_VX;
    if ((data & 0xfc00707fu) == 0xec002057u)
        return RISCV_VWMUL_VV;
    if ((data & 0xfc00707fu) == 0xec006057u)
        return RISCV_VWMUL_VX;
    if ((data & 0xfc00707fu) == 0xe8002057u)
        return RISCV_VWMULSU_VV;
    if ((data & 0xfc00707fu) == 0xe8006057u)
        return RISCV_VWMULSU_VX;
    if ((data & 0xfc00707fu) == 0xe0002057u)
        return RISCV_VWMULU_VV;
    if ((data & 0xfc00707fu) == 0xe0006057u)
        return RISCV_VWMULU_VX;
    if ((data & 0xfc00707fu) == 0xc4000057u)
        return RISCV_VWREDSUM_VS;
    if ((data & 0xfc00707fu) == 0xc0000057u)
        return RISCV_VWREDSUMU_VS;
    if ((data & 0xfc00707fu) == 0xd4003057u)
        return RISCV_VWSLL_VI;
    if ((data & 0xfc00707fu) == 0xd4000057u)
        return RISCV_VWSLL_VV;
    if ((data & 0xfc00707fu) == 0xd4004057u)
        return RISCV_VWSLL_VX;
    if ((data & 0xfc00707fu) == 0xcc002057u)
        return RISCV_VWSUB_VV;
    if ((data & 0xfc00707fu) == 0xcc006057u)
        return RISCV_VWSUB_VX;
    if ((data & 0xfc00707fu) == 0xdc002057u)
        return RISCV_VWSUB_WV;
    if ((data & 0xfc00707fu) == 0xdc006057u)
        return RISCV_VWSUB_WX;
    if ((data & 0xfc00707fu) == 0xc8002057u)
        return RISCV_VWSUBU_VV;
    if ((data & 0xfc00707fu) == 0xc8006057u)
        return RISCV_VWSUBU_VX;
    if ((data & 0xfc00707fu) == 0xd8002057u)
        return RISCV_VWSUBU_WV;
    if ((data & 0xfc00707fu) == 0xd8006057u)
        return RISCV_VWSUBU_WX;
    if ((data & 0xfc00707fu) == 0x2c003057u)
        return RISCV_VXOR_VI;
    if ((data & 0xfc00707fu) == 0x2c000057u)
        return RISCV_VXOR_VV;
    if ((data & 0xfc00707fu) == 0x2c004057u)
        return RISCV_VXOR_VX;
    if ((data & 0xfc0ff07fu) == 0x48032057u)
        return RISCV_VZEXT_VF2;
    if ((data & 0xfc0ff07fu) == 0x48022057u)
        return RISCV_VZEXT_VF4;
    if ((data & 0xfc0ff07fu) == 0x48012057u)
        return RISCV_VZEXT_VF8;
    if ((data & 0xfc00707fu) == 0xf8002057u)
        return RISCV_VZIP_VV;
    if ((data & 0xfe0070ffu) == 0x200209bu)
        return RISCV_WADD;
    if ((data & 0xfe0070ffu) == 0xa00209bu)
        return RISCV_WADDA;
    if ((data & 0xfe0070ffu) == 0x1a00209bu)
        return RISCV_WADDAU;
    if ((data & 0xfe0070ffu) == 0x1200209bu)
        return RISCV_WADDU;
    if ((data & 0xffffffffu) == 0x10500073u)
        return RISCV_WFI;
    if ((data & 0xfe0070ffu) == 0x2a00209bu)
        return RISCV_WMACC;
    if ((data & 0xfe0070ffu) == 0x6a00209bu)
        return RISCV_WMACCSU;
    if ((data & 0xfe0070ffu) == 0x3a00209bu)
        return RISCV_WMACCU;
    if ((data & 0xfe0070ffu) == 0x2200209bu)
        return RISCV_WMUL;
    if ((data & 0xfe0070ffu) == 0x6200209bu)
        return RISCV_WMULSU;
    if ((data & 0xfe0070ffu) == 0x3200209bu)
        return RISCV_WMULU;
    if ((data & 0xffffffffu) == 0xd00073u)
        return RISCV_WRS_NTO;
    if ((data & 0xffffffffu) == 0x1d00073u)
        return RISCV_WRS_STO;
    if ((data & 0xfe0070ffu) == 0x4e00201bu)
        return RISCV_WSLA;
    if ((data & 0xfc0070ffu) == 0x4400201bu)
        return RISCV_WSLAI;
    if ((data & 0xfe0070ffu) == 0xe00201bu)
        return RISCV_WSLL;
    if ((data & 0xfc0070ffu) == 0x400201bu)
        return RISCV_WSLLI;
    if ((data & 0xfe0070ffu) == 0x4200209bu)
        return RISCV_WSUB;
    if ((data & 0xfe0070ffu) == 0x4a00209bu)
        return RISCV_WSUBA;
    if ((data & 0xfe0070ffu) == 0x5a00209bu)
        return RISCV_WSUBAU;
    if ((data & 0xfe0070ffu) == 0x5200209bu)
        return RISCV_WSUBU;
    if ((data & 0xfe0070ffu) == 0x7a00201bu)
        return RISCV_WZIP16P;
    if ((data & 0xfe0070ffu) == 0x7800201bu)
        return RISCV_WZIP8P;
    if ((data & 0xfe00707fu) == 0x40004033u)
        return RISCV_XNOR;
    if ((data & 0xfe00707fu) == 0x4033u)
        return RISCV_XOR;
    if ((data & 0x707fu) == 0x4013u)
        return RISCV_XORI;
    if ((data & 0xfe00707fu) == 0x28006033u)
        return RISCV_XPERM16;
    if ((data & 0xfe00707fu) == 0x28000033u)
        return RISCV_XPERM32;
    if ((data & 0xfe00707fu) == 0x28002033u)
        return RISCV_XPERM4;
    if ((data & 0xfe00707fu) == 0x28004033u)
        return RISCV_XPERM8;
    if ((data & 0xfff0707fu) == 0x8f01013u)
        return RISCV_ZIP;
    if ((data & 0xfe00707fu) == 0xf600203bu)
        return RISCV_ZIP16HP;
    if ((data & 0xfe00707fu) == 0xf200203bu)
        return RISCV_ZIP16P;
    if ((data & 0xfe00707fu) == 0xf400203bu)
        return RISCV_ZIP8HP;
    if ((data & 0xfe00707fu) == 0xf000203bu)
        return RISCV_ZIP8P;
    return RISCV_INVALID;
}
