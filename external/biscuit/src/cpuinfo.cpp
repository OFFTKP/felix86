// Copyright (c), 2022, KNS Group LLC (YADRO)
//
// Use of this source code is governed by an MIT-style
// license that can be found in the LICENSE file or at
// https://opensource.org/licenses/MIT.

#include <biscuit/assert.hpp>
#include <biscuit/cpuinfo.hpp>

#if defined(__linux__) && defined(__riscv)
#include <csignal>
#include <utility>
#include <asm/hwcap.h>
#include <sys/auxv.h>
#include <sys/mman.h>
#include <sys/prctl.h>
#include <sys/syscall.h>
#include <unistd.h>

#ifdef SYS_riscv_hwprobe
#include <asm/hwprobe.h>
#endif
#endif

#ifndef RISCV_HWPROBE_BASE_BEHAVIOR_IMA
#define RISCV_HWPROBE_BASE_BEHAVIOR_IMA (1ULL << 0)
#endif

#ifndef RISCV_HWPROBE_IMA_FD
#define RISCV_HWPROBE_IMA_FD (1ULL << 0)
#endif

#ifndef RISCV_HWPROBE_IMA_C
#define RISCV_HWPROBE_IMA_C (1ULL << 1)
#endif

#ifndef RISCV_HWPROBE_IMA_V
#define RISCV_HWPROBE_IMA_V (1ULL << 2)
#endif

#ifndef RISCV_HWPROBE_EXT_ZBA
#define RISCV_HWPROBE_EXT_ZBA (1ULL << 3)
#endif

#ifndef RISCV_HWPROBE_EXT_ZBB
#define RISCV_HWPROBE_EXT_ZBB (1ULL << 4)
#endif

#ifndef RISCV_HWPROBE_EXT_ZBS
#define RISCV_HWPROBE_EXT_ZBS (1ULL << 5)
#endif

#ifndef RISCV_HWPROBE_EXT_ZICBOZ
#define RISCV_HWPROBE_EXT_ZICBOZ (1ULL << 6)
#endif

#ifndef RISCV_HWPROBE_EXT_ZBC
#define RISCV_HWPROBE_EXT_ZBC (1ULL << 7)
#endif

#ifndef RISCV_HWPROBE_EXT_ZBKB
#define RISCV_HWPROBE_EXT_ZBKB (1ULL << 8)
#endif

#ifndef RISCV_HWPROBE_EXT_ZBKC
#define RISCV_HWPROBE_EXT_ZBKC (1ULL << 9)
#endif

#ifndef RISCV_HWPROBE_EXT_ZBKX
#define RISCV_HWPROBE_EXT_ZBKX (1ULL << 10)
#endif

#ifndef RISCV_HWPROBE_EXT_ZKND
#define RISCV_HWPROBE_EXT_ZKND (1ULL << 11)
#endif

#ifndef RISCV_HWPROBE_EXT_ZKNE
#define RISCV_HWPROBE_EXT_ZKNE (1ULL << 12)
#endif

#ifndef RISCV_HWPROBE_EXT_ZKNH
#define RISCV_HWPROBE_EXT_ZKNH (1ULL << 13)
#endif

#ifndef RISCV_HWPROBE_EXT_ZKSED
#define RISCV_HWPROBE_EXT_ZKSED (1ULL << 14)
#endif

#ifndef RISCV_HWPROBE_EXT_ZKSH
#define RISCV_HWPROBE_EXT_ZKSH (1ULL << 15)
#endif

#ifndef RISCV_HWPROBE_EXT_ZKT
#define RISCV_HWPROBE_EXT_ZKT (1ULL << 16)
#endif

#ifndef RISCV_HWPROBE_EXT_ZVBB
#define RISCV_HWPROBE_EXT_ZVBB (1ULL << 17)
#endif

#ifndef RISCV_HWPROBE_EXT_ZVBC
#define RISCV_HWPROBE_EXT_ZVBC (1ULL << 18)
#endif

#ifndef RISCV_HWPROBE_EXT_ZVKB
#define RISCV_HWPROBE_EXT_ZVKB (1ULL << 19)
#endif

#ifndef RISCV_HWPROBE_EXT_ZVKG
#define RISCV_HWPROBE_EXT_ZVKG (1ULL << 20)
#endif

#ifndef RISCV_HWPROBE_EXT_ZVKNED
#define RISCV_HWPROBE_EXT_ZVKNED (1ULL << 21)
#endif

#ifndef RISCV_HWPROBE_EXT_ZVKNHA
#define RISCV_HWPROBE_EXT_ZVKNHA (1ULL << 22)
#endif

#ifndef RISCV_HWPROBE_EXT_ZVKNHB
#define RISCV_HWPROBE_EXT_ZVKNHB (1ULL << 23)
#endif

#ifndef RISCV_HWPROBE_EXT_ZVKSED
#define RISCV_HWPROBE_EXT_ZVKSED (1ULL << 24)
#endif

#ifndef RISCV_HWPROBE_EXT_ZVKSH
#define RISCV_HWPROBE_EXT_ZVKSH (1ULL << 25)
#endif

#ifndef RISCV_HWPROBE_EXT_ZVKT
#define RISCV_HWPROBE_EXT_ZVKT (1ULL << 26)
#endif

#ifndef RISCV_HWPROBE_EXT_ZFH
#define RISCV_HWPROBE_EXT_ZFH (1ULL << 27)
#endif

#ifndef RISCV_HWPROBE_EXT_ZFHMIN
#define RISCV_HWPROBE_EXT_ZFHMIN (1ULL << 28)
#endif

#ifndef RISCV_HWPROBE_EXT_ZIHINTNTL
#define RISCV_HWPROBE_EXT_ZIHINTNTL (1ULL << 29)
#endif

#ifndef RISCV_HWPROBE_EXT_ZVFH
#define RISCV_HWPROBE_EXT_ZVFH (1ULL << 30)
#endif

#ifndef RISCV_HWPROBE_EXT_ZVFHMIN
#define RISCV_HWPROBE_EXT_ZVFHMIN (1ULL << 31)
#endif

#ifndef RISCV_HWPROBE_EXT_ZFA
#define RISCV_HWPROBE_EXT_ZFA (1ULL << 32)
#endif

#ifndef RISCV_HWPROBE_EXT_ZTSO
#define RISCV_HWPROBE_EXT_ZTSO (1ULL << 33)
#endif

#ifndef RISCV_HWPROBE_EXT_ZACAS
#define RISCV_HWPROBE_EXT_ZACAS (1ULL << 34)
#endif

#ifndef RISCV_HWPROBE_EXT_ZICOND
#define RISCV_HWPROBE_EXT_ZICOND (1ULL << 35)
#endif

#ifndef RISCV_HWPROBE_EXT_ZIHINTPAUSE
#define RISCV_HWPROBE_EXT_ZIHINTPAUSE (1ULL << 36)
#endif

#ifndef RISCV_HWPROBE_EXT_ZVE32X
#define RISCV_HWPROBE_EXT_ZVE32X (1ULL << 37)
#endif

#ifndef RISCV_HWPROBE_EXT_ZVE32F
#define RISCV_HWPROBE_EXT_ZVE32F (1ULL << 38)
#endif

#ifndef RISCV_HWPROBE_EXT_ZVE64X
#define RISCV_HWPROBE_EXT_ZVE64X (1ULL << 39)
#endif

#ifndef RISCV_HWPROBE_EXT_ZVE64F
#define RISCV_HWPROBE_EXT_ZVE64F (1ULL << 40)
#endif

#ifndef RISCV_HWPROBE_EXT_ZVE64D
#define RISCV_HWPROBE_EXT_ZVE64D (1ULL << 41)
#endif

#ifndef RISCV_HWPROBE_EXT_ZIMOP
#define RISCV_HWPROBE_EXT_ZIMOP (1ULL << 42)
#endif

#ifndef RISCV_HWPROBE_EXT_ZCA
#define RISCV_HWPROBE_EXT_ZCA (1ULL << 43)
#endif

#ifndef RISCV_HWPROBE_EXT_ZCB
#define RISCV_HWPROBE_EXT_ZCB (1ULL << 44)
#endif

#ifndef RISCV_HWPROBE_EXT_ZCD
#define RISCV_HWPROBE_EXT_ZCD (1ULL << 45)
#endif

#ifndef RISCV_HWPROBE_EXT_ZCF
#define RISCV_HWPROBE_EXT_ZCF (1ULL << 46)
#endif

#ifndef RISCV_HWPROBE_EXT_ZCMOP
#define RISCV_HWPROBE_EXT_ZCMOP (1ULL << 47)
#endif

#ifndef RISCV_HWPROBE_EXT_ZAWRS
#define RISCV_HWPROBE_EXT_ZAWRS (1ULL << 48)
#endif

#ifndef RISCV_HWPROBE_EXT_SUPM
#define RISCV_HWPROBE_EXT_SUPM (1ULL << 49)
#endif

#ifndef RISCV_HWPROBE_EXT_ZICNTR
#define RISCV_HWPROBE_EXT_ZICNTR (1ULL << 50)
#endif

#ifndef RISCV_HWPROBE_EXT_ZIHPM
#define RISCV_HWPROBE_EXT_ZIHPM (1ULL << 51)
#endif

#ifndef RISCV_HWPROBE_EXT_ZFBFMIN
#define RISCV_HWPROBE_EXT_ZFBFMIN (1ULL << 52)
#endif

#ifndef RISCV_HWPROBE_EXT_ZVFBFMIN
#define RISCV_HWPROBE_EXT_ZVFBFMIN (1ULL << 53)
#endif

#ifndef RISCV_HWPROBE_EXT_ZVFBFWMA
#define RISCV_HWPROBE_EXT_ZVFBFWMA (1ULL << 54)
#endif

#ifndef RISCV_HWPROBE_EXT_ZICBOM
#define RISCV_HWPROBE_EXT_ZICBOM (1ULL << 55)
#endif

#ifndef RISCV_HWPROBE_EXT_ZAAMO
#define RISCV_HWPROBE_EXT_ZAAMO (1ULL << 56)
#endif

#ifndef RISCV_HWPROBE_EXT_ZALRSC
#define RISCV_HWPROBE_EXT_ZALRSC (1ULL << 57)
#endif

namespace {

#if defined(__linux__) && defined(__riscv)
bool UseSigillHandler(biscuit::RISCVExtension extension) {
    using namespace biscuit;
    switch (extension) {
    case RISCVExtension::I:
    case RISCVExtension::M:
    case RISCVExtension::A:
    case RISCVExtension::F:
    case RISCVExtension::D:
    case RISCVExtension::C:
    case RISCVExtension::V:
    case RISCVExtension::Zba:
    case RISCVExtension::Zbb:
    case RISCVExtension::Zbs:
    case RISCVExtension::Zicboz:
    case RISCVExtension::Zbc:
    case RISCVExtension::Zbkx:
    case RISCVExtension::Zknd:
    case RISCVExtension::Zkne:
    case RISCVExtension::Zknh:
    case RISCVExtension::Zksed:
    case RISCVExtension::Zksh:
    case RISCVExtension::Zvbb:
    case RISCVExtension::Zvbc:
    case RISCVExtension::Zvkb:
    case RISCVExtension::Zvkg:
    case RISCVExtension::Zvkned:
    case RISCVExtension::Zvknha:
    case RISCVExtension::Zvknhb:
    case RISCVExtension::Zvksed:
    case RISCVExtension::Zvksh:
    case RISCVExtension::Zfh:
    case RISCVExtension::Zfhmin:
    case RISCVExtension::Zvfh:
    case RISCVExtension::Zvfhmin:
    case RISCVExtension::Zfa:
    case RISCVExtension::Zacas:
    case RISCVExtension::Zicond:
    case RISCVExtension::Zicntr:
    case RISCVExtension::Zihpm:
    case RISCVExtension::Zfbfmin:
    case RISCVExtension::Zicbom: {
        return true;
    }
    default: {
        return false;
    }
    }
}

void SigillHandler(int, siginfo_t*, void* ctx) {
    mcontext_t* mctx = &((ucontext_t*)ctx)->uc_mcontext;

    // Since we hit SIGILL, set return value to false
    mctx->__gregs[REG_A0] = 0;

    // Instead of incrementing the PC, just set it to the return value immediately
    // after we return from the signal handler. This way we don't need to check if
    // it's a compressed instruction or not to find out how much to increment PC by.
    // Additionally, some extensions may require checking multiple instructions.
    mctx->__gregs[REG_PC] = mctx->__gregs[REG_RA];
}

void SetEGW(biscuit::Assembler& as, biscuit::SEW eew, uint32_t egs) {
    using namespace biscuit;
    uint32_t vlen = CPUInfo().GetVlenb() * 8;
    uint32_t egw = 0;
    switch (eew) {
    case SEW::E32: {
        egw = 32 * egs;
        break;
    }
    case SEW::E64: {
        egw = 64 * egs;
        break;
    }
    default: {
        BISCUIT_ASSERT(false);
    }
    }

    // When EGW > VLEN we need to use grouping
    if (egw > vlen) {
        // Use the maximum possible grouping as it's simpler
        // We use registers that are always far apart by 8 (v0, v8, v16, v24)
        biscuit::Label ok;
        as.VSETIVLI(t0, egs, eew, LMUL::M8);
        as.LI(t1, egs);
        as.BGEU(t0, t1, &ok);

        // If we get here then the new VL < EGS so emit an illegal instruction
        // to signal we don't have this extension.
        as.C_UNDEF();

        as.Bind(&ok);
    } else {
        as.VSETIVLI(x0, egs, eew);
    }
}

void EmitInstruction(biscuit::Assembler& as, biscuit::RISCVExtension extension) {
    // t0 points to valid memory for instructions that need it
    using namespace biscuit;
    switch (extension) {
    case RISCVExtension::I: {
        as.ADD(t1, t2, t3);
        break;
    }
    case RISCVExtension::M: {
        as.MUL(t1, t2, t3);
        break;
    }
    case RISCVExtension::A: {
        as.AMOADD_D(Ordering::AQRL, t1, t2, t0);
        break;
    }
    case RISCVExtension::F: {
        as.FADD_S(ft0, ft1, ft2);
        break;
    }
    case RISCVExtension::D: {
        as.FADD_D(ft0, ft1, ft2);
        break;
    }
    case RISCVExtension::C: {
        as.C_ADD(t1, t2);
        break;
    }
    case RISCVExtension::V: {
        as.VSETIVLI(x0, 1, SEW::E32);
        as.VADD(v1, v2, v3);
        break;
    }
    case RISCVExtension::Zba: {
        as.ADDUW(t1, t2, t3);
        as.SH1ADD(t1, t2, t3);
        as.SH2ADD(t1, t2, t3);
        as.SH3ADD(t1, t2, t3);
        as.SH1ADDUW(t1, t2, t3);
        as.SH2ADDUW(t1, t2, t3);
        as.SH3ADDUW(t1, t2, t3);
        as.SLLIUW(t1, t2, 1);
        break;
    }
    case RISCVExtension::Zbb: {
        as.ANDN(t1, t2, t3);
        as.ORN(t1, t2, t3);
        as.XNOR(t1, t2, t3);
        as.CLZ(t1, t2);
        as.CLZW(t1, t2);
        as.CTZ(t1, t2);
        as.CTZW(t1, t2);
        as.CPOP(t1, t2);
        as.CPOPW(t1, t2);
        as.MAX(t1, t2, t3);
        as.MAXU(t1, t2, t3);
        as.MIN(t1, t2, t3);
        as.MINU(t1, t2, t3);
        as.SEXTB(t1, t2);
        as.SEXTH(t1, t2);
        as.ZEXTH(t1, t2);
        as.ROL(t1, t2, t3);
        as.ROLW(t1, t2, t3);
        as.ROR(t1, t2, t3);
        as.RORI(t1, t2, 1);
        as.RORIW(t1, t2, 1);
        as.RORW(t1, t2, t3);
        as.ORCB(t1, t2);
        as.REV8(t1, t2);
        break;
    }
    case RISCVExtension::Zbs: {
        as.BCLR(t1, t2, t3);
        as.BCLRI(t1, t2, 1);
        as.BEXT(t1, t2, t3);
        as.BEXTI(t1, t2, 1);
        as.BINV(t1, t2, t3);
        as.BINVI(t1, t2, 1);
        as.BSET(t1, t2, t3);
        as.BSETI(t1, t2, 1);
        break;
    }
    case RISCVExtension::Zicboz: {
        as.CBO_ZERO(t0);
        break;
    }
    case RISCVExtension::Zbc: {
        as.CLMUL(t1, t2, t3);
        as.CLMULH(t1, t2, t3);
        as.CLMULR(t1, t2, t3);
        break;
    }
    case RISCVExtension::Zbkx: {
        as.XPERM4(t1, t2, t3);
        as.XPERM8(t1, t2, t3);
        break;
    }
    case RISCVExtension::Zknd: {
        as.AES64DSM(t1, t2, t3);
        break;
    }
    case RISCVExtension::Zkne: {
        as.AES64ESM(t1, t2, t3);
        break;
    }
    case RISCVExtension::Zknh: {
        as.SHA256SUM0(t1, t2);
        break;
    }
    case RISCVExtension::Zksed: {
        as.SM4ED(t1, t2, t3, 0);
        break;
    }
    case RISCVExtension::Zksh: {
        as.SM3P0(t1, t2);
        break;
    }
    case RISCVExtension::Zvbb: {
        as.VSETIVLI(x0, 1, SEW::E32);
        as.VANDN(v1, v2, v3);
        as.VBREV(v1, v2);
        as.VBREV8(v1, v2);
        as.VREV8(v1, v2);
        as.VCLZ(v1, v2);
        as.VCTZ(v1, v2);
        as.VCPOP(v1, v2);
        as.VROL(v1, v2, v3);
        as.VROR(v1, v2, v3);
        as.VWSLL(v2, v4, v6);
        break;
    }
    case RISCVExtension::Zvbc: {
        SetEGW(as, SEW::E64, 1);
        as.VCLMUL(v8, v16, v24);
        as.VCLMULH(v8, v16, v24);
        break;
    }
    case RISCVExtension::Zvkb: {
        as.VSETIVLI(x0, 1, SEW::E32);
        as.VANDN(v1, v2, v3);
        as.VBREV8(v1, v2);
        as.VREV8(v1, v2);
        as.VROL(v1, v2, v3);
        as.VROR(v1, v2, v3);
        break;
    }
    case RISCVExtension::Zvkg: {
        SetEGW(as, SEW::E32, 4);
        as.VGHSH(v8, v16, v24);
        break;
    }
    case RISCVExtension::Zvkned: {
        SetEGW(as, SEW::E32, 4);
        as.VAESEM_VV(v8, v16);
        break;
    }
    case RISCVExtension::Zvknha: {
        SetEGW(as, SEW::E32, 4);
        as.VSHA2MS(v8, v16, v24);
        break;
    }
    case RISCVExtension::Zvknhb: {
        SetEGW(as, SEW::E64, 4);
        as.VSHA2MS(v8, v16, v24);
        break;
    }
    case RISCVExtension::Zvksed: {
        SetEGW(as, SEW::E32, 8);
        as.VSM4R_VV(v8, v16);
        break;
    }
    case RISCVExtension::Zvksh: {
        SetEGW(as, SEW::E32, 8);
        as.VSM3ME(v8, v16, v24);
        break;
    }
    case RISCVExtension::Zfh: {
        as.FMV_H_X(ft0, x0);
        as.FMV_H_X(ft1, x0);
        as.FMV_H_X(ft2, x0);
        as.FADD_H(ft0, ft1, f2);
        break;
    }
    case RISCVExtension::Zfhmin: {
        as.FMV_H_X(ft0, x0);
        break;
    }
    case RISCVExtension::Zvfh: {
        as.VSETIVLI(x0, 1, SEW::E16);
        as.VMV(v2, x0);
        as.VMV(v3, x0);
        as.VFADD(v1, v2, v3);
        break;
    }
    case RISCVExtension::Zvfhmin: {
        as.VSETIVLI(x0, 1, SEW::E16);
        as.VMV(v8, x0);
        as.VFWCVT_F_F(v0, v8);
        break;
    }
    case RISCVExtension::Zfa: {
        as.FLI_S(ft0, 2.0);
        break;
    }
    case RISCVExtension::Zacas: {
        as.AMOCAS_D(Ordering::AQRL, t1, t2, t0);
        break;
    }
    case RISCVExtension::Zicond: {
        as.CZERO_EQZ(t1, t2, t3);
        break;
    }
    case RISCVExtension::Zicntr: {
        as.RDTIME(t1);
        break;
    }
    case RISCVExtension::Zihpm: {
        as.CSRR(t1, CSR::HPMCounter3);
        break;
    }
    case RISCVExtension::Zfbfmin: {
        as.FMV_W_X(ft0, x0);
        as.FCVT_BF16_S(ft1, ft0);
        break;
    }
    case RISCVExtension::Zicbom: {
        as.CBO_FLUSH(t0);
        break;
    }
    default: {
        BISCUIT_ASSERT(false);
    }
    }
}

bool CheckExtensionSigill(biscuit::RISCVExtension extension) {
    using namespace biscuit;

    struct sigaction sa, old_sa;
    sa.sa_sigaction = SigillHandler;
    sa.sa_flags = SA_SIGINFO;
    sigemptyset(&sa.sa_mask);

    int result = sigaction(SIGILL, &sa, &old_sa);
    BISCUIT_ASSERT(result == 0);

    uint64_t valid_memory[2]; // for extensions that might need to use a memory address
    auto* memory = static_cast<uint8_t*>(mmap(nullptr, 4096, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
    BISCUIT_ASSERT(memory != MAP_FAILED);

    biscuit::Assembler as(memory, 4096);
    bool (*function)(void*) = (bool (*)(void*))as.GetCursorPointer();
    as.MV(t0, a0); // instructions that need to access memory will be pointed here
    as.LI(a0, 1);  // return true unless if we hit SIGILL
    EmitInstruction(as, extension);
    as.RET();

    result = mprotect(memory, 4096, PROT_READ | PROT_EXEC);
    BISCUIT_ASSERT(result == 0);

    bool has_extension = function(&valid_memory);

    result = munmap(memory, 4096);
    BISCUIT_ASSERT(result == 0);

    result = sigaction(SIGILL, &old_sa, nullptr);
    BISCUIT_ASSERT(result == 0);

    return has_extension;
}

bool CheckExtensionSyscall(biscuit::RISCVExtension extension) {
    using namespace biscuit;
    static const auto [ima, features0] = []() {
#ifdef SYS_riscv_hwprobe
        riscv_hwprobe pairs[] = {
            {RISCV_HWPROBE_KEY_BASE_BEHAVIOR, 0},
            {RISCV_HWPROBE_KEY_IMA_EXT_0, 0},
        };

        syscall(SYS_riscv_hwprobe, pairs, std::size(pairs), 0, nullptr, 0);
        uint64_t ima = pairs[0].value;
        uint64_t features0 = pairs[1].value;
#else
        uint64_t ima = 0;
        uint64_t features0 = 0;
#endif
        return std::make_pair(ima, features0);
    }();

    switch (extension) {
    case RISCVExtension::I:
    case RISCVExtension::M:
    case RISCVExtension::A: {
        return ima == RISCV_HWPROBE_BASE_BEHAVIOR_IMA;
    }
    case RISCVExtension::F:
    case RISCVExtension::D:
        return (features0 & RISCV_HWPROBE_IMA_FD) != 0;
    case RISCVExtension::C:
        return (features0 & RISCV_HWPROBE_IMA_C) != 0;
    case RISCVExtension::V:
        return (features0 & RISCV_HWPROBE_IMA_V) != 0;
    case RISCVExtension::Zba:
        return (features0 & RISCV_HWPROBE_EXT_ZBA) != 0;
    case RISCVExtension::Zbb:
        return (features0 & RISCV_HWPROBE_EXT_ZBB) != 0;
    case RISCVExtension::Zbs:
        return (features0 & RISCV_HWPROBE_EXT_ZBS) != 0;
    case RISCVExtension::Zicboz:
        return (features0 & RISCV_HWPROBE_EXT_ZICBOZ) != 0;
    case RISCVExtension::Zbc:
        return (features0 & RISCV_HWPROBE_EXT_ZBC) != 0;
    case RISCVExtension::Zbkb:
        return (features0 & RISCV_HWPROBE_EXT_ZBKB) != 0;
    case RISCVExtension::Zbkc:
        return (features0 & RISCV_HWPROBE_EXT_ZBKC) != 0;
    case RISCVExtension::Zbkx:
        return (features0 & RISCV_HWPROBE_EXT_ZBKX) != 0;
    case RISCVExtension::Zknd:
        return (features0 & RISCV_HWPROBE_EXT_ZKND) != 0;
    case RISCVExtension::Zkne:
        return (features0 & RISCV_HWPROBE_EXT_ZKNE) != 0;
    case RISCVExtension::Zknh:
        return (features0 & RISCV_HWPROBE_EXT_ZKNH) != 0;
    case RISCVExtension::Zksed:
        return (features0 & RISCV_HWPROBE_EXT_ZKSED) != 0;
    case RISCVExtension::Zksh:
        return (features0 & RISCV_HWPROBE_EXT_ZKSH) != 0;
    case RISCVExtension::Zkt:
        return (features0 & RISCV_HWPROBE_EXT_ZKT) != 0;
    case RISCVExtension::Zvbb:
        return (features0 & RISCV_HWPROBE_EXT_ZVBB) != 0;
    case RISCVExtension::Zvbc:
        return (features0 & RISCV_HWPROBE_EXT_ZVBC) != 0;
    case RISCVExtension::Zvkb:
        return (features0 & RISCV_HWPROBE_EXT_ZVKB) != 0;
    case RISCVExtension::Zvkg:
        return (features0 & RISCV_HWPROBE_EXT_ZVKG) != 0;
    case RISCVExtension::Zvkned:
        return (features0 & RISCV_HWPROBE_EXT_ZVKNED) != 0;
    case RISCVExtension::Zvknha:
        return (features0 & RISCV_HWPROBE_EXT_ZVKNHA) != 0;
    case RISCVExtension::Zvknhb:
        return (features0 & RISCV_HWPROBE_EXT_ZVKNHB) != 0;
    case RISCVExtension::Zvksed:
        return (features0 & RISCV_HWPROBE_EXT_ZVKSED) != 0;
    case RISCVExtension::Zvksh:
        return (features0 & RISCV_HWPROBE_EXT_ZVKSH) != 0;
    case RISCVExtension::Zvkt:
        return (features0 & RISCV_HWPROBE_EXT_ZVKT) != 0;
    case RISCVExtension::Zfh:
        return (features0 & RISCV_HWPROBE_EXT_ZFH) != 0;
    case RISCVExtension::Zfhmin:
        return (features0 & RISCV_HWPROBE_EXT_ZFHMIN) != 0;
    case RISCVExtension::Zihintntl:
        return (features0 & RISCV_HWPROBE_EXT_ZIHINTNTL) != 0;
    case RISCVExtension::Zvfh:
        return (features0 & RISCV_HWPROBE_EXT_ZVFH) != 0;
    case RISCVExtension::Zvfhmin:
        return (features0 & RISCV_HWPROBE_EXT_ZVFHMIN) != 0;
    case RISCVExtension::Zfa:
        return (features0 & RISCV_HWPROBE_EXT_ZFA) != 0;
    case RISCVExtension::Ztso:
        return (features0 & RISCV_HWPROBE_EXT_ZTSO) != 0;
    case RISCVExtension::Zacas:
        return (features0 & RISCV_HWPROBE_EXT_ZACAS) != 0;
    case RISCVExtension::Zicond:
        return (features0 & RISCV_HWPROBE_EXT_ZICOND) != 0;
    case RISCVExtension::Zihintpause:
        return (features0 & RISCV_HWPROBE_EXT_ZIHINTPAUSE) != 0;
    case RISCVExtension::Zve32f:
        return (features0 & RISCV_HWPROBE_EXT_ZVE32F) != 0;
    case RISCVExtension::Zve32x:
        return (features0 & RISCV_HWPROBE_EXT_ZVE32X) != 0;
    case RISCVExtension::Zve64d:
        return (features0 & RISCV_HWPROBE_EXT_ZVE64D) != 0;
    case RISCVExtension::Zve64f:
        return (features0 & RISCV_HWPROBE_EXT_ZVE64F) != 0;
    case RISCVExtension::Zve64x:
        return (features0 & RISCV_HWPROBE_EXT_ZVE64X) != 0;
    case RISCVExtension::Zimop:
        return (features0 & RISCV_HWPROBE_EXT_ZIMOP) != 0;
    case RISCVExtension::Zca:
        return (features0 & RISCV_HWPROBE_EXT_ZCA) != 0;
    case RISCVExtension::Zcb:
        return (features0 & RISCV_HWPROBE_EXT_ZCB) != 0;
    case RISCVExtension::Zcd:
        return (features0 & RISCV_HWPROBE_EXT_ZCD) != 0;
    case RISCVExtension::Zcf:
        return (features0 & RISCV_HWPROBE_EXT_ZCF) != 0;
    case RISCVExtension::Zcmop:
        return (features0 & RISCV_HWPROBE_EXT_ZCMOP) != 0;
    case RISCVExtension::Zawrs:
        return (features0 & RISCV_HWPROBE_EXT_ZAWRS) != 0;
    case RISCVExtension::Supm:
        return (features0 & RISCV_HWPROBE_EXT_SUPM) != 0;
    case RISCVExtension::Zicntr:
        return (features0 & RISCV_HWPROBE_EXT_ZICNTR) != 0;
    case RISCVExtension::Zihpm:
        return (features0 & RISCV_HWPROBE_EXT_ZIHPM) != 0;
    case RISCVExtension::Zfbfmin:
        return (features0 & RISCV_HWPROBE_EXT_ZFBFMIN) != 0;
    case RISCVExtension::Zvfbfmin:
        return (features0 & RISCV_HWPROBE_EXT_ZVFBFMIN) != 0;
    case RISCVExtension::Zvfbfwma:
        return (features0 & RISCV_HWPROBE_EXT_ZVFBFWMA) != 0;
    case RISCVExtension::Zicbom:
        return (features0 & RISCV_HWPROBE_EXT_ZICBOM) != 0;
    case RISCVExtension::Zaamo:
        return (features0 & RISCV_HWPROBE_EXT_ZAAMO) != 0;
    case RISCVExtension::Zalrsc:
        return (features0 & RISCV_HWPROBE_EXT_ZALRSC) != 0;
    }

    return false;
}
#endif

} // namespace

namespace biscuit {

bool CPUInfo::Has([[maybe_unused]] RISCVExtension extension) const {
#if defined(__riscv) && defined(__linux__)
    if (UseSigillHandler(extension)) {
        return CheckExtensionSigill(extension);
    } else {
        return CheckExtensionSyscall(extension);
    }
#else
    return false;
#endif
}

uint32_t CPUInfo::GetVlenb() const {
    if (Has(RISCVExtension::V)) {
        static CSRReader<CSR::VLenb> csrReader;
        const static auto getVLEN = csrReader.GetCode<uint32_t (*)()>();
        return getVLEN();
    }

    return 0;
}

} // namespace biscuit
