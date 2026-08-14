// riscv-opcodes commit 5f869fc6fead9a58a05ac2715accb8e7635e6315
#include "disassembler.h"
#include "decoder.h"
#include <cstdio>
#include <cstdint>
#include <cstddef>
#define BX(v, b) (((v) >> b) & 1)
#define FX(v, high, low) (((v) >> low) & ((1U << ((high) - (low) + 1)) - 1))
#define SIGN_EXTEND(val, val_sz) (((int32_t)(val) << (32 - (val_sz))) >> (32 - (val_sz)))
static const char riscv_gpr[32][9] = {
    "zero", "ra", "sp", "gp", "tp", "t0", "t1", "t2", "s0", "s1", "a0", "a1", "a2", "a3", "a4", "a5",
    "a6", "a7", "s2", "s3", "s4", "s5", "s6", "s7", "s8", "s9", "s10", "s11", "t3", "t4", "t5", "t6",
};
static const char riscv_fpr[32][5] = {
    "f0", "f1", "f2", "f3", "f4", "f5", "f6", "f7", "f8", "f9", "f10", "f11", "f12", "f13", "f14", "f15",
    "f16", "f17", "f18", "f19", "f20", "f21", "f22", "f23", "f24", "f25", "f26", "f27", "f28", "f29", "f30", "f31",
};
static const char riscv_vpr[32][4] = {
    "v0", "v1", "v2", "v3", "v4", "v5", "v6", "v7", "v8", "v9", "v10", "v11", "v12", "v13", "v14", "v15",
    "v16", "v17", "v18", "v19", "v20", "v21", "v22", "v23", "v24", "v25", "v26", "v27", "v28", "v29", "v30", "v31",
};
static const char riscv_vm[2][5] = {"v0.t", "none"};
static const char riscv_aq[4][5] = {"none", "aq"};
static const char riscv_rl[4][5] = {"none", "rl"};
static const char riscv_rm[8][8] = {"rne", "rtz", "rdn", "rup", "rmm", "unknown", "unknown", "dyn"};
static const char riscv_nf[8][4] = {"1", "2", "3", "4", "5", "6", "7", "8"};
static const char riscv_lmul[8][9] = {"m1", "m2", "m4", "m8", "reserved", "mf8", "mf4", "mf2"};
static const char riscv_sew[8][9] = {"e8", "e16", "e32", "e64", "reserved", "reserved", "reserved", "reserved"};
static const char riscv_vt[2][3] = {"tu", "ta"};
static const char riscv_vm_[2][3] = {"mu", "ma"};
std::string riscv_disassemble(uint32_t data, uint64_t addr)
{
    char buff[200];
    (void)addr;
    switch (riscv_get_mnemonic(data)) {
    case RISCV_ABS: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s", "ABS", riscv_gpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)]);
    } break;
    case RISCV_ABSW: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s", "ABSW", riscv_gpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)]);
    } break;
    case RISCV_ADD: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "ADD", riscv_gpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 24, 20)]);
    } break;
    case RISCV_ADD_UW: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "ADD.UW", riscv_gpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 24, 20)]);
    } break;
    case RISCV_ADDD: {
        snprintf(buff, sizeof(buff), "%-15s (%s, %s), (%s, %s), (%s, %s)", "ADDD", riscv_gpr[2 * FX(data, 24, 21)], riscv_gpr[2 * FX(data, 24, 21) + 1], riscv_gpr[2 * FX(data, 19, 16)], riscv_gpr[2 * FX(data, 19, 16) + 1], riscv_gpr[2 * FX(data, 11, 8)], riscv_gpr[2 * FX(data, 11, 8) + 1]);
    } break;
    case RISCV_ADDI: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, 0x%x(%d)", "ADDI", riscv_gpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)], (unsigned int)(SIGN_EXTEND(FX(data, 31, 20), 12)), (int)(SIGN_EXTEND(FX(data, 31, 20), 12)));
    } break;
    case RISCV_ADDIW: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, 0x%x(%d)", "ADDIW", riscv_gpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)], (unsigned int)(SIGN_EXTEND(FX(data, 31, 20), 12)), (int)(SIGN_EXTEND(FX(data, 31, 20), 12)));
    } break;
    case RISCV_ADDW: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "ADDW", riscv_gpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 24, 20)]);
    } break;
    case RISCV_AES32DSI: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, 0x%x(%d)", "AES32DSI", riscv_gpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 24, 20)], (unsigned int)(FX(data, 31, 30)), (int)(FX(data, 31, 30)));
    } break;
    case RISCV_AES32DSMI: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, 0x%x(%d)", "AES32DSMI", riscv_gpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 24, 20)], (unsigned int)(FX(data, 31, 30)), (int)(FX(data, 31, 30)));
    } break;
    case RISCV_AES32ESI: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, 0x%x(%d)", "AES32ESI", riscv_gpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 24, 20)], (unsigned int)(FX(data, 31, 30)), (int)(FX(data, 31, 30)));
    } break;
    case RISCV_AES32ESMI: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, 0x%x(%d)", "AES32ESMI", riscv_gpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 24, 20)], (unsigned int)(FX(data, 31, 30)), (int)(FX(data, 31, 30)));
    } break;
    case RISCV_AES64DS: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "AES64DS", riscv_gpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 24, 20)]);
    } break;
    case RISCV_AES64DSM: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "AES64DSM", riscv_gpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 24, 20)]);
    } break;
    case RISCV_AES64ES: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "AES64ES", riscv_gpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 24, 20)]);
    } break;
    case RISCV_AES64ESM: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "AES64ESM", riscv_gpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 24, 20)]);
    } break;
    case RISCV_AES64IM: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s", "AES64IM", riscv_gpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)]);
    } break;
    case RISCV_AES64KS1I: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, 0x%x(%d)", "AES64KS1I", riscv_gpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)], (unsigned int)(FX(data, 23, 20)), (int)(FX(data, 23, 20)));
    } break;
    case RISCV_AES64KS2: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "AES64KS2", riscv_gpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 24, 20)]);
    } break;
    case RISCV_AMOADD_B: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s, %s", "AMOADD.B", riscv_gpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 24, 20)], riscv_aq[FX(data, 26, 26)], riscv_rl[FX(data, 25, 25)]);
    } break;
    case RISCV_AMOADD_D: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s, %s", "AMOADD.D", riscv_gpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 24, 20)], riscv_aq[FX(data, 26, 26)], riscv_rl[FX(data, 25, 25)]);
    } break;
    case RISCV_AMOADD_H: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s, %s", "AMOADD.H", riscv_gpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 24, 20)], riscv_aq[FX(data, 26, 26)], riscv_rl[FX(data, 25, 25)]);
    } break;
    case RISCV_AMOADD_W: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s, %s", "AMOADD.W", riscv_gpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 24, 20)], riscv_aq[FX(data, 26, 26)], riscv_rl[FX(data, 25, 25)]);
    } break;
    case RISCV_AMOAND_B: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s, %s", "AMOAND.B", riscv_gpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 24, 20)], riscv_aq[FX(data, 26, 26)], riscv_rl[FX(data, 25, 25)]);
    } break;
    case RISCV_AMOAND_D: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s, %s", "AMOAND.D", riscv_gpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 24, 20)], riscv_aq[FX(data, 26, 26)], riscv_rl[FX(data, 25, 25)]);
    } break;
    case RISCV_AMOAND_H: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s, %s", "AMOAND.H", riscv_gpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 24, 20)], riscv_aq[FX(data, 26, 26)], riscv_rl[FX(data, 25, 25)]);
    } break;
    case RISCV_AMOAND_W: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s, %s", "AMOAND.W", riscv_gpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 24, 20)], riscv_aq[FX(data, 26, 26)], riscv_rl[FX(data, 25, 25)]);
    } break;
    case RISCV_AMOCAS_B: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s, %s", "AMOCAS.B", riscv_gpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 24, 20)], riscv_aq[FX(data, 26, 26)], riscv_rl[FX(data, 25, 25)]);
    } break;
    case RISCV_AMOCAS_D: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s, %s", "AMOCAS.D", riscv_gpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 24, 20)], riscv_aq[FX(data, 26, 26)], riscv_rl[FX(data, 25, 25)]);
    } break;
    case RISCV_AMOCAS_H: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s, %s", "AMOCAS.H", riscv_gpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 24, 20)], riscv_aq[FX(data, 26, 26)], riscv_rl[FX(data, 25, 25)]);
    } break;
    case RISCV_AMOCAS_Q: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s, %s", "AMOCAS.Q", riscv_gpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 24, 20)], riscv_aq[FX(data, 26, 26)], riscv_rl[FX(data, 25, 25)]);
    } break;
    case RISCV_AMOCAS_W: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s, %s", "AMOCAS.W", riscv_gpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 24, 20)], riscv_aq[FX(data, 26, 26)], riscv_rl[FX(data, 25, 25)]);
    } break;
    case RISCV_AMOMAX_B: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s, %s", "AMOMAX.B", riscv_gpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 24, 20)], riscv_aq[FX(data, 26, 26)], riscv_rl[FX(data, 25, 25)]);
    } break;
    case RISCV_AMOMAX_D: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s, %s", "AMOMAX.D", riscv_gpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 24, 20)], riscv_aq[FX(data, 26, 26)], riscv_rl[FX(data, 25, 25)]);
    } break;
    case RISCV_AMOMAX_H: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s, %s", "AMOMAX.H", riscv_gpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 24, 20)], riscv_aq[FX(data, 26, 26)], riscv_rl[FX(data, 25, 25)]);
    } break;
    case RISCV_AMOMAX_W: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s, %s", "AMOMAX.W", riscv_gpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 24, 20)], riscv_aq[FX(data, 26, 26)], riscv_rl[FX(data, 25, 25)]);
    } break;
    case RISCV_AMOMAXU_B: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s, %s", "AMOMAXU.B", riscv_gpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 24, 20)], riscv_aq[FX(data, 26, 26)], riscv_rl[FX(data, 25, 25)]);
    } break;
    case RISCV_AMOMAXU_D: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s, %s", "AMOMAXU.D", riscv_gpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 24, 20)], riscv_aq[FX(data, 26, 26)], riscv_rl[FX(data, 25, 25)]);
    } break;
    case RISCV_AMOMAXU_H: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s, %s", "AMOMAXU.H", riscv_gpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 24, 20)], riscv_aq[FX(data, 26, 26)], riscv_rl[FX(data, 25, 25)]);
    } break;
    case RISCV_AMOMAXU_W: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s, %s", "AMOMAXU.W", riscv_gpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 24, 20)], riscv_aq[FX(data, 26, 26)], riscv_rl[FX(data, 25, 25)]);
    } break;
    case RISCV_AMOMIN_B: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s, %s", "AMOMIN.B", riscv_gpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 24, 20)], riscv_aq[FX(data, 26, 26)], riscv_rl[FX(data, 25, 25)]);
    } break;
    case RISCV_AMOMIN_D: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s, %s", "AMOMIN.D", riscv_gpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 24, 20)], riscv_aq[FX(data, 26, 26)], riscv_rl[FX(data, 25, 25)]);
    } break;
    case RISCV_AMOMIN_H: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s, %s", "AMOMIN.H", riscv_gpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 24, 20)], riscv_aq[FX(data, 26, 26)], riscv_rl[FX(data, 25, 25)]);
    } break;
    case RISCV_AMOMIN_W: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s, %s", "AMOMIN.W", riscv_gpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 24, 20)], riscv_aq[FX(data, 26, 26)], riscv_rl[FX(data, 25, 25)]);
    } break;
    case RISCV_AMOMINU_B: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s, %s", "AMOMINU.B", riscv_gpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 24, 20)], riscv_aq[FX(data, 26, 26)], riscv_rl[FX(data, 25, 25)]);
    } break;
    case RISCV_AMOMINU_D: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s, %s", "AMOMINU.D", riscv_gpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 24, 20)], riscv_aq[FX(data, 26, 26)], riscv_rl[FX(data, 25, 25)]);
    } break;
    case RISCV_AMOMINU_H: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s, %s", "AMOMINU.H", riscv_gpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 24, 20)], riscv_aq[FX(data, 26, 26)], riscv_rl[FX(data, 25, 25)]);
    } break;
    case RISCV_AMOMINU_W: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s, %s", "AMOMINU.W", riscv_gpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 24, 20)], riscv_aq[FX(data, 26, 26)], riscv_rl[FX(data, 25, 25)]);
    } break;
    case RISCV_AMOOR_B: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s, %s", "AMOOR.B", riscv_gpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 24, 20)], riscv_aq[FX(data, 26, 26)], riscv_rl[FX(data, 25, 25)]);
    } break;
    case RISCV_AMOOR_D: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s, %s", "AMOOR.D", riscv_gpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 24, 20)], riscv_aq[FX(data, 26, 26)], riscv_rl[FX(data, 25, 25)]);
    } break;
    case RISCV_AMOOR_H: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s, %s", "AMOOR.H", riscv_gpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 24, 20)], riscv_aq[FX(data, 26, 26)], riscv_rl[FX(data, 25, 25)]);
    } break;
    case RISCV_AMOOR_W: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s, %s", "AMOOR.W", riscv_gpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 24, 20)], riscv_aq[FX(data, 26, 26)], riscv_rl[FX(data, 25, 25)]);
    } break;
    case RISCV_AMOSWAP_B: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s, %s", "AMOSWAP.B", riscv_gpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 24, 20)], riscv_aq[FX(data, 26, 26)], riscv_rl[FX(data, 25, 25)]);
    } break;
    case RISCV_AMOSWAP_D: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s, %s", "AMOSWAP.D", riscv_gpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 24, 20)], riscv_aq[FX(data, 26, 26)], riscv_rl[FX(data, 25, 25)]);
    } break;
    case RISCV_AMOSWAP_H: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s, %s", "AMOSWAP.H", riscv_gpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 24, 20)], riscv_aq[FX(data, 26, 26)], riscv_rl[FX(data, 25, 25)]);
    } break;
    case RISCV_AMOSWAP_W: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s, %s", "AMOSWAP.W", riscv_gpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 24, 20)], riscv_aq[FX(data, 26, 26)], riscv_rl[FX(data, 25, 25)]);
    } break;
    case RISCV_AMOXOR_B: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s, %s", "AMOXOR.B", riscv_gpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 24, 20)], riscv_aq[FX(data, 26, 26)], riscv_rl[FX(data, 25, 25)]);
    } break;
    case RISCV_AMOXOR_D: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s, %s", "AMOXOR.D", riscv_gpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 24, 20)], riscv_aq[FX(data, 26, 26)], riscv_rl[FX(data, 25, 25)]);
    } break;
    case RISCV_AMOXOR_H: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s, %s", "AMOXOR.H", riscv_gpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 24, 20)], riscv_aq[FX(data, 26, 26)], riscv_rl[FX(data, 25, 25)]);
    } break;
    case RISCV_AMOXOR_W: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s, %s", "AMOXOR.W", riscv_gpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 24, 20)], riscv_aq[FX(data, 26, 26)], riscv_rl[FX(data, 25, 25)]);
    } break;
    case RISCV_AND: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "AND", riscv_gpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 24, 20)]);
    } break;
    case RISCV_ANDI: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, 0x%x(%d)", "ANDI", riscv_gpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)], (unsigned int)(SIGN_EXTEND(FX(data, 31, 20), 12)), (int)(SIGN_EXTEND(FX(data, 31, 20), 12)));
    } break;
    case RISCV_ANDN: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "ANDN", riscv_gpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 24, 20)]);
    } break;
    case RISCV_AUIPC: {
        snprintf(buff, sizeof(buff), "%-15s %s, 0x%x(%d)", "AUIPC", riscv_gpr[FX(data, 11, 7)], (unsigned int)(FX(data, 31, 12)), (int)(FX(data, 31, 12)));
    } break;
    case RISCV_BCLR: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "BCLR", riscv_gpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 24, 20)]);
    } break;
    case RISCV_BCLRI: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, 0x%x(%d)", "BCLRI", riscv_gpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)], (unsigned int)(FX(data, 25, 20)), (int)(FX(data, 25, 20)));
    } break;
    case RISCV_BEQ: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, 0x%x(%d)", "BEQ", riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 24, 20)], (unsigned int)(SIGN_EXTEND(((BX(data, 31) << 12) | (BX(data, 7) << 11) | (FX(data, 30, 25) << 5) | (FX(data, 11, 8) << 1)), 13)), (int)(SIGN_EXTEND(((BX(data, 31) << 12) | (BX(data, 7) << 11) | (FX(data, 30, 25) << 5) | (FX(data, 11, 8) << 1)), 13)));
    } break;
    case RISCV_BEQI: {
        snprintf(buff, sizeof(buff), "%-15s 0x%x(%d), 0x%x(%d), %s", "BEQI", (unsigned int)(SIGN_EXTEND(((FX(data, 31, 25) << 5) | FX(data, 11, 7)), 12)), (int)(SIGN_EXTEND(((FX(data, 31, 25) << 5) | FX(data, 11, 7)), 12)), (unsigned int)(SIGN_EXTEND(FX(data, 24, 20), 5)), (int)(SIGN_EXTEND(FX(data, 24, 20), 5)), riscv_gpr[FX(data, 19, 15)]);
    } break;
    case RISCV_BEXT: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "BEXT", riscv_gpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 24, 20)]);
    } break;
    case RISCV_BEXTI: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, 0x%x(%d)", "BEXTI", riscv_gpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)], (unsigned int)(FX(data, 25, 20)), (int)(FX(data, 25, 20)));
    } break;
    case RISCV_BGE: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, 0x%x(%d)", "BGE", riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 24, 20)], (unsigned int)(SIGN_EXTEND(((BX(data, 31) << 12) | (BX(data, 7) << 11) | (FX(data, 30, 25) << 5) | (FX(data, 11, 8) << 1)), 13)), (int)(SIGN_EXTEND(((BX(data, 31) << 12) | (BX(data, 7) << 11) | (FX(data, 30, 25) << 5) | (FX(data, 11, 8) << 1)), 13)));
    } break;
    case RISCV_BGEU: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, 0x%x(%d)", "BGEU", riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 24, 20)], (unsigned int)(SIGN_EXTEND(((BX(data, 31) << 12) | (BX(data, 7) << 11) | (FX(data, 30, 25) << 5) | (FX(data, 11, 8) << 1)), 13)), (int)(SIGN_EXTEND(((BX(data, 31) << 12) | (BX(data, 7) << 11) | (FX(data, 30, 25) << 5) | (FX(data, 11, 8) << 1)), 13)));
    } break;
    case RISCV_BINV: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "BINV", riscv_gpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 24, 20)]);
    } break;
    case RISCV_BINVI: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, 0x%x(%d)", "BINVI", riscv_gpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)], (unsigned int)(FX(data, 25, 20)), (int)(FX(data, 25, 20)));
    } break;
    case RISCV_BLT: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, 0x%x(%d)", "BLT", riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 24, 20)], (unsigned int)(SIGN_EXTEND(((BX(data, 31) << 12) | (BX(data, 7) << 11) | (FX(data, 30, 25) << 5) | (FX(data, 11, 8) << 1)), 13)), (int)(SIGN_EXTEND(((BX(data, 31) << 12) | (BX(data, 7) << 11) | (FX(data, 30, 25) << 5) | (FX(data, 11, 8) << 1)), 13)));
    } break;
    case RISCV_BLTU: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, 0x%x(%d)", "BLTU", riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 24, 20)], (unsigned int)(SIGN_EXTEND(((BX(data, 31) << 12) | (BX(data, 7) << 11) | (FX(data, 30, 25) << 5) | (FX(data, 11, 8) << 1)), 13)), (int)(SIGN_EXTEND(((BX(data, 31) << 12) | (BX(data, 7) << 11) | (FX(data, 30, 25) << 5) | (FX(data, 11, 8) << 1)), 13)));
    } break;
    case RISCV_BNE: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, 0x%x(%d)", "BNE", riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 24, 20)], (unsigned int)(SIGN_EXTEND(((BX(data, 31) << 12) | (BX(data, 7) << 11) | (FX(data, 30, 25) << 5) | (FX(data, 11, 8) << 1)), 13)), (int)(SIGN_EXTEND(((BX(data, 31) << 12) | (BX(data, 7) << 11) | (FX(data, 30, 25) << 5) | (FX(data, 11, 8) << 1)), 13)));
    } break;
    case RISCV_BNEI: {
        snprintf(buff, sizeof(buff), "%-15s 0x%x(%d), 0x%x(%d), %s", "BNEI", (unsigned int)(SIGN_EXTEND(((FX(data, 31, 25) << 5) | FX(data, 11, 7)), 12)), (int)(SIGN_EXTEND(((FX(data, 31, 25) << 5) | FX(data, 11, 7)), 12)), (unsigned int)(SIGN_EXTEND(FX(data, 24, 20), 5)), (int)(SIGN_EXTEND(FX(data, 24, 20), 5)), riscv_gpr[FX(data, 19, 15)]);
    } break;
    case RISCV_BREV8: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s", "BREV8", riscv_gpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)]);
    } break;
    case RISCV_BSET: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "BSET", riscv_gpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 24, 20)]);
    } break;
    case RISCV_BSETI: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, 0x%x(%d)", "BSETI", riscv_gpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)], (unsigned int)(FX(data, 25, 20)), (int)(FX(data, 25, 20)));
    } break;
    case RISCV_C_EBREAK: {
        snprintf(buff, sizeof(buff), "%-15s ", "C.EBREAK");
    } break;
    case RISCV_C_JALR: {
        snprintf(buff, sizeof(buff), "%-15s %s", "C.JALR", riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_C_ADD: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s", "C.ADD", riscv_gpr[FX(data, 11, 7)], riscv_gpr[FX(data, 6, 2)]);
    } break;
    case RISCV_C_NOP: {
        snprintf(buff, sizeof(buff), "%-15s ", "C.NOP");
    } break;
    case RISCV_C_ADDI: {
        snprintf(buff, sizeof(buff), "%-15s %s, 0x%x(%d)", "C.ADDI", riscv_gpr[FX(data, 11, 7)], (unsigned int)(SIGN_EXTEND(((BX(data, 12) << 5) | FX(data, 6, 2)), 6)), (int)(SIGN_EXTEND(((BX(data, 12) << 5) | FX(data, 6, 2)), 6)));
    } break;
    case RISCV_C_ADDI16SP: {
        snprintf(buff, sizeof(buff), "%-15s 0x%x(%d)", "C.ADDI16SP", (unsigned int)(SIGN_EXTEND(((BX(data, 12) << 9) | (BX(data, 6) << 4) | (BX(data, 5) << 6) | (FX(data, 4, 3) << 7) | (BX(data, 2) << 5)), 10)), (int)(SIGN_EXTEND(((BX(data, 12) << 9) | (BX(data, 6) << 4) | (BX(data, 5) << 6) | (FX(data, 4, 3) << 7) | (BX(data, 2) << 5)), 10)));
    } break;
    case RISCV_C_ADDI4SPN: {
        snprintf(buff, sizeof(buff), "%-15s %s, 0x%x(%d)", "C.ADDI4SPN", riscv_gpr[8 + FX(data, 4, 2)], (unsigned int)((FX(data, 10, 7) << 6) | (FX(data, 12, 11) << 4) | (BX(data, 5) << 3) | (BX(data, 6) << 2)), (int)((FX(data, 10, 7) << 6) | (FX(data, 12, 11) << 4) | (BX(data, 5) << 3) | (BX(data, 6) << 2)));
    } break;
    case RISCV_C_ADDIW: {
        snprintf(buff, sizeof(buff), "%-15s %s, 0x%x(%d)", "C.ADDIW", riscv_gpr[FX(data, 11, 7)], (unsigned int)(SIGN_EXTEND(((BX(data, 12) << 5) | FX(data, 6, 2)), 6)), (int)(SIGN_EXTEND(((BX(data, 12) << 5) | FX(data, 6, 2)), 6)));
    } break;
    case RISCV_C_ADDW: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s", "C.ADDW", riscv_gpr[8 + FX(data, 9, 7)], riscv_gpr[8 + FX(data, 4, 2)]);
    } break;
    case RISCV_C_AND: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s", "C.AND", riscv_gpr[8 + FX(data, 9, 7)], riscv_gpr[8 + FX(data, 4, 2)]);
    } break;
    case RISCV_C_ANDI: {
        snprintf(buff, sizeof(buff), "%-15s %s, 0x%x(%d)", "C.ANDI", riscv_gpr[8 + FX(data, 9, 7)], (unsigned int)(SIGN_EXTEND(((BX(data, 12) << 5) | FX(data, 6, 2)), 6)), (int)(SIGN_EXTEND(((BX(data, 12) << 5) | FX(data, 6, 2)), 6)));
    } break;
    case RISCV_C_BEQZ: {
        snprintf(buff, sizeof(buff), "%-15s %s, 0x%x(%d)", "C.BEQZ", riscv_gpr[8 + FX(data, 9, 7)], (unsigned int)(SIGN_EXTEND(((BX(data, 12) << 8) | (FX(data, 11, 10) << 3) | (FX(data, 6, 5) << 6) | (FX(data, 4, 3) << 1) | (BX(data, 2) << 5)), 9)), (int)(SIGN_EXTEND(((BX(data, 12) << 8) | (FX(data, 11, 10) << 3) | (FX(data, 6, 5) << 6) | (FX(data, 4, 3) << 1) | (BX(data, 2) << 5)), 9)));
    } break;
    case RISCV_C_BNEZ: {
        snprintf(buff, sizeof(buff), "%-15s %s, 0x%x(%d)", "C.BNEZ", riscv_gpr[8 + FX(data, 9, 7)], (unsigned int)(SIGN_EXTEND(((BX(data, 12) << 8) | (FX(data, 11, 10) << 3) | (FX(data, 6, 5) << 6) | (FX(data, 4, 3) << 1) | (BX(data, 2) << 5)), 9)), (int)(SIGN_EXTEND(((BX(data, 12) << 8) | (FX(data, 11, 10) << 3) | (FX(data, 6, 5) << 6) | (FX(data, 4, 3) << 1) | (BX(data, 2) << 5)), 9)));
    } break;
    case RISCV_C_FLD: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, 0x%x(%d)", "C.FLD", riscv_fpr[8 + FX(data, 4, 2)], riscv_gpr[8 + FX(data, 9, 7)], (unsigned int)((FX(data, 6, 5) << 6) | (FX(data, 12, 10) << 3)), (int)((FX(data, 6, 5) << 6) | (FX(data, 12, 10) << 3)));
    } break;
    case RISCV_C_FLDSP: {
        snprintf(buff, sizeof(buff), "%-15s %s, 0x%x(%d)", "C.FLDSP", riscv_fpr[FX(data, 11, 7)], (unsigned int)((FX(data, 4, 2) << 6) | (BX(data, 12) << 5) | (FX(data, 6, 5) << 3)), (int)((FX(data, 4, 2) << 6) | (BX(data, 12) << 5) | (FX(data, 6, 5) << 3)));
    } break;
    case RISCV_C_FLW: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, 0x%x(%d)", "C.FLW", riscv_fpr[8 + FX(data, 4, 2)], riscv_gpr[8 + FX(data, 9, 7)], (unsigned int)((FX(data, 12, 10) << 3) | (BX(data, 6) << 2) | (BX(data, 5) << 6)), (int)((FX(data, 12, 10) << 3) | (BX(data, 6) << 2) | (BX(data, 5) << 6)));
    } break;
    case RISCV_C_FLWSP: {
        snprintf(buff, sizeof(buff), "%-15s %s, 0x%x(%d)", "C.FLWSP", riscv_fpr[FX(data, 11, 7)], (unsigned int)((FX(data, 3, 2) << 6) | (BX(data, 12) << 5) | (FX(data, 6, 4) << 2)), (int)((FX(data, 3, 2) << 6) | (BX(data, 12) << 5) | (FX(data, 6, 4) << 2)));
    } break;
    case RISCV_C_FSD: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, 0x%x(%d)", "C.FSD", riscv_fpr[8 + FX(data, 4, 2)], riscv_gpr[8 + FX(data, 9, 7)], (unsigned int)((FX(data, 6, 5) << 6) | (FX(data, 12, 10) << 3)), (int)((FX(data, 6, 5) << 6) | (FX(data, 12, 10) << 3)));
    } break;
    case RISCV_C_FSDSP: {
        snprintf(buff, sizeof(buff), "%-15s %s, 0x%x(%d)", "C.FSDSP", riscv_gpr[FX(data, 6, 2)], (unsigned int)((FX(data, 9, 7) << 6) | (FX(data, 12, 10) << 3)), (int)((FX(data, 9, 7) << 6) | (FX(data, 12, 10) << 3)));
    } break;
    case RISCV_C_FSW: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, 0x%x(%d)", "C.FSW", riscv_fpr[8 + FX(data, 4, 2)], riscv_gpr[8 + FX(data, 9, 7)], (unsigned int)((FX(data, 12, 10) << 3) | (BX(data, 6) << 2) | (BX(data, 5) << 6)), (int)((FX(data, 12, 10) << 3) | (BX(data, 6) << 2) | (BX(data, 5) << 6)));
    } break;
    case RISCV_C_FSWSP: {
        snprintf(buff, sizeof(buff), "%-15s %s, 0x%x(%d)", "C.FSWSP", riscv_gpr[FX(data, 6, 2)], (unsigned int)((FX(data, 8, 7) << 6) | (FX(data, 12, 9) << 2)), (int)((FX(data, 8, 7) << 6) | (FX(data, 12, 9) << 2)));
    } break;
    case RISCV_C_J: {
        snprintf(buff, sizeof(buff), "%-15s 0x%x(%d)", "C.J", (unsigned int)(SIGN_EXTEND(((BX(data, 12) << 11) | (BX(data, 11) << 4) | (FX(data, 10, 9) << 8) | (BX(data, 8) << 10) | (BX(data, 7) << 6) | (BX(data, 6) << 7) | (FX(data, 5, 3) << 1) | (BX(data, 2) << 5)), 12)), (int)(SIGN_EXTEND(((BX(data, 12) << 11) | (BX(data, 11) << 4) | (FX(data, 10, 9) << 8) | (BX(data, 8) << 10) | (BX(data, 7) << 6) | (BX(data, 6) << 7) | (FX(data, 5, 3) << 1) | (BX(data, 2) << 5)), 12)));
    } break;
    case RISCV_C_JAL: {
        snprintf(buff, sizeof(buff), "%-15s 0x%x(%d)", "C.JAL", (unsigned int)(SIGN_EXTEND(((BX(data, 12) << 11) | (BX(data, 11) << 4) | (FX(data, 10, 9) << 8) | (BX(data, 8) << 10) | (BX(data, 7) << 6) | (BX(data, 6) << 7) | (FX(data, 5, 3) << 1) | (BX(data, 2) << 5)), 12)), (int)(SIGN_EXTEND(((BX(data, 12) << 11) | (BX(data, 11) << 4) | (FX(data, 10, 9) << 8) | (BX(data, 8) << 10) | (BX(data, 7) << 6) | (BX(data, 6) << 7) | (FX(data, 5, 3) << 1) | (BX(data, 2) << 5)), 12)));
    } break;
    case RISCV_C_JR: {
        snprintf(buff, sizeof(buff), "%-15s %s", "C.JR", riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_C_LBU: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, 0x%x(%d)", "C.LBU", riscv_gpr[8 + FX(data, 4, 2)], riscv_gpr[8 + FX(data, 9, 7)], (unsigned int)((BX(data, 5) << 1) | BX(data, 6)), (int)((BX(data, 5) << 1) | BX(data, 6)));
    } break;
    case RISCV_C_LD: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, 0x%x(%d)", "C.LD", riscv_gpr[8 + FX(data, 4, 2)], riscv_gpr[8 + FX(data, 9, 7)], (unsigned int)((FX(data, 6, 5) << 6) | (FX(data, 12, 10) << 3)), (int)((FX(data, 6, 5) << 6) | (FX(data, 12, 10) << 3)));
    } break;
    case RISCV_C_LDSP: {
        snprintf(buff, sizeof(buff), "%-15s %s, 0x%x(%d)", "C.LDSP", riscv_gpr[FX(data, 11, 7)], (unsigned int)((FX(data, 4, 2) << 6) | (BX(data, 12) << 5) | (FX(data, 6, 5) << 3)), (int)((FX(data, 4, 2) << 6) | (BX(data, 12) << 5) | (FX(data, 6, 5) << 3)));
    } break;
    case RISCV_C_LH: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, 0x%x(%d)", "C.LH", riscv_gpr[8 + FX(data, 4, 2)], riscv_gpr[8 + FX(data, 9, 7)], (unsigned int)((BX(data, 5) << 1)), (int)((BX(data, 5) << 1)));
    } break;
    case RISCV_C_LHU: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, 0x%x(%d)", "C.LHU", riscv_gpr[8 + FX(data, 4, 2)], riscv_gpr[8 + FX(data, 9, 7)], (unsigned int)((BX(data, 5) << 1)), (int)((BX(data, 5) << 1)));
    } break;
    case RISCV_C_LI: {
        snprintf(buff, sizeof(buff), "%-15s %s, 0x%x(%d)", "C.LI", riscv_gpr[FX(data, 11, 7)], (unsigned int)(SIGN_EXTEND(((BX(data, 12) << 5) | FX(data, 6, 2)), 6)), (int)(SIGN_EXTEND(((BX(data, 12) << 5) | FX(data, 6, 2)), 6)));
    } break;
    case RISCV_C_MOP_N: {
        char namebuf[32];
        snprintf(namebuf, sizeof(namebuf), "C.MOP.%u", FX(data, 11, 7));
        snprintf(buff, sizeof(buff), "%-15s ", namebuf);
    } break;
    case RISCV_C_LUI: {
        snprintf(buff, sizeof(buff), "%-15s %s, 0x%x(%d)", "C.LUI", riscv_gpr[FX(data, 11, 7)], (unsigned int)(SIGN_EXTEND(((BX(data, 12) << 17) | (FX(data, 6, 2) << 12)), 18)), (int)(SIGN_EXTEND(((BX(data, 12) << 17) | (FX(data, 6, 2) << 12)), 18)));
    } break;
    case RISCV_C_LW: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, 0x%x(%d)", "C.LW", riscv_gpr[8 + FX(data, 4, 2)], riscv_gpr[8 + FX(data, 9, 7)], (unsigned int)((FX(data, 12, 10) << 3) | (BX(data, 6) << 2) | (BX(data, 5) << 6)), (int)((FX(data, 12, 10) << 3) | (BX(data, 6) << 2) | (BX(data, 5) << 6)));
    } break;
    case RISCV_C_LWSP: {
        snprintf(buff, sizeof(buff), "%-15s %s, 0x%x(%d)", "C.LWSP", riscv_gpr[FX(data, 11, 7)], (unsigned int)((FX(data, 3, 2) << 6) | (BX(data, 12) << 5) | (FX(data, 6, 4) << 2)), (int)((FX(data, 3, 2) << 6) | (BX(data, 12) << 5) | (FX(data, 6, 4) << 2)));
    } break;
    case RISCV_C_MUL: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s", "C.MUL", riscv_gpr[8 + FX(data, 9, 7)], riscv_gpr[8 + FX(data, 4, 2)]);
    } break;
    case RISCV_C_MV: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s", "C.MV", riscv_gpr[FX(data, 11, 7)], riscv_gpr[FX(data, 6, 2)]);
    } break;
    case RISCV_C_NOT: {
        snprintf(buff, sizeof(buff), "%-15s %s", "C.NOT", riscv_gpr[8 + FX(data, 9, 7)]);
    } break;
    case RISCV_C_OR: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s", "C.OR", riscv_gpr[8 + FX(data, 9, 7)], riscv_gpr[8 + FX(data, 4, 2)]);
    } break;
    case RISCV_C_SB: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, 0x%x(%d)", "C.SB", riscv_gpr[8 + FX(data, 4, 2)], riscv_gpr[8 + FX(data, 9, 7)], (unsigned int)((BX(data, 5) << 1) | BX(data, 6)), (int)((BX(data, 5) << 1) | BX(data, 6)));
    } break;
    case RISCV_C_SD: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, 0x%x(%d)", "C.SD", riscv_gpr[8 + FX(data, 4, 2)], riscv_gpr[8 + FX(data, 9, 7)], (unsigned int)((FX(data, 6, 5) << 6) | (FX(data, 12, 10) << 3)), (int)((FX(data, 6, 5) << 6) | (FX(data, 12, 10) << 3)));
    } break;
    case RISCV_C_SDSP: {
        snprintf(buff, sizeof(buff), "%-15s %s, 0x%x(%d)", "C.SDSP", riscv_gpr[FX(data, 6, 2)], (unsigned int)((FX(data, 9, 7) << 6) | (FX(data, 12, 10) << 3)), (int)((FX(data, 9, 7) << 6) | (FX(data, 12, 10) << 3)));
    } break;
    case RISCV_C_SEXT_B: {
        snprintf(buff, sizeof(buff), "%-15s %s", "C.SEXT.B", riscv_gpr[8 + FX(data, 9, 7)]);
    } break;
    case RISCV_C_SEXT_H: {
        snprintf(buff, sizeof(buff), "%-15s %s", "C.SEXT.H", riscv_gpr[8 + FX(data, 9, 7)]);
    } break;
    case RISCV_C_SH: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, 0x%x(%d)", "C.SH", riscv_gpr[8 + FX(data, 4, 2)], riscv_gpr[8 + FX(data, 9, 7)], (unsigned int)((BX(data, 5) << 1)), (int)((BX(data, 5) << 1)));
    } break;
    case RISCV_C_SLLI: {
        snprintf(buff, sizeof(buff), "%-15s %s, 0x%x(%d)", "C.SLLI", riscv_gpr[FX(data, 11, 7)], (unsigned int)((BX(data, 12) << 5) | FX(data, 6, 2)), (int)((BX(data, 12) << 5) | FX(data, 6, 2)));
    } break;
    case RISCV_C_SRAI: {
        snprintf(buff, sizeof(buff), "%-15s %s, 0x%x(%d)", "C.SRAI", riscv_gpr[8 + FX(data, 9, 7)], (unsigned int)((BX(data, 12) << 5) | FX(data, 6, 2)), (int)((BX(data, 12) << 5) | FX(data, 6, 2)));
    } break;
    case RISCV_C_SRLI: {
        snprintf(buff, sizeof(buff), "%-15s %s, 0x%x(%d)", "C.SRLI", riscv_gpr[8 + FX(data, 9, 7)], (unsigned int)((BX(data, 12) << 5) | FX(data, 6, 2)), (int)((BX(data, 12) << 5) | FX(data, 6, 2)));
    } break;
    case RISCV_C_SUB: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s", "C.SUB", riscv_gpr[8 + FX(data, 9, 7)], riscv_gpr[8 + FX(data, 4, 2)]);
    } break;
    case RISCV_C_SUBW: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s", "C.SUBW", riscv_gpr[8 + FX(data, 9, 7)], riscv_gpr[8 + FX(data, 4, 2)]);
    } break;
    case RISCV_C_SW: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, 0x%x(%d)", "C.SW", riscv_gpr[8 + FX(data, 4, 2)], riscv_gpr[8 + FX(data, 9, 7)], (unsigned int)((FX(data, 12, 10) << 3) | (BX(data, 6) << 2) | (BX(data, 5) << 6)), (int)((FX(data, 12, 10) << 3) | (BX(data, 6) << 2) | (BX(data, 5) << 6)));
    } break;
    case RISCV_C_SWSP: {
        snprintf(buff, sizeof(buff), "%-15s %s, 0x%x(%d)", "C.SWSP", riscv_gpr[FX(data, 6, 2)], (unsigned int)((FX(data, 8, 7) << 6) | (FX(data, 12, 9) << 2)), (int)((FX(data, 8, 7) << 6) | (FX(data, 12, 9) << 2)));
    } break;
    case RISCV_C_XOR: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s", "C.XOR", riscv_gpr[8 + FX(data, 9, 7)], riscv_gpr[8 + FX(data, 4, 2)]);
    } break;
    case RISCV_C_ZEXT_B: {
        snprintf(buff, sizeof(buff), "%-15s %s", "C.ZEXT.B", riscv_gpr[8 + FX(data, 9, 7)]);
    } break;
    case RISCV_C_ZEXT_H: {
        snprintf(buff, sizeof(buff), "%-15s %s", "C.ZEXT.H", riscv_gpr[8 + FX(data, 9, 7)]);
    } break;
    case RISCV_C_ZEXT_W: {
        snprintf(buff, sizeof(buff), "%-15s %s", "C.ZEXT.W", riscv_gpr[8 + FX(data, 9, 7)]);
    } break;
    case RISCV_CBO_CLEAN: {
        snprintf(buff, sizeof(buff), "%-15s %s", "CBO.CLEAN", riscv_gpr[FX(data, 19, 15)]);
    } break;
    case RISCV_CBO_FLUSH: {
        snprintf(buff, sizeof(buff), "%-15s %s", "CBO.FLUSH", riscv_gpr[FX(data, 19, 15)]);
    } break;
    case RISCV_CBO_INVAL: {
        snprintf(buff, sizeof(buff), "%-15s %s", "CBO.INVAL", riscv_gpr[FX(data, 19, 15)]);
    } break;
    case RISCV_CBO_ZERO: {
        snprintf(buff, sizeof(buff), "%-15s %s", "CBO.ZERO", riscv_gpr[FX(data, 19, 15)]);
    } break;
    case RISCV_CLMUL: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "CLMUL", riscv_gpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 24, 20)]);
    } break;
    case RISCV_CLMULH: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "CLMULH", riscv_gpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 24, 20)]);
    } break;
    case RISCV_CLMULR: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "CLMULR", riscv_gpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 24, 20)]);
    } break;
    case RISCV_CLS: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s", "CLS", riscv_gpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)]);
    } break;
    case RISCV_CLSW: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s", "CLSW", riscv_gpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)]);
    } break;
    case RISCV_CLZ: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s", "CLZ", riscv_gpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)]);
    } break;
    case RISCV_CLZW: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s", "CLZW", riscv_gpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)]);
    } break;
    case RISCV_CPOP: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s", "CPOP", riscv_gpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)]);
    } break;
    case RISCV_CPOPW: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s", "CPOPW", riscv_gpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)]);
    } break;
    case RISCV_CSRRC: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, 0x%x(%d)", "CSRRC", riscv_gpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)], (unsigned int)(FX(data, 31, 20)), (int)(FX(data, 31, 20)));
    } break;
    case RISCV_CSRRCI: {
        snprintf(buff, sizeof(buff), "%-15s %s, 0x%x(%d), 0x%x(%d)", "CSRRCI", riscv_gpr[FX(data, 11, 7)], (unsigned int)(FX(data, 31, 20)), (int)(FX(data, 31, 20)), (unsigned int)(FX(data, 19, 15)), (int)(FX(data, 19, 15)));
    } break;
    case RISCV_CSRRS: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, 0x%x(%d)", "CSRRS", riscv_gpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)], (unsigned int)(FX(data, 31, 20)), (int)(FX(data, 31, 20)));
    } break;
    case RISCV_CSRRSI: {
        snprintf(buff, sizeof(buff), "%-15s %s, 0x%x(%d), 0x%x(%d)", "CSRRSI", riscv_gpr[FX(data, 11, 7)], (unsigned int)(FX(data, 31, 20)), (int)(FX(data, 31, 20)), (unsigned int)(FX(data, 19, 15)), (int)(FX(data, 19, 15)));
    } break;
    case RISCV_CSRRW: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, 0x%x(%d)", "CSRRW", riscv_gpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)], (unsigned int)(FX(data, 31, 20)), (int)(FX(data, 31, 20)));
    } break;
    case RISCV_CSRRWI: {
        snprintf(buff, sizeof(buff), "%-15s %s, 0x%x(%d), 0x%x(%d)", "CSRRWI", riscv_gpr[FX(data, 11, 7)], (unsigned int)(FX(data, 31, 20)), (int)(FX(data, 31, 20)), (unsigned int)(FX(data, 19, 15)), (int)(FX(data, 19, 15)));
    } break;
    case RISCV_CTZ: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s", "CTZ", riscv_gpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)]);
    } break;
    case RISCV_CTZW: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s", "CTZW", riscv_gpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)]);
    } break;
    case RISCV_CZERO_EQZ: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "CZERO.EQZ", riscv_gpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 24, 20)]);
    } break;
    case RISCV_CZERO_NEZ: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "CZERO.NEZ", riscv_gpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 24, 20)]);
    } break;
    case RISCV_DIV: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "DIV", riscv_gpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 24, 20)]);
    } break;
    case RISCV_DIVU: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "DIVU", riscv_gpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 24, 20)]);
    } break;
    case RISCV_DIVUW: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "DIVUW", riscv_gpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 24, 20)]);
    } break;
    case RISCV_DIVW: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "DIVW", riscv_gpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 24, 20)]);
    } break;
    case RISCV_DRET: {
        snprintf(buff, sizeof(buff), "%-15s ", "DRET");
    } break;
    case RISCV_EBREAK: {
        snprintf(buff, sizeof(buff), "%-15s ", "EBREAK");
    } break;
    case RISCV_ECALL: {
        snprintf(buff, sizeof(buff), "%-15s ", "ECALL");
    } break;
    case RISCV_FADD_D: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "FADD.D", riscv_fpr[FX(data, 11, 7)], riscv_fpr[FX(data, 19, 15)], riscv_fpr[FX(data, 24, 20)], riscv_rm[FX(data, 14, 12)]);
    } break;
    case RISCV_FADD_H: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "FADD.H", riscv_fpr[FX(data, 11, 7)], riscv_fpr[FX(data, 19, 15)], riscv_fpr[FX(data, 24, 20)], riscv_rm[FX(data, 14, 12)]);
    } break;
    case RISCV_FADD_Q: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "FADD.Q", riscv_fpr[FX(data, 11, 7)], riscv_fpr[FX(data, 19, 15)], riscv_fpr[FX(data, 24, 20)], riscv_rm[FX(data, 14, 12)]);
    } break;
    case RISCV_FADD_S: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "FADD.S", riscv_fpr[FX(data, 11, 7)], riscv_fpr[FX(data, 19, 15)], riscv_fpr[FX(data, 24, 20)], riscv_rm[FX(data, 14, 12)]);
    } break;
    case RISCV_FCLASS_D: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s", "FCLASS.D", riscv_gpr[FX(data, 11, 7)], riscv_fpr[FX(data, 19, 15)]);
    } break;
    case RISCV_FCLASS_H: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s", "FCLASS.H", riscv_gpr[FX(data, 11, 7)], riscv_fpr[FX(data, 19, 15)]);
    } break;
    case RISCV_FCLASS_Q: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s", "FCLASS.Q", riscv_gpr[FX(data, 11, 7)], riscv_fpr[FX(data, 19, 15)]);
    } break;
    case RISCV_FCLASS_S: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s", "FCLASS.S", riscv_gpr[FX(data, 11, 7)], riscv_fpr[FX(data, 19, 15)]);
    } break;
    case RISCV_FCVT_BF16_S: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "FCVT.BF16.S", riscv_fpr[FX(data, 11, 7)], riscv_fpr[FX(data, 19, 15)], riscv_rm[FX(data, 14, 12)]);
    } break;
    case RISCV_FCVT_D_H: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "FCVT.D.H", riscv_fpr[FX(data, 11, 7)], riscv_fpr[FX(data, 19, 15)], riscv_rm[FX(data, 14, 12)]);
    } break;
    case RISCV_FCVT_D_L: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "FCVT.D.L", riscv_fpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)], riscv_rm[FX(data, 14, 12)]);
    } break;
    case RISCV_FCVT_D_LU: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "FCVT.D.LU", riscv_fpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)], riscv_rm[FX(data, 14, 12)]);
    } break;
    case RISCV_FCVT_D_Q: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "FCVT.D.Q", riscv_fpr[FX(data, 11, 7)], riscv_fpr[FX(data, 19, 15)], riscv_rm[FX(data, 14, 12)]);
    } break;
    case RISCV_FCVT_D_S: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "FCVT.D.S", riscv_fpr[FX(data, 11, 7)], riscv_fpr[FX(data, 19, 15)], riscv_rm[FX(data, 14, 12)]);
    } break;
    case RISCV_FCVT_D_W: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "FCVT.D.W", riscv_fpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)], riscv_rm[FX(data, 14, 12)]);
    } break;
    case RISCV_FCVT_D_WU: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "FCVT.D.WU", riscv_fpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)], riscv_rm[FX(data, 14, 12)]);
    } break;
    case RISCV_FCVT_H_D: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "FCVT.H.D", riscv_fpr[FX(data, 11, 7)], riscv_fpr[FX(data, 19, 15)], riscv_rm[FX(data, 14, 12)]);
    } break;
    case RISCV_FCVT_H_L: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "FCVT.H.L", riscv_gpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)], riscv_rm[FX(data, 14, 12)]);
    } break;
    case RISCV_FCVT_H_LU: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "FCVT.H.LU", riscv_gpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)], riscv_rm[FX(data, 14, 12)]);
    } break;
    case RISCV_FCVT_H_Q: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "FCVT.H.Q", riscv_fpr[FX(data, 11, 7)], riscv_fpr[FX(data, 19, 15)], riscv_rm[FX(data, 14, 12)]);
    } break;
    case RISCV_FCVT_H_S: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "FCVT.H.S", riscv_fpr[FX(data, 11, 7)], riscv_fpr[FX(data, 19, 15)], riscv_rm[FX(data, 14, 12)]);
    } break;
    case RISCV_FCVT_H_W: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "FCVT.H.W", riscv_fpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)], riscv_rm[FX(data, 14, 12)]);
    } break;
    case RISCV_FCVT_H_WU: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "FCVT.H.WU", riscv_fpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)], riscv_rm[FX(data, 14, 12)]);
    } break;
    case RISCV_FCVT_L_D: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "FCVT.L.D", riscv_gpr[FX(data, 11, 7)], riscv_fpr[FX(data, 19, 15)], riscv_rm[FX(data, 14, 12)]);
    } break;
    case RISCV_FCVT_L_H: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "FCVT.L.H", riscv_gpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)], riscv_rm[FX(data, 14, 12)]);
    } break;
    case RISCV_FCVT_L_Q: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "FCVT.L.Q", riscv_gpr[FX(data, 11, 7)], riscv_fpr[FX(data, 19, 15)], riscv_rm[FX(data, 14, 12)]);
    } break;
    case RISCV_FCVT_L_S: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "FCVT.L.S", riscv_gpr[FX(data, 11, 7)], riscv_fpr[FX(data, 19, 15)], riscv_rm[FX(data, 14, 12)]);
    } break;
    case RISCV_FCVT_LU_D: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "FCVT.LU.D", riscv_gpr[FX(data, 11, 7)], riscv_fpr[FX(data, 19, 15)], riscv_rm[FX(data, 14, 12)]);
    } break;
    case RISCV_FCVT_LU_H: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "FCVT.LU.H", riscv_gpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)], riscv_rm[FX(data, 14, 12)]);
    } break;
    case RISCV_FCVT_LU_Q: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "FCVT.LU.Q", riscv_gpr[FX(data, 11, 7)], riscv_fpr[FX(data, 19, 15)], riscv_rm[FX(data, 14, 12)]);
    } break;
    case RISCV_FCVT_LU_S: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "FCVT.LU.S", riscv_gpr[FX(data, 11, 7)], riscv_fpr[FX(data, 19, 15)], riscv_rm[FX(data, 14, 12)]);
    } break;
    case RISCV_FCVT_Q_D: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "FCVT.Q.D", riscv_fpr[FX(data, 11, 7)], riscv_fpr[FX(data, 19, 15)], riscv_rm[FX(data, 14, 12)]);
    } break;
    case RISCV_FCVT_Q_H: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "FCVT.Q.H", riscv_fpr[FX(data, 11, 7)], riscv_fpr[FX(data, 19, 15)], riscv_rm[FX(data, 14, 12)]);
    } break;
    case RISCV_FCVT_Q_L: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "FCVT.Q.L", riscv_fpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)], riscv_rm[FX(data, 14, 12)]);
    } break;
    case RISCV_FCVT_Q_LU: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "FCVT.Q.LU", riscv_fpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)], riscv_rm[FX(data, 14, 12)]);
    } break;
    case RISCV_FCVT_Q_S: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "FCVT.Q.S", riscv_fpr[FX(data, 11, 7)], riscv_fpr[FX(data, 19, 15)], riscv_rm[FX(data, 14, 12)]);
    } break;
    case RISCV_FCVT_Q_W: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "FCVT.Q.W", riscv_fpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)], riscv_rm[FX(data, 14, 12)]);
    } break;
    case RISCV_FCVT_Q_WU: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "FCVT.Q.WU", riscv_fpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)], riscv_rm[FX(data, 14, 12)]);
    } break;
    case RISCV_FCVT_S_BF16: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "FCVT.S.BF16", riscv_fpr[FX(data, 11, 7)], riscv_fpr[FX(data, 19, 15)], riscv_rm[FX(data, 14, 12)]);
    } break;
    case RISCV_FCVT_S_D: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "FCVT.S.D", riscv_fpr[FX(data, 11, 7)], riscv_fpr[FX(data, 19, 15)], riscv_rm[FX(data, 14, 12)]);
    } break;
    case RISCV_FCVT_S_H: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "FCVT.S.H", riscv_fpr[FX(data, 11, 7)], riscv_fpr[FX(data, 19, 15)], riscv_rm[FX(data, 14, 12)]);
    } break;
    case RISCV_FCVT_S_L: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "FCVT.S.L", riscv_fpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)], riscv_rm[FX(data, 14, 12)]);
    } break;
    case RISCV_FCVT_S_LU: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "FCVT.S.LU", riscv_fpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)], riscv_rm[FX(data, 14, 12)]);
    } break;
    case RISCV_FCVT_S_Q: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "FCVT.S.Q", riscv_fpr[FX(data, 11, 7)], riscv_fpr[FX(data, 19, 15)], riscv_rm[FX(data, 14, 12)]);
    } break;
    case RISCV_FCVT_S_W: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "FCVT.S.W", riscv_fpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)], riscv_rm[FX(data, 14, 12)]);
    } break;
    case RISCV_FCVT_S_WU: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "FCVT.S.WU", riscv_fpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)], riscv_rm[FX(data, 14, 12)]);
    } break;
    case RISCV_FCVT_W_D: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "FCVT.W.D", riscv_gpr[FX(data, 11, 7)], riscv_fpr[FX(data, 19, 15)], riscv_rm[FX(data, 14, 12)]);
    } break;
    case RISCV_FCVT_W_H: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "FCVT.W.H", riscv_gpr[FX(data, 11, 7)], riscv_fpr[FX(data, 19, 15)], riscv_rm[FX(data, 14, 12)]);
    } break;
    case RISCV_FCVT_W_Q: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "FCVT.W.Q", riscv_gpr[FX(data, 11, 7)], riscv_fpr[FX(data, 19, 15)], riscv_rm[FX(data, 14, 12)]);
    } break;
    case RISCV_FCVT_W_S: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "FCVT.W.S", riscv_gpr[FX(data, 11, 7)], riscv_fpr[FX(data, 19, 15)], riscv_rm[FX(data, 14, 12)]);
    } break;
    case RISCV_FCVT_WU_D: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "FCVT.WU.D", riscv_gpr[FX(data, 11, 7)], riscv_fpr[FX(data, 19, 15)], riscv_rm[FX(data, 14, 12)]);
    } break;
    case RISCV_FCVT_WU_H: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "FCVT.WU.H", riscv_gpr[FX(data, 11, 7)], riscv_fpr[FX(data, 19, 15)], riscv_rm[FX(data, 14, 12)]);
    } break;
    case RISCV_FCVT_WU_Q: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "FCVT.WU.Q", riscv_gpr[FX(data, 11, 7)], riscv_fpr[FX(data, 19, 15)], riscv_rm[FX(data, 14, 12)]);
    } break;
    case RISCV_FCVT_WU_S: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "FCVT.WU.S", riscv_gpr[FX(data, 11, 7)], riscv_fpr[FX(data, 19, 15)], riscv_rm[FX(data, 14, 12)]);
    } break;
    case RISCV_FCVTMOD_W_D: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s", "FCVTMOD.W.D", riscv_gpr[FX(data, 11, 7)], riscv_fpr[FX(data, 19, 15)]);
    } break;
    case RISCV_FDIV_D: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "FDIV.D", riscv_fpr[FX(data, 11, 7)], riscv_fpr[FX(data, 19, 15)], riscv_fpr[FX(data, 24, 20)], riscv_rm[FX(data, 14, 12)]);
    } break;
    case RISCV_FDIV_H: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "FDIV.H", riscv_fpr[FX(data, 11, 7)], riscv_fpr[FX(data, 19, 15)], riscv_fpr[FX(data, 24, 20)], riscv_rm[FX(data, 14, 12)]);
    } break;
    case RISCV_FDIV_Q: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "FDIV.Q", riscv_fpr[FX(data, 11, 7)], riscv_fpr[FX(data, 19, 15)], riscv_fpr[FX(data, 24, 20)], riscv_rm[FX(data, 14, 12)]);
    } break;
    case RISCV_FDIV_S: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "FDIV.S", riscv_fpr[FX(data, 11, 7)], riscv_fpr[FX(data, 19, 15)], riscv_fpr[FX(data, 24, 20)], riscv_rm[FX(data, 14, 12)]);
    } break;
    case RISCV_FENCE: {
        snprintf(buff, sizeof(buff), "%-15s", "FENCE");
    } break;
    case RISCV_FENCE_I: {
        snprintf(buff, sizeof(buff), "%-15s 0x%x(%d), %s, %s", "FENCE.I", (unsigned int)(SIGN_EXTEND(FX(data, 31, 20), 12)), (int)(SIGN_EXTEND(FX(data, 31, 20), 12)), riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_FEQ_D: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "FEQ.D", riscv_gpr[FX(data, 11, 7)], riscv_fpr[FX(data, 19, 15)], riscv_fpr[FX(data, 24, 20)]);
    } break;
    case RISCV_FEQ_H: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "FEQ.H", riscv_gpr[FX(data, 11, 7)], riscv_fpr[FX(data, 19, 15)], riscv_fpr[FX(data, 24, 20)]);
    } break;
    case RISCV_FEQ_Q: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "FEQ.Q", riscv_gpr[FX(data, 11, 7)], riscv_fpr[FX(data, 19, 15)], riscv_fpr[FX(data, 24, 20)]);
    } break;
    case RISCV_FEQ_S: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "FEQ.S", riscv_gpr[FX(data, 11, 7)], riscv_fpr[FX(data, 19, 15)], riscv_fpr[FX(data, 24, 20)]);
    } break;
    case RISCV_FLD: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, 0x%x(%d)", "FLD", riscv_fpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)], (unsigned int)(SIGN_EXTEND(FX(data, 31, 20), 12)), (int)(SIGN_EXTEND(FX(data, 31, 20), 12)));
    } break;
    case RISCV_FLE_D: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "FLE.D", riscv_gpr[FX(data, 11, 7)], riscv_fpr[FX(data, 19, 15)], riscv_fpr[FX(data, 24, 20)]);
    } break;
    case RISCV_FLE_H: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "FLE.H", riscv_gpr[FX(data, 11, 7)], riscv_fpr[FX(data, 19, 15)], riscv_fpr[FX(data, 24, 20)]);
    } break;
    case RISCV_FLE_Q: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "FLE.Q", riscv_gpr[FX(data, 11, 7)], riscv_fpr[FX(data, 19, 15)], riscv_fpr[FX(data, 24, 20)]);
    } break;
    case RISCV_FLE_S: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "FLE.S", riscv_gpr[FX(data, 11, 7)], riscv_fpr[FX(data, 19, 15)], riscv_fpr[FX(data, 24, 20)]);
    } break;
    case RISCV_FLEQ_D: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "FLEQ.D", riscv_gpr[FX(data, 11, 7)], riscv_fpr[FX(data, 19, 15)], riscv_fpr[FX(data, 24, 20)]);
    } break;
    case RISCV_FLEQ_H: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "FLEQ.H", riscv_gpr[FX(data, 11, 7)], riscv_fpr[FX(data, 19, 15)], riscv_fpr[FX(data, 24, 20)]);
    } break;
    case RISCV_FLEQ_Q: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "FLEQ.Q", riscv_gpr[FX(data, 11, 7)], riscv_fpr[FX(data, 19, 15)], riscv_fpr[FX(data, 24, 20)]);
    } break;
    case RISCV_FLEQ_S: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "FLEQ.S", riscv_gpr[FX(data, 11, 7)], riscv_fpr[FX(data, 19, 15)], riscv_fpr[FX(data, 24, 20)]);
    } break;
    case RISCV_FLH: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, 0x%x(%d)", "FLH", riscv_fpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)], (unsigned int)(SIGN_EXTEND(FX(data, 31, 20), 12)), (int)(SIGN_EXTEND(FX(data, 31, 20), 12)));
    } break;
    case RISCV_FLI_D: {
        snprintf(buff, sizeof(buff), "%-15s %s, 0x%x(%d)", "FLI.D", riscv_fpr[FX(data, 11, 7)], (unsigned int)(FX(data, 19, 15)), (int)(FX(data, 19, 15)));
    } break;
    case RISCV_FLI_H: {
        snprintf(buff, sizeof(buff), "%-15s %s, 0x%x(%d)", "FLI.H", riscv_fpr[FX(data, 11, 7)], (unsigned int)(FX(data, 19, 15)), (int)(FX(data, 19, 15)));
    } break;
    case RISCV_FLI_Q: {
        snprintf(buff, sizeof(buff), "%-15s %s, 0x%x(%d)", "FLI.Q", riscv_fpr[FX(data, 11, 7)], (unsigned int)(FX(data, 19, 15)), (int)(FX(data, 19, 15)));
    } break;
    case RISCV_FLI_S: {
        snprintf(buff, sizeof(buff), "%-15s %s, 0x%x(%d)", "FLI.S", riscv_fpr[FX(data, 11, 7)], (unsigned int)(FX(data, 19, 15)), (int)(FX(data, 19, 15)));
    } break;
    case RISCV_FLQ: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, 0x%x(%d)", "FLQ", riscv_fpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)], (unsigned int)(SIGN_EXTEND(FX(data, 31, 20), 12)), (int)(SIGN_EXTEND(FX(data, 31, 20), 12)));
    } break;
    case RISCV_FLT_D: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "FLT.D", riscv_gpr[FX(data, 11, 7)], riscv_fpr[FX(data, 19, 15)], riscv_fpr[FX(data, 24, 20)]);
    } break;
    case RISCV_FLT_H: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "FLT.H", riscv_gpr[FX(data, 11, 7)], riscv_fpr[FX(data, 19, 15)], riscv_fpr[FX(data, 24, 20)]);
    } break;
    case RISCV_FLT_Q: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "FLT.Q", riscv_gpr[FX(data, 11, 7)], riscv_fpr[FX(data, 19, 15)], riscv_fpr[FX(data, 24, 20)]);
    } break;
    case RISCV_FLT_S: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "FLT.S", riscv_gpr[FX(data, 11, 7)], riscv_fpr[FX(data, 19, 15)], riscv_fpr[FX(data, 24, 20)]);
    } break;
    case RISCV_FLTQ_D: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "FLTQ.D", riscv_gpr[FX(data, 11, 7)], riscv_fpr[FX(data, 19, 15)], riscv_fpr[FX(data, 24, 20)]);
    } break;
    case RISCV_FLTQ_H: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "FLTQ.H", riscv_gpr[FX(data, 11, 7)], riscv_fpr[FX(data, 19, 15)], riscv_fpr[FX(data, 24, 20)]);
    } break;
    case RISCV_FLTQ_Q: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "FLTQ.Q", riscv_gpr[FX(data, 11, 7)], riscv_fpr[FX(data, 19, 15)], riscv_fpr[FX(data, 24, 20)]);
    } break;
    case RISCV_FLTQ_S: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "FLTQ.S", riscv_gpr[FX(data, 11, 7)], riscv_fpr[FX(data, 19, 15)], riscv_fpr[FX(data, 24, 20)]);
    } break;
    case RISCV_FLW: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, 0x%x(%d)", "FLW", riscv_fpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)], (unsigned int)(SIGN_EXTEND(FX(data, 31, 20), 12)), (int)(SIGN_EXTEND(FX(data, 31, 20), 12)));
    } break;
    case RISCV_FMADD_D: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s, %s", "FMADD.D", riscv_fpr[FX(data, 11, 7)], riscv_fpr[FX(data, 19, 15)], riscv_fpr[FX(data, 24, 20)], riscv_fpr[FX(data, 31, 27)], riscv_rm[FX(data, 14, 12)]);
    } break;
    case RISCV_FMADD_H: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s, %s", "FMADD.H", riscv_fpr[FX(data, 11, 7)], riscv_fpr[FX(data, 19, 15)], riscv_fpr[FX(data, 24, 20)], riscv_fpr[FX(data, 31, 27)], riscv_rm[FX(data, 14, 12)]);
    } break;
    case RISCV_FMADD_Q: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s, %s", "FMADD.Q", riscv_fpr[FX(data, 11, 7)], riscv_fpr[FX(data, 19, 15)], riscv_fpr[FX(data, 24, 20)], riscv_fpr[FX(data, 31, 27)], riscv_rm[FX(data, 14, 12)]);
    } break;
    case RISCV_FMADD_S: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s, %s", "FMADD.S", riscv_fpr[FX(data, 11, 7)], riscv_fpr[FX(data, 19, 15)], riscv_fpr[FX(data, 24, 20)], riscv_fpr[FX(data, 31, 27)], riscv_rm[FX(data, 14, 12)]);
    } break;
    case RISCV_FMAX_D: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "FMAX.D", riscv_fpr[FX(data, 11, 7)], riscv_fpr[FX(data, 19, 15)], riscv_fpr[FX(data, 24, 20)]);
    } break;
    case RISCV_FMAX_H: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "FMAX.H", riscv_fpr[FX(data, 11, 7)], riscv_fpr[FX(data, 19, 15)], riscv_fpr[FX(data, 24, 20)]);
    } break;
    case RISCV_FMAX_Q: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "FMAX.Q", riscv_fpr[FX(data, 11, 7)], riscv_fpr[FX(data, 19, 15)], riscv_fpr[FX(data, 24, 20)]);
    } break;
    case RISCV_FMAX_S: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "FMAX.S", riscv_fpr[FX(data, 11, 7)], riscv_fpr[FX(data, 19, 15)], riscv_fpr[FX(data, 24, 20)]);
    } break;
    case RISCV_FMAXM_D: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "FMAXM.D", riscv_fpr[FX(data, 11, 7)], riscv_fpr[FX(data, 19, 15)], riscv_fpr[FX(data, 24, 20)]);
    } break;
    case RISCV_FMAXM_H: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "FMAXM.H", riscv_fpr[FX(data, 11, 7)], riscv_fpr[FX(data, 19, 15)], riscv_fpr[FX(data, 24, 20)]);
    } break;
    case RISCV_FMAXM_Q: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "FMAXM.Q", riscv_fpr[FX(data, 11, 7)], riscv_fpr[FX(data, 19, 15)], riscv_fpr[FX(data, 24, 20)]);
    } break;
    case RISCV_FMAXM_S: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "FMAXM.S", riscv_fpr[FX(data, 11, 7)], riscv_fpr[FX(data, 19, 15)], riscv_fpr[FX(data, 24, 20)]);
    } break;
    case RISCV_FMIN_D: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "FMIN.D", riscv_fpr[FX(data, 11, 7)], riscv_fpr[FX(data, 19, 15)], riscv_fpr[FX(data, 24, 20)]);
    } break;
    case RISCV_FMIN_H: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "FMIN.H", riscv_fpr[FX(data, 11, 7)], riscv_fpr[FX(data, 19, 15)], riscv_fpr[FX(data, 24, 20)]);
    } break;
    case RISCV_FMIN_Q: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "FMIN.Q", riscv_fpr[FX(data, 11, 7)], riscv_fpr[FX(data, 19, 15)], riscv_fpr[FX(data, 24, 20)]);
    } break;
    case RISCV_FMIN_S: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "FMIN.S", riscv_fpr[FX(data, 11, 7)], riscv_fpr[FX(data, 19, 15)], riscv_fpr[FX(data, 24, 20)]);
    } break;
    case RISCV_FMINM_D: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "FMINM.D", riscv_fpr[FX(data, 11, 7)], riscv_fpr[FX(data, 19, 15)], riscv_fpr[FX(data, 24, 20)]);
    } break;
    case RISCV_FMINM_H: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "FMINM.H", riscv_fpr[FX(data, 11, 7)], riscv_fpr[FX(data, 19, 15)], riscv_fpr[FX(data, 24, 20)]);
    } break;
    case RISCV_FMINM_Q: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "FMINM.Q", riscv_fpr[FX(data, 11, 7)], riscv_fpr[FX(data, 19, 15)], riscv_fpr[FX(data, 24, 20)]);
    } break;
    case RISCV_FMINM_S: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "FMINM.S", riscv_fpr[FX(data, 11, 7)], riscv_fpr[FX(data, 19, 15)], riscv_fpr[FX(data, 24, 20)]);
    } break;
    case RISCV_FMSUB_D: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s, %s", "FMSUB.D", riscv_fpr[FX(data, 11, 7)], riscv_fpr[FX(data, 19, 15)], riscv_fpr[FX(data, 24, 20)], riscv_fpr[FX(data, 31, 27)], riscv_rm[FX(data, 14, 12)]);
    } break;
    case RISCV_FMSUB_H: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s, %s", "FMSUB.H", riscv_fpr[FX(data, 11, 7)], riscv_fpr[FX(data, 19, 15)], riscv_fpr[FX(data, 24, 20)], riscv_fpr[FX(data, 31, 27)], riscv_rm[FX(data, 14, 12)]);
    } break;
    case RISCV_FMSUB_Q: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s, %s", "FMSUB.Q", riscv_fpr[FX(data, 11, 7)], riscv_fpr[FX(data, 19, 15)], riscv_fpr[FX(data, 24, 20)], riscv_fpr[FX(data, 31, 27)], riscv_rm[FX(data, 14, 12)]);
    } break;
    case RISCV_FMSUB_S: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s, %s", "FMSUB.S", riscv_fpr[FX(data, 11, 7)], riscv_fpr[FX(data, 19, 15)], riscv_fpr[FX(data, 24, 20)], riscv_fpr[FX(data, 31, 27)], riscv_rm[FX(data, 14, 12)]);
    } break;
    case RISCV_FMUL_D: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "FMUL.D", riscv_fpr[FX(data, 11, 7)], riscv_fpr[FX(data, 19, 15)], riscv_fpr[FX(data, 24, 20)], riscv_rm[FX(data, 14, 12)]);
    } break;
    case RISCV_FMUL_H: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "FMUL.H", riscv_fpr[FX(data, 11, 7)], riscv_fpr[FX(data, 19, 15)], riscv_fpr[FX(data, 24, 20)], riscv_rm[FX(data, 14, 12)]);
    } break;
    case RISCV_FMUL_Q: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "FMUL.Q", riscv_fpr[FX(data, 11, 7)], riscv_fpr[FX(data, 19, 15)], riscv_fpr[FX(data, 24, 20)], riscv_rm[FX(data, 14, 12)]);
    } break;
    case RISCV_FMUL_S: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "FMUL.S", riscv_fpr[FX(data, 11, 7)], riscv_fpr[FX(data, 19, 15)], riscv_fpr[FX(data, 24, 20)], riscv_rm[FX(data, 14, 12)]);
    } break;
    case RISCV_FMV_D_X: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s", "FMV.D.X", riscv_fpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)]);
    } break;
    case RISCV_FMV_H_X: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s", "FMV.H.X", riscv_fpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)]);
    } break;
    case RISCV_FMV_W_X: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s", "FMV.W.X", riscv_fpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)]);
    } break;
    case RISCV_FMV_X_D: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s", "FMV.X.D", riscv_gpr[FX(data, 11, 7)], riscv_fpr[FX(data, 19, 15)]);
    } break;
    case RISCV_FMV_X_H: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s", "FMV.X.H", riscv_gpr[FX(data, 11, 7)], riscv_fpr[FX(data, 19, 15)]);
    } break;
    case RISCV_FMV_X_W: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s", "FMV.X.W", riscv_gpr[FX(data, 11, 7)], riscv_fpr[FX(data, 19, 15)]);
    } break;
    case RISCV_FMVH_X_D: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s", "FMVH.X.D", riscv_gpr[FX(data, 11, 7)], riscv_fpr[FX(data, 19, 15)]);
    } break;
    case RISCV_FMVH_X_Q: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s", "FMVH.X.Q", riscv_gpr[FX(data, 11, 7)], riscv_fpr[FX(data, 19, 15)]);
    } break;
    case RISCV_FMVP_D_X: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "FMVP.D.X", riscv_fpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 24, 20)]);
    } break;
    case RISCV_FMVP_Q_X: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "FMVP.Q.X", riscv_fpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 24, 20)]);
    } break;
    case RISCV_FNMADD_D: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s, %s", "FNMADD.D", riscv_fpr[FX(data, 11, 7)], riscv_fpr[FX(data, 19, 15)], riscv_fpr[FX(data, 24, 20)], riscv_fpr[FX(data, 31, 27)], riscv_rm[FX(data, 14, 12)]);
    } break;
    case RISCV_FNMADD_H: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s, %s", "FNMADD.H", riscv_fpr[FX(data, 11, 7)], riscv_fpr[FX(data, 19, 15)], riscv_fpr[FX(data, 24, 20)], riscv_fpr[FX(data, 31, 27)], riscv_rm[FX(data, 14, 12)]);
    } break;
    case RISCV_FNMADD_Q: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s, %s", "FNMADD.Q", riscv_fpr[FX(data, 11, 7)], riscv_fpr[FX(data, 19, 15)], riscv_fpr[FX(data, 24, 20)], riscv_fpr[FX(data, 31, 27)], riscv_rm[FX(data, 14, 12)]);
    } break;
    case RISCV_FNMADD_S: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s, %s", "FNMADD.S", riscv_fpr[FX(data, 11, 7)], riscv_fpr[FX(data, 19, 15)], riscv_fpr[FX(data, 24, 20)], riscv_fpr[FX(data, 31, 27)], riscv_rm[FX(data, 14, 12)]);
    } break;
    case RISCV_FNMSUB_D: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s, %s", "FNMSUB.D", riscv_fpr[FX(data, 11, 7)], riscv_fpr[FX(data, 19, 15)], riscv_fpr[FX(data, 24, 20)], riscv_fpr[FX(data, 31, 27)], riscv_rm[FX(data, 14, 12)]);
    } break;
    case RISCV_FNMSUB_H: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s, %s", "FNMSUB.H", riscv_fpr[FX(data, 11, 7)], riscv_fpr[FX(data, 19, 15)], riscv_fpr[FX(data, 24, 20)], riscv_fpr[FX(data, 31, 27)], riscv_rm[FX(data, 14, 12)]);
    } break;
    case RISCV_FNMSUB_Q: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s, %s", "FNMSUB.Q", riscv_fpr[FX(data, 11, 7)], riscv_fpr[FX(data, 19, 15)], riscv_fpr[FX(data, 24, 20)], riscv_fpr[FX(data, 31, 27)], riscv_rm[FX(data, 14, 12)]);
    } break;
    case RISCV_FNMSUB_S: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s, %s", "FNMSUB.S", riscv_fpr[FX(data, 11, 7)], riscv_fpr[FX(data, 19, 15)], riscv_fpr[FX(data, 24, 20)], riscv_fpr[FX(data, 31, 27)], riscv_rm[FX(data, 14, 12)]);
    } break;
    case RISCV_FROUND_D: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "FROUND.D", riscv_fpr[FX(data, 11, 7)], riscv_fpr[FX(data, 19, 15)], riscv_rm[FX(data, 14, 12)]);
    } break;
    case RISCV_FROUND_H: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "FROUND.H", riscv_fpr[FX(data, 11, 7)], riscv_fpr[FX(data, 19, 15)], riscv_rm[FX(data, 14, 12)]);
    } break;
    case RISCV_FROUND_Q: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "FROUND.Q", riscv_fpr[FX(data, 11, 7)], riscv_fpr[FX(data, 19, 15)], riscv_rm[FX(data, 14, 12)]);
    } break;
    case RISCV_FROUND_S: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "FROUND.S", riscv_fpr[FX(data, 11, 7)], riscv_fpr[FX(data, 19, 15)], riscv_rm[FX(data, 14, 12)]);
    } break;
    case RISCV_FROUNDNX_D: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "FROUNDNX.D", riscv_fpr[FX(data, 11, 7)], riscv_fpr[FX(data, 19, 15)], riscv_rm[FX(data, 14, 12)]);
    } break;
    case RISCV_FROUNDNX_H: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "FROUNDNX.H", riscv_fpr[FX(data, 11, 7)], riscv_fpr[FX(data, 19, 15)], riscv_rm[FX(data, 14, 12)]);
    } break;
    case RISCV_FROUNDNX_Q: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "FROUNDNX.Q", riscv_fpr[FX(data, 11, 7)], riscv_fpr[FX(data, 19, 15)], riscv_rm[FX(data, 14, 12)]);
    } break;
    case RISCV_FROUNDNX_S: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "FROUNDNX.S", riscv_fpr[FX(data, 11, 7)], riscv_fpr[FX(data, 19, 15)], riscv_rm[FX(data, 14, 12)]);
    } break;
    case RISCV_FSD: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, 0x%x(%d)", "FSD", riscv_fpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], (unsigned int)(SIGN_EXTEND(((FX(data, 31, 25) << 5) | FX(data, 11, 7)), 12)), (int)(SIGN_EXTEND(((FX(data, 31, 25) << 5) | FX(data, 11, 7)), 12)));
    } break;
    case RISCV_FSGNJ_D: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "FSGNJ.D", riscv_fpr[FX(data, 11, 7)], riscv_fpr[FX(data, 19, 15)], riscv_fpr[FX(data, 24, 20)]);
    } break;
    case RISCV_FSGNJ_H: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "FSGNJ.H", riscv_fpr[FX(data, 11, 7)], riscv_fpr[FX(data, 19, 15)], riscv_fpr[FX(data, 24, 20)]);
    } break;
    case RISCV_FSGNJ_Q: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "FSGNJ.Q", riscv_fpr[FX(data, 11, 7)], riscv_fpr[FX(data, 19, 15)], riscv_fpr[FX(data, 24, 20)]);
    } break;
    case RISCV_FSGNJ_S: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "FSGNJ.S", riscv_fpr[FX(data, 11, 7)], riscv_fpr[FX(data, 19, 15)], riscv_fpr[FX(data, 24, 20)]);
    } break;
    case RISCV_FSGNJN_D: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "FSGNJN.D", riscv_fpr[FX(data, 11, 7)], riscv_fpr[FX(data, 19, 15)], riscv_fpr[FX(data, 24, 20)]);
    } break;
    case RISCV_FSGNJN_H: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "FSGNJN.H", riscv_fpr[FX(data, 11, 7)], riscv_fpr[FX(data, 19, 15)], riscv_fpr[FX(data, 24, 20)]);
    } break;
    case RISCV_FSGNJN_Q: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "FSGNJN.Q", riscv_fpr[FX(data, 11, 7)], riscv_fpr[FX(data, 19, 15)], riscv_fpr[FX(data, 24, 20)]);
    } break;
    case RISCV_FSGNJN_S: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "FSGNJN.S", riscv_fpr[FX(data, 11, 7)], riscv_fpr[FX(data, 19, 15)], riscv_fpr[FX(data, 24, 20)]);
    } break;
    case RISCV_FSGNJX_D: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "FSGNJX.D", riscv_fpr[FX(data, 11, 7)], riscv_fpr[FX(data, 19, 15)], riscv_fpr[FX(data, 24, 20)]);
    } break;
    case RISCV_FSGNJX_H: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "FSGNJX.H", riscv_fpr[FX(data, 11, 7)], riscv_fpr[FX(data, 19, 15)], riscv_fpr[FX(data, 24, 20)]);
    } break;
    case RISCV_FSGNJX_Q: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "FSGNJX.Q", riscv_fpr[FX(data, 11, 7)], riscv_fpr[FX(data, 19, 15)], riscv_fpr[FX(data, 24, 20)]);
    } break;
    case RISCV_FSGNJX_S: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "FSGNJX.S", riscv_fpr[FX(data, 11, 7)], riscv_fpr[FX(data, 19, 15)], riscv_fpr[FX(data, 24, 20)]);
    } break;
    case RISCV_FSH: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, 0x%x(%d)", "FSH", riscv_fpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], (unsigned int)(SIGN_EXTEND(((FX(data, 31, 25) << 5) | FX(data, 11, 7)), 12)), (int)(SIGN_EXTEND(((FX(data, 31, 25) << 5) | FX(data, 11, 7)), 12)));
    } break;
    case RISCV_FSQ: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, 0x%x(%d)", "FSQ", riscv_fpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], (unsigned int)(SIGN_EXTEND(((FX(data, 31, 25) << 5) | FX(data, 11, 7)), 12)), (int)(SIGN_EXTEND(((FX(data, 31, 25) << 5) | FX(data, 11, 7)), 12)));
    } break;
    case RISCV_FSQRT_D: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "FSQRT.D", riscv_fpr[FX(data, 11, 7)], riscv_fpr[FX(data, 19, 15)], riscv_rm[FX(data, 14, 12)]);
    } break;
    case RISCV_FSQRT_H: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "FSQRT.H", riscv_fpr[FX(data, 11, 7)], riscv_fpr[FX(data, 19, 15)], riscv_rm[FX(data, 14, 12)]);
    } break;
    case RISCV_FSQRT_Q: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "FSQRT.Q", riscv_fpr[FX(data, 11, 7)], riscv_fpr[FX(data, 19, 15)], riscv_rm[FX(data, 14, 12)]);
    } break;
    case RISCV_FSQRT_S: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "FSQRT.S", riscv_fpr[FX(data, 11, 7)], riscv_fpr[FX(data, 19, 15)], riscv_rm[FX(data, 14, 12)]);
    } break;
    case RISCV_FSUB_D: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "FSUB.D", riscv_fpr[FX(data, 11, 7)], riscv_fpr[FX(data, 19, 15)], riscv_fpr[FX(data, 24, 20)], riscv_rm[FX(data, 14, 12)]);
    } break;
    case RISCV_FSUB_H: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "FSUB.H", riscv_fpr[FX(data, 11, 7)], riscv_fpr[FX(data, 19, 15)], riscv_fpr[FX(data, 24, 20)], riscv_rm[FX(data, 14, 12)]);
    } break;
    case RISCV_FSUB_Q: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "FSUB.Q", riscv_fpr[FX(data, 11, 7)], riscv_fpr[FX(data, 19, 15)], riscv_fpr[FX(data, 24, 20)], riscv_rm[FX(data, 14, 12)]);
    } break;
    case RISCV_FSUB_S: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "FSUB.S", riscv_fpr[FX(data, 11, 7)], riscv_fpr[FX(data, 19, 15)], riscv_fpr[FX(data, 24, 20)], riscv_rm[FX(data, 14, 12)]);
    } break;
    case RISCV_FSW: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, 0x%x(%d)", "FSW", riscv_fpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], (unsigned int)(SIGN_EXTEND(((FX(data, 31, 25) << 5) | FX(data, 11, 7)), 12)), (int)(SIGN_EXTEND(((FX(data, 31, 25) << 5) | FX(data, 11, 7)), 12)));
    } break;
    case RISCV_HFENCE_GVMA: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s", "HFENCE.GVMA", riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 24, 20)]);
    } break;
    case RISCV_HFENCE_VVMA: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s", "HFENCE.VVMA", riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 24, 20)]);
    } break;
    case RISCV_HINVAL_GVMA: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s", "HINVAL.GVMA", riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 24, 20)]);
    } break;
    case RISCV_HINVAL_VVMA: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s", "HINVAL.VVMA", riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 24, 20)]);
    } break;
    case RISCV_HLV_B: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s", "HLV.B", riscv_gpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)]);
    } break;
    case RISCV_HLV_BU: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s", "HLV.BU", riscv_gpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)]);
    } break;
    case RISCV_HLV_D: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s", "HLV.D", riscv_gpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)]);
    } break;
    case RISCV_HLV_H: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s", "HLV.H", riscv_gpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)]);
    } break;
    case RISCV_HLV_HU: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s", "HLV.HU", riscv_gpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)]);
    } break;
    case RISCV_HLV_W: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s", "HLV.W", riscv_gpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)]);
    } break;
    case RISCV_HLV_WU: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s", "HLV.WU", riscv_gpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)]);
    } break;
    case RISCV_HLVX_HU: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s", "HLVX.HU", riscv_gpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)]);
    } break;
    case RISCV_HLVX_WU: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s", "HLVX.WU", riscv_gpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)]);
    } break;
    case RISCV_HSV_B: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s", "HSV.B", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)]);
    } break;
    case RISCV_HSV_D: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s", "HSV.D", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)]);
    } break;
    case RISCV_HSV_H: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s", "HSV.H", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)]);
    } break;
    case RISCV_HSV_W: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s", "HSV.W", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)]);
    } break;
    case RISCV_JAL: {
        snprintf(buff, sizeof(buff), "%-15s %s, 0x%x(%d)", "JAL", riscv_gpr[FX(data, 11, 7)], (unsigned int)(SIGN_EXTEND((BX(data, 31) << 20) | (FX(data, 19, 12) << 12) | (BX(data, 20) << 11) | (FX(data, 30, 21) << 1), 21)), (int)(SIGN_EXTEND((BX(data, 31) << 20) | (FX(data, 19, 12) << 12) | (BX(data, 20) << 11) | (FX(data, 30, 21) << 1), 21)));
    } break;
    case RISCV_JALR: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, 0x%x(%d)", "JALR", riscv_gpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)], (unsigned int)(SIGN_EXTEND(FX(data, 31, 20), 12)), (int)(SIGN_EXTEND(FX(data, 31, 20), 12)));
    } break;
    case RISCV_LB: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, 0x%x(%d)", "LB", riscv_gpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)], (unsigned int)(SIGN_EXTEND(FX(data, 31, 20), 12)), (int)(SIGN_EXTEND(FX(data, 31, 20), 12)));
    } break;
    case RISCV_LB_AQ: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "LB.AQ", riscv_gpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)], riscv_rl[FX(data, 25, 25)]);
    } break;
    case RISCV_LBU: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, 0x%x(%d)", "LBU", riscv_gpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)], (unsigned int)(SIGN_EXTEND(FX(data, 31, 20), 12)), (int)(SIGN_EXTEND(FX(data, 31, 20), 12)));
    } break;
    case RISCV_LD: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, 0x%x(%d)", "LD", riscv_gpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)], (unsigned int)(SIGN_EXTEND(FX(data, 31, 20), 12)), (int)(SIGN_EXTEND(FX(data, 31, 20), 12)));
    } break;
    case RISCV_LD_AQ: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "LD.AQ", riscv_gpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)], riscv_rl[FX(data, 25, 25)]);
    } break;
    case RISCV_LH: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, 0x%x(%d)", "LH", riscv_gpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)], (unsigned int)(SIGN_EXTEND(FX(data, 31, 20), 12)), (int)(SIGN_EXTEND(FX(data, 31, 20), 12)));
    } break;
    case RISCV_LH_AQ: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "LH.AQ", riscv_gpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)], riscv_rl[FX(data, 25, 25)]);
    } break;
    case RISCV_LHU: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, 0x%x(%d)", "LHU", riscv_gpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)], (unsigned int)(SIGN_EXTEND(FX(data, 31, 20), 12)), (int)(SIGN_EXTEND(FX(data, 31, 20), 12)));
    } break;
    case RISCV_LR_B: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "LR.B", riscv_gpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)], riscv_aq[FX(data, 26, 26)], riscv_rl[FX(data, 25, 25)]);
    } break;
    case RISCV_LR_D: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "LR.D", riscv_gpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)], riscv_aq[FX(data, 26, 26)], riscv_rl[FX(data, 25, 25)]);
    } break;
    case RISCV_LR_H: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "LR.H", riscv_gpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)], riscv_aq[FX(data, 26, 26)], riscv_rl[FX(data, 25, 25)]);
    } break;
    case RISCV_LR_W: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "LR.W", riscv_gpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)], riscv_aq[FX(data, 26, 26)], riscv_rl[FX(data, 25, 25)]);
    } break;
    case RISCV_LUI: {
        snprintf(buff, sizeof(buff), "%-15s %s, 0x%x(%d)", "LUI", riscv_gpr[FX(data, 11, 7)], (unsigned int)(FX(data, 31, 12)), (int)(FX(data, 31, 12)));
    } break;
    case RISCV_LW: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, 0x%x(%d)", "LW", riscv_gpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)], (unsigned int)(SIGN_EXTEND(FX(data, 31, 20), 12)), (int)(SIGN_EXTEND(FX(data, 31, 20), 12)));
    } break;
    case RISCV_LW_AQ: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "LW.AQ", riscv_gpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)], riscv_rl[FX(data, 25, 25)]);
    } break;
    case RISCV_LWU: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, 0x%x(%d)", "LWU", riscv_gpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)], (unsigned int)(SIGN_EXTEND(FX(data, 31, 20), 12)), (int)(SIGN_EXTEND(FX(data, 31, 20), 12)));
    } break;
    case RISCV_LXD: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "LXD", riscv_gpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 24, 20)]);
    } break;
    case RISCV_LXH: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "LXH", riscv_gpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 24, 20)]);
    } break;
    case RISCV_LXHU: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "LXHU", riscv_gpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 24, 20)]);
    } break;
    case RISCV_LXSB: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "LXSB", riscv_gpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 24, 20)]);
    } break;
    case RISCV_LXSBU: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "LXSBU", riscv_gpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 24, 20)]);
    } break;
    case RISCV_LXSD: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "LXSD", riscv_gpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 24, 20)]);
    } break;
    case RISCV_LXSH: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "LXSH", riscv_gpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 24, 20)]);
    } break;
    case RISCV_LXSHU: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "LXSHU", riscv_gpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 24, 20)]);
    } break;
    case RISCV_LXSUWB: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "LXSUWB", riscv_gpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 24, 20)]);
    } break;
    case RISCV_LXSUWBU: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "LXSUWBU", riscv_gpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 24, 20)]);
    } break;
    case RISCV_LXSUWD: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "LXSUWD", riscv_gpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 24, 20)]);
    } break;
    case RISCV_LXSUWH: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "LXSUWH", riscv_gpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 24, 20)]);
    } break;
    case RISCV_LXSUWHU: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "LXSUWHU", riscv_gpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 24, 20)]);
    } break;
    case RISCV_LXSUWW: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "LXSUWW", riscv_gpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 24, 20)]);
    } break;
    case RISCV_LXSUWWU: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "LXSUWWU", riscv_gpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 24, 20)]);
    } break;
    case RISCV_LXSW: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "LXSW", riscv_gpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 24, 20)]);
    } break;
    case RISCV_LXSWU: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "LXSWU", riscv_gpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 24, 20)]);
    } break;
    case RISCV_LXW: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "LXW", riscv_gpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 24, 20)]);
    } break;
    case RISCV_LXWU: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "LXWU", riscv_gpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 24, 20)]);
    } break;
    case RISCV_MACC_W00: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "MACC.W00", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_MACC_W01: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "MACC.W01", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_MACC_W11: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "MACC.W11", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_MACCSU_W00: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "MACCSU.W00", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_MACCSU_W11: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "MACCSU.W11", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_MACCU_W00: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "MACCU.W00", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_MACCU_W01: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "MACCU.W01", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_MACCU_W11: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "MACCU.W11", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_MAX: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "MAX", riscv_gpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 24, 20)]);
    } break;
    case RISCV_MAXU: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "MAXU", riscv_gpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 24, 20)]);
    } break;
    case RISCV_MERGE: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "MERGE", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_MIN: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "MIN", riscv_gpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 24, 20)]);
    } break;
    case RISCV_MINU: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "MINU", riscv_gpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 24, 20)]);
    } break;
    case RISCV_MNRET: {
        snprintf(buff, sizeof(buff), "%-15s ", "MNRET");
    } break;
    case RISCV_MOP_R_N: {
        char namebuf[32];
        snprintf(namebuf, sizeof(namebuf), "MOP.R.%u", (FX(data, 30, 30) << 4) | (FX(data, 27, 26) << 2) | FX(data, 21, 20));
        snprintf(buff, sizeof(buff), "%-15s %s, %s", namebuf, riscv_gpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)]);
    } break;
    case RISCV_MOP_RR_N: {
        char namebuf[32];
        snprintf(namebuf, sizeof(namebuf), "MOP.RR.%u", (FX(data, 30, 30) << 2) | FX(data, 27, 26));
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", namebuf, riscv_gpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 24, 20)]);
    } break;
    case RISCV_MQACC_W00: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "MQACC.W00", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_MQACC_W01: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "MQACC.W01", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_MQACC_W11: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "MQACC.W11", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_MQRACC_W00: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "MQRACC.W00", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_MQRACC_W01: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "MQRACC.W01", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_MQRACC_W11: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "MQRACC.W11", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_MQRWACC: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, (%s, %s)", "MQRWACC", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[2 * FX(data, 11, 8)], riscv_gpr[2 * FX(data, 11, 8) + 1]);
    } break;
    case RISCV_MQWACC: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, (%s, %s)", "MQWACC", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[2 * FX(data, 11, 8)], riscv_gpr[2 * FX(data, 11, 8) + 1]);
    } break;
    case RISCV_MRET: {
        snprintf(buff, sizeof(buff), "%-15s ", "MRET");
    } break;
    case RISCV_MUL: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "MUL", riscv_gpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 24, 20)]);
    } break;
    case RISCV_MUL_W00: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "MUL.W00", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_MUL_W01: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "MUL.W01", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_MUL_W11: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "MUL.W11", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_MULH: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "MULH", riscv_gpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 24, 20)]);
    } break;
    case RISCV_MULHSU: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "MULHSU", riscv_gpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 24, 20)]);
    } break;
    case RISCV_MULHU: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "MULHU", riscv_gpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 24, 20)]);
    } break;
    case RISCV_MULSU_W00: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "MULSU.W00", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_MULSU_W11: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "MULSU.W11", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_MULU_W00: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "MULU.W00", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_MULU_W01: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "MULU.W01", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_MULU_W11: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "MULU.W11", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_MULW: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "MULW", riscv_gpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 24, 20)]);
    } break;
    case RISCV_MVM: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "MVM", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_MVMN: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "MVMN", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_NCLIP: {
        snprintf(buff, sizeof(buff), "%-15s %s, (%s, %s), %s", "NCLIP", riscv_gpr[FX(data, 24, 20)], riscv_gpr[2 * FX(data, 19, 16)], riscv_gpr[2 * FX(data, 19, 16) + 1], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_NCLIPI: {
        snprintf(buff, sizeof(buff), "%-15s 0x%x(%d), (%s, %s), %s", "NCLIPI", (unsigned int)(FX(data, 25, 20)), (int)(FX(data, 25, 20)), riscv_gpr[2 * FX(data, 19, 16)], riscv_gpr[2 * FX(data, 19, 16) + 1], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_NCLIPIU: {
        snprintf(buff, sizeof(buff), "%-15s 0x%x(%d), (%s, %s), %s", "NCLIPIU", (unsigned int)(FX(data, 25, 20)), (int)(FX(data, 25, 20)), riscv_gpr[2 * FX(data, 19, 16)], riscv_gpr[2 * FX(data, 19, 16) + 1], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_NCLIPR: {
        snprintf(buff, sizeof(buff), "%-15s %s, (%s, %s), %s", "NCLIPR", riscv_gpr[FX(data, 24, 20)], riscv_gpr[2 * FX(data, 19, 16)], riscv_gpr[2 * FX(data, 19, 16) + 1], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_NCLIPRI: {
        snprintf(buff, sizeof(buff), "%-15s 0x%x(%d), (%s, %s), %s", "NCLIPRI", (unsigned int)(FX(data, 25, 20)), (int)(FX(data, 25, 20)), riscv_gpr[2 * FX(data, 19, 16)], riscv_gpr[2 * FX(data, 19, 16) + 1], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_NCLIPRIU: {
        snprintf(buff, sizeof(buff), "%-15s 0x%x(%d), (%s, %s), %s", "NCLIPRIU", (unsigned int)(FX(data, 25, 20)), (int)(FX(data, 25, 20)), riscv_gpr[2 * FX(data, 19, 16)], riscv_gpr[2 * FX(data, 19, 16) + 1], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_NCLIPRU: {
        snprintf(buff, sizeof(buff), "%-15s %s, (%s, %s), %s", "NCLIPRU", riscv_gpr[FX(data, 24, 20)], riscv_gpr[2 * FX(data, 19, 16)], riscv_gpr[2 * FX(data, 19, 16) + 1], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_NCLIPU: {
        snprintf(buff, sizeof(buff), "%-15s %s, (%s, %s), %s", "NCLIPU", riscv_gpr[FX(data, 24, 20)], riscv_gpr[2 * FX(data, 19, 16)], riscv_gpr[2 * FX(data, 19, 16) + 1], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_NSRA: {
        snprintf(buff, sizeof(buff), "%-15s %s, (%s, %s), %s", "NSRA", riscv_gpr[FX(data, 24, 20)], riscv_gpr[2 * FX(data, 19, 16)], riscv_gpr[2 * FX(data, 19, 16) + 1], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_NSRAI: {
        snprintf(buff, sizeof(buff), "%-15s 0x%x(%d), (%s, %s), %s", "NSRAI", (unsigned int)(FX(data, 25, 20)), (int)(FX(data, 25, 20)), riscv_gpr[2 * FX(data, 19, 16)], riscv_gpr[2 * FX(data, 19, 16) + 1], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_NSRAR: {
        snprintf(buff, sizeof(buff), "%-15s %s, (%s, %s), %s", "NSRAR", riscv_gpr[FX(data, 24, 20)], riscv_gpr[2 * FX(data, 19, 16)], riscv_gpr[2 * FX(data, 19, 16) + 1], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_NSRARI: {
        snprintf(buff, sizeof(buff), "%-15s 0x%x(%d), (%s, %s), %s", "NSRARI", (unsigned int)(FX(data, 25, 20)), (int)(FX(data, 25, 20)), riscv_gpr[2 * FX(data, 19, 16)], riscv_gpr[2 * FX(data, 19, 16) + 1], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_NSRL: {
        snprintf(buff, sizeof(buff), "%-15s %s, (%s, %s), %s", "NSRL", riscv_gpr[FX(data, 24, 20)], riscv_gpr[2 * FX(data, 19, 16)], riscv_gpr[2 * FX(data, 19, 16) + 1], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_NSRLI: {
        snprintf(buff, sizeof(buff), "%-15s 0x%x(%d), (%s, %s), %s", "NSRLI", (unsigned int)(FX(data, 25, 20)), (int)(FX(data, 25, 20)), riscv_gpr[2 * FX(data, 19, 16)], riscv_gpr[2 * FX(data, 19, 16) + 1], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_OR: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "OR", riscv_gpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 24, 20)]);
    } break;
    case RISCV_ORC_B: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s", "ORC.B", riscv_gpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)]);
    } break;
    case RISCV_ORI: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, 0x%x(%d)", "ORI", riscv_gpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)], (unsigned int)(SIGN_EXTEND(FX(data, 31, 20), 12)), (int)(SIGN_EXTEND(FX(data, 31, 20), 12)));
    } break;
    case RISCV_ORN: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "ORN", riscv_gpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 24, 20)]);
    } break;
    case RISCV_PAADD_B: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "PAADD.B", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PAADD_DB: {
        snprintf(buff, sizeof(buff), "%-15s (%s, %s), (%s, %s), (%s, %s)", "PAADD.DB", riscv_gpr[2 * FX(data, 24, 21)], riscv_gpr[2 * FX(data, 24, 21) + 1], riscv_gpr[2 * FX(data, 19, 16)], riscv_gpr[2 * FX(data, 19, 16) + 1], riscv_gpr[2 * FX(data, 11, 8)], riscv_gpr[2 * FX(data, 11, 8) + 1]);
    } break;
    case RISCV_PAADD_DH: {
        snprintf(buff, sizeof(buff), "%-15s (%s, %s), (%s, %s), (%s, %s)", "PAADD.DH", riscv_gpr[2 * FX(data, 24, 21)], riscv_gpr[2 * FX(data, 24, 21) + 1], riscv_gpr[2 * FX(data, 19, 16)], riscv_gpr[2 * FX(data, 19, 16) + 1], riscv_gpr[2 * FX(data, 11, 8)], riscv_gpr[2 * FX(data, 11, 8) + 1]);
    } break;
    case RISCV_PAADD_DW: {
        snprintf(buff, sizeof(buff), "%-15s (%s, %s), (%s, %s), (%s, %s)", "PAADD.DW", riscv_gpr[2 * FX(data, 24, 21)], riscv_gpr[2 * FX(data, 24, 21) + 1], riscv_gpr[2 * FX(data, 19, 16)], riscv_gpr[2 * FX(data, 19, 16) + 1], riscv_gpr[2 * FX(data, 11, 8)], riscv_gpr[2 * FX(data, 11, 8) + 1]);
    } break;
    case RISCV_PAADD_H: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "PAADD.H", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PAADD_W: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "PAADD.W", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PAADDU_B: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "PAADDU.B", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PAADDU_DB: {
        snprintf(buff, sizeof(buff), "%-15s (%s, %s), (%s, %s), (%s, %s)", "PAADDU.DB", riscv_gpr[2 * FX(data, 24, 21)], riscv_gpr[2 * FX(data, 24, 21) + 1], riscv_gpr[2 * FX(data, 19, 16)], riscv_gpr[2 * FX(data, 19, 16) + 1], riscv_gpr[2 * FX(data, 11, 8)], riscv_gpr[2 * FX(data, 11, 8) + 1]);
    } break;
    case RISCV_PAADDU_DH: {
        snprintf(buff, sizeof(buff), "%-15s (%s, %s), (%s, %s), (%s, %s)", "PAADDU.DH", riscv_gpr[2 * FX(data, 24, 21)], riscv_gpr[2 * FX(data, 24, 21) + 1], riscv_gpr[2 * FX(data, 19, 16)], riscv_gpr[2 * FX(data, 19, 16) + 1], riscv_gpr[2 * FX(data, 11, 8)], riscv_gpr[2 * FX(data, 11, 8) + 1]);
    } break;
    case RISCV_PAADDU_DW: {
        snprintf(buff, sizeof(buff), "%-15s (%s, %s), (%s, %s), (%s, %s)", "PAADDU.DW", riscv_gpr[2 * FX(data, 24, 21)], riscv_gpr[2 * FX(data, 24, 21) + 1], riscv_gpr[2 * FX(data, 19, 16)], riscv_gpr[2 * FX(data, 19, 16) + 1], riscv_gpr[2 * FX(data, 11, 8)], riscv_gpr[2 * FX(data, 11, 8) + 1]);
    } break;
    case RISCV_PAADDU_H: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "PAADDU.H", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PAADDU_W: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "PAADDU.W", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PAAS_DHX: {
        snprintf(buff, sizeof(buff), "%-15s (%s, %s), (%s, %s), (%s, %s)", "PAAS.DHX", riscv_gpr[2 * FX(data, 24, 21)], riscv_gpr[2 * FX(data, 24, 21) + 1], riscv_gpr[2 * FX(data, 19, 16)], riscv_gpr[2 * FX(data, 19, 16) + 1], riscv_gpr[2 * FX(data, 11, 8)], riscv_gpr[2 * FX(data, 11, 8) + 1]);
    } break;
    case RISCV_PAAS_HX: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "PAAS.HX", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PAAS_WX: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "PAAS.WX", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PABD_B: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "PABD.B", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PABD_DB: {
        snprintf(buff, sizeof(buff), "%-15s (%s, %s), (%s, %s), (%s, %s)", "PABD.DB", riscv_gpr[2 * FX(data, 24, 21)], riscv_gpr[2 * FX(data, 24, 21) + 1], riscv_gpr[2 * FX(data, 19, 16)], riscv_gpr[2 * FX(data, 19, 16) + 1], riscv_gpr[2 * FX(data, 11, 8)], riscv_gpr[2 * FX(data, 11, 8) + 1]);
    } break;
    case RISCV_PABD_DH: {
        snprintf(buff, sizeof(buff), "%-15s (%s, %s), (%s, %s), (%s, %s)", "PABD.DH", riscv_gpr[2 * FX(data, 24, 21)], riscv_gpr[2 * FX(data, 24, 21) + 1], riscv_gpr[2 * FX(data, 19, 16)], riscv_gpr[2 * FX(data, 19, 16) + 1], riscv_gpr[2 * FX(data, 11, 8)], riscv_gpr[2 * FX(data, 11, 8) + 1]);
    } break;
    case RISCV_PABD_H: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "PABD.H", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PABDSUMAU_B: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "PABDSUMAU.B", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PABDSUMU_B: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "PABDSUMU.B", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PABDU_B: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "PABDU.B", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PABDU_DB: {
        snprintf(buff, sizeof(buff), "%-15s (%s, %s), (%s, %s), (%s, %s)", "PABDU.DB", riscv_gpr[2 * FX(data, 24, 21)], riscv_gpr[2 * FX(data, 24, 21) + 1], riscv_gpr[2 * FX(data, 19, 16)], riscv_gpr[2 * FX(data, 19, 16) + 1], riscv_gpr[2 * FX(data, 11, 8)], riscv_gpr[2 * FX(data, 11, 8) + 1]);
    } break;
    case RISCV_PABDU_DH: {
        snprintf(buff, sizeof(buff), "%-15s (%s, %s), (%s, %s), (%s, %s)", "PABDU.DH", riscv_gpr[2 * FX(data, 24, 21)], riscv_gpr[2 * FX(data, 24, 21) + 1], riscv_gpr[2 * FX(data, 19, 16)], riscv_gpr[2 * FX(data, 19, 16) + 1], riscv_gpr[2 * FX(data, 11, 8)], riscv_gpr[2 * FX(data, 11, 8) + 1]);
    } break;
    case RISCV_PABDU_H: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "PABDU.H", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_ZEXT_H_RV32: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s", "ZEXT.H", riscv_gpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)]);
    } break;
    case RISCV_PACK: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "PACK", riscv_gpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 24, 20)]);
    } break;
    case RISCV_PACKH: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "PACKH", riscv_gpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 24, 20)]);
    } break;
    case RISCV_ZEXT_H: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s", "ZEXT.H", riscv_gpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)]);
    } break;
    case RISCV_PACKW: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "PACKW", riscv_gpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 24, 20)]);
    } break;
    case RISCV_PADD_B: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "PADD.B", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PADD_BS: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "PADD.BS", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PADD_DB: {
        snprintf(buff, sizeof(buff), "%-15s (%s, %s), (%s, %s), (%s, %s)", "PADD.DB", riscv_gpr[2 * FX(data, 24, 21)], riscv_gpr[2 * FX(data, 24, 21) + 1], riscv_gpr[2 * FX(data, 19, 16)], riscv_gpr[2 * FX(data, 19, 16) + 1], riscv_gpr[2 * FX(data, 11, 8)], riscv_gpr[2 * FX(data, 11, 8) + 1]);
    } break;
    case RISCV_PADD_DBS: {
        snprintf(buff, sizeof(buff), "%-15s %s, (%s, %s), (%s, %s)", "PADD.DBS", riscv_gpr[FX(data, 24, 20)], riscv_gpr[2 * FX(data, 19, 16)], riscv_gpr[2 * FX(data, 19, 16) + 1], riscv_gpr[2 * FX(data, 11, 8)], riscv_gpr[2 * FX(data, 11, 8) + 1]);
    } break;
    case RISCV_PADD_DH: {
        snprintf(buff, sizeof(buff), "%-15s (%s, %s), (%s, %s), (%s, %s)", "PADD.DH", riscv_gpr[2 * FX(data, 24, 21)], riscv_gpr[2 * FX(data, 24, 21) + 1], riscv_gpr[2 * FX(data, 19, 16)], riscv_gpr[2 * FX(data, 19, 16) + 1], riscv_gpr[2 * FX(data, 11, 8)], riscv_gpr[2 * FX(data, 11, 8) + 1]);
    } break;
    case RISCV_PADD_DHS: {
        snprintf(buff, sizeof(buff), "%-15s %s, (%s, %s), (%s, %s)", "PADD.DHS", riscv_gpr[FX(data, 24, 20)], riscv_gpr[2 * FX(data, 19, 16)], riscv_gpr[2 * FX(data, 19, 16) + 1], riscv_gpr[2 * FX(data, 11, 8)], riscv_gpr[2 * FX(data, 11, 8) + 1]);
    } break;
    case RISCV_PADD_DW: {
        snprintf(buff, sizeof(buff), "%-15s (%s, %s), (%s, %s), (%s, %s)", "PADD.DW", riscv_gpr[2 * FX(data, 24, 21)], riscv_gpr[2 * FX(data, 24, 21) + 1], riscv_gpr[2 * FX(data, 19, 16)], riscv_gpr[2 * FX(data, 19, 16) + 1], riscv_gpr[2 * FX(data, 11, 8)], riscv_gpr[2 * FX(data, 11, 8) + 1]);
    } break;
    case RISCV_PADD_DWS: {
        snprintf(buff, sizeof(buff), "%-15s %s, (%s, %s), (%s, %s)", "PADD.DWS", riscv_gpr[FX(data, 24, 20)], riscv_gpr[2 * FX(data, 19, 16)], riscv_gpr[2 * FX(data, 19, 16) + 1], riscv_gpr[2 * FX(data, 11, 8)], riscv_gpr[2 * FX(data, 11, 8) + 1]);
    } break;
    case RISCV_PADD_H: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "PADD.H", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PADD_HS: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "PADD.HS", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PADD_W: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "PADD.W", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PADD_WS: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "PADD.WS", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PAS_DHX: {
        snprintf(buff, sizeof(buff), "%-15s (%s, %s), (%s, %s), (%s, %s)", "PAS.DHX", riscv_gpr[2 * FX(data, 24, 21)], riscv_gpr[2 * FX(data, 24, 21) + 1], riscv_gpr[2 * FX(data, 19, 16)], riscv_gpr[2 * FX(data, 19, 16) + 1], riscv_gpr[2 * FX(data, 11, 8)], riscv_gpr[2 * FX(data, 11, 8) + 1]);
    } break;
    case RISCV_PAS_HX: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "PAS.HX", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PAS_WX: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "PAS.WX", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PASA_DHX: {
        snprintf(buff, sizeof(buff), "%-15s (%s, %s), (%s, %s), (%s, %s)", "PASA.DHX", riscv_gpr[2 * FX(data, 24, 21)], riscv_gpr[2 * FX(data, 24, 21) + 1], riscv_gpr[2 * FX(data, 19, 16)], riscv_gpr[2 * FX(data, 19, 16) + 1], riscv_gpr[2 * FX(data, 11, 8)], riscv_gpr[2 * FX(data, 11, 8) + 1]);
    } break;
    case RISCV_PASA_HX: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "PASA.HX", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PASA_WX: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "PASA.WX", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PASUB_B: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "PASUB.B", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PASUB_DB: {
        snprintf(buff, sizeof(buff), "%-15s (%s, %s), (%s, %s), (%s, %s)", "PASUB.DB", riscv_gpr[2 * FX(data, 24, 21)], riscv_gpr[2 * FX(data, 24, 21) + 1], riscv_gpr[2 * FX(data, 19, 16)], riscv_gpr[2 * FX(data, 19, 16) + 1], riscv_gpr[2 * FX(data, 11, 8)], riscv_gpr[2 * FX(data, 11, 8) + 1]);
    } break;
    case RISCV_PASUB_DH: {
        snprintf(buff, sizeof(buff), "%-15s (%s, %s), (%s, %s), (%s, %s)", "PASUB.DH", riscv_gpr[2 * FX(data, 24, 21)], riscv_gpr[2 * FX(data, 24, 21) + 1], riscv_gpr[2 * FX(data, 19, 16)], riscv_gpr[2 * FX(data, 19, 16) + 1], riscv_gpr[2 * FX(data, 11, 8)], riscv_gpr[2 * FX(data, 11, 8) + 1]);
    } break;
    case RISCV_PASUB_DW: {
        snprintf(buff, sizeof(buff), "%-15s (%s, %s), (%s, %s), (%s, %s)", "PASUB.DW", riscv_gpr[2 * FX(data, 24, 21)], riscv_gpr[2 * FX(data, 24, 21) + 1], riscv_gpr[2 * FX(data, 19, 16)], riscv_gpr[2 * FX(data, 19, 16) + 1], riscv_gpr[2 * FX(data, 11, 8)], riscv_gpr[2 * FX(data, 11, 8) + 1]);
    } break;
    case RISCV_PASUB_H: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "PASUB.H", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PASUB_W: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "PASUB.W", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PASUBU_B: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "PASUBU.B", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PASUBU_DB: {
        snprintf(buff, sizeof(buff), "%-15s (%s, %s), (%s, %s), (%s, %s)", "PASUBU.DB", riscv_gpr[2 * FX(data, 24, 21)], riscv_gpr[2 * FX(data, 24, 21) + 1], riscv_gpr[2 * FX(data, 19, 16)], riscv_gpr[2 * FX(data, 19, 16) + 1], riscv_gpr[2 * FX(data, 11, 8)], riscv_gpr[2 * FX(data, 11, 8) + 1]);
    } break;
    case RISCV_PASUBU_DH: {
        snprintf(buff, sizeof(buff), "%-15s (%s, %s), (%s, %s), (%s, %s)", "PASUBU.DH", riscv_gpr[2 * FX(data, 24, 21)], riscv_gpr[2 * FX(data, 24, 21) + 1], riscv_gpr[2 * FX(data, 19, 16)], riscv_gpr[2 * FX(data, 19, 16) + 1], riscv_gpr[2 * FX(data, 11, 8)], riscv_gpr[2 * FX(data, 11, 8) + 1]);
    } break;
    case RISCV_PASUBU_DW: {
        snprintf(buff, sizeof(buff), "%-15s (%s, %s), (%s, %s), (%s, %s)", "PASUBU.DW", riscv_gpr[2 * FX(data, 24, 21)], riscv_gpr[2 * FX(data, 24, 21) + 1], riscv_gpr[2 * FX(data, 19, 16)], riscv_gpr[2 * FX(data, 19, 16) + 1], riscv_gpr[2 * FX(data, 11, 8)], riscv_gpr[2 * FX(data, 11, 8) + 1]);
    } break;
    case RISCV_PASUBU_H: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "PASUBU.H", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PASUBU_W: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "PASUBU.W", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PLI_B: {
        snprintf(buff, sizeof(buff), "%-15s 0x%x(%d), %s", "PLI.B", (unsigned int)(SIGN_EXTEND(FX(data, 23, 16), 8)), (int)(SIGN_EXTEND(FX(data, 23, 16), 8)), riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PLI_DB: {
        snprintf(buff, sizeof(buff), "%-15s 0x%x(%d), (%s, %s)", "PLI.DB", (unsigned int)(SIGN_EXTEND(FX(data, 23, 16), 8)), (int)(SIGN_EXTEND(FX(data, 23, 16), 8)), riscv_gpr[2 * FX(data, 11, 8)], riscv_gpr[2 * FX(data, 11, 8) + 1]);
    } break;
    case RISCV_PLI_DH: {
        snprintf(buff, sizeof(buff), "%-15s 0x%x(%d), (%s, %s)", "PLI.DH", (unsigned int)(SIGN_EXTEND(FX(data, 24, 15), 10)), (int)(SIGN_EXTEND(FX(data, 24, 15), 10)), riscv_gpr[2 * FX(data, 11, 8)], riscv_gpr[2 * FX(data, 11, 8) + 1]);
    } break;
    case RISCV_PLI_H: {
        snprintf(buff, sizeof(buff), "%-15s 0x%x(%d), %s", "PLI.H", (unsigned int)(SIGN_EXTEND(FX(data, 24, 15), 10)), (int)(SIGN_EXTEND(FX(data, 24, 15), 10)), riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PLI_W: {
        snprintf(buff, sizeof(buff), "%-15s 0x%x(%d), %s", "PLI.W", (unsigned int)(SIGN_EXTEND(FX(data, 24, 15), 10)), (int)(SIGN_EXTEND(FX(data, 24, 15), 10)), riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PLUI_DH: {
        snprintf(buff, sizeof(buff), "%-15s 0x%x(%d), (%s, %s)", "PLUI.DH", (unsigned int)(SIGN_EXTEND(FX(data, 24, 15), 10)), (int)(SIGN_EXTEND(FX(data, 24, 15), 10)), riscv_gpr[2 * FX(data, 11, 8)], riscv_gpr[2 * FX(data, 11, 8) + 1]);
    } break;
    case RISCV_PLUI_H: {
        snprintf(buff, sizeof(buff), "%-15s 0x%x(%d), %s", "PLUI.H", (unsigned int)(SIGN_EXTEND(FX(data, 24, 15), 10)), (int)(SIGN_EXTEND(FX(data, 24, 15), 10)), riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PLUI_W: {
        snprintf(buff, sizeof(buff), "%-15s 0x%x(%d), %s", "PLUI.W", (unsigned int)(SIGN_EXTEND(FX(data, 24, 15), 10)), (int)(SIGN_EXTEND(FX(data, 24, 15), 10)), riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PM2ADD_H: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "PM2ADD.H", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PM2ADD_HX: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "PM2ADD.HX", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PM2ADD_W: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "PM2ADD.W", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PM2ADD_WX: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "PM2ADD.WX", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PM2ADDA_H: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "PM2ADDA.H", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PM2ADDA_HX: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "PM2ADDA.HX", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PM2ADDA_W: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "PM2ADDA.W", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PM2ADDA_WX: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "PM2ADDA.WX", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PM2ADDASU_H: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "PM2ADDASU.H", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PM2ADDASU_W: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "PM2ADDASU.W", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PM2ADDAU_H: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "PM2ADDAU.H", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PM2ADDAU_W: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "PM2ADDAU.W", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PM2ADDSU_H: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "PM2ADDSU.H", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PM2ADDSU_W: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "PM2ADDSU.W", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PM2ADDU_H: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "PM2ADDU.H", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PM2ADDU_W: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "PM2ADDU.W", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PM2SADD_H: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "PM2SADD.H", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PM2SADD_HX: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "PM2SADD.HX", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PM2SUB_H: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "PM2SUB.H", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PM2SUB_HX: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "PM2SUB.HX", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PM2SUB_W: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "PM2SUB.W", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PM2SUB_WX: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "PM2SUB.WX", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PM2SUBA_H: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "PM2SUBA.H", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PM2SUBA_HX: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "PM2SUBA.HX", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PM2SUBA_W: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "PM2SUBA.W", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PM2SUBA_WX: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "PM2SUBA.WX", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PM2WADD_H: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, (%s, %s)", "PM2WADD.H", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[2 * FX(data, 11, 8)], riscv_gpr[2 * FX(data, 11, 8) + 1]);
    } break;
    case RISCV_PM2WADD_HX: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, (%s, %s)", "PM2WADD.HX", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[2 * FX(data, 11, 8)], riscv_gpr[2 * FX(data, 11, 8) + 1]);
    } break;
    case RISCV_PM2WADDA_H: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, (%s, %s)", "PM2WADDA.H", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[2 * FX(data, 11, 8)], riscv_gpr[2 * FX(data, 11, 8) + 1]);
    } break;
    case RISCV_PM2WADDA_HX: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, (%s, %s)", "PM2WADDA.HX", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[2 * FX(data, 11, 8)], riscv_gpr[2 * FX(data, 11, 8) + 1]);
    } break;
    case RISCV_PM2WADDASU_H: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, (%s, %s)", "PM2WADDASU.H", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[2 * FX(data, 11, 8)], riscv_gpr[2 * FX(data, 11, 8) + 1]);
    } break;
    case RISCV_PM2WADDAU_H: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, (%s, %s)", "PM2WADDAU.H", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[2 * FX(data, 11, 8)], riscv_gpr[2 * FX(data, 11, 8) + 1]);
    } break;
    case RISCV_PM2WADDSU_H: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, (%s, %s)", "PM2WADDSU.H", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[2 * FX(data, 11, 8)], riscv_gpr[2 * FX(data, 11, 8) + 1]);
    } break;
    case RISCV_PM2WADDU_H: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, (%s, %s)", "PM2WADDU.H", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[2 * FX(data, 11, 8)], riscv_gpr[2 * FX(data, 11, 8) + 1]);
    } break;
    case RISCV_PM2WSUB_H: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, (%s, %s)", "PM2WSUB.H", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[2 * FX(data, 11, 8)], riscv_gpr[2 * FX(data, 11, 8) + 1]);
    } break;
    case RISCV_PM2WSUB_HX: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, (%s, %s)", "PM2WSUB.HX", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[2 * FX(data, 11, 8)], riscv_gpr[2 * FX(data, 11, 8) + 1]);
    } break;
    case RISCV_PM2WSUBA_H: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, (%s, %s)", "PM2WSUBA.H", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[2 * FX(data, 11, 8)], riscv_gpr[2 * FX(data, 11, 8) + 1]);
    } break;
    case RISCV_PM2WSUBA_HX: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, (%s, %s)", "PM2WSUBA.HX", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[2 * FX(data, 11, 8)], riscv_gpr[2 * FX(data, 11, 8) + 1]);
    } break;
    case RISCV_PM4ADD_B: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "PM4ADD.B", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PM4ADD_H: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "PM4ADD.H", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PM4ADDA_B: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "PM4ADDA.B", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PM4ADDA_H: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "PM4ADDA.H", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PM4ADDASU_B: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "PM4ADDASU.B", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PM4ADDASU_H: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "PM4ADDASU.H", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PM4ADDAU_B: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "PM4ADDAU.B", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PM4ADDAU_H: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "PM4ADDAU.H", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PM4ADDSU_B: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "PM4ADDSU.B", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PM4ADDSU_H: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "PM4ADDSU.H", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PM4ADDU_B: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "PM4ADDU.B", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PM4ADDU_H: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "PM4ADDU.H", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PMACC_W_H00: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "PMACC.W.H00", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PMACC_W_H01: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "PMACC.W.H01", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PMACC_W_H11: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "PMACC.W.H11", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PMACCSU_W_H00: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "PMACCSU.W.H00", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PMACCSU_W_H11: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "PMACCSU.W.H11", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PMACCU_W_H00: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "PMACCU.W.H00", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PMACCU_W_H01: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "PMACCU.W.H01", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PMACCU_W_H11: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "PMACCU.W.H11", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PMAX_B: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "PMAX.B", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PMAX_DB: {
        snprintf(buff, sizeof(buff), "%-15s (%s, %s), (%s, %s), (%s, %s)", "PMAX.DB", riscv_gpr[2 * FX(data, 24, 21)], riscv_gpr[2 * FX(data, 24, 21) + 1], riscv_gpr[2 * FX(data, 19, 16)], riscv_gpr[2 * FX(data, 19, 16) + 1], riscv_gpr[2 * FX(data, 11, 8)], riscv_gpr[2 * FX(data, 11, 8) + 1]);
    } break;
    case RISCV_PMAX_DH: {
        snprintf(buff, sizeof(buff), "%-15s (%s, %s), (%s, %s), (%s, %s)", "PMAX.DH", riscv_gpr[2 * FX(data, 24, 21)], riscv_gpr[2 * FX(data, 24, 21) + 1], riscv_gpr[2 * FX(data, 19, 16)], riscv_gpr[2 * FX(data, 19, 16) + 1], riscv_gpr[2 * FX(data, 11, 8)], riscv_gpr[2 * FX(data, 11, 8) + 1]);
    } break;
    case RISCV_PMAX_DW: {
        snprintf(buff, sizeof(buff), "%-15s (%s, %s), (%s, %s), (%s, %s)", "PMAX.DW", riscv_gpr[2 * FX(data, 24, 21)], riscv_gpr[2 * FX(data, 24, 21) + 1], riscv_gpr[2 * FX(data, 19, 16)], riscv_gpr[2 * FX(data, 19, 16) + 1], riscv_gpr[2 * FX(data, 11, 8)], riscv_gpr[2 * FX(data, 11, 8) + 1]);
    } break;
    case RISCV_PMAX_H: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "PMAX.H", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PMAX_W: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "PMAX.W", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PMAXU_B: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "PMAXU.B", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PMAXU_DB: {
        snprintf(buff, sizeof(buff), "%-15s (%s, %s), (%s, %s), (%s, %s)", "PMAXU.DB", riscv_gpr[2 * FX(data, 24, 21)], riscv_gpr[2 * FX(data, 24, 21) + 1], riscv_gpr[2 * FX(data, 19, 16)], riscv_gpr[2 * FX(data, 19, 16) + 1], riscv_gpr[2 * FX(data, 11, 8)], riscv_gpr[2 * FX(data, 11, 8) + 1]);
    } break;
    case RISCV_PMAXU_DH: {
        snprintf(buff, sizeof(buff), "%-15s (%s, %s), (%s, %s), (%s, %s)", "PMAXU.DH", riscv_gpr[2 * FX(data, 24, 21)], riscv_gpr[2 * FX(data, 24, 21) + 1], riscv_gpr[2 * FX(data, 19, 16)], riscv_gpr[2 * FX(data, 19, 16) + 1], riscv_gpr[2 * FX(data, 11, 8)], riscv_gpr[2 * FX(data, 11, 8) + 1]);
    } break;
    case RISCV_PMAXU_DW: {
        snprintf(buff, sizeof(buff), "%-15s (%s, %s), (%s, %s), (%s, %s)", "PMAXU.DW", riscv_gpr[2 * FX(data, 24, 21)], riscv_gpr[2 * FX(data, 24, 21) + 1], riscv_gpr[2 * FX(data, 19, 16)], riscv_gpr[2 * FX(data, 19, 16) + 1], riscv_gpr[2 * FX(data, 11, 8)], riscv_gpr[2 * FX(data, 11, 8) + 1]);
    } break;
    case RISCV_PMAXU_H: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "PMAXU.H", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PMAXU_W: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "PMAXU.W", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PMHACC_H: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "PMHACC.H", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PMHACC_H_B0: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "PMHACC.H.B0", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PMHACC_H_B1: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "PMHACC.H.B1", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PMHACC_W: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "PMHACC.W", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PMHACC_W_H0: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "PMHACC.W.H0", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PMHACC_W_H1: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "PMHACC.W.H1", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PMHACCSU_H: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "PMHACCSU.H", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PMHACCSU_H_B0: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "PMHACCSU.H.B0", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PMHACCSU_H_B1: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "PMHACCSU.H.B1", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PMHACCSU_W: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "PMHACCSU.W", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PMHACCSU_W_H0: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "PMHACCSU.W.H0", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PMHACCSU_W_H1: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "PMHACCSU.W.H1", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PMHACCU_H: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "PMHACCU.H", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PMHACCU_W: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "PMHACCU.W", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PMHRACC_H: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "PMHRACC.H", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PMHRACC_W: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "PMHRACC.W", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PMHRACCSU_H: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "PMHRACCSU.H", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PMHRACCSU_W: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "PMHRACCSU.W", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PMHRACCU_H: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "PMHRACCU.H", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PMHRACCU_W: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "PMHRACCU.W", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PMIN_B: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "PMIN.B", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PMIN_DB: {
        snprintf(buff, sizeof(buff), "%-15s (%s, %s), (%s, %s), (%s, %s)", "PMIN.DB", riscv_gpr[2 * FX(data, 24, 21)], riscv_gpr[2 * FX(data, 24, 21) + 1], riscv_gpr[2 * FX(data, 19, 16)], riscv_gpr[2 * FX(data, 19, 16) + 1], riscv_gpr[2 * FX(data, 11, 8)], riscv_gpr[2 * FX(data, 11, 8) + 1]);
    } break;
    case RISCV_PMIN_DH: {
        snprintf(buff, sizeof(buff), "%-15s (%s, %s), (%s, %s), (%s, %s)", "PMIN.DH", riscv_gpr[2 * FX(data, 24, 21)], riscv_gpr[2 * FX(data, 24, 21) + 1], riscv_gpr[2 * FX(data, 19, 16)], riscv_gpr[2 * FX(data, 19, 16) + 1], riscv_gpr[2 * FX(data, 11, 8)], riscv_gpr[2 * FX(data, 11, 8) + 1]);
    } break;
    case RISCV_PMIN_DW: {
        snprintf(buff, sizeof(buff), "%-15s (%s, %s), (%s, %s), (%s, %s)", "PMIN.DW", riscv_gpr[2 * FX(data, 24, 21)], riscv_gpr[2 * FX(data, 24, 21) + 1], riscv_gpr[2 * FX(data, 19, 16)], riscv_gpr[2 * FX(data, 19, 16) + 1], riscv_gpr[2 * FX(data, 11, 8)], riscv_gpr[2 * FX(data, 11, 8) + 1]);
    } break;
    case RISCV_PMIN_H: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "PMIN.H", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PMIN_W: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "PMIN.W", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PMINU_B: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "PMINU.B", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PMINU_DB: {
        snprintf(buff, sizeof(buff), "%-15s (%s, %s), (%s, %s), (%s, %s)", "PMINU.DB", riscv_gpr[2 * FX(data, 24, 21)], riscv_gpr[2 * FX(data, 24, 21) + 1], riscv_gpr[2 * FX(data, 19, 16)], riscv_gpr[2 * FX(data, 19, 16) + 1], riscv_gpr[2 * FX(data, 11, 8)], riscv_gpr[2 * FX(data, 11, 8) + 1]);
    } break;
    case RISCV_PMINU_DH: {
        snprintf(buff, sizeof(buff), "%-15s (%s, %s), (%s, %s), (%s, %s)", "PMINU.DH", riscv_gpr[2 * FX(data, 24, 21)], riscv_gpr[2 * FX(data, 24, 21) + 1], riscv_gpr[2 * FX(data, 19, 16)], riscv_gpr[2 * FX(data, 19, 16) + 1], riscv_gpr[2 * FX(data, 11, 8)], riscv_gpr[2 * FX(data, 11, 8) + 1]);
    } break;
    case RISCV_PMINU_DW: {
        snprintf(buff, sizeof(buff), "%-15s (%s, %s), (%s, %s), (%s, %s)", "PMINU.DW", riscv_gpr[2 * FX(data, 24, 21)], riscv_gpr[2 * FX(data, 24, 21) + 1], riscv_gpr[2 * FX(data, 19, 16)], riscv_gpr[2 * FX(data, 19, 16) + 1], riscv_gpr[2 * FX(data, 11, 8)], riscv_gpr[2 * FX(data, 11, 8) + 1]);
    } break;
    case RISCV_PMINU_H: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "PMINU.H", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PMINU_W: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "PMINU.W", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PMQ2ADD_H: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "PMQ2ADD.H", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PMQ2ADD_W: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "PMQ2ADD.W", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PMQ2ADDA_H: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "PMQ2ADDA.H", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PMQ2ADDA_W: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "PMQ2ADDA.W", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PMQACC_W_H00: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "PMQACC.W.H00", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PMQACC_W_H01: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "PMQACC.W.H01", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PMQACC_W_H11: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "PMQACC.W.H11", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PMQR2ADD_H: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "PMQR2ADD.H", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PMQR2ADD_W: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "PMQR2ADD.W", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PMQR2ADDA_H: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "PMQR2ADDA.H", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PMQR2ADDA_W: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "PMQR2ADDA.W", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PMQRACC_W_H00: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "PMQRACC.W.H00", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PMQRACC_W_H01: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "PMQRACC.W.H01", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PMQRACC_W_H11: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "PMQRACC.W.H11", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PMQRWACC_H: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, (%s, %s)", "PMQRWACC.H", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[2 * FX(data, 11, 8)], riscv_gpr[2 * FX(data, 11, 8) + 1]);
    } break;
    case RISCV_PMQWACC_H: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, (%s, %s)", "PMQWACC.H", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[2 * FX(data, 11, 8)], riscv_gpr[2 * FX(data, 11, 8) + 1]);
    } break;
    case RISCV_PMSEQ_B: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "PMSEQ.B", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PMSEQ_DB: {
        snprintf(buff, sizeof(buff), "%-15s (%s, %s), (%s, %s), (%s, %s)", "PMSEQ.DB", riscv_gpr[2 * FX(data, 24, 21)], riscv_gpr[2 * FX(data, 24, 21) + 1], riscv_gpr[2 * FX(data, 19, 16)], riscv_gpr[2 * FX(data, 19, 16) + 1], riscv_gpr[2 * FX(data, 11, 8)], riscv_gpr[2 * FX(data, 11, 8) + 1]);
    } break;
    case RISCV_PMSEQ_DH: {
        snprintf(buff, sizeof(buff), "%-15s (%s, %s), (%s, %s), (%s, %s)", "PMSEQ.DH", riscv_gpr[2 * FX(data, 24, 21)], riscv_gpr[2 * FX(data, 24, 21) + 1], riscv_gpr[2 * FX(data, 19, 16)], riscv_gpr[2 * FX(data, 19, 16) + 1], riscv_gpr[2 * FX(data, 11, 8)], riscv_gpr[2 * FX(data, 11, 8) + 1]);
    } break;
    case RISCV_PMSEQ_DW: {
        snprintf(buff, sizeof(buff), "%-15s (%s, %s), (%s, %s), (%s, %s)", "PMSEQ.DW", riscv_gpr[2 * FX(data, 24, 21)], riscv_gpr[2 * FX(data, 24, 21) + 1], riscv_gpr[2 * FX(data, 19, 16)], riscv_gpr[2 * FX(data, 19, 16) + 1], riscv_gpr[2 * FX(data, 11, 8)], riscv_gpr[2 * FX(data, 11, 8) + 1]);
    } break;
    case RISCV_PMSEQ_H: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "PMSEQ.H", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PMSEQ_W: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "PMSEQ.W", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PMSLT_B: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "PMSLT.B", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PMSLT_DB: {
        snprintf(buff, sizeof(buff), "%-15s (%s, %s), (%s, %s), (%s, %s)", "PMSLT.DB", riscv_gpr[2 * FX(data, 24, 21)], riscv_gpr[2 * FX(data, 24, 21) + 1], riscv_gpr[2 * FX(data, 19, 16)], riscv_gpr[2 * FX(data, 19, 16) + 1], riscv_gpr[2 * FX(data, 11, 8)], riscv_gpr[2 * FX(data, 11, 8) + 1]);
    } break;
    case RISCV_PMSLT_DH: {
        snprintf(buff, sizeof(buff), "%-15s (%s, %s), (%s, %s), (%s, %s)", "PMSLT.DH", riscv_gpr[2 * FX(data, 24, 21)], riscv_gpr[2 * FX(data, 24, 21) + 1], riscv_gpr[2 * FX(data, 19, 16)], riscv_gpr[2 * FX(data, 19, 16) + 1], riscv_gpr[2 * FX(data, 11, 8)], riscv_gpr[2 * FX(data, 11, 8) + 1]);
    } break;
    case RISCV_PMSLT_DW: {
        snprintf(buff, sizeof(buff), "%-15s (%s, %s), (%s, %s), (%s, %s)", "PMSLT.DW", riscv_gpr[2 * FX(data, 24, 21)], riscv_gpr[2 * FX(data, 24, 21) + 1], riscv_gpr[2 * FX(data, 19, 16)], riscv_gpr[2 * FX(data, 19, 16) + 1], riscv_gpr[2 * FX(data, 11, 8)], riscv_gpr[2 * FX(data, 11, 8) + 1]);
    } break;
    case RISCV_PMSLT_H: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "PMSLT.H", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PMSLT_W: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "PMSLT.W", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PMSLTU_B: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "PMSLTU.B", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PMSLTU_DB: {
        snprintf(buff, sizeof(buff), "%-15s (%s, %s), (%s, %s), (%s, %s)", "PMSLTU.DB", riscv_gpr[2 * FX(data, 24, 21)], riscv_gpr[2 * FX(data, 24, 21) + 1], riscv_gpr[2 * FX(data, 19, 16)], riscv_gpr[2 * FX(data, 19, 16) + 1], riscv_gpr[2 * FX(data, 11, 8)], riscv_gpr[2 * FX(data, 11, 8) + 1]);
    } break;
    case RISCV_PMSLTU_DH: {
        snprintf(buff, sizeof(buff), "%-15s (%s, %s), (%s, %s), (%s, %s)", "PMSLTU.DH", riscv_gpr[2 * FX(data, 24, 21)], riscv_gpr[2 * FX(data, 24, 21) + 1], riscv_gpr[2 * FX(data, 19, 16)], riscv_gpr[2 * FX(data, 19, 16) + 1], riscv_gpr[2 * FX(data, 11, 8)], riscv_gpr[2 * FX(data, 11, 8) + 1]);
    } break;
    case RISCV_PMSLTU_DW: {
        snprintf(buff, sizeof(buff), "%-15s (%s, %s), (%s, %s), (%s, %s)", "PMSLTU.DW", riscv_gpr[2 * FX(data, 24, 21)], riscv_gpr[2 * FX(data, 24, 21) + 1], riscv_gpr[2 * FX(data, 19, 16)], riscv_gpr[2 * FX(data, 19, 16) + 1], riscv_gpr[2 * FX(data, 11, 8)], riscv_gpr[2 * FX(data, 11, 8) + 1]);
    } break;
    case RISCV_PMSLTU_H: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "PMSLTU.H", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PMSLTU_W: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "PMSLTU.W", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PMUL_H_B00: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "PMUL.H.B00", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PMUL_H_B01: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "PMUL.H.B01", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PMUL_H_B11: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "PMUL.H.B11", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PMUL_W_H00: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "PMUL.W.H00", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PMUL_W_H01: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "PMUL.W.H01", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PMUL_W_H11: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "PMUL.W.H11", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PMULH_H: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "PMULH.H", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PMULH_H_B0: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "PMULH.H.B0", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PMULH_H_B1: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "PMULH.H.B1", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PMULH_W: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "PMULH.W", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PMULH_W_H0: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "PMULH.W.H0", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PMULH_W_H1: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "PMULH.W.H1", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PMULHR_H: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "PMULHR.H", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PMULHR_W: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "PMULHR.W", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PMULHRSU_H: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "PMULHRSU.H", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PMULHRSU_W: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "PMULHRSU.W", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PMULHRU_H: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "PMULHRU.H", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PMULHRU_W: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "PMULHRU.W", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PMULHSU_H: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "PMULHSU.H", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PMULHSU_H_B0: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "PMULHSU.H.B0", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PMULHSU_H_B1: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "PMULHSU.H.B1", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PMULHSU_W: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "PMULHSU.W", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PMULHSU_W_H0: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "PMULHSU.W.H0", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PMULHSU_W_H1: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "PMULHSU.W.H1", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PMULHU_H: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "PMULHU.H", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PMULHU_W: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "PMULHU.W", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PMULQ_H: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "PMULQ.H", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PMULQ_W: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "PMULQ.W", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PMULQR_H: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "PMULQR.H", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PMULQR_W: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "PMULQR.W", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PMULSU_H_B00: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "PMULSU.H.B00", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PMULSU_H_B11: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "PMULSU.H.B11", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PMULSU_W_H00: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "PMULSU.W.H00", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PMULSU_W_H11: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "PMULSU.W.H11", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PMULU_H_B00: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "PMULU.H.B00", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PMULU_H_B01: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "PMULU.H.B01", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PMULU_H_B11: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "PMULU.H.B11", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PMULU_W_H00: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "PMULU.W.H00", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PMULU_W_H01: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "PMULU.W.H01", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PMULU_W_H11: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "PMULU.W.H11", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PNCLIP_BS: {
        snprintf(buff, sizeof(buff), "%-15s %s, (%s, %s), %s", "PNCLIP.BS", riscv_gpr[FX(data, 24, 20)], riscv_gpr[2 * FX(data, 19, 16)], riscv_gpr[2 * FX(data, 19, 16) + 1], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PNCLIP_HS: {
        snprintf(buff, sizeof(buff), "%-15s %s, (%s, %s), %s", "PNCLIP.HS", riscv_gpr[FX(data, 24, 20)], riscv_gpr[2 * FX(data, 19, 16)], riscv_gpr[2 * FX(data, 19, 16) + 1], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PNCLIPI_B: {
        snprintf(buff, sizeof(buff), "%-15s 0x%x(%d), (%s, %s), %s", "PNCLIPI.B", (unsigned int)(FX(data, 23, 20)), (int)(FX(data, 23, 20)), riscv_gpr[2 * FX(data, 19, 16)], riscv_gpr[2 * FX(data, 19, 16) + 1], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PNCLIPI_H: {
        snprintf(buff, sizeof(buff), "%-15s 0x%x(%d), (%s, %s), %s", "PNCLIPI.H", (unsigned int)(FX(data, 24, 20)), (int)(FX(data, 24, 20)), riscv_gpr[2 * FX(data, 19, 16)], riscv_gpr[2 * FX(data, 19, 16) + 1], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PNCLIPIU_B: {
        snprintf(buff, sizeof(buff), "%-15s 0x%x(%d), (%s, %s), %s", "PNCLIPIU.B", (unsigned int)(FX(data, 23, 20)), (int)(FX(data, 23, 20)), riscv_gpr[2 * FX(data, 19, 16)], riscv_gpr[2 * FX(data, 19, 16) + 1], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PNCLIPIU_H: {
        snprintf(buff, sizeof(buff), "%-15s 0x%x(%d), (%s, %s), %s", "PNCLIPIU.H", (unsigned int)(FX(data, 24, 20)), (int)(FX(data, 24, 20)), riscv_gpr[2 * FX(data, 19, 16)], riscv_gpr[2 * FX(data, 19, 16) + 1], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PNCLIPP_B: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "PNCLIPP.B", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PNCLIPP_H: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "PNCLIPP.H", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PNCLIPP_W: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "PNCLIPP.W", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PNCLIPR_BS: {
        snprintf(buff, sizeof(buff), "%-15s %s, (%s, %s), %s", "PNCLIPR.BS", riscv_gpr[FX(data, 24, 20)], riscv_gpr[2 * FX(data, 19, 16)], riscv_gpr[2 * FX(data, 19, 16) + 1], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PNCLIPR_HS: {
        snprintf(buff, sizeof(buff), "%-15s %s, (%s, %s), %s", "PNCLIPR.HS", riscv_gpr[FX(data, 24, 20)], riscv_gpr[2 * FX(data, 19, 16)], riscv_gpr[2 * FX(data, 19, 16) + 1], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PNCLIPRI_B: {
        snprintf(buff, sizeof(buff), "%-15s 0x%x(%d), (%s, %s), %s", "PNCLIPRI.B", (unsigned int)(FX(data, 23, 20)), (int)(FX(data, 23, 20)), riscv_gpr[2 * FX(data, 19, 16)], riscv_gpr[2 * FX(data, 19, 16) + 1], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PNCLIPRI_H: {
        snprintf(buff, sizeof(buff), "%-15s 0x%x(%d), (%s, %s), %s", "PNCLIPRI.H", (unsigned int)(FX(data, 24, 20)), (int)(FX(data, 24, 20)), riscv_gpr[2 * FX(data, 19, 16)], riscv_gpr[2 * FX(data, 19, 16) + 1], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PNCLIPRIU_B: {
        snprintf(buff, sizeof(buff), "%-15s 0x%x(%d), (%s, %s), %s", "PNCLIPRIU.B", (unsigned int)(FX(data, 23, 20)), (int)(FX(data, 23, 20)), riscv_gpr[2 * FX(data, 19, 16)], riscv_gpr[2 * FX(data, 19, 16) + 1], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PNCLIPRIU_H: {
        snprintf(buff, sizeof(buff), "%-15s 0x%x(%d), (%s, %s), %s", "PNCLIPRIU.H", (unsigned int)(FX(data, 24, 20)), (int)(FX(data, 24, 20)), riscv_gpr[2 * FX(data, 19, 16)], riscv_gpr[2 * FX(data, 19, 16) + 1], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PNCLIPRU_BS: {
        snprintf(buff, sizeof(buff), "%-15s %s, (%s, %s), %s", "PNCLIPRU.BS", riscv_gpr[FX(data, 24, 20)], riscv_gpr[2 * FX(data, 19, 16)], riscv_gpr[2 * FX(data, 19, 16) + 1], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PNCLIPRU_HS: {
        snprintf(buff, sizeof(buff), "%-15s %s, (%s, %s), %s", "PNCLIPRU.HS", riscv_gpr[FX(data, 24, 20)], riscv_gpr[2 * FX(data, 19, 16)], riscv_gpr[2 * FX(data, 19, 16) + 1], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PNCLIPU_BS: {
        snprintf(buff, sizeof(buff), "%-15s %s, (%s, %s), %s", "PNCLIPU.BS", riscv_gpr[FX(data, 24, 20)], riscv_gpr[2 * FX(data, 19, 16)], riscv_gpr[2 * FX(data, 19, 16) + 1], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PNCLIPU_HS: {
        snprintf(buff, sizeof(buff), "%-15s %s, (%s, %s), %s", "PNCLIPU.HS", riscv_gpr[FX(data, 24, 20)], riscv_gpr[2 * FX(data, 19, 16)], riscv_gpr[2 * FX(data, 19, 16) + 1], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PNCLIPUP_B: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "PNCLIPUP.B", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PNCLIPUP_H: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "PNCLIPUP.H", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PNCLIPUP_W: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "PNCLIPUP.W", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PNSRA_BS: {
        snprintf(buff, sizeof(buff), "%-15s %s, (%s, %s), %s", "PNSRA.BS", riscv_gpr[FX(data, 24, 20)], riscv_gpr[2 * FX(data, 19, 16)], riscv_gpr[2 * FX(data, 19, 16) + 1], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PNSRA_HS: {
        snprintf(buff, sizeof(buff), "%-15s %s, (%s, %s), %s", "PNSRA.HS", riscv_gpr[FX(data, 24, 20)], riscv_gpr[2 * FX(data, 19, 16)], riscv_gpr[2 * FX(data, 19, 16) + 1], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PNSRAI_B: {
        snprintf(buff, sizeof(buff), "%-15s 0x%x(%d), (%s, %s), %s", "PNSRAI.B", (unsigned int)(FX(data, 23, 20)), (int)(FX(data, 23, 20)), riscv_gpr[2 * FX(data, 19, 16)], riscv_gpr[2 * FX(data, 19, 16) + 1], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PNSRAI_H: {
        snprintf(buff, sizeof(buff), "%-15s 0x%x(%d), (%s, %s), %s", "PNSRAI.H", (unsigned int)(FX(data, 24, 20)), (int)(FX(data, 24, 20)), riscv_gpr[2 * FX(data, 19, 16)], riscv_gpr[2 * FX(data, 19, 16) + 1], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PNSRAR_BS: {
        snprintf(buff, sizeof(buff), "%-15s %s, (%s, %s), %s", "PNSRAR.BS", riscv_gpr[FX(data, 24, 20)], riscv_gpr[2 * FX(data, 19, 16)], riscv_gpr[2 * FX(data, 19, 16) + 1], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PNSRAR_HS: {
        snprintf(buff, sizeof(buff), "%-15s %s, (%s, %s), %s", "PNSRAR.HS", riscv_gpr[FX(data, 24, 20)], riscv_gpr[2 * FX(data, 19, 16)], riscv_gpr[2 * FX(data, 19, 16) + 1], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PNSRARI_B: {
        snprintf(buff, sizeof(buff), "%-15s 0x%x(%d), (%s, %s), %s", "PNSRARI.B", (unsigned int)(FX(data, 23, 20)), (int)(FX(data, 23, 20)), riscv_gpr[2 * FX(data, 19, 16)], riscv_gpr[2 * FX(data, 19, 16) + 1], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PNSRARI_H: {
        snprintf(buff, sizeof(buff), "%-15s 0x%x(%d), (%s, %s), %s", "PNSRARI.H", (unsigned int)(FX(data, 24, 20)), (int)(FX(data, 24, 20)), riscv_gpr[2 * FX(data, 19, 16)], riscv_gpr[2 * FX(data, 19, 16) + 1], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PNSRL_BS: {
        snprintf(buff, sizeof(buff), "%-15s %s, (%s, %s), %s", "PNSRL.BS", riscv_gpr[FX(data, 24, 20)], riscv_gpr[2 * FX(data, 19, 16)], riscv_gpr[2 * FX(data, 19, 16) + 1], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PNSRL_HS: {
        snprintf(buff, sizeof(buff), "%-15s %s, (%s, %s), %s", "PNSRL.HS", riscv_gpr[FX(data, 24, 20)], riscv_gpr[2 * FX(data, 19, 16)], riscv_gpr[2 * FX(data, 19, 16) + 1], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PNSRLI_B: {
        snprintf(buff, sizeof(buff), "%-15s 0x%x(%d), (%s, %s), %s", "PNSRLI.B", (unsigned int)(FX(data, 23, 20)), (int)(FX(data, 23, 20)), riscv_gpr[2 * FX(data, 19, 16)], riscv_gpr[2 * FX(data, 19, 16) + 1], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PNSRLI_H: {
        snprintf(buff, sizeof(buff), "%-15s 0x%x(%d), (%s, %s), %s", "PNSRLI.H", (unsigned int)(FX(data, 24, 20)), (int)(FX(data, 24, 20)), riscv_gpr[2 * FX(data, 19, 16)], riscv_gpr[2 * FX(data, 19, 16) + 1], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PPAIRE_B: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "PPAIRE.B", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PPAIRE_DB: {
        snprintf(buff, sizeof(buff), "%-15s (%s, %s), (%s, %s), (%s, %s)", "PPAIRE.DB", riscv_gpr[2 * FX(data, 24, 21)], riscv_gpr[2 * FX(data, 24, 21) + 1], riscv_gpr[2 * FX(data, 19, 16)], riscv_gpr[2 * FX(data, 19, 16) + 1], riscv_gpr[2 * FX(data, 11, 8)], riscv_gpr[2 * FX(data, 11, 8) + 1]);
    } break;
    case RISCV_PPAIRE_DH: {
        snprintf(buff, sizeof(buff), "%-15s (%s, %s), (%s, %s), (%s, %s)", "PPAIRE.DH", riscv_gpr[2 * FX(data, 24, 21)], riscv_gpr[2 * FX(data, 24, 21) + 1], riscv_gpr[2 * FX(data, 19, 16)], riscv_gpr[2 * FX(data, 19, 16) + 1], riscv_gpr[2 * FX(data, 11, 8)], riscv_gpr[2 * FX(data, 11, 8) + 1]);
    } break;
    case RISCV_PPAIRE_H: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "PPAIRE.H", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PPAIREO_B: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "PPAIREO.B", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PPAIREO_DB: {
        snprintf(buff, sizeof(buff), "%-15s (%s, %s), (%s, %s), (%s, %s)", "PPAIREO.DB", riscv_gpr[2 * FX(data, 24, 21)], riscv_gpr[2 * FX(data, 24, 21) + 1], riscv_gpr[2 * FX(data, 19, 16)], riscv_gpr[2 * FX(data, 19, 16) + 1], riscv_gpr[2 * FX(data, 11, 8)], riscv_gpr[2 * FX(data, 11, 8) + 1]);
    } break;
    case RISCV_PPAIREO_DH: {
        snprintf(buff, sizeof(buff), "%-15s (%s, %s), (%s, %s), (%s, %s)", "PPAIREO.DH", riscv_gpr[2 * FX(data, 24, 21)], riscv_gpr[2 * FX(data, 24, 21) + 1], riscv_gpr[2 * FX(data, 19, 16)], riscv_gpr[2 * FX(data, 19, 16) + 1], riscv_gpr[2 * FX(data, 11, 8)], riscv_gpr[2 * FX(data, 11, 8) + 1]);
    } break;
    case RISCV_PPAIREO_H: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "PPAIREO.H", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PPAIREO_W: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "PPAIREO.W", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PPAIRO_B: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "PPAIRO.B", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PPAIRO_DB: {
        snprintf(buff, sizeof(buff), "%-15s (%s, %s), (%s, %s), (%s, %s)", "PPAIRO.DB", riscv_gpr[2 * FX(data, 24, 21)], riscv_gpr[2 * FX(data, 24, 21) + 1], riscv_gpr[2 * FX(data, 19, 16)], riscv_gpr[2 * FX(data, 19, 16) + 1], riscv_gpr[2 * FX(data, 11, 8)], riscv_gpr[2 * FX(data, 11, 8) + 1]);
    } break;
    case RISCV_PPAIRO_DH: {
        snprintf(buff, sizeof(buff), "%-15s (%s, %s), (%s, %s), (%s, %s)", "PPAIRO.DH", riscv_gpr[2 * FX(data, 24, 21)], riscv_gpr[2 * FX(data, 24, 21) + 1], riscv_gpr[2 * FX(data, 19, 16)], riscv_gpr[2 * FX(data, 19, 16) + 1], riscv_gpr[2 * FX(data, 11, 8)], riscv_gpr[2 * FX(data, 11, 8) + 1]);
    } break;
    case RISCV_PPAIRO_H: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "PPAIRO.H", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PPAIRO_W: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "PPAIRO.W", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PPAIROE_B: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "PPAIROE.B", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PPAIROE_DB: {
        snprintf(buff, sizeof(buff), "%-15s (%s, %s), (%s, %s), (%s, %s)", "PPAIROE.DB", riscv_gpr[2 * FX(data, 24, 21)], riscv_gpr[2 * FX(data, 24, 21) + 1], riscv_gpr[2 * FX(data, 19, 16)], riscv_gpr[2 * FX(data, 19, 16) + 1], riscv_gpr[2 * FX(data, 11, 8)], riscv_gpr[2 * FX(data, 11, 8) + 1]);
    } break;
    case RISCV_PPAIROE_DH: {
        snprintf(buff, sizeof(buff), "%-15s (%s, %s), (%s, %s), (%s, %s)", "PPAIROE.DH", riscv_gpr[2 * FX(data, 24, 21)], riscv_gpr[2 * FX(data, 24, 21) + 1], riscv_gpr[2 * FX(data, 19, 16)], riscv_gpr[2 * FX(data, 19, 16) + 1], riscv_gpr[2 * FX(data, 11, 8)], riscv_gpr[2 * FX(data, 11, 8) + 1]);
    } break;
    case RISCV_PPAIROE_H: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "PPAIROE.H", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PPAIROE_W: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "PPAIROE.W", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PREDSUM_BS: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "PREDSUM.BS", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PREDSUM_DBS: {
        snprintf(buff, sizeof(buff), "%-15s %s, (%s, %s), %s", "PREDSUM.DBS", riscv_gpr[FX(data, 24, 20)], riscv_gpr[2 * FX(data, 19, 16)], riscv_gpr[2 * FX(data, 19, 16) + 1], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PREDSUM_DHS: {
        snprintf(buff, sizeof(buff), "%-15s %s, (%s, %s), %s", "PREDSUM.DHS", riscv_gpr[FX(data, 24, 20)], riscv_gpr[2 * FX(data, 19, 16)], riscv_gpr[2 * FX(data, 19, 16) + 1], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PREDSUM_HS: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "PREDSUM.HS", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PREDSUM_WS: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "PREDSUM.WS", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PREDSUMU_BS: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "PREDSUMU.BS", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PREDSUMU_DBS: {
        snprintf(buff, sizeof(buff), "%-15s %s, (%s, %s), %s", "PREDSUMU.DBS", riscv_gpr[FX(data, 24, 20)], riscv_gpr[2 * FX(data, 19, 16)], riscv_gpr[2 * FX(data, 19, 16) + 1], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PREDSUMU_DHS: {
        snprintf(buff, sizeof(buff), "%-15s %s, (%s, %s), %s", "PREDSUMU.DHS", riscv_gpr[FX(data, 24, 20)], riscv_gpr[2 * FX(data, 19, 16)], riscv_gpr[2 * FX(data, 19, 16) + 1], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PREDSUMU_HS: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "PREDSUMU.HS", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PREDSUMU_WS: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "PREDSUMU.WS", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PSA_DHX: {
        snprintf(buff, sizeof(buff), "%-15s (%s, %s), (%s, %s), (%s, %s)", "PSA.DHX", riscv_gpr[2 * FX(data, 24, 21)], riscv_gpr[2 * FX(data, 24, 21) + 1], riscv_gpr[2 * FX(data, 19, 16)], riscv_gpr[2 * FX(data, 19, 16) + 1], riscv_gpr[2 * FX(data, 11, 8)], riscv_gpr[2 * FX(data, 11, 8) + 1]);
    } break;
    case RISCV_PSA_HX: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "PSA.HX", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PSA_WX: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "PSA.WX", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PSABS_B: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s", "PSABS.B", riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PSABS_DB: {
        snprintf(buff, sizeof(buff), "%-15s (%s, %s), (%s, %s)", "PSABS.DB", riscv_gpr[2 * FX(data, 19, 16)], riscv_gpr[2 * FX(data, 19, 16) + 1], riscv_gpr[2 * FX(data, 11, 8)], riscv_gpr[2 * FX(data, 11, 8) + 1]);
    } break;
    case RISCV_PSABS_DH: {
        snprintf(buff, sizeof(buff), "%-15s (%s, %s), (%s, %s)", "PSABS.DH", riscv_gpr[2 * FX(data, 19, 16)], riscv_gpr[2 * FX(data, 19, 16) + 1], riscv_gpr[2 * FX(data, 11, 8)], riscv_gpr[2 * FX(data, 11, 8) + 1]);
    } break;
    case RISCV_PSABS_H: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s", "PSABS.H", riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PSADD_B: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "PSADD.B", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PSADD_DB: {
        snprintf(buff, sizeof(buff), "%-15s (%s, %s), (%s, %s), (%s, %s)", "PSADD.DB", riscv_gpr[2 * FX(data, 24, 21)], riscv_gpr[2 * FX(data, 24, 21) + 1], riscv_gpr[2 * FX(data, 19, 16)], riscv_gpr[2 * FX(data, 19, 16) + 1], riscv_gpr[2 * FX(data, 11, 8)], riscv_gpr[2 * FX(data, 11, 8) + 1]);
    } break;
    case RISCV_PSADD_DH: {
        snprintf(buff, sizeof(buff), "%-15s (%s, %s), (%s, %s), (%s, %s)", "PSADD.DH", riscv_gpr[2 * FX(data, 24, 21)], riscv_gpr[2 * FX(data, 24, 21) + 1], riscv_gpr[2 * FX(data, 19, 16)], riscv_gpr[2 * FX(data, 19, 16) + 1], riscv_gpr[2 * FX(data, 11, 8)], riscv_gpr[2 * FX(data, 11, 8) + 1]);
    } break;
    case RISCV_PSADD_DW: {
        snprintf(buff, sizeof(buff), "%-15s (%s, %s), (%s, %s), (%s, %s)", "PSADD.DW", riscv_gpr[2 * FX(data, 24, 21)], riscv_gpr[2 * FX(data, 24, 21) + 1], riscv_gpr[2 * FX(data, 19, 16)], riscv_gpr[2 * FX(data, 19, 16) + 1], riscv_gpr[2 * FX(data, 11, 8)], riscv_gpr[2 * FX(data, 11, 8) + 1]);
    } break;
    case RISCV_PSADD_H: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "PSADD.H", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PSADD_W: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "PSADD.W", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PSADDU_B: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "PSADDU.B", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PSADDU_DB: {
        snprintf(buff, sizeof(buff), "%-15s (%s, %s), (%s, %s), (%s, %s)", "PSADDU.DB", riscv_gpr[2 * FX(data, 24, 21)], riscv_gpr[2 * FX(data, 24, 21) + 1], riscv_gpr[2 * FX(data, 19, 16)], riscv_gpr[2 * FX(data, 19, 16) + 1], riscv_gpr[2 * FX(data, 11, 8)], riscv_gpr[2 * FX(data, 11, 8) + 1]);
    } break;
    case RISCV_PSADDU_DH: {
        snprintf(buff, sizeof(buff), "%-15s (%s, %s), (%s, %s), (%s, %s)", "PSADDU.DH", riscv_gpr[2 * FX(data, 24, 21)], riscv_gpr[2 * FX(data, 24, 21) + 1], riscv_gpr[2 * FX(data, 19, 16)], riscv_gpr[2 * FX(data, 19, 16) + 1], riscv_gpr[2 * FX(data, 11, 8)], riscv_gpr[2 * FX(data, 11, 8) + 1]);
    } break;
    case RISCV_PSADDU_DW: {
        snprintf(buff, sizeof(buff), "%-15s (%s, %s), (%s, %s), (%s, %s)", "PSADDU.DW", riscv_gpr[2 * FX(data, 24, 21)], riscv_gpr[2 * FX(data, 24, 21) + 1], riscv_gpr[2 * FX(data, 19, 16)], riscv_gpr[2 * FX(data, 19, 16) + 1], riscv_gpr[2 * FX(data, 11, 8)], riscv_gpr[2 * FX(data, 11, 8) + 1]);
    } break;
    case RISCV_PSADDU_H: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "PSADDU.H", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PSADDU_W: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "PSADDU.W", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PSAS_DHX: {
        snprintf(buff, sizeof(buff), "%-15s (%s, %s), (%s, %s), (%s, %s)", "PSAS.DHX", riscv_gpr[2 * FX(data, 24, 21)], riscv_gpr[2 * FX(data, 24, 21) + 1], riscv_gpr[2 * FX(data, 19, 16)], riscv_gpr[2 * FX(data, 19, 16) + 1], riscv_gpr[2 * FX(data, 11, 8)], riscv_gpr[2 * FX(data, 11, 8) + 1]);
    } break;
    case RISCV_PSAS_HX: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "PSAS.HX", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PSAS_WX: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "PSAS.WX", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PSATI_DH: {
        snprintf(buff, sizeof(buff), "%-15s 0x%x(%d), (%s, %s), (%s, %s)", "PSATI.DH", (unsigned int)(FX(data, 23, 20)), (int)(FX(data, 23, 20)), riscv_gpr[2 * FX(data, 19, 16)], riscv_gpr[2 * FX(data, 19, 16) + 1], riscv_gpr[2 * FX(data, 11, 8)], riscv_gpr[2 * FX(data, 11, 8) + 1]);
    } break;
    case RISCV_PSATI_DW: {
        snprintf(buff, sizeof(buff), "%-15s 0x%x(%d), (%s, %s), (%s, %s)", "PSATI.DW", (unsigned int)(FX(data, 24, 20)), (int)(FX(data, 24, 20)), riscv_gpr[2 * FX(data, 19, 16)], riscv_gpr[2 * FX(data, 19, 16) + 1], riscv_gpr[2 * FX(data, 11, 8)], riscv_gpr[2 * FX(data, 11, 8) + 1]);
    } break;
    case RISCV_PSATI_H: {
        snprintf(buff, sizeof(buff), "%-15s 0x%x(%d), %s, %s", "PSATI.H", (unsigned int)(FX(data, 23, 20)), (int)(FX(data, 23, 20)), riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PSATI_W: {
        snprintf(buff, sizeof(buff), "%-15s 0x%x(%d), %s, %s", "PSATI.W", (unsigned int)(FX(data, 24, 20)), (int)(FX(data, 24, 20)), riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PSEXT_DH_B: {
        snprintf(buff, sizeof(buff), "%-15s (%s, %s), (%s, %s)", "PSEXT.DH.B", riscv_gpr[2 * FX(data, 19, 16)], riscv_gpr[2 * FX(data, 19, 16) + 1], riscv_gpr[2 * FX(data, 11, 8)], riscv_gpr[2 * FX(data, 11, 8) + 1]);
    } break;
    case RISCV_PSEXT_DW_B: {
        snprintf(buff, sizeof(buff), "%-15s (%s, %s), (%s, %s)", "PSEXT.DW.B", riscv_gpr[2 * FX(data, 19, 16)], riscv_gpr[2 * FX(data, 19, 16) + 1], riscv_gpr[2 * FX(data, 11, 8)], riscv_gpr[2 * FX(data, 11, 8) + 1]);
    } break;
    case RISCV_PSEXT_DW_H: {
        snprintf(buff, sizeof(buff), "%-15s (%s, %s), (%s, %s)", "PSEXT.DW.H", riscv_gpr[2 * FX(data, 19, 16)], riscv_gpr[2 * FX(data, 19, 16) + 1], riscv_gpr[2 * FX(data, 11, 8)], riscv_gpr[2 * FX(data, 11, 8) + 1]);
    } break;
    case RISCV_PSEXT_H_B: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s", "PSEXT.H.B", riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PSEXT_W_B: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s", "PSEXT.W.B", riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PSEXT_W_H: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s", "PSEXT.W.H", riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PSH1ADD_DH: {
        snprintf(buff, sizeof(buff), "%-15s (%s, %s), (%s, %s), (%s, %s)", "PSH1ADD.DH", riscv_gpr[2 * FX(data, 24, 21)], riscv_gpr[2 * FX(data, 24, 21) + 1], riscv_gpr[2 * FX(data, 19, 16)], riscv_gpr[2 * FX(data, 19, 16) + 1], riscv_gpr[2 * FX(data, 11, 8)], riscv_gpr[2 * FX(data, 11, 8) + 1]);
    } break;
    case RISCV_PSH1ADD_DW: {
        snprintf(buff, sizeof(buff), "%-15s (%s, %s), (%s, %s), (%s, %s)", "PSH1ADD.DW", riscv_gpr[2 * FX(data, 24, 21)], riscv_gpr[2 * FX(data, 24, 21) + 1], riscv_gpr[2 * FX(data, 19, 16)], riscv_gpr[2 * FX(data, 19, 16) + 1], riscv_gpr[2 * FX(data, 11, 8)], riscv_gpr[2 * FX(data, 11, 8) + 1]);
    } break;
    case RISCV_PSH1ADD_H: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "PSH1ADD.H", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PSH1ADD_W: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "PSH1ADD.W", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PSLL_BS: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "PSLL.BS", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PSLL_DBS: {
        snprintf(buff, sizeof(buff), "%-15s %s, (%s, %s), (%s, %s)", "PSLL.DBS", riscv_gpr[FX(data, 24, 20)], riscv_gpr[2 * FX(data, 19, 16)], riscv_gpr[2 * FX(data, 19, 16) + 1], riscv_gpr[2 * FX(data, 11, 8)], riscv_gpr[2 * FX(data, 11, 8) + 1]);
    } break;
    case RISCV_PSLL_DHS: {
        snprintf(buff, sizeof(buff), "%-15s %s, (%s, %s), (%s, %s)", "PSLL.DHS", riscv_gpr[FX(data, 24, 20)], riscv_gpr[2 * FX(data, 19, 16)], riscv_gpr[2 * FX(data, 19, 16) + 1], riscv_gpr[2 * FX(data, 11, 8)], riscv_gpr[2 * FX(data, 11, 8) + 1]);
    } break;
    case RISCV_PSLL_DWS: {
        snprintf(buff, sizeof(buff), "%-15s %s, (%s, %s), (%s, %s)", "PSLL.DWS", riscv_gpr[FX(data, 24, 20)], riscv_gpr[2 * FX(data, 19, 16)], riscv_gpr[2 * FX(data, 19, 16) + 1], riscv_gpr[2 * FX(data, 11, 8)], riscv_gpr[2 * FX(data, 11, 8) + 1]);
    } break;
    case RISCV_PSLL_HS: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "PSLL.HS", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PSLL_WS: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "PSLL.WS", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PSLLI_B: {
        snprintf(buff, sizeof(buff), "%-15s 0x%x(%d), %s, %s", "PSLLI.B", (unsigned int)(FX(data, 22, 20)), (int)(FX(data, 22, 20)), riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PSLLI_DB: {
        snprintf(buff, sizeof(buff), "%-15s 0x%x(%d), (%s, %s), (%s, %s)", "PSLLI.DB", (unsigned int)(FX(data, 22, 20)), (int)(FX(data, 22, 20)), riscv_gpr[2 * FX(data, 19, 16)], riscv_gpr[2 * FX(data, 19, 16) + 1], riscv_gpr[2 * FX(data, 11, 8)], riscv_gpr[2 * FX(data, 11, 8) + 1]);
    } break;
    case RISCV_PSLLI_DH: {
        snprintf(buff, sizeof(buff), "%-15s 0x%x(%d), (%s, %s), (%s, %s)", "PSLLI.DH", (unsigned int)(FX(data, 23, 20)), (int)(FX(data, 23, 20)), riscv_gpr[2 * FX(data, 19, 16)], riscv_gpr[2 * FX(data, 19, 16) + 1], riscv_gpr[2 * FX(data, 11, 8)], riscv_gpr[2 * FX(data, 11, 8) + 1]);
    } break;
    case RISCV_PSLLI_DW: {
        snprintf(buff, sizeof(buff), "%-15s 0x%x(%d), (%s, %s), (%s, %s)", "PSLLI.DW", (unsigned int)(FX(data, 24, 20)), (int)(FX(data, 24, 20)), riscv_gpr[2 * FX(data, 19, 16)], riscv_gpr[2 * FX(data, 19, 16) + 1], riscv_gpr[2 * FX(data, 11, 8)], riscv_gpr[2 * FX(data, 11, 8) + 1]);
    } break;
    case RISCV_PSLLI_H: {
        snprintf(buff, sizeof(buff), "%-15s 0x%x(%d), %s, %s", "PSLLI.H", (unsigned int)(FX(data, 23, 20)), (int)(FX(data, 23, 20)), riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PSLLI_W: {
        snprintf(buff, sizeof(buff), "%-15s 0x%x(%d), %s, %s", "PSLLI.W", (unsigned int)(FX(data, 24, 20)), (int)(FX(data, 24, 20)), riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PSRA_BS: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "PSRA.BS", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PSRA_DBS: {
        snprintf(buff, sizeof(buff), "%-15s %s, (%s, %s), (%s, %s)", "PSRA.DBS", riscv_gpr[FX(data, 24, 20)], riscv_gpr[2 * FX(data, 19, 16)], riscv_gpr[2 * FX(data, 19, 16) + 1], riscv_gpr[2 * FX(data, 11, 8)], riscv_gpr[2 * FX(data, 11, 8) + 1]);
    } break;
    case RISCV_PSRA_DHS: {
        snprintf(buff, sizeof(buff), "%-15s %s, (%s, %s), (%s, %s)", "PSRA.DHS", riscv_gpr[FX(data, 24, 20)], riscv_gpr[2 * FX(data, 19, 16)], riscv_gpr[2 * FX(data, 19, 16) + 1], riscv_gpr[2 * FX(data, 11, 8)], riscv_gpr[2 * FX(data, 11, 8) + 1]);
    } break;
    case RISCV_PSRA_DWS: {
        snprintf(buff, sizeof(buff), "%-15s %s, (%s, %s), (%s, %s)", "PSRA.DWS", riscv_gpr[FX(data, 24, 20)], riscv_gpr[2 * FX(data, 19, 16)], riscv_gpr[2 * FX(data, 19, 16) + 1], riscv_gpr[2 * FX(data, 11, 8)], riscv_gpr[2 * FX(data, 11, 8) + 1]);
    } break;
    case RISCV_PSRA_HS: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "PSRA.HS", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PSRA_WS: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "PSRA.WS", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PSRAI_B: {
        snprintf(buff, sizeof(buff), "%-15s 0x%x(%d), %s, %s", "PSRAI.B", (unsigned int)(FX(data, 22, 20)), (int)(FX(data, 22, 20)), riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PSRAI_DB: {
        snprintf(buff, sizeof(buff), "%-15s 0x%x(%d), (%s, %s), (%s, %s)", "PSRAI.DB", (unsigned int)(FX(data, 22, 20)), (int)(FX(data, 22, 20)), riscv_gpr[2 * FX(data, 19, 16)], riscv_gpr[2 * FX(data, 19, 16) + 1], riscv_gpr[2 * FX(data, 11, 8)], riscv_gpr[2 * FX(data, 11, 8) + 1]);
    } break;
    case RISCV_PSRAI_DH: {
        snprintf(buff, sizeof(buff), "%-15s 0x%x(%d), (%s, %s), (%s, %s)", "PSRAI.DH", (unsigned int)(FX(data, 23, 20)), (int)(FX(data, 23, 20)), riscv_gpr[2 * FX(data, 19, 16)], riscv_gpr[2 * FX(data, 19, 16) + 1], riscv_gpr[2 * FX(data, 11, 8)], riscv_gpr[2 * FX(data, 11, 8) + 1]);
    } break;
    case RISCV_PSRAI_DW: {
        snprintf(buff, sizeof(buff), "%-15s 0x%x(%d), (%s, %s), (%s, %s)", "PSRAI.DW", (unsigned int)(FX(data, 24, 20)), (int)(FX(data, 24, 20)), riscv_gpr[2 * FX(data, 19, 16)], riscv_gpr[2 * FX(data, 19, 16) + 1], riscv_gpr[2 * FX(data, 11, 8)], riscv_gpr[2 * FX(data, 11, 8) + 1]);
    } break;
    case RISCV_PSRAI_H: {
        snprintf(buff, sizeof(buff), "%-15s 0x%x(%d), %s, %s", "PSRAI.H", (unsigned int)(FX(data, 23, 20)), (int)(FX(data, 23, 20)), riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PSRAI_W: {
        snprintf(buff, sizeof(buff), "%-15s 0x%x(%d), %s, %s", "PSRAI.W", (unsigned int)(FX(data, 24, 20)), (int)(FX(data, 24, 20)), riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PSRARI_DH: {
        snprintf(buff, sizeof(buff), "%-15s 0x%x(%d), (%s, %s), (%s, %s)", "PSRARI.DH", (unsigned int)(FX(data, 23, 20)), (int)(FX(data, 23, 20)), riscv_gpr[2 * FX(data, 19, 16)], riscv_gpr[2 * FX(data, 19, 16) + 1], riscv_gpr[2 * FX(data, 11, 8)], riscv_gpr[2 * FX(data, 11, 8) + 1]);
    } break;
    case RISCV_PSRARI_DW: {
        snprintf(buff, sizeof(buff), "%-15s 0x%x(%d), (%s, %s), (%s, %s)", "PSRARI.DW", (unsigned int)(FX(data, 24, 20)), (int)(FX(data, 24, 20)), riscv_gpr[2 * FX(data, 19, 16)], riscv_gpr[2 * FX(data, 19, 16) + 1], riscv_gpr[2 * FX(data, 11, 8)], riscv_gpr[2 * FX(data, 11, 8) + 1]);
    } break;
    case RISCV_PSRARI_H: {
        snprintf(buff, sizeof(buff), "%-15s 0x%x(%d), %s, %s", "PSRARI.H", (unsigned int)(FX(data, 23, 20)), (int)(FX(data, 23, 20)), riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PSRARI_W: {
        snprintf(buff, sizeof(buff), "%-15s 0x%x(%d), %s, %s", "PSRARI.W", (unsigned int)(FX(data, 24, 20)), (int)(FX(data, 24, 20)), riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PSRL_BS: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "PSRL.BS", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PSRL_DBS: {
        snprintf(buff, sizeof(buff), "%-15s %s, (%s, %s), (%s, %s)", "PSRL.DBS", riscv_gpr[FX(data, 24, 20)], riscv_gpr[2 * FX(data, 19, 16)], riscv_gpr[2 * FX(data, 19, 16) + 1], riscv_gpr[2 * FX(data, 11, 8)], riscv_gpr[2 * FX(data, 11, 8) + 1]);
    } break;
    case RISCV_PSRL_DHS: {
        snprintf(buff, sizeof(buff), "%-15s %s, (%s, %s), (%s, %s)", "PSRL.DHS", riscv_gpr[FX(data, 24, 20)], riscv_gpr[2 * FX(data, 19, 16)], riscv_gpr[2 * FX(data, 19, 16) + 1], riscv_gpr[2 * FX(data, 11, 8)], riscv_gpr[2 * FX(data, 11, 8) + 1]);
    } break;
    case RISCV_PSRL_DWS: {
        snprintf(buff, sizeof(buff), "%-15s %s, (%s, %s), (%s, %s)", "PSRL.DWS", riscv_gpr[FX(data, 24, 20)], riscv_gpr[2 * FX(data, 19, 16)], riscv_gpr[2 * FX(data, 19, 16) + 1], riscv_gpr[2 * FX(data, 11, 8)], riscv_gpr[2 * FX(data, 11, 8) + 1]);
    } break;
    case RISCV_PSRL_HS: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "PSRL.HS", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PSRL_WS: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "PSRL.WS", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PSRLI_B: {
        snprintf(buff, sizeof(buff), "%-15s 0x%x(%d), %s, %s", "PSRLI.B", (unsigned int)(FX(data, 22, 20)), (int)(FX(data, 22, 20)), riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PSRLI_DB: {
        snprintf(buff, sizeof(buff), "%-15s 0x%x(%d), (%s, %s), (%s, %s)", "PSRLI.DB", (unsigned int)(FX(data, 22, 20)), (int)(FX(data, 22, 20)), riscv_gpr[2 * FX(data, 19, 16)], riscv_gpr[2 * FX(data, 19, 16) + 1], riscv_gpr[2 * FX(data, 11, 8)], riscv_gpr[2 * FX(data, 11, 8) + 1]);
    } break;
    case RISCV_PSRLI_DH: {
        snprintf(buff, sizeof(buff), "%-15s 0x%x(%d), (%s, %s), (%s, %s)", "PSRLI.DH", (unsigned int)(FX(data, 23, 20)), (int)(FX(data, 23, 20)), riscv_gpr[2 * FX(data, 19, 16)], riscv_gpr[2 * FX(data, 19, 16) + 1], riscv_gpr[2 * FX(data, 11, 8)], riscv_gpr[2 * FX(data, 11, 8) + 1]);
    } break;
    case RISCV_PSRLI_DW: {
        snprintf(buff, sizeof(buff), "%-15s 0x%x(%d), (%s, %s), (%s, %s)", "PSRLI.DW", (unsigned int)(FX(data, 24, 20)), (int)(FX(data, 24, 20)), riscv_gpr[2 * FX(data, 19, 16)], riscv_gpr[2 * FX(data, 19, 16) + 1], riscv_gpr[2 * FX(data, 11, 8)], riscv_gpr[2 * FX(data, 11, 8) + 1]);
    } break;
    case RISCV_PSRLI_H: {
        snprintf(buff, sizeof(buff), "%-15s 0x%x(%d), %s, %s", "PSRLI.H", (unsigned int)(FX(data, 23, 20)), (int)(FX(data, 23, 20)), riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PSRLI_W: {
        snprintf(buff, sizeof(buff), "%-15s 0x%x(%d), %s, %s", "PSRLI.W", (unsigned int)(FX(data, 24, 20)), (int)(FX(data, 24, 20)), riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PSSA_DHX: {
        snprintf(buff, sizeof(buff), "%-15s (%s, %s), (%s, %s), (%s, %s)", "PSSA.DHX", riscv_gpr[2 * FX(data, 24, 21)], riscv_gpr[2 * FX(data, 24, 21) + 1], riscv_gpr[2 * FX(data, 19, 16)], riscv_gpr[2 * FX(data, 19, 16) + 1], riscv_gpr[2 * FX(data, 11, 8)], riscv_gpr[2 * FX(data, 11, 8) + 1]);
    } break;
    case RISCV_PSSA_HX: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "PSSA.HX", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PSSA_WX: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "PSSA.WX", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PSSH1SADD_DH: {
        snprintf(buff, sizeof(buff), "%-15s (%s, %s), (%s, %s), (%s, %s)", "PSSH1SADD.DH", riscv_gpr[2 * FX(data, 24, 21)], riscv_gpr[2 * FX(data, 24, 21) + 1], riscv_gpr[2 * FX(data, 19, 16)], riscv_gpr[2 * FX(data, 19, 16) + 1], riscv_gpr[2 * FX(data, 11, 8)], riscv_gpr[2 * FX(data, 11, 8) + 1]);
    } break;
    case RISCV_PSSH1SADD_DW: {
        snprintf(buff, sizeof(buff), "%-15s (%s, %s), (%s, %s), (%s, %s)", "PSSH1SADD.DW", riscv_gpr[2 * FX(data, 24, 21)], riscv_gpr[2 * FX(data, 24, 21) + 1], riscv_gpr[2 * FX(data, 19, 16)], riscv_gpr[2 * FX(data, 19, 16) + 1], riscv_gpr[2 * FX(data, 11, 8)], riscv_gpr[2 * FX(data, 11, 8) + 1]);
    } break;
    case RISCV_PSSH1SADD_H: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "PSSH1SADD.H", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PSSH1SADD_W: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "PSSH1SADD.W", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PSSHA_DHS: {
        snprintf(buff, sizeof(buff), "%-15s %s, (%s, %s), (%s, %s)", "PSSHA.DHS", riscv_gpr[FX(data, 24, 20)], riscv_gpr[2 * FX(data, 19, 16)], riscv_gpr[2 * FX(data, 19, 16) + 1], riscv_gpr[2 * FX(data, 11, 8)], riscv_gpr[2 * FX(data, 11, 8) + 1]);
    } break;
    case RISCV_PSSHA_DWS: {
        snprintf(buff, sizeof(buff), "%-15s %s, (%s, %s), (%s, %s)", "PSSHA.DWS", riscv_gpr[FX(data, 24, 20)], riscv_gpr[2 * FX(data, 19, 16)], riscv_gpr[2 * FX(data, 19, 16) + 1], riscv_gpr[2 * FX(data, 11, 8)], riscv_gpr[2 * FX(data, 11, 8) + 1]);
    } break;
    case RISCV_PSSHA_HS: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "PSSHA.HS", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PSSHA_WS: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "PSSHA.WS", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PSSHAR_DHS: {
        snprintf(buff, sizeof(buff), "%-15s %s, (%s, %s), (%s, %s)", "PSSHAR.DHS", riscv_gpr[FX(data, 24, 20)], riscv_gpr[2 * FX(data, 19, 16)], riscv_gpr[2 * FX(data, 19, 16) + 1], riscv_gpr[2 * FX(data, 11, 8)], riscv_gpr[2 * FX(data, 11, 8) + 1]);
    } break;
    case RISCV_PSSHAR_DWS: {
        snprintf(buff, sizeof(buff), "%-15s %s, (%s, %s), (%s, %s)", "PSSHAR.DWS", riscv_gpr[FX(data, 24, 20)], riscv_gpr[2 * FX(data, 19, 16)], riscv_gpr[2 * FX(data, 19, 16) + 1], riscv_gpr[2 * FX(data, 11, 8)], riscv_gpr[2 * FX(data, 11, 8) + 1]);
    } break;
    case RISCV_PSSHAR_HS: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "PSSHAR.HS", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PSSHAR_WS: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "PSSHAR.WS", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PSSHL_DHS: {
        snprintf(buff, sizeof(buff), "%-15s %s, (%s, %s), (%s, %s)", "PSSHL.DHS", riscv_gpr[FX(data, 24, 20)], riscv_gpr[2 * FX(data, 19, 16)], riscv_gpr[2 * FX(data, 19, 16) + 1], riscv_gpr[2 * FX(data, 11, 8)], riscv_gpr[2 * FX(data, 11, 8) + 1]);
    } break;
    case RISCV_PSSHL_DWS: {
        snprintf(buff, sizeof(buff), "%-15s %s, (%s, %s), (%s, %s)", "PSSHL.DWS", riscv_gpr[FX(data, 24, 20)], riscv_gpr[2 * FX(data, 19, 16)], riscv_gpr[2 * FX(data, 19, 16) + 1], riscv_gpr[2 * FX(data, 11, 8)], riscv_gpr[2 * FX(data, 11, 8) + 1]);
    } break;
    case RISCV_PSSHL_HS: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "PSSHL.HS", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PSSHL_WS: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "PSSHL.WS", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PSSHLR_DHS: {
        snprintf(buff, sizeof(buff), "%-15s %s, (%s, %s), (%s, %s)", "PSSHLR.DHS", riscv_gpr[FX(data, 24, 20)], riscv_gpr[2 * FX(data, 19, 16)], riscv_gpr[2 * FX(data, 19, 16) + 1], riscv_gpr[2 * FX(data, 11, 8)], riscv_gpr[2 * FX(data, 11, 8) + 1]);
    } break;
    case RISCV_PSSHLR_DWS: {
        snprintf(buff, sizeof(buff), "%-15s %s, (%s, %s), (%s, %s)", "PSSHLR.DWS", riscv_gpr[FX(data, 24, 20)], riscv_gpr[2 * FX(data, 19, 16)], riscv_gpr[2 * FX(data, 19, 16) + 1], riscv_gpr[2 * FX(data, 11, 8)], riscv_gpr[2 * FX(data, 11, 8) + 1]);
    } break;
    case RISCV_PSSHLR_HS: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "PSSHLR.HS", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PSSHLR_WS: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "PSSHLR.WS", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PSSLAI_DH: {
        snprintf(buff, sizeof(buff), "%-15s 0x%x(%d), (%s, %s), (%s, %s)", "PSSLAI.DH", (unsigned int)(FX(data, 23, 20)), (int)(FX(data, 23, 20)), riscv_gpr[2 * FX(data, 19, 16)], riscv_gpr[2 * FX(data, 19, 16) + 1], riscv_gpr[2 * FX(data, 11, 8)], riscv_gpr[2 * FX(data, 11, 8) + 1]);
    } break;
    case RISCV_PSSLAI_DW: {
        snprintf(buff, sizeof(buff), "%-15s 0x%x(%d), (%s, %s), (%s, %s)", "PSSLAI.DW", (unsigned int)(FX(data, 24, 20)), (int)(FX(data, 24, 20)), riscv_gpr[2 * FX(data, 19, 16)], riscv_gpr[2 * FX(data, 19, 16) + 1], riscv_gpr[2 * FX(data, 11, 8)], riscv_gpr[2 * FX(data, 11, 8) + 1]);
    } break;
    case RISCV_PSSLAI_H: {
        snprintf(buff, sizeof(buff), "%-15s 0x%x(%d), %s, %s", "PSSLAI.H", (unsigned int)(FX(data, 23, 20)), (int)(FX(data, 23, 20)), riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PSSLAI_W: {
        snprintf(buff, sizeof(buff), "%-15s 0x%x(%d), %s, %s", "PSSLAI.W", (unsigned int)(FX(data, 24, 20)), (int)(FX(data, 24, 20)), riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PSSUB_B: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "PSSUB.B", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PSSUB_DB: {
        snprintf(buff, sizeof(buff), "%-15s (%s, %s), (%s, %s), (%s, %s)", "PSSUB.DB", riscv_gpr[2 * FX(data, 24, 21)], riscv_gpr[2 * FX(data, 24, 21) + 1], riscv_gpr[2 * FX(data, 19, 16)], riscv_gpr[2 * FX(data, 19, 16) + 1], riscv_gpr[2 * FX(data, 11, 8)], riscv_gpr[2 * FX(data, 11, 8) + 1]);
    } break;
    case RISCV_PSSUB_DH: {
        snprintf(buff, sizeof(buff), "%-15s (%s, %s), (%s, %s), (%s, %s)", "PSSUB.DH", riscv_gpr[2 * FX(data, 24, 21)], riscv_gpr[2 * FX(data, 24, 21) + 1], riscv_gpr[2 * FX(data, 19, 16)], riscv_gpr[2 * FX(data, 19, 16) + 1], riscv_gpr[2 * FX(data, 11, 8)], riscv_gpr[2 * FX(data, 11, 8) + 1]);
    } break;
    case RISCV_PSSUB_DW: {
        snprintf(buff, sizeof(buff), "%-15s (%s, %s), (%s, %s), (%s, %s)", "PSSUB.DW", riscv_gpr[2 * FX(data, 24, 21)], riscv_gpr[2 * FX(data, 24, 21) + 1], riscv_gpr[2 * FX(data, 19, 16)], riscv_gpr[2 * FX(data, 19, 16) + 1], riscv_gpr[2 * FX(data, 11, 8)], riscv_gpr[2 * FX(data, 11, 8) + 1]);
    } break;
    case RISCV_PSSUB_H: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "PSSUB.H", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PSSUB_W: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "PSSUB.W", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PSSUBU_B: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "PSSUBU.B", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PSSUBU_DB: {
        snprintf(buff, sizeof(buff), "%-15s (%s, %s), (%s, %s), (%s, %s)", "PSSUBU.DB", riscv_gpr[2 * FX(data, 24, 21)], riscv_gpr[2 * FX(data, 24, 21) + 1], riscv_gpr[2 * FX(data, 19, 16)], riscv_gpr[2 * FX(data, 19, 16) + 1], riscv_gpr[2 * FX(data, 11, 8)], riscv_gpr[2 * FX(data, 11, 8) + 1]);
    } break;
    case RISCV_PSSUBU_DH: {
        snprintf(buff, sizeof(buff), "%-15s (%s, %s), (%s, %s), (%s, %s)", "PSSUBU.DH", riscv_gpr[2 * FX(data, 24, 21)], riscv_gpr[2 * FX(data, 24, 21) + 1], riscv_gpr[2 * FX(data, 19, 16)], riscv_gpr[2 * FX(data, 19, 16) + 1], riscv_gpr[2 * FX(data, 11, 8)], riscv_gpr[2 * FX(data, 11, 8) + 1]);
    } break;
    case RISCV_PSSUBU_DW: {
        snprintf(buff, sizeof(buff), "%-15s (%s, %s), (%s, %s), (%s, %s)", "PSSUBU.DW", riscv_gpr[2 * FX(data, 24, 21)], riscv_gpr[2 * FX(data, 24, 21) + 1], riscv_gpr[2 * FX(data, 19, 16)], riscv_gpr[2 * FX(data, 19, 16) + 1], riscv_gpr[2 * FX(data, 11, 8)], riscv_gpr[2 * FX(data, 11, 8) + 1]);
    } break;
    case RISCV_PSSUBU_H: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "PSSUBU.H", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PSSUBU_W: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "PSSUBU.W", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PSUB_B: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "PSUB.B", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PSUB_DB: {
        snprintf(buff, sizeof(buff), "%-15s (%s, %s), (%s, %s), (%s, %s)", "PSUB.DB", riscv_gpr[2 * FX(data, 24, 21)], riscv_gpr[2 * FX(data, 24, 21) + 1], riscv_gpr[2 * FX(data, 19, 16)], riscv_gpr[2 * FX(data, 19, 16) + 1], riscv_gpr[2 * FX(data, 11, 8)], riscv_gpr[2 * FX(data, 11, 8) + 1]);
    } break;
    case RISCV_PSUB_DH: {
        snprintf(buff, sizeof(buff), "%-15s (%s, %s), (%s, %s), (%s, %s)", "PSUB.DH", riscv_gpr[2 * FX(data, 24, 21)], riscv_gpr[2 * FX(data, 24, 21) + 1], riscv_gpr[2 * FX(data, 19, 16)], riscv_gpr[2 * FX(data, 19, 16) + 1], riscv_gpr[2 * FX(data, 11, 8)], riscv_gpr[2 * FX(data, 11, 8) + 1]);
    } break;
    case RISCV_PSUB_DW: {
        snprintf(buff, sizeof(buff), "%-15s (%s, %s), (%s, %s), (%s, %s)", "PSUB.DW", riscv_gpr[2 * FX(data, 24, 21)], riscv_gpr[2 * FX(data, 24, 21) + 1], riscv_gpr[2 * FX(data, 19, 16)], riscv_gpr[2 * FX(data, 19, 16) + 1], riscv_gpr[2 * FX(data, 11, 8)], riscv_gpr[2 * FX(data, 11, 8) + 1]);
    } break;
    case RISCV_PSUB_H: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "PSUB.H", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PSUB_W: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "PSUB.W", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PUSATI_DH: {
        snprintf(buff, sizeof(buff), "%-15s 0x%x(%d), (%s, %s), (%s, %s)", "PUSATI.DH", (unsigned int)(FX(data, 23, 20)), (int)(FX(data, 23, 20)), riscv_gpr[2 * FX(data, 19, 16)], riscv_gpr[2 * FX(data, 19, 16) + 1], riscv_gpr[2 * FX(data, 11, 8)], riscv_gpr[2 * FX(data, 11, 8) + 1]);
    } break;
    case RISCV_PUSATI_DW: {
        snprintf(buff, sizeof(buff), "%-15s 0x%x(%d), (%s, %s), (%s, %s)", "PUSATI.DW", (unsigned int)(FX(data, 24, 20)), (int)(FX(data, 24, 20)), riscv_gpr[2 * FX(data, 19, 16)], riscv_gpr[2 * FX(data, 19, 16) + 1], riscv_gpr[2 * FX(data, 11, 8)], riscv_gpr[2 * FX(data, 11, 8) + 1]);
    } break;
    case RISCV_PUSATI_H: {
        snprintf(buff, sizeof(buff), "%-15s 0x%x(%d), %s, %s", "PUSATI.H", (unsigned int)(FX(data, 23, 20)), (int)(FX(data, 23, 20)), riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PUSATI_W: {
        snprintf(buff, sizeof(buff), "%-15s 0x%x(%d), %s, %s", "PUSATI.W", (unsigned int)(FX(data, 24, 20)), (int)(FX(data, 24, 20)), riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_PWADD_B: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, (%s, %s)", "PWADD.B", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[2 * FX(data, 11, 8)], riscv_gpr[2 * FX(data, 11, 8) + 1]);
    } break;
    case RISCV_PWADD_H: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, (%s, %s)", "PWADD.H", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[2 * FX(data, 11, 8)], riscv_gpr[2 * FX(data, 11, 8) + 1]);
    } break;
    case RISCV_PWADDA_B: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, (%s, %s)", "PWADDA.B", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[2 * FX(data, 11, 8)], riscv_gpr[2 * FX(data, 11, 8) + 1]);
    } break;
    case RISCV_PWADDA_H: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, (%s, %s)", "PWADDA.H", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[2 * FX(data, 11, 8)], riscv_gpr[2 * FX(data, 11, 8) + 1]);
    } break;
    case RISCV_PWADDAU_B: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, (%s, %s)", "PWADDAU.B", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[2 * FX(data, 11, 8)], riscv_gpr[2 * FX(data, 11, 8) + 1]);
    } break;
    case RISCV_PWADDAU_H: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, (%s, %s)", "PWADDAU.H", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[2 * FX(data, 11, 8)], riscv_gpr[2 * FX(data, 11, 8) + 1]);
    } break;
    case RISCV_PWADDU_B: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, (%s, %s)", "PWADDU.B", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[2 * FX(data, 11, 8)], riscv_gpr[2 * FX(data, 11, 8) + 1]);
    } break;
    case RISCV_PWADDU_H: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, (%s, %s)", "PWADDU.H", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[2 * FX(data, 11, 8)], riscv_gpr[2 * FX(data, 11, 8) + 1]);
    } break;
    case RISCV_PWMACC_H: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, (%s, %s)", "PWMACC.H", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[2 * FX(data, 11, 8)], riscv_gpr[2 * FX(data, 11, 8) + 1]);
    } break;
    case RISCV_PWMACCSU_H: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, (%s, %s)", "PWMACCSU.H", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[2 * FX(data, 11, 8)], riscv_gpr[2 * FX(data, 11, 8) + 1]);
    } break;
    case RISCV_PWMACCU_H: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, (%s, %s)", "PWMACCU.H", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[2 * FX(data, 11, 8)], riscv_gpr[2 * FX(data, 11, 8) + 1]);
    } break;
    case RISCV_PWMUL_B: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, (%s, %s)", "PWMUL.B", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[2 * FX(data, 11, 8)], riscv_gpr[2 * FX(data, 11, 8) + 1]);
    } break;
    case RISCV_PWMUL_H: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, (%s, %s)", "PWMUL.H", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[2 * FX(data, 11, 8)], riscv_gpr[2 * FX(data, 11, 8) + 1]);
    } break;
    case RISCV_PWMULSU_B: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, (%s, %s)", "PWMULSU.B", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[2 * FX(data, 11, 8)], riscv_gpr[2 * FX(data, 11, 8) + 1]);
    } break;
    case RISCV_PWMULSU_H: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, (%s, %s)", "PWMULSU.H", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[2 * FX(data, 11, 8)], riscv_gpr[2 * FX(data, 11, 8) + 1]);
    } break;
    case RISCV_PWMULU_B: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, (%s, %s)", "PWMULU.B", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[2 * FX(data, 11, 8)], riscv_gpr[2 * FX(data, 11, 8) + 1]);
    } break;
    case RISCV_PWMULU_H: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, (%s, %s)", "PWMULU.H", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[2 * FX(data, 11, 8)], riscv_gpr[2 * FX(data, 11, 8) + 1]);
    } break;
    case RISCV_PWSLA_BS: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, (%s, %s)", "PWSLA.BS", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[2 * FX(data, 11, 8)], riscv_gpr[2 * FX(data, 11, 8) + 1]);
    } break;
    case RISCV_PWSLA_HS: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, (%s, %s)", "PWSLA.HS", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[2 * FX(data, 11, 8)], riscv_gpr[2 * FX(data, 11, 8) + 1]);
    } break;
    case RISCV_PWSLAI_B: {
        snprintf(buff, sizeof(buff), "%-15s 0x%x(%d), %s, (%s, %s)", "PWSLAI.B", (unsigned int)(FX(data, 23, 20)), (int)(FX(data, 23, 20)), riscv_gpr[FX(data, 19, 15)], riscv_gpr[2 * FX(data, 11, 8)], riscv_gpr[2 * FX(data, 11, 8) + 1]);
    } break;
    case RISCV_PWSLAI_H: {
        snprintf(buff, sizeof(buff), "%-15s 0x%x(%d), %s, (%s, %s)", "PWSLAI.H", (unsigned int)(FX(data, 24, 20)), (int)(FX(data, 24, 20)), riscv_gpr[FX(data, 19, 15)], riscv_gpr[2 * FX(data, 11, 8)], riscv_gpr[2 * FX(data, 11, 8) + 1]);
    } break;
    case RISCV_PWSLL_BS: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, (%s, %s)", "PWSLL.BS", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[2 * FX(data, 11, 8)], riscv_gpr[2 * FX(data, 11, 8) + 1]);
    } break;
    case RISCV_PWSLL_HS: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, (%s, %s)", "PWSLL.HS", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[2 * FX(data, 11, 8)], riscv_gpr[2 * FX(data, 11, 8) + 1]);
    } break;
    case RISCV_PWSLLI_B: {
        snprintf(buff, sizeof(buff), "%-15s 0x%x(%d), %s, (%s, %s)", "PWSLLI.B", (unsigned int)(FX(data, 23, 20)), (int)(FX(data, 23, 20)), riscv_gpr[FX(data, 19, 15)], riscv_gpr[2 * FX(data, 11, 8)], riscv_gpr[2 * FX(data, 11, 8) + 1]);
    } break;
    case RISCV_PWSLLI_H: {
        snprintf(buff, sizeof(buff), "%-15s 0x%x(%d), %s, (%s, %s)", "PWSLLI.H", (unsigned int)(FX(data, 24, 20)), (int)(FX(data, 24, 20)), riscv_gpr[FX(data, 19, 15)], riscv_gpr[2 * FX(data, 11, 8)], riscv_gpr[2 * FX(data, 11, 8) + 1]);
    } break;
    case RISCV_PWSUB_B: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, (%s, %s)", "PWSUB.B", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[2 * FX(data, 11, 8)], riscv_gpr[2 * FX(data, 11, 8) + 1]);
    } break;
    case RISCV_PWSUB_H: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, (%s, %s)", "PWSUB.H", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[2 * FX(data, 11, 8)], riscv_gpr[2 * FX(data, 11, 8) + 1]);
    } break;
    case RISCV_PWSUBA_B: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, (%s, %s)", "PWSUBA.B", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[2 * FX(data, 11, 8)], riscv_gpr[2 * FX(data, 11, 8) + 1]);
    } break;
    case RISCV_PWSUBA_H: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, (%s, %s)", "PWSUBA.H", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[2 * FX(data, 11, 8)], riscv_gpr[2 * FX(data, 11, 8) + 1]);
    } break;
    case RISCV_PWSUBAU_B: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, (%s, %s)", "PWSUBAU.B", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[2 * FX(data, 11, 8)], riscv_gpr[2 * FX(data, 11, 8) + 1]);
    } break;
    case RISCV_PWSUBAU_H: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, (%s, %s)", "PWSUBAU.H", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[2 * FX(data, 11, 8)], riscv_gpr[2 * FX(data, 11, 8) + 1]);
    } break;
    case RISCV_PWSUBU_B: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, (%s, %s)", "PWSUBU.B", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[2 * FX(data, 11, 8)], riscv_gpr[2 * FX(data, 11, 8) + 1]);
    } break;
    case RISCV_PWSUBU_H: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, (%s, %s)", "PWSUBU.H", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[2 * FX(data, 11, 8)], riscv_gpr[2 * FX(data, 11, 8) + 1]);
    } break;
    case RISCV_REM: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "REM", riscv_gpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 24, 20)]);
    } break;
    case RISCV_REMU: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "REMU", riscv_gpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 24, 20)]);
    } break;
    case RISCV_REMUW: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "REMUW", riscv_gpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 24, 20)]);
    } break;
    case RISCV_REMW: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "REMW", riscv_gpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 24, 20)]);
    } break;
    case RISCV_REV: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s", "REV", riscv_gpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)]);
    } break;
    case RISCV_REV_RV32: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s", "REV.RV32", riscv_gpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)]);
    } break;
    case RISCV_REV16: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s", "REV16", riscv_gpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)]);
    } break;
    case RISCV_REV8: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s", "REV8", riscv_gpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)]);
    } break;
    case RISCV_REV8_RV32: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s", "REV8.RV32", riscv_gpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)]);
    } break;
    case RISCV_ROL: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "ROL", riscv_gpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 24, 20)]);
    } break;
    case RISCV_ROLW: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "ROLW", riscv_gpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 24, 20)]);
    } break;
    case RISCV_ROR: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "ROR", riscv_gpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 24, 20)]);
    } break;
    case RISCV_RORI: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, 0x%x(%d)", "RORI", riscv_gpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)], (unsigned int)(FX(data, 25, 20)), (int)(FX(data, 25, 20)));
    } break;
    case RISCV_RORIW: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, 0x%x(%d)", "RORIW", riscv_gpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)], (unsigned int)(FX(data, 24, 20)), (int)(FX(data, 24, 20)));
    } break;
    case RISCV_RORW: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "RORW", riscv_gpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 24, 20)]);
    } break;
    case RISCV_SATI: {
        snprintf(buff, sizeof(buff), "%-15s 0x%x(%d), %s, %s", "SATI", (unsigned int)(FX(data, 25, 20)), (int)(FX(data, 25, 20)), riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_SB: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, 0x%x(%d)", "SB", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], (unsigned int)(SIGN_EXTEND(((FX(data, 31, 25) << 5) | FX(data, 11, 7)), 12)), (int)(SIGN_EXTEND(((FX(data, 31, 25) << 5) | FX(data, 11, 7)), 12)));
    } break;
    case RISCV_SB_RL: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "SB.RL", riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 24, 20)], riscv_aq[FX(data, 26, 26)]);
    } break;
    case RISCV_SC_B: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s, %s", "SC.B", riscv_gpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 24, 20)], riscv_aq[FX(data, 26, 26)], riscv_rl[FX(data, 25, 25)]);
    } break;
    case RISCV_SC_D: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s, %s", "SC.D", riscv_gpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 24, 20)], riscv_aq[FX(data, 26, 26)], riscv_rl[FX(data, 25, 25)]);
    } break;
    case RISCV_SC_H: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s, %s", "SC.H", riscv_gpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 24, 20)], riscv_aq[FX(data, 26, 26)], riscv_rl[FX(data, 25, 25)]);
    } break;
    case RISCV_SC_W: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s, %s", "SC.W", riscv_gpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 24, 20)], riscv_aq[FX(data, 26, 26)], riscv_rl[FX(data, 25, 25)]);
    } break;
    case RISCV_SCTRCLR: {
        snprintf(buff, sizeof(buff), "%-15s ", "SCTRCLR");
    } break;
    case RISCV_SD: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, 0x%x(%d)", "SD", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], (unsigned int)(SIGN_EXTEND(((FX(data, 31, 25) << 5) | FX(data, 11, 7)), 12)), (int)(SIGN_EXTEND(((FX(data, 31, 25) << 5) | FX(data, 11, 7)), 12)));
    } break;
    case RISCV_SD_RL: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "SD.RL", riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 24, 20)], riscv_aq[FX(data, 26, 26)]);
    } break;
    case RISCV_SEXT_B: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s", "SEXT.B", riscv_gpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)]);
    } break;
    case RISCV_SEXT_H: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s", "SEXT.H", riscv_gpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)]);
    } break;
    case RISCV_SFENCE_INVAL_IR: {
        snprintf(buff, sizeof(buff), "%-15s ", "SFENCE.INVAL.IR");
    } break;
    case RISCV_SFENCE_VMA: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s", "SFENCE.VMA", riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 24, 20)]);
    } break;
    case RISCV_SFENCE_W_INVAL: {
        snprintf(buff, sizeof(buff), "%-15s ", "SFENCE.W.INVAL");
    } break;
    case RISCV_SH: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, 0x%x(%d)", "SH", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], (unsigned int)(SIGN_EXTEND(((FX(data, 31, 25) << 5) | FX(data, 11, 7)), 12)), (int)(SIGN_EXTEND(((FX(data, 31, 25) << 5) | FX(data, 11, 7)), 12)));
    } break;
    case RISCV_SH_RL: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "SH.RL", riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 24, 20)], riscv_aq[FX(data, 26, 26)]);
    } break;
    case RISCV_SH1ADD: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "SH1ADD", riscv_gpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 24, 20)]);
    } break;
    case RISCV_SH1ADD_UW: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "SH1ADD.UW", riscv_gpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 24, 20)]);
    } break;
    case RISCV_SH2ADD: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "SH2ADD", riscv_gpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 24, 20)]);
    } break;
    case RISCV_SH2ADD_UW: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "SH2ADD.UW", riscv_gpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 24, 20)]);
    } break;
    case RISCV_SH3ADD: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "SH3ADD", riscv_gpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 24, 20)]);
    } break;
    case RISCV_SH3ADD_UW: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "SH3ADD.UW", riscv_gpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 24, 20)]);
    } break;
    case RISCV_SHA: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "SHA", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_SHA256SIG0: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s", "SHA256SIG0", riscv_gpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)]);
    } break;
    case RISCV_SHA256SIG1: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s", "SHA256SIG1", riscv_gpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)]);
    } break;
    case RISCV_SHA256SUM0: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s", "SHA256SUM0", riscv_gpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)]);
    } break;
    case RISCV_SHA256SUM1: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s", "SHA256SUM1", riscv_gpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)]);
    } break;
    case RISCV_SHA512SIG0: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s", "SHA512SIG0", riscv_gpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)]);
    } break;
    case RISCV_SHA512SIG0H: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "SHA512SIG0H", riscv_gpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 24, 20)]);
    } break;
    case RISCV_SHA512SIG0L: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "SHA512SIG0L", riscv_gpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 24, 20)]);
    } break;
    case RISCV_SHA512SIG1: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s", "SHA512SIG1", riscv_gpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)]);
    } break;
    case RISCV_SHA512SIG1H: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "SHA512SIG1H", riscv_gpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 24, 20)]);
    } break;
    case RISCV_SHA512SIG1L: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "SHA512SIG1L", riscv_gpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 24, 20)]);
    } break;
    case RISCV_SHA512SUM0: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s", "SHA512SUM0", riscv_gpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)]);
    } break;
    case RISCV_SHA512SUM0R: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "SHA512SUM0R", riscv_gpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 24, 20)]);
    } break;
    case RISCV_SHA512SUM1: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s", "SHA512SUM1", riscv_gpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)]);
    } break;
    case RISCV_SHA512SUM1R: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "SHA512SUM1R", riscv_gpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 24, 20)]);
    } break;
    case RISCV_SHAR: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "SHAR", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_SHL: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "SHL", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_SHLR: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "SHLR", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_SINVAL_VMA: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s", "SINVAL.VMA", riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 24, 20)]);
    } break;
    case RISCV_SLL: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "SLL", riscv_gpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 24, 20)]);
    } break;
    case RISCV_SLLI: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, 0x%x(%d)", "SLLI", riscv_gpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)], (unsigned int)(FX(data, 25, 20)), (int)(FX(data, 25, 20)));
    } break;
    case RISCV_SLLI_UW: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, 0x%x(%d)", "SLLI.UW", riscv_gpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)], (unsigned int)(FX(data, 25, 20)), (int)(FX(data, 25, 20)));
    } break;
    case RISCV_SLLIW: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, 0x%x(%d)", "SLLIW", riscv_gpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)], (unsigned int)(FX(data, 24, 20)), (int)(FX(data, 24, 20)));
    } break;
    case RISCV_SLLW: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "SLLW", riscv_gpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 24, 20)]);
    } break;
    case RISCV_SLT: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "SLT", riscv_gpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 24, 20)]);
    } break;
    case RISCV_SLTI: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, 0x%x(%d)", "SLTI", riscv_gpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)], (unsigned int)(SIGN_EXTEND(FX(data, 31, 20), 12)), (int)(SIGN_EXTEND(FX(data, 31, 20), 12)));
    } break;
    case RISCV_SLTIU: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, 0x%x(%d)", "SLTIU", riscv_gpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)], (unsigned int)(SIGN_EXTEND(FX(data, 31, 20), 12)), (int)(SIGN_EXTEND(FX(data, 31, 20), 12)));
    } break;
    case RISCV_SLTU: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "SLTU", riscv_gpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 24, 20)]);
    } break;
    case RISCV_SLX: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "SLX", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_SM3P0: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s", "SM3P0", riscv_gpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)]);
    } break;
    case RISCV_SM3P1: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s", "SM3P1", riscv_gpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)]);
    } break;
    case RISCV_SM4ED: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, 0x%x(%d)", "SM4ED", riscv_gpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 24, 20)], (unsigned int)(FX(data, 31, 30)), (int)(FX(data, 31, 30)));
    } break;
    case RISCV_SM4KS: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, 0x%x(%d)", "SM4KS", riscv_gpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 24, 20)], (unsigned int)(FX(data, 31, 30)), (int)(FX(data, 31, 30)));
    } break;
    case RISCV_SRA: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "SRA", riscv_gpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 24, 20)]);
    } break;
    case RISCV_SRAI: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, 0x%x(%d)", "SRAI", riscv_gpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)], (unsigned int)(FX(data, 25, 20)), (int)(FX(data, 25, 20)));
    } break;
    case RISCV_SRAIW: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, 0x%x(%d)", "SRAIW", riscv_gpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)], (unsigned int)(FX(data, 24, 20)), (int)(FX(data, 24, 20)));
    } break;
    case RISCV_SRARI: {
        snprintf(buff, sizeof(buff), "%-15s 0x%x(%d), %s, %s", "SRARI", (unsigned int)(FX(data, 25, 20)), (int)(FX(data, 25, 20)), riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_SRAW: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "SRAW", riscv_gpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 24, 20)]);
    } break;
    case RISCV_SRET: {
        snprintf(buff, sizeof(buff), "%-15s ", "SRET");
    } break;
    case RISCV_SRL: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "SRL", riscv_gpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 24, 20)]);
    } break;
    case RISCV_SRLI: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, 0x%x(%d)", "SRLI", riscv_gpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)], (unsigned int)(FX(data, 25, 20)), (int)(FX(data, 25, 20)));
    } break;
    case RISCV_SRLIW: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, 0x%x(%d)", "SRLIW", riscv_gpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)], (unsigned int)(FX(data, 24, 20)), (int)(FX(data, 24, 20)));
    } break;
    case RISCV_SRLW: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "SRLW", riscv_gpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 24, 20)]);
    } break;
    case RISCV_SRX: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "SRX", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_SSAMOSWAP_D: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s, %s", "SSAMOSWAP.D", riscv_gpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 24, 20)], riscv_aq[FX(data, 26, 26)], riscv_rl[FX(data, 25, 25)]);
    } break;
    case RISCV_SSAMOSWAP_W: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s, %s", "SSAMOSWAP.W", riscv_gpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 24, 20)], riscv_aq[FX(data, 26, 26)], riscv_rl[FX(data, 25, 25)]);
    } break;
    case RISCV_SUB: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "SUB", riscv_gpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 24, 20)]);
    } break;
    case RISCV_SUBD: {
        snprintf(buff, sizeof(buff), "%-15s (%s, %s), (%s, %s), (%s, %s)", "SUBD", riscv_gpr[2 * FX(data, 24, 21)], riscv_gpr[2 * FX(data, 24, 21) + 1], riscv_gpr[2 * FX(data, 19, 16)], riscv_gpr[2 * FX(data, 19, 16) + 1], riscv_gpr[2 * FX(data, 11, 8)], riscv_gpr[2 * FX(data, 11, 8) + 1]);
    } break;
    case RISCV_SUBW: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "SUBW", riscv_gpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 24, 20)]);
    } break;
    case RISCV_SW: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, 0x%x(%d)", "SW", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], (unsigned int)(SIGN_EXTEND(((FX(data, 31, 25) << 5) | FX(data, 11, 7)), 12)), (int)(SIGN_EXTEND(((FX(data, 31, 25) << 5) | FX(data, 11, 7)), 12)));
    } break;
    case RISCV_SW_RL: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "SW.RL", riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 24, 20)], riscv_aq[FX(data, 26, 26)]);
    } break;
    case RISCV_UNZIP: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s", "UNZIP", riscv_gpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)]);
    } break;
    case RISCV_UNZIP16HP: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "UNZIP16HP", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_UNZIP16P: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "UNZIP16P", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_UNZIP8HP: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "UNZIP8HP", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_UNZIP8P: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "UNZIP8P", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_USATI: {
        snprintf(buff, sizeof(buff), "%-15s 0x%x(%d), %s, %s", "USATI", (unsigned int)(FX(data, 25, 20)), (int)(FX(data, 25, 20)), riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_VAADD_VV: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VAADD.VV", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_vpr[FX(data, 19, 15)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VAADD_VX: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VAADD.VX", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VAADDU_VV: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VAADDU.VV", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_vpr[FX(data, 19, 15)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VAADDU_VX: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VAADDU.VX", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VABD_VV: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VABD.VV", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_vpr[FX(data, 19, 15)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VABDU_VV: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VABDU.VV", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_vpr[FX(data, 19, 15)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VABS_V: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "VABS.V", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VADC_VIM: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, 0x%x(%d), %s", "VADC.VIM", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], (unsigned int)(SIGN_EXTEND(FX(data, 19, 15), 5)), (int)(SIGN_EXTEND(FX(data, 19, 15), 5)), "v0");
    } break;
    case RISCV_VADC_VVM: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VADC.VVM", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_vpr[FX(data, 19, 15)], "v0");
    } break;
    case RISCV_VADC_VXM: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VADC.VXM", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], "v0");
    } break;
    case RISCV_VADD_VI: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, 0x%x(%d), %s", "VADD.VI", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], (unsigned int)(SIGN_EXTEND(FX(data, 19, 15), 5)), (int)(SIGN_EXTEND(FX(data, 19, 15), 5)), riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VADD_VV: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VADD.VV", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_vpr[FX(data, 19, 15)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VADD_VX: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VADD.VX", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VAESDF_VS: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s", "VAESDF.VS", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)]);
    } break;
    case RISCV_VAESDF_VV: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s", "VAESDF.VV", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)]);
    } break;
    case RISCV_VAESDM_VS: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s", "VAESDM.VS", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)]);
    } break;
    case RISCV_VAESDM_VV: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s", "VAESDM.VV", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)]);
    } break;
    case RISCV_VAESEF_VS: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s", "VAESEF.VS", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)]);
    } break;
    case RISCV_VAESEF_VV: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s", "VAESEF.VV", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)]);
    } break;
    case RISCV_VAESEM_VS: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s", "VAESEM.VS", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)]);
    } break;
    case RISCV_VAESEM_VV: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s", "VAESEM.VV", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)]);
    } break;
    case RISCV_VAESKF1_VI: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, 0x%x(%d)", "VAESKF1.VI", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], (unsigned int)(FX(data, 19, 15)), (int)(FX(data, 19, 15)));
    } break;
    case RISCV_VAESKF2_VI: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, 0x%x(%d)", "VAESKF2.VI", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], (unsigned int)(FX(data, 19, 15)), (int)(FX(data, 19, 15)));
    } break;
    case RISCV_VAESZ_VS: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s", "VAESZ.VS", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)]);
    } break;
    case RISCV_VAND_VI: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, 0x%x(%d), %s", "VAND.VI", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], (unsigned int)(SIGN_EXTEND(FX(data, 19, 15), 5)), (int)(SIGN_EXTEND(FX(data, 19, 15), 5)), riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VAND_VV: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VAND.VV", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_vpr[FX(data, 19, 15)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VAND_VX: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VAND.VX", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VANDN_VV: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VANDN.VV", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_vpr[FX(data, 19, 15)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VANDN_VX: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VANDN.VX", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VASUB_VV: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VASUB.VV", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_vpr[FX(data, 19, 15)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VASUB_VX: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VASUB.VX", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VASUBU_VV: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VASUBU.VV", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_vpr[FX(data, 19, 15)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VASUBU_VX: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VASUBU.VX", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VBREV_V: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "VBREV.V", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VBREV8_V: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "VBREV8.V", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VCLMUL_VV: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VCLMUL.VV", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_vpr[FX(data, 19, 15)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VCLMUL_VX: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VCLMUL.VX", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VCLMULH_VV: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VCLMULH.VV", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_vpr[FX(data, 19, 15)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VCLMULH_VX: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VCLMULH.VX", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VCLZ_V: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "VCLZ.V", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VCOMPRESS_VM: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "VCOMPRESS.VM", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_vpr[FX(data, 19, 15)]);
    } break;
    case RISCV_VCPOP_M: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "VCPOP.M", riscv_gpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VCPOP_V: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "VCPOP.V", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VCTZ_V: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "VCTZ.V", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VDIV_VV: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VDIV.VV", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_vpr[FX(data, 19, 15)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VDIV_VX: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VDIV.VX", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VDIVU_VV: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VDIVU.VV", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_vpr[FX(data, 19, 15)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VDIVU_VX: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VDIVU.VX", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VDOT4A_VV: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VDOT4A.VV", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_vpr[FX(data, 19, 15)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VDOT4A_VX: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VDOT4A.VX", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VDOT4ASU_VV: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VDOT4ASU.VV", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_vpr[FX(data, 19, 15)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VDOT4ASU_VX: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VDOT4ASU.VX", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VDOT4AU_VV: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VDOT4AU.VV", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_vpr[FX(data, 19, 15)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VDOT4AU_VX: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VDOT4AU.VX", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VDOT4AUS_VX: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VDOT4AUS.VX", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VFADD_VF: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VFADD.VF", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_fpr[FX(data, 19, 15)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VFADD_VV: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VFADD.VV", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_vpr[FX(data, 19, 15)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VFBDOTA_VV: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VFBDOTA.VV", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_vpr[FX(data, 19, 15)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VFCLASS_V: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "VFCLASS.V", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VFCVT_F_X_V: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "VFCVT.F.X.V", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VFCVT_F_XU_V: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "VFCVT.F.XU.V", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VFCVT_RTZ_X_F_V: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "VFCVT.RTZ.X.F.V", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VFCVT_RTZ_XU_F_V: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "VFCVT.RTZ.XU.F.V", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VFCVT_X_F_V: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "VFCVT.X.F.V", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VFCVT_XU_F_V: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "VFCVT.XU.F.V", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VFDIV_VF: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VFDIV.VF", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_fpr[FX(data, 19, 15)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VFDIV_VV: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VFDIV.VV", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_vpr[FX(data, 19, 15)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VFEXT_VF2: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "VFEXT.VF2", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VFIRST_M: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "VFIRST.M", riscv_gpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VFMACC_VF: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VFMACC.VF", riscv_vpr[FX(data, 11, 7)], riscv_fpr[FX(data, 19, 15)], riscv_vpr[FX(data, 24, 20)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VFMACC_VV: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VFMACC.VV", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 19, 15)], riscv_vpr[FX(data, 24, 20)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VFMADD_VF: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VFMADD.VF", riscv_vpr[FX(data, 11, 7)], riscv_fpr[FX(data, 19, 15)], riscv_vpr[FX(data, 24, 20)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VFMADD_VV: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VFMADD.VV", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 19, 15)], riscv_vpr[FX(data, 24, 20)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VFMAX_VF: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VFMAX.VF", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_fpr[FX(data, 19, 15)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VFMAX_VV: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VFMAX.VV", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_vpr[FX(data, 19, 15)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VFMERGE_VFM: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "VFMERGE.VFM", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_fpr[FX(data, 19, 15)]);
    } break;
    case RISCV_VFMIN_VF: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VFMIN.VF", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_fpr[FX(data, 19, 15)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VFMIN_VV: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VFMIN.VV", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_vpr[FX(data, 19, 15)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VFMSAC_VF: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VFMSAC.VF", riscv_vpr[FX(data, 11, 7)], riscv_fpr[FX(data, 19, 15)], riscv_vpr[FX(data, 24, 20)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VFMSAC_VV: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VFMSAC.VV", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 19, 15)], riscv_vpr[FX(data, 24, 20)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VFMSUB_VF: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VFMSUB.VF", riscv_vpr[FX(data, 11, 7)], riscv_fpr[FX(data, 19, 15)], riscv_vpr[FX(data, 24, 20)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VFMSUB_VV: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VFMSUB.VV", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 19, 15)], riscv_vpr[FX(data, 24, 20)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VFMUL_VF: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VFMUL.VF", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_fpr[FX(data, 19, 15)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VFMUL_VV: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VFMUL.VV", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_vpr[FX(data, 19, 15)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VFMV_F_S: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s", "VFMV.F.S", riscv_fpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)]);
    } break;
    case RISCV_VFMV_S_F: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s", "VFMV.S.F", riscv_vpr[FX(data, 11, 7)], riscv_fpr[FX(data, 19, 15)]);
    } break;
    case RISCV_VFMV_V_F: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s", "VFMV.V.F", riscv_vpr[FX(data, 11, 7)], riscv_fpr[FX(data, 19, 15)]);
    } break;
    case RISCV_VFNCVT_F_F_Q: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "VFNCVT.F.F.Q", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VFNCVT_F_F_W: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "VFNCVT.F.F.W", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VFNCVT_F_X_W: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "VFNCVT.F.X.W", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VFNCVT_F_XU_W: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "VFNCVT.F.XU.W", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VFNCVT_ROD_F_F_W: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "VFNCVT.ROD.F.F.W", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VFNCVT_RTZ_X_F_W: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "VFNCVT.RTZ.X.F.W", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VFNCVT_RTZ_XU_F_W: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "VFNCVT.RTZ.XU.F.W", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VFNCVT_SAT_F_F_Q: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "VFNCVT.SAT.F.F.Q", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VFNCVT_X_F_W: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "VFNCVT.X.F.W", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VFNCVT_XU_F_W: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "VFNCVT.XU.F.W", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VFNCVTBF16_F_F_W: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "VFNCVTBF16.F.F.W", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VFNCVTBF16_SAT_F_F_W: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "VFNCVTBF16.SAT.F.F.W", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VFNMACC_VF: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VFNMACC.VF", riscv_vpr[FX(data, 11, 7)], riscv_fpr[FX(data, 19, 15)], riscv_vpr[FX(data, 24, 20)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VFNMACC_VV: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VFNMACC.VV", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 19, 15)], riscv_vpr[FX(data, 24, 20)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VFNMADD_VF: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VFNMADD.VF", riscv_vpr[FX(data, 11, 7)], riscv_fpr[FX(data, 19, 15)], riscv_vpr[FX(data, 24, 20)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VFNMADD_VV: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VFNMADD.VV", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 19, 15)], riscv_vpr[FX(data, 24, 20)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VFNMSAC_VF: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VFNMSAC.VF", riscv_vpr[FX(data, 11, 7)], riscv_fpr[FX(data, 19, 15)], riscv_vpr[FX(data, 24, 20)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VFNMSAC_VV: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VFNMSAC.VV", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 19, 15)], riscv_vpr[FX(data, 24, 20)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VFNMSUB_VF: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VFNMSUB.VF", riscv_vpr[FX(data, 11, 7)], riscv_fpr[FX(data, 19, 15)], riscv_vpr[FX(data, 24, 20)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VFNMSUB_VV: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VFNMSUB.VV", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 19, 15)], riscv_vpr[FX(data, 24, 20)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VFQWBDOTA_ALT_VV: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VFQWBDOTA.ALT.VV", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_vpr[FX(data, 19, 15)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VFQWBDOTA_VV: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VFQWBDOTA.VV", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_vpr[FX(data, 19, 15)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VFQWDOTA_ALT_VV: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VFQWDOTA.ALT.VV", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_vpr[FX(data, 19, 15)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VFQWDOTA_VV: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VFQWDOTA.VV", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_vpr[FX(data, 19, 15)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VFRDIV_VF: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VFRDIV.VF", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_fpr[FX(data, 19, 15)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VFREC7_V: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "VFREC7.V", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VFREDMAX_VS: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VFREDMAX.VS", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_vpr[FX(data, 19, 15)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VFREDMIN_VS: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VFREDMIN.VS", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_vpr[FX(data, 19, 15)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VFREDOSUM_VS: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VFREDOSUM.VS", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_vpr[FX(data, 19, 15)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VFREDUSUM_VS: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VFREDUSUM.VS", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_vpr[FX(data, 19, 15)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VFRSQRT7_V: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "VFRSQRT7.V", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VFRSUB_VF: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VFRSUB.VF", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_fpr[FX(data, 19, 15)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VFSGNJ_VF: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VFSGNJ.VF", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_fpr[FX(data, 19, 15)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VFSGNJ_VV: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VFSGNJ.VV", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_vpr[FX(data, 19, 15)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VFSGNJN_VF: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VFSGNJN.VF", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_fpr[FX(data, 19, 15)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VFSGNJN_VV: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VFSGNJN.VV", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_vpr[FX(data, 19, 15)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VFSGNJX_VF: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VFSGNJX.VF", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_fpr[FX(data, 19, 15)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VFSGNJX_VV: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VFSGNJX.VV", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_vpr[FX(data, 19, 15)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VFSLIDE1DOWN_VF: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VFSLIDE1DOWN.VF", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_fpr[FX(data, 19, 15)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VFSLIDE1UP_VF: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VFSLIDE1UP.VF", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_fpr[FX(data, 19, 15)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VFSQRT_V: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "VFSQRT.V", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VFSUB_VF: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VFSUB.VF", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_fpr[FX(data, 19, 15)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VFSUB_VV: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VFSUB.VV", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_vpr[FX(data, 19, 15)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VFWADD_VF: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VFWADD.VF", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_fpr[FX(data, 19, 15)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VFWADD_VV: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VFWADD.VV", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_vpr[FX(data, 19, 15)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VFWADD_WF: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VFWADD.WF", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_fpr[FX(data, 19, 15)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VFWADD_WV: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VFWADD.WV", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_vpr[FX(data, 19, 15)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VFWBDOTA_VV: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VFWBDOTA.VV", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_vpr[FX(data, 19, 15)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VFWCVT_F_F_V: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "VFWCVT.F.F.V", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VFWCVT_F_X_V: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "VFWCVT.F.X.V", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VFWCVT_F_XU_V: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "VFWCVT.F.XU.V", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VFWCVT_RTZ_X_F_V: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "VFWCVT.RTZ.X.F.V", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VFWCVT_RTZ_XU_F_V: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "VFWCVT.RTZ.XU.F.V", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VFWCVT_X_F_V: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "VFWCVT.X.F.V", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VFWCVT_XU_F_V: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "VFWCVT.XU.F.V", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VFWCVTBF16_F_F_V: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "VFWCVTBF16.F.F.V", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VFWDOTA_VV: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VFWDOTA.VV", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_vpr[FX(data, 19, 15)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VFWMACC_VF: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VFWMACC.VF", riscv_vpr[FX(data, 11, 7)], riscv_fpr[FX(data, 19, 15)], riscv_vpr[FX(data, 24, 20)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VFWMACC_VV: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VFWMACC.VV", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 19, 15)], riscv_vpr[FX(data, 24, 20)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VFWMACCBF16_VF: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VFWMACCBF16.VF", riscv_vpr[FX(data, 11, 7)], riscv_fpr[FX(data, 19, 15)], riscv_vpr[FX(data, 24, 20)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VFWMACCBF16_VV: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VFWMACCBF16.VV", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 19, 15)], riscv_vpr[FX(data, 24, 20)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VFWMSAC_VF: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VFWMSAC.VF", riscv_vpr[FX(data, 11, 7)], riscv_fpr[FX(data, 19, 15)], riscv_vpr[FX(data, 24, 20)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VFWMSAC_VV: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VFWMSAC.VV", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 19, 15)], riscv_vpr[FX(data, 24, 20)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VFWMUL_VF: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VFWMUL.VF", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_fpr[FX(data, 19, 15)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VFWMUL_VV: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VFWMUL.VV", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_vpr[FX(data, 19, 15)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VFWNMACC_VF: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VFWNMACC.VF", riscv_vpr[FX(data, 11, 7)], riscv_fpr[FX(data, 19, 15)], riscv_vpr[FX(data, 24, 20)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VFWNMACC_VV: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VFWNMACC.VV", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 19, 15)], riscv_vpr[FX(data, 24, 20)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VFWNMSAC_VF: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VFWNMSAC.VF", riscv_vpr[FX(data, 11, 7)], riscv_fpr[FX(data, 19, 15)], riscv_vpr[FX(data, 24, 20)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VFWNMSAC_VV: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VFWNMSAC.VV", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 19, 15)], riscv_vpr[FX(data, 24, 20)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VFWREDOSUM_VS: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VFWREDOSUM.VS", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_vpr[FX(data, 19, 15)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VFWREDUSUM_VS: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VFWREDUSUM.VS", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_vpr[FX(data, 19, 15)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VFWSUB_VF: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VFWSUB.VF", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_fpr[FX(data, 19, 15)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VFWSUB_VV: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VFWSUB.VV", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_vpr[FX(data, 19, 15)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VFWSUB_WF: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VFWSUB.WF", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_fpr[FX(data, 19, 15)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VFWSUB_WV: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VFWSUB.WV", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_vpr[FX(data, 19, 15)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VGHSH_VV: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "VGHSH.VV", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_vpr[FX(data, 19, 15)]);
    } break;
    case RISCV_VGMUL_VV: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s", "VGMUL.VV", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)]);
    } break;
    case RISCV_VID_V: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s", "VID.V", riscv_vpr[FX(data, 11, 7)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VIOTA_M: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "VIOTA.M", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VL1RE16_V: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s", "VL1RE16.V", riscv_vpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)]);
    } break;
    case RISCV_VL1RE32_V: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s", "VL1RE32.V", riscv_vpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)]);
    } break;
    case RISCV_VL1RE64_V: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s", "VL1RE64.V", riscv_vpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)]);
    } break;
    case RISCV_VL1RE8_V: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s", "VL1RE8.V", riscv_vpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)]);
    } break;
    case RISCV_VL2RE16_V: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s", "VL2RE16.V", riscv_vpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)]);
    } break;
    case RISCV_VL2RE32_V: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s", "VL2RE32.V", riscv_vpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)]);
    } break;
    case RISCV_VL2RE64_V: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s", "VL2RE64.V", riscv_vpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)]);
    } break;
    case RISCV_VL2RE8_V: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s", "VL2RE8.V", riscv_vpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)]);
    } break;
    case RISCV_VL4RE16_V: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s", "VL4RE16.V", riscv_vpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)]);
    } break;
    case RISCV_VL4RE32_V: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s", "VL4RE32.V", riscv_vpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)]);
    } break;
    case RISCV_VL4RE64_V: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s", "VL4RE64.V", riscv_vpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)]);
    } break;
    case RISCV_VL4RE8_V: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s", "VL4RE8.V", riscv_vpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)]);
    } break;
    case RISCV_VL8RE16_V: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s", "VL8RE16.V", riscv_vpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)]);
    } break;
    case RISCV_VL8RE32_V: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s", "VL8RE32.V", riscv_vpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)]);
    } break;
    case RISCV_VL8RE64_V: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s", "VL8RE64.V", riscv_vpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)]);
    } break;
    case RISCV_VL8RE8_V: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s", "VL8RE8.V", riscv_vpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)]);
    } break;
    case RISCV_VLE16_V: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VLE16.V", riscv_vpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)], riscv_vm[FX(data, 25, 25)], riscv_nf[FX(data, 31, 29)]);
    } break;
    case RISCV_VLE16FF_V: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VLE16FF.V", riscv_vpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)], riscv_vm[FX(data, 25, 25)], riscv_nf[FX(data, 31, 29)]);
    } break;
    case RISCV_VLE32_V: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VLE32.V", riscv_vpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)], riscv_vm[FX(data, 25, 25)], riscv_nf[FX(data, 31, 29)]);
    } break;
    case RISCV_VLE32FF_V: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VLE32FF.V", riscv_vpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)], riscv_vm[FX(data, 25, 25)], riscv_nf[FX(data, 31, 29)]);
    } break;
    case RISCV_VLE64_V: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VLE64.V", riscv_vpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)], riscv_vm[FX(data, 25, 25)], riscv_nf[FX(data, 31, 29)]);
    } break;
    case RISCV_VLE64FF_V: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VLE64FF.V", riscv_vpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)], riscv_vm[FX(data, 25, 25)], riscv_nf[FX(data, 31, 29)]);
    } break;
    case RISCV_VLE8_V: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VLE8.V", riscv_vpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)], riscv_vm[FX(data, 25, 25)], riscv_nf[FX(data, 31, 29)]);
    } break;
    case RISCV_VLE8FF_V: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VLE8FF.V", riscv_vpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)], riscv_vm[FX(data, 25, 25)], riscv_nf[FX(data, 31, 29)]);
    } break;
    case RISCV_VLM_V: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s", "VLM.V", riscv_vpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)]);
    } break;
    case RISCV_VLOXEI16_V: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s, %s", "VLOXEI16.V", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_vm[FX(data, 25, 25)], riscv_nf[FX(data, 31, 29)]);
    } break;
    case RISCV_VLOXEI32_V: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s, %s", "VLOXEI32.V", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_vm[FX(data, 25, 25)], riscv_nf[FX(data, 31, 29)]);
    } break;
    case RISCV_VLOXEI64_V: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s, %s", "VLOXEI64.V", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_vm[FX(data, 25, 25)], riscv_nf[FX(data, 31, 29)]);
    } break;
    case RISCV_VLOXEI8_V: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s, %s", "VLOXEI8.V", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_vm[FX(data, 25, 25)], riscv_nf[FX(data, 31, 29)]);
    } break;
    case RISCV_VLSE16_V: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s, %s", "VLSE16.V", riscv_vpr[FX(data, 11, 7)], riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_vm[FX(data, 25, 25)], riscv_nf[FX(data, 31, 29)]);
    } break;
    case RISCV_VLSE32_V: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s, %s", "VLSE32.V", riscv_vpr[FX(data, 11, 7)], riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_vm[FX(data, 25, 25)], riscv_nf[FX(data, 31, 29)]);
    } break;
    case RISCV_VLSE64_V: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s, %s", "VLSE64.V", riscv_vpr[FX(data, 11, 7)], riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_vm[FX(data, 25, 25)], riscv_nf[FX(data, 31, 29)]);
    } break;
    case RISCV_VLSE8_V: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s, %s", "VLSE8.V", riscv_vpr[FX(data, 11, 7)], riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_vm[FX(data, 25, 25)], riscv_nf[FX(data, 31, 29)]);
    } break;
    case RISCV_VLUXEI16_V: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s, %s", "VLUXEI16.V", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_vm[FX(data, 25, 25)], riscv_nf[FX(data, 31, 29)]);
    } break;
    case RISCV_VLUXEI32_V: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s, %s", "VLUXEI32.V", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_vm[FX(data, 25, 25)], riscv_nf[FX(data, 31, 29)]);
    } break;
    case RISCV_VLUXEI64_V: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s, %s", "VLUXEI64.V", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_vm[FX(data, 25, 25)], riscv_nf[FX(data, 31, 29)]);
    } break;
    case RISCV_VLUXEI8_V: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s, %s", "VLUXEI8.V", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_vm[FX(data, 25, 25)], riscv_nf[FX(data, 31, 29)]);
    } break;
    case RISCV_VMACC_VV: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VMACC.VV", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 19, 15)], riscv_vpr[FX(data, 24, 20)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VMACC_VX: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VMACC.VX", riscv_vpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)], riscv_vpr[FX(data, 24, 20)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VMADC_VI: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, 0x%x(%d)", "VMADC.VI", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], (unsigned int)(SIGN_EXTEND(FX(data, 19, 15), 5)), (int)(SIGN_EXTEND(FX(data, 19, 15), 5)));
    } break;
    case RISCV_VMADC_VIM: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, 0x%x(%d), %s", "VMADC.VIM", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], (unsigned int)(SIGN_EXTEND(FX(data, 19, 15), 5)), (int)(SIGN_EXTEND(FX(data, 19, 15), 5)), "v0");
    } break;
    case RISCV_VMADC_VV: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "VMADC.VV", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_vpr[FX(data, 19, 15)]);
    } break;
    case RISCV_VMADC_VVM: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VMADC.VVM", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_vpr[FX(data, 19, 15)], "v0");
    } break;
    case RISCV_VMADC_VX: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "VMADC.VX", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)]);
    } break;
    case RISCV_VMADC_VXM: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VMADC.VXM", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], "v0");
    } break;
    case RISCV_VMADD_VV: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VMADD.VV", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 19, 15)], riscv_vpr[FX(data, 24, 20)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VMADD_VX: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VMADD.VX", riscv_vpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)], riscv_vpr[FX(data, 24, 20)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VMAND_MM: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "VMAND.MM", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_vpr[FX(data, 19, 15)]);
    } break;
    case RISCV_VMANDN_MM: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "VMANDN.MM", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_vpr[FX(data, 19, 15)]);
    } break;
    case RISCV_VMAX_VV: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VMAX.VV", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_vpr[FX(data, 19, 15)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VMAX_VX: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VMAX.VX", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VMAXU_VV: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VMAXU.VV", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_vpr[FX(data, 19, 15)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VMAXU_VX: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VMAXU.VX", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VMERGE_VIM: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, 0x%x(%d)", "VMERGE.VIM", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], (unsigned int)(SIGN_EXTEND(FX(data, 19, 15), 5)), (int)(SIGN_EXTEND(FX(data, 19, 15), 5)));
    } break;
    case RISCV_VMERGE_VVM: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "VMERGE.VVM", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_vpr[FX(data, 19, 15)]);
    } break;
    case RISCV_VMERGE_VXM: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "VMERGE.VXM", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)]);
    } break;
    case RISCV_VMFEQ_VF: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VMFEQ.VF", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_fpr[FX(data, 19, 15)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VMFEQ_VV: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VMFEQ.VV", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_vpr[FX(data, 19, 15)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VMFGE_VF: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VMFGE.VF", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_fpr[FX(data, 19, 15)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VMFGT_VF: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VMFGT.VF", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_fpr[FX(data, 19, 15)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VMFLE_VF: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VMFLE.VF", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_fpr[FX(data, 19, 15)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VMFLE_VV: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VMFLE.VV", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_vpr[FX(data, 19, 15)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VMFLT_VF: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VMFLT.VF", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_fpr[FX(data, 19, 15)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VMFLT_VV: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VMFLT.VV", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_vpr[FX(data, 19, 15)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VMFNE_VF: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VMFNE.VF", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_fpr[FX(data, 19, 15)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VMFNE_VV: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VMFNE.VV", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_vpr[FX(data, 19, 15)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VMIN_VV: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VMIN.VV", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_vpr[FX(data, 19, 15)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VMIN_VX: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VMIN.VX", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VMINU_VV: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VMINU.VV", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_vpr[FX(data, 19, 15)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VMINU_VX: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VMINU.VX", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VMNAND_MM: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "VMNAND.MM", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_vpr[FX(data, 19, 15)]);
    } break;
    case RISCV_VMNOR_MM: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "VMNOR.MM", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_vpr[FX(data, 19, 15)]);
    } break;
    case RISCV_VMOR_MM: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "VMOR.MM", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_vpr[FX(data, 19, 15)]);
    } break;
    case RISCV_VMORN_MM: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "VMORN.MM", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_vpr[FX(data, 19, 15)]);
    } break;
    case RISCV_VMSBC_VV: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "VMSBC.VV", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_vpr[FX(data, 19, 15)]);
    } break;
    case RISCV_VMSBC_VVM: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VMSBC.VVM", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_vpr[FX(data, 19, 15)], "v0");
    } break;
    case RISCV_VMSBC_VX: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "VMSBC.VX", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)]);
    } break;
    case RISCV_VMSBC_VXM: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VMSBC.VXM", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], "v0");
    } break;
    case RISCV_VMSBF_M: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "VMSBF.M", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VMSEQ_VI: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, 0x%x(%d), %s", "VMSEQ.VI", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], (unsigned int)(SIGN_EXTEND(FX(data, 19, 15), 5)), (int)(SIGN_EXTEND(FX(data, 19, 15), 5)), riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VMSEQ_VV: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VMSEQ.VV", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_vpr[FX(data, 19, 15)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VMSEQ_VX: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VMSEQ.VX", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VMSGT_VI: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, 0x%x(%d), %s", "VMSGT.VI", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], (unsigned int)(SIGN_EXTEND(FX(data, 19, 15), 5)), (int)(SIGN_EXTEND(FX(data, 19, 15), 5)), riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VMSGT_VX: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VMSGT.VX", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VMSGTU_VI: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, 0x%x(%d), %s", "VMSGTU.VI", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], (unsigned int)(SIGN_EXTEND(FX(data, 19, 15), 5)), (int)(SIGN_EXTEND(FX(data, 19, 15), 5)), riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VMSGTU_VX: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VMSGTU.VX", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VMSIF_M: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "VMSIF.M", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VMSLE_VI: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, 0x%x(%d), %s", "VMSLE.VI", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], (unsigned int)(SIGN_EXTEND(FX(data, 19, 15), 5)), (int)(SIGN_EXTEND(FX(data, 19, 15), 5)), riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VMSLE_VV: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VMSLE.VV", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_vpr[FX(data, 19, 15)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VMSLE_VX: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VMSLE.VX", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VMSLEU_VI: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, 0x%x(%d), %s", "VMSLEU.VI", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], (unsigned int)(SIGN_EXTEND(FX(data, 19, 15), 5)), (int)(SIGN_EXTEND(FX(data, 19, 15), 5)), riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VMSLEU_VV: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VMSLEU.VV", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_vpr[FX(data, 19, 15)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VMSLEU_VX: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VMSLEU.VX", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VMSLT_VV: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VMSLT.VV", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_vpr[FX(data, 19, 15)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VMSLT_VX: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VMSLT.VX", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VMSLTU_VV: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VMSLTU.VV", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_vpr[FX(data, 19, 15)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VMSLTU_VX: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VMSLTU.VX", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VMSNE_VI: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, 0x%x(%d), %s", "VMSNE.VI", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], (unsigned int)(SIGN_EXTEND(FX(data, 19, 15), 5)), (int)(SIGN_EXTEND(FX(data, 19, 15), 5)), riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VMSNE_VV: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VMSNE.VV", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_vpr[FX(data, 19, 15)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VMSNE_VX: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VMSNE.VX", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VMSOF_M: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "VMSOF.M", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VMUL_VV: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VMUL.VV", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_vpr[FX(data, 19, 15)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VMUL_VX: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VMUL.VX", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VMULH_VV: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VMULH.VV", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_vpr[FX(data, 19, 15)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VMULH_VX: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VMULH.VX", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VMULHSU_VV: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VMULHSU.VV", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_vpr[FX(data, 19, 15)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VMULHSU_VX: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VMULHSU.VX", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VMULHU_VV: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VMULHU.VV", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_vpr[FX(data, 19, 15)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VMULHU_VX: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VMULHU.VX", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VMV_S_X: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s", "VMV.S.X", riscv_vpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)]);
    } break;
    case RISCV_VMV_V_I: {
        snprintf(buff, sizeof(buff), "%-15s %s, 0x%x(%d)", "VMV.V.I", riscv_vpr[FX(data, 11, 7)], (unsigned int)(SIGN_EXTEND(FX(data, 19, 15), 5)), (int)(SIGN_EXTEND(FX(data, 19, 15), 5)));
    } break;
    case RISCV_VMV_V_V: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s", "VMV.V.V", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 19, 15)]);
    } break;
    case RISCV_VMV_V_X: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s", "VMV.V.X", riscv_vpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)]);
    } break;
    case RISCV_VMV_X_S: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s", "VMV.X.S", riscv_gpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)]);
    } break;
    case RISCV_VMV1R_V: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s", "VMV1R.V", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)]);
    } break;
    case RISCV_VMV2R_V: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s", "VMV2R.V", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)]);
    } break;
    case RISCV_VMV4R_V: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s", "VMV4R.V", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)]);
    } break;
    case RISCV_VMV8R_V: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s", "VMV8R.V", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)]);
    } break;
    case RISCV_VMXNOR_MM: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "VMXNOR.MM", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_vpr[FX(data, 19, 15)]);
    } break;
    case RISCV_VMXOR_MM: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "VMXOR.MM", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_vpr[FX(data, 19, 15)]);
    } break;
    case RISCV_VNCLIP_WI: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, 0x%x(%d), %s", "VNCLIP.WI", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], (unsigned int)(FX(data, 19, 15)), (int)(FX(data, 19, 15)), riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VNCLIP_WV: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VNCLIP.WV", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_vpr[FX(data, 19, 15)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VNCLIP_WX: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VNCLIP.WX", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VNCLIPU_WI: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, 0x%x(%d), %s", "VNCLIPU.WI", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], (unsigned int)(FX(data, 19, 15)), (int)(FX(data, 19, 15)), riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VNCLIPU_WV: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VNCLIPU.WV", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_vpr[FX(data, 19, 15)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VNCLIPU_WX: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VNCLIPU.WX", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VNMSAC_VV: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VNMSAC.VV", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 19, 15)], riscv_vpr[FX(data, 24, 20)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VNMSAC_VX: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VNMSAC.VX", riscv_vpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)], riscv_vpr[FX(data, 24, 20)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VNMSUB_VV: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VNMSUB.VV", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 19, 15)], riscv_vpr[FX(data, 24, 20)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VNMSUB_VX: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VNMSUB.VX", riscv_vpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)], riscv_vpr[FX(data, 24, 20)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VNSRA_WI: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, 0x%x(%d), %s", "VNSRA.WI", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], (unsigned int)(FX(data, 19, 15)), (int)(FX(data, 19, 15)), riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VNSRA_WV: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VNSRA.WV", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_vpr[FX(data, 19, 15)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VNSRA_WX: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VNSRA.WX", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VNSRL_WI: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, 0x%x(%d), %s", "VNSRL.WI", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], (unsigned int)(FX(data, 19, 15)), (int)(FX(data, 19, 15)), riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VNSRL_WV: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VNSRL.WV", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_vpr[FX(data, 19, 15)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VNSRL_WX: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VNSRL.WX", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VOR_VI: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, 0x%x(%d), %s", "VOR.VI", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], (unsigned int)(SIGN_EXTEND(FX(data, 19, 15), 5)), (int)(SIGN_EXTEND(FX(data, 19, 15), 5)), riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VOR_VV: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VOR.VV", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_vpr[FX(data, 19, 15)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VOR_VX: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VOR.VX", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VPAIRE_VV: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VPAIRE.VV", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_vpr[FX(data, 19, 15)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VPAIRO_VV: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VPAIRO.VV", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_vpr[FX(data, 19, 15)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VQWBDOTAS_VV: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VQWBDOTAS.VV", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_vpr[FX(data, 19, 15)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VQWBDOTAU_VV: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VQWBDOTAU.VV", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_vpr[FX(data, 19, 15)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VQWDOTAS_VV: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VQWDOTAS.VV", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_vpr[FX(data, 19, 15)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VQWDOTAU_VV: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VQWDOTAU.VV", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_vpr[FX(data, 19, 15)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VREDAND_VS: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VREDAND.VS", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_vpr[FX(data, 19, 15)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VREDMAX_VS: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VREDMAX.VS", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_vpr[FX(data, 19, 15)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VREDMAXU_VS: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VREDMAXU.VS", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_vpr[FX(data, 19, 15)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VREDMIN_VS: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VREDMIN.VS", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_vpr[FX(data, 19, 15)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VREDMINU_VS: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VREDMINU.VS", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_vpr[FX(data, 19, 15)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VREDOR_VS: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VREDOR.VS", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_vpr[FX(data, 19, 15)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VREDSUM_VS: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VREDSUM.VS", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_vpr[FX(data, 19, 15)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VREDXOR_VS: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VREDXOR.VS", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_vpr[FX(data, 19, 15)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VREM_VV: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VREM.VV", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_vpr[FX(data, 19, 15)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VREM_VX: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VREM.VX", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VREMU_VV: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VREMU.VV", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_vpr[FX(data, 19, 15)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VREMU_VX: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VREMU.VX", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VREV8_V: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "VREV8.V", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VRGATHER_VI: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, 0x%x(%d), %s", "VRGATHER.VI", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], (unsigned int)(FX(data, 19, 15)), (int)(FX(data, 19, 15)), riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VRGATHER_VV: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VRGATHER.VV", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_vpr[FX(data, 19, 15)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VRGATHER_VX: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VRGATHER.VX", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VRGATHEREI16_VV: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VRGATHEREI16.VV", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_vpr[FX(data, 19, 15)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VROL_VV: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VROL.VV", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_vpr[FX(data, 19, 15)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VROL_VX: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VROL.VX", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VROR_VI: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, 0x%x(%d), %s", "VROR.VI", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], (unsigned int)((FX(data, 26, 26) << 5) | FX(data, 19, 15)), (int)((FX(data, 26, 26) << 5) | FX(data, 19, 15)), riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VROR_VV: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VROR.VV", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_vpr[FX(data, 19, 15)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VROR_VX: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VROR.VX", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VRSUB_VI: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, 0x%x(%d), %s", "VRSUB.VI", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], (unsigned int)(SIGN_EXTEND(FX(data, 19, 15), 5)), (int)(SIGN_EXTEND(FX(data, 19, 15), 5)), riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VRSUB_VX: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VRSUB.VX", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VS1R_V: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s", "VS1R.V", riscv_vpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)]);
    } break;
    case RISCV_VS2R_V: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s", "VS2R.V", riscv_vpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)]);
    } break;
    case RISCV_VS4R_V: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s", "VS4R.V", riscv_vpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)]);
    } break;
    case RISCV_VS8R_V: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s", "VS8R.V", riscv_vpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)]);
    } break;
    case RISCV_VSADD_VI: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, 0x%x(%d), %s", "VSADD.VI", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], (unsigned int)(SIGN_EXTEND(FX(data, 19, 15), 5)), (int)(SIGN_EXTEND(FX(data, 19, 15), 5)), riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VSADD_VV: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VSADD.VV", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_vpr[FX(data, 19, 15)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VSADD_VX: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VSADD.VX", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VSADDU_VI: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, 0x%x(%d), %s", "VSADDU.VI", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], (unsigned int)(SIGN_EXTEND(FX(data, 19, 15), 5)), (int)(SIGN_EXTEND(FX(data, 19, 15), 5)), riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VSADDU_VV: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VSADDU.VV", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_vpr[FX(data, 19, 15)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VSADDU_VX: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VSADDU.VX", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VSBC_VVM: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VSBC.VVM", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_vpr[FX(data, 19, 15)], "v0");
    } break;
    case RISCV_VSBC_VXM: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VSBC.VXM", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], "v0");
    } break;
    case RISCV_VSE16_V: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VSE16.V", riscv_vpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)], riscv_vm[FX(data, 25, 25)], riscv_nf[FX(data, 31, 29)]);
    } break;
    case RISCV_VSE32_V: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VSE32.V", riscv_vpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)], riscv_vm[FX(data, 25, 25)], riscv_nf[FX(data, 31, 29)]);
    } break;
    case RISCV_VSE64_V: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VSE64.V", riscv_vpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)], riscv_vm[FX(data, 25, 25)], riscv_nf[FX(data, 31, 29)]);
    } break;
    case RISCV_VSE8_V: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VSE8.V", riscv_vpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)], riscv_vm[FX(data, 25, 25)], riscv_nf[FX(data, 31, 29)]);
    } break;
    case RISCV_VSETIVLI: {
        snprintf(buff, sizeof(buff), "%-15s %s, %d, %s, %s, %s, %s", "VSETIVLI", riscv_gpr[FX(data, 11, 7)], (int)(FX(data, 19, 15)), riscv_sew[(FX(data, 30, 20) >> 3) & 7], riscv_lmul[FX(data, 30, 20) & 7], riscv_vt[(FX(data, 30, 20) >> 6) & 1], riscv_vm_[(FX(data, 30, 20) >> 7) & 1]);
    } break;
    case RISCV_VSETVL: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "VSETVL", riscv_gpr[FX(data, 11, 7)], riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)]);
    } break;
    case RISCV_VSETVLI: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s, %s, %s", "VSETVLI", riscv_gpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)], riscv_sew[(FX(data, 30, 20) >> 3) & 7], riscv_lmul[FX(data, 30, 20) & 7], riscv_vt[(FX(data, 30, 20) >> 6) & 1], riscv_vm_[(FX(data, 30, 20) >> 7) & 1]);
    } break;
    case RISCV_VSEXT_VF2: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "VSEXT.VF2", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VSEXT_VF4: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "VSEXT.VF4", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VSEXT_VF8: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "VSEXT.VF8", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VSHA2CH_VV: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "VSHA2CH.VV", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_vpr[FX(data, 19, 15)]);
    } break;
    case RISCV_VSHA2CL_VV: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "VSHA2CL.VV", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_vpr[FX(data, 19, 15)]);
    } break;
    case RISCV_VSHA2MS_VV: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "VSHA2MS.VV", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_vpr[FX(data, 19, 15)]);
    } break;
    case RISCV_VSLIDE1DOWN_VX: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VSLIDE1DOWN.VX", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VSLIDE1UP_VX: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VSLIDE1UP.VX", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VSLIDEDOWN_VI: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, 0x%x(%d), %s", "VSLIDEDOWN.VI", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], (unsigned int)(FX(data, 19, 15)), (int)(FX(data, 19, 15)), riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VSLIDEDOWN_VX: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VSLIDEDOWN.VX", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VSLIDEUP_VI: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, 0x%x(%d), %s", "VSLIDEUP.VI", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], (unsigned int)(FX(data, 19, 15)), (int)(FX(data, 19, 15)), riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VSLIDEUP_VX: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VSLIDEUP.VX", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VSLL_VI: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, 0x%x(%d), %s", "VSLL.VI", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], (unsigned int)(FX(data, 19, 15)), (int)(FX(data, 19, 15)), riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VSLL_VV: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VSLL.VV", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_vpr[FX(data, 19, 15)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VSLL_VX: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VSLL.VX", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VSM_V: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s", "VSM.V", riscv_vpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)]);
    } break;
    case RISCV_VSM3C_VI: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, 0x%x(%d)", "VSM3C.VI", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], (unsigned int)(FX(data, 19, 15)), (int)(FX(data, 19, 15)));
    } break;
    case RISCV_VSM3ME_VV: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "VSM3ME.VV", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_vpr[FX(data, 19, 15)]);
    } break;
    case RISCV_VSM4K_VI: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, 0x%x(%d)", "VSM4K.VI", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], (unsigned int)(FX(data, 19, 15)), (int)(FX(data, 19, 15)));
    } break;
    case RISCV_VSM4R_VS: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s", "VSM4R.VS", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)]);
    } break;
    case RISCV_VSM4R_VV: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s", "VSM4R.VV", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)]);
    } break;
    case RISCV_VSMUL_VV: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VSMUL.VV", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_vpr[FX(data, 19, 15)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VSMUL_VX: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VSMUL.VX", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VSOXEI16_V: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s, %s", "VSOXEI16.V", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_vm[FX(data, 25, 25)], riscv_nf[FX(data, 31, 29)]);
    } break;
    case RISCV_VSOXEI32_V: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s, %s", "VSOXEI32.V", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_vm[FX(data, 25, 25)], riscv_nf[FX(data, 31, 29)]);
    } break;
    case RISCV_VSOXEI64_V: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s, %s", "VSOXEI64.V", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_vm[FX(data, 25, 25)], riscv_nf[FX(data, 31, 29)]);
    } break;
    case RISCV_VSOXEI8_V: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s, %s", "VSOXEI8.V", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_vm[FX(data, 25, 25)], riscv_nf[FX(data, 31, 29)]);
    } break;
    case RISCV_VSRA_VI: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, 0x%x(%d), %s", "VSRA.VI", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], (unsigned int)(FX(data, 19, 15)), (int)(FX(data, 19, 15)), riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VSRA_VV: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VSRA.VV", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_vpr[FX(data, 19, 15)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VSRA_VX: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VSRA.VX", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VSRL_VI: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, 0x%x(%d), %s", "VSRL.VI", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], (unsigned int)(FX(data, 19, 15)), (int)(FX(data, 19, 15)), riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VSRL_VV: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VSRL.VV", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_vpr[FX(data, 19, 15)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VSRL_VX: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VSRL.VX", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VSSE16_V: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s, %s", "VSSE16.V", riscv_vpr[FX(data, 11, 7)], riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_vm[FX(data, 25, 25)], riscv_nf[FX(data, 31, 29)]);
    } break;
    case RISCV_VSSE32_V: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s, %s", "VSSE32.V", riscv_vpr[FX(data, 11, 7)], riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_vm[FX(data, 25, 25)], riscv_nf[FX(data, 31, 29)]);
    } break;
    case RISCV_VSSE64_V: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s, %s", "VSSE64.V", riscv_vpr[FX(data, 11, 7)], riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_vm[FX(data, 25, 25)], riscv_nf[FX(data, 31, 29)]);
    } break;
    case RISCV_VSSE8_V: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s, %s", "VSSE8.V", riscv_vpr[FX(data, 11, 7)], riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_vm[FX(data, 25, 25)], riscv_nf[FX(data, 31, 29)]);
    } break;
    case RISCV_VSSRA_VI: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, 0x%x(%d), %s", "VSSRA.VI", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], (unsigned int)(FX(data, 19, 15)), (int)(FX(data, 19, 15)), riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VSSRA_VV: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VSSRA.VV", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_vpr[FX(data, 19, 15)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VSSRA_VX: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VSSRA.VX", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VSSRL_VI: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, 0x%x(%d), %s", "VSSRL.VI", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], (unsigned int)(FX(data, 19, 15)), (int)(FX(data, 19, 15)), riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VSSRL_VV: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VSSRL.VV", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_vpr[FX(data, 19, 15)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VSSRL_VX: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VSSRL.VX", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VSSUB_VV: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VSSUB.VV", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_vpr[FX(data, 19, 15)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VSSUB_VX: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VSSUB.VX", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VSSUBU_VV: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VSSUBU.VV", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_vpr[FX(data, 19, 15)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VSSUBU_VX: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VSSUBU.VX", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VSUB_VV: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VSUB.VV", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_vpr[FX(data, 19, 15)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VSUB_VX: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VSUB.VX", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VSUXEI16_V: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s, %s", "VSUXEI16.V", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_vm[FX(data, 25, 25)], riscv_nf[FX(data, 31, 29)]);
    } break;
    case RISCV_VSUXEI32_V: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s, %s", "VSUXEI32.V", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_vm[FX(data, 25, 25)], riscv_nf[FX(data, 31, 29)]);
    } break;
    case RISCV_VSUXEI64_V: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s, %s", "VSUXEI64.V", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_vm[FX(data, 25, 25)], riscv_nf[FX(data, 31, 29)]);
    } break;
    case RISCV_VSUXEI8_V: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s, %s", "VSUXEI8.V", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_vm[FX(data, 25, 25)], riscv_nf[FX(data, 31, 29)]);
    } break;
    case RISCV_VUNZIPE_V: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "VUNZIPE.V", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VUNZIPO_V: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "VUNZIPO.V", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VWABDA_VV: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VWABDA.VV", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_vpr[FX(data, 19, 15)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VWABDAU_VV: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VWABDAU.VV", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_vpr[FX(data, 19, 15)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VWADD_VV: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VWADD.VV", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_vpr[FX(data, 19, 15)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VWADD_VX: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VWADD.VX", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VWADD_WV: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VWADD.WV", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_vpr[FX(data, 19, 15)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VWADD_WX: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VWADD.WX", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VWADDU_VV: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VWADDU.VV", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_vpr[FX(data, 19, 15)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VWADDU_VX: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VWADDU.VX", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VWADDU_WV: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VWADDU.WV", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_vpr[FX(data, 19, 15)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VWADDU_WX: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VWADDU.WX", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VWMACC_VV: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VWMACC.VV", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 19, 15)], riscv_vpr[FX(data, 24, 20)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VWMACC_VX: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VWMACC.VX", riscv_vpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)], riscv_vpr[FX(data, 24, 20)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VWMACCSU_VV: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VWMACCSU.VV", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 19, 15)], riscv_vpr[FX(data, 24, 20)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VWMACCSU_VX: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VWMACCSU.VX", riscv_vpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)], riscv_vpr[FX(data, 24, 20)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VWMACCU_VV: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VWMACCU.VV", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 19, 15)], riscv_vpr[FX(data, 24, 20)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VWMACCU_VX: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VWMACCU.VX", riscv_vpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)], riscv_vpr[FX(data, 24, 20)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VWMACCUS_VX: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VWMACCUS.VX", riscv_vpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)], riscv_vpr[FX(data, 24, 20)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VWMUL_VV: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VWMUL.VV", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_vpr[FX(data, 19, 15)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VWMUL_VX: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VWMUL.VX", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VWMULSU_VV: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VWMULSU.VV", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_vpr[FX(data, 19, 15)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VWMULSU_VX: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VWMULSU.VX", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VWMULU_VV: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VWMULU.VV", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_vpr[FX(data, 19, 15)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VWMULU_VX: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VWMULU.VX", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VWREDSUM_VS: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VWREDSUM.VS", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_vpr[FX(data, 19, 15)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VWREDSUMU_VS: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VWREDSUMU.VS", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_vpr[FX(data, 19, 15)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VWSLL_VI: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, 0x%x(%d), %s", "VWSLL.VI", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], (unsigned int)(FX(data, 19, 15)), (int)(FX(data, 19, 15)), riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VWSLL_VV: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VWSLL.VV", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_vpr[FX(data, 19, 15)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VWSLL_VX: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VWSLL.VX", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VWSUB_VV: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VWSUB.VV", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_vpr[FX(data, 19, 15)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VWSUB_VX: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VWSUB.VX", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VWSUB_WV: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VWSUB.WV", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_vpr[FX(data, 19, 15)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VWSUB_WX: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VWSUB.WX", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VWSUBU_VV: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VWSUBU.VV", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_vpr[FX(data, 19, 15)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VWSUBU_VX: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VWSUBU.VX", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VWSUBU_WV: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VWSUBU.WV", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_vpr[FX(data, 19, 15)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VWSUBU_WX: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VWSUBU.WX", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VXOR_VI: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, 0x%x(%d), %s", "VXOR.VI", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], (unsigned int)(SIGN_EXTEND(FX(data, 19, 15), 5)), (int)(SIGN_EXTEND(FX(data, 19, 15), 5)), riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VXOR_VV: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VXOR.VV", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_vpr[FX(data, 19, 15)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VXOR_VX: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VXOR.VX", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VZEXT_VF2: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "VZEXT.VF2", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VZEXT_VF4: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "VZEXT.VF4", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VZEXT_VF8: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "VZEXT.VF8", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_VZIP_VV: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s, %s", "VZIP.VV", riscv_vpr[FX(data, 11, 7)], riscv_vpr[FX(data, 24, 20)], riscv_vpr[FX(data, 19, 15)], riscv_vm[FX(data, 25, 25)]);
    } break;
    case RISCV_WADD: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, (%s, %s)", "WADD", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[2 * FX(data, 11, 8)], riscv_gpr[2 * FX(data, 11, 8) + 1]);
    } break;
    case RISCV_WADDA: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, (%s, %s)", "WADDA", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[2 * FX(data, 11, 8)], riscv_gpr[2 * FX(data, 11, 8) + 1]);
    } break;
    case RISCV_WADDAU: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, (%s, %s)", "WADDAU", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[2 * FX(data, 11, 8)], riscv_gpr[2 * FX(data, 11, 8) + 1]);
    } break;
    case RISCV_WADDU: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, (%s, %s)", "WADDU", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[2 * FX(data, 11, 8)], riscv_gpr[2 * FX(data, 11, 8) + 1]);
    } break;
    case RISCV_WFI: {
        snprintf(buff, sizeof(buff), "%-15s ", "WFI");
    } break;
    case RISCV_WMACC: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, (%s, %s)", "WMACC", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[2 * FX(data, 11, 8)], riscv_gpr[2 * FX(data, 11, 8) + 1]);
    } break;
    case RISCV_WMACCSU: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, (%s, %s)", "WMACCSU", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[2 * FX(data, 11, 8)], riscv_gpr[2 * FX(data, 11, 8) + 1]);
    } break;
    case RISCV_WMACCU: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, (%s, %s)", "WMACCU", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[2 * FX(data, 11, 8)], riscv_gpr[2 * FX(data, 11, 8) + 1]);
    } break;
    case RISCV_WMUL: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, (%s, %s)", "WMUL", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[2 * FX(data, 11, 8)], riscv_gpr[2 * FX(data, 11, 8) + 1]);
    } break;
    case RISCV_WMULSU: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, (%s, %s)", "WMULSU", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[2 * FX(data, 11, 8)], riscv_gpr[2 * FX(data, 11, 8) + 1]);
    } break;
    case RISCV_WMULU: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, (%s, %s)", "WMULU", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[2 * FX(data, 11, 8)], riscv_gpr[2 * FX(data, 11, 8) + 1]);
    } break;
    case RISCV_WRS_NTO: {
        snprintf(buff, sizeof(buff), "%-15s ", "WRS.NTO");
    } break;
    case RISCV_WRS_STO: {
        snprintf(buff, sizeof(buff), "%-15s ", "WRS.STO");
    } break;
    case RISCV_WSLA: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, (%s, %s)", "WSLA", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[2 * FX(data, 11, 8)], riscv_gpr[2 * FX(data, 11, 8) + 1]);
    } break;
    case RISCV_WSLAI: {
        snprintf(buff, sizeof(buff), "%-15s 0x%x(%d), %s, (%s, %s)", "WSLAI", (unsigned int)(FX(data, 25, 20)), (int)(FX(data, 25, 20)), riscv_gpr[FX(data, 19, 15)], riscv_gpr[2 * FX(data, 11, 8)], riscv_gpr[2 * FX(data, 11, 8) + 1]);
    } break;
    case RISCV_WSLL: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, (%s, %s)", "WSLL", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[2 * FX(data, 11, 8)], riscv_gpr[2 * FX(data, 11, 8) + 1]);
    } break;
    case RISCV_WSLLI: {
        snprintf(buff, sizeof(buff), "%-15s 0x%x(%d), %s, (%s, %s)", "WSLLI", (unsigned int)(FX(data, 25, 20)), (int)(FX(data, 25, 20)), riscv_gpr[FX(data, 19, 15)], riscv_gpr[2 * FX(data, 11, 8)], riscv_gpr[2 * FX(data, 11, 8) + 1]);
    } break;
    case RISCV_WSUB: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, (%s, %s)", "WSUB", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[2 * FX(data, 11, 8)], riscv_gpr[2 * FX(data, 11, 8) + 1]);
    } break;
    case RISCV_WSUBA: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, (%s, %s)", "WSUBA", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[2 * FX(data, 11, 8)], riscv_gpr[2 * FX(data, 11, 8) + 1]);
    } break;
    case RISCV_WSUBAU: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, (%s, %s)", "WSUBAU", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[2 * FX(data, 11, 8)], riscv_gpr[2 * FX(data, 11, 8) + 1]);
    } break;
    case RISCV_WSUBU: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, (%s, %s)", "WSUBU", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[2 * FX(data, 11, 8)], riscv_gpr[2 * FX(data, 11, 8) + 1]);
    } break;
    case RISCV_WZIP16P: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, (%s, %s)", "WZIP16P", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[2 * FX(data, 11, 8)], riscv_gpr[2 * FX(data, 11, 8) + 1]);
    } break;
    case RISCV_WZIP8P: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, (%s, %s)", "WZIP8P", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[2 * FX(data, 11, 8)], riscv_gpr[2 * FX(data, 11, 8) + 1]);
    } break;
    case RISCV_XNOR: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "XNOR", riscv_gpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 24, 20)]);
    } break;
    case RISCV_XOR: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "XOR", riscv_gpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 24, 20)]);
    } break;
    case RISCV_XORI: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, 0x%x(%d)", "XORI", riscv_gpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)], (unsigned int)(SIGN_EXTEND(FX(data, 31, 20), 12)), (int)(SIGN_EXTEND(FX(data, 31, 20), 12)));
    } break;
    case RISCV_XPERM16: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "XPERM16", riscv_gpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 24, 20)]);
    } break;
    case RISCV_XPERM32: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "XPERM32", riscv_gpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 24, 20)]);
    } break;
    case RISCV_XPERM4: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "XPERM4", riscv_gpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 24, 20)]);
    } break;
    case RISCV_XPERM8: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "XPERM8", riscv_gpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 24, 20)]);
    } break;
    case RISCV_ZIP: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s", "ZIP", riscv_gpr[FX(data, 11, 7)], riscv_gpr[FX(data, 19, 15)]);
    } break;
    case RISCV_ZIP16HP: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "ZIP16HP", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_ZIP16P: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "ZIP16P", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_ZIP8HP: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "ZIP8HP", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    case RISCV_ZIP8P: {
        snprintf(buff, sizeof(buff), "%-15s %s, %s, %s", "ZIP8P", riscv_gpr[FX(data, 24, 20)], riscv_gpr[FX(data, 19, 15)], riscv_gpr[FX(data, 11, 7)]);
    } break;
    default:
        snprintf(buff, sizeof(buff), "%08X ???", __builtin_bswap32(data));
        break;
    }
    return std::string(buff);
}
