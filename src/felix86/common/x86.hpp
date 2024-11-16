#pragma once

#include <cstddef>
#include "felix86/backend/registers.hpp"
#include "felix86/common/log.hpp"
#include "felix86/common/utility.hpp"

enum class Group1 : u8 {
    Add = 0,
    Or = 1,
    Adc = 2,
    Sbb = 3,
    And = 4,
    Sub = 5,
    Xor = 6,
    Cmp = 7,
};

enum class Group2 : u8 {
    Rol = 0,
    Ror = 1,
    Rcl = 2,
    Rcr = 3,
    Shl = 4,
    Shr = 5,
    Sal = 6,
    Sar = 7,
};

enum class Group3 : u8 {
    Test = 0,
    Test_ = 1,
    Not = 2,
    Neg = 3,
    Mul = 4,
    IMul = 5,
    Div = 6,
    IDiv = 7,
};

enum class Group4 : u8 {
    Inc = 0,
    Dec = 1,
};

enum class Group5 : u8 {
    Inc = 0,
    Dec = 1,
    Call = 2,
    CallF = 3,
    Jmp = 4,
    JmpF = 5,
    Push = 6,
};

enum class Group14 : u8 {
    PSrlQ = 2,
    PSrlDQ = 3,
    PSllQ = 6,
    PSllDQ = 7,
};

enum class Group15 : u8 {
    FxSave = 0,
    FxrStor = 1,
    LdMxcsr = 2,
    StMxcsr = 3,
    XSave = 4,
    LFence = 5,
    MFence = 6,
    SFence = 7,
};

enum x86_rep_e {
    NONE,
    REP,
    REP_Z,
    REP_NZ,
};

typedef enum : u8 {
    X86_REF_RAX,
    X86_REF_RCX,
    X86_REF_RDX,
    X86_REF_RBX,
    X86_REF_RSP,
    X86_REF_RBP,
    X86_REF_RSI,
    X86_REF_RDI,
    X86_REF_R8,
    X86_REF_R9,
    X86_REF_R10,
    X86_REF_R11,
    X86_REF_R12,
    X86_REF_R13,
    X86_REF_R14,
    X86_REF_R15,
    X86_REF_ST0,
    X86_REF_ST1,
    X86_REF_ST2,
    X86_REF_ST3,
    X86_REF_ST4,
    X86_REF_ST5,
    X86_REF_ST6,
    X86_REF_ST7,
    X86_REF_XMM0,
    X86_REF_XMM1,
    X86_REF_XMM2,
    X86_REF_XMM3,
    X86_REF_XMM4,
    X86_REF_XMM5,
    X86_REF_XMM6,
    X86_REF_XMM7,
    X86_REF_XMM8,
    X86_REF_XMM9,
    X86_REF_XMM10,
    X86_REF_XMM11,
    X86_REF_XMM12,
    X86_REF_XMM13,
    X86_REF_XMM14,
    X86_REF_XMM15,
    X86_REF_RIP,
    X86_REF_CF,
    X86_REF_PF,
    X86_REF_AF,
    X86_REF_ZF,
    X86_REF_SF,
    X86_REF_DF,
    X86_REF_OF,
    X86_REF_GS,
    X86_REF_FS,

    X86_REF_COUNT,
} x86_ref_e;

typedef enum : u8 {
    X86_OP_TYPE_NONE,
    X86_OP_TYPE_MEMORY,
    X86_OP_TYPE_REGISTER,
    X86_OP_TYPE_IMMEDIATE,
} x86_operand_type_e;

typedef enum : u8 {
    X86_SIZE_BYTE,
    X86_SIZE_WORD,
    X86_SIZE_DWORD,
    X86_SIZE_QWORD,
    X86_SIZE_MM,
    X86_SIZE_XMM,
} x86_size_e;

struct XmmReg {
    u64 data[2];
};
static_assert(sizeof(XmmReg) == 16);

struct ThreadState {
    u64 gprs[16]{};
    u64 rip{};
    u64 fp[8]{}; // we support 64-bit precision instead of 80-bit for speed and simplicity
    XmmReg xmm[16]{};
    bool cf{};
    bool pf{};
    bool af{};
    bool zf{};
    bool sf{};
    bool of{};
    bool df{};
    u64 gsbase{};
    u64 fsbase{};

    u64 robust_futex_list{};
    u64 set_child_tid{};
    u64 clear_child_tid{};
    u64 brk_current_address{};

    u8 exit_reason{};

    // Storage for saved RISC-V registers, per thread, for when it's time to completely
    // exit dispatcher and stop the emulator
    u64 gpr_storage[Registers::GetSavedGPRs().size()]{};

    u64 GetGpr(x86_ref_e ref) const {
        if (ref < X86_REF_RAX || ref > X86_REF_R15) {
            ERROR("Invalid GPR reference: %d", ref);
            return 0;
        }

        return gprs[ref - X86_REF_RAX];
    }

    void SetGpr(x86_ref_e ref, u64 value) {
        if (ref < X86_REF_RAX || ref > X86_REF_R15) {
            ERROR("Invalid GPR reference: %d", ref);
        }

        gprs[ref - X86_REF_RAX] = value;
    }

    bool GetFlag(x86_ref_e flag) const {
        switch (flag) {
        case X86_REF_CF:
            return cf;
        case X86_REF_PF:
            return pf;
        case X86_REF_AF:
            return af;
        case X86_REF_ZF:
            return zf;
        case X86_REF_SF:
            return sf;
        case X86_REF_DF:
            return df;
        case X86_REF_OF:
            return of;
        default:
            ERROR("Invalid flag reference: %d", flag);
            return false;
        }
    }

    void SetFlag(x86_ref_e flag, bool value) {
        switch (flag) {
        case X86_REF_CF:
            cf = value;
            break;
        case X86_REF_PF:
            pf = value;
            break;
        case X86_REF_AF:
            af = value;
            break;
        case X86_REF_ZF:
            zf = value;
            break;
        case X86_REF_SF:
            sf = value;
            break;
        case X86_REF_DF:
            df = value;
            break;
        case X86_REF_OF:
            of = value;
            break;
        default:
            ERROR("Invalid flag reference: %d", flag);
        }
    }

    XmmReg GetXmmReg(x86_ref_e ref) const {
        if (ref < X86_REF_XMM0 || ref > X86_REF_XMM15) {
            ERROR("Invalid XMM register reference: %d", ref);
            return {};
        }

        return xmm[ref - X86_REF_XMM0];
    }

    void SetXmmReg(x86_ref_e ref, const XmmReg& value) {
        if (ref < X86_REF_XMM0 || ref > X86_REF_XMM15) {
            ERROR("Invalid XMM register reference: %d", ref);
            return;
        }

        xmm[ref - X86_REF_XMM0] = value;
    }

    u64 GetRip() const {
        return rip;
    }

    void SetRip(u64 value) {
        rip = value;
    }

    u64 GetGSBase() const {
        return gsbase;
    }

    void SetGSBase(u64 value) {
        gsbase = value;
    }

    u64 GetFSBase() const {
        return fsbase;
    }

    void SetFSBase(u64 value) {
        fsbase = value;
    }
};

typedef union {
    struct {
        u64 x87 : 1;
        u64 sse : 1;
        u64 avx : 1;
        u64 mpx : 2;
        u64 avx512 : 3;
        u64 pt : 1;
        u64 pkru : 1;
        u64 pasid : 1;
        u64 cet_u : 1;
        u64 cet_s : 1;
        u64 hdc : 1;
        u64 uintr : 1;
        u64 lbr : 1;
        u64 hwp : 1;
        u64 xtilecfg : 1;
        u64 xtiledata : 1;
        u64 apx : 1;
        u64 : 44;
    };

    u64 raw;
} xcr0_reg_t;

typedef union {
    struct {
        u8 rm : 3;
        u8 reg : 3;
        u8 mod : 2;
    };

    u8 raw;
} modrm_t;

typedef union {
    struct {
        u8 base : 3;
        u8 index : 3;
        u8 scale : 2;
    };

    u8 raw;
} sib_t;
