#pragma once

#include "biscuit/isa.hpp"
#include "felix86/common/address.hpp"
#include "felix86/common/log.hpp"
#include "felix86/common/utility.hpp"
#include "felix86/hle/signals.hpp"

#define C0_BIT (1 << 8)
#define C1_BIT (1 << 9)
#define C2_BIT (1 << 10)
#define C3_BIT (1 << 14)

struct Recompiler;

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
    X86_REF_YMM0,
    X86_REF_YMM1,
    X86_REF_YMM2,
    X86_REF_YMM3,
    X86_REF_YMM4,
    X86_REF_YMM5,
    X86_REF_YMM6,
    X86_REF_YMM7,
    X86_REF_YMM8,
    X86_REF_YMM9,
    X86_REF_YMM10,
    X86_REF_YMM11,
    X86_REF_YMM12,
    X86_REF_YMM13,
    X86_REF_YMM14,
    X86_REF_YMM15,
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
    X86_REF_CS,
    X86_REF_SS,
    X86_REF_DS,
    X86_REF_ES,

    X86_REF_COUNT,
} x86_ref_e;

typedef enum : u8 {
    X86_SIZE_BYTE,
    X86_SIZE_WORD,
    X86_SIZE_DWORD,
    X86_SIZE_QWORD,
    X86_SIZE_MM,
    X86_SIZE_XMM,
    X86_SIZE_YMM,
    X86_SIZE_BYTE_HIGH,
    X86_SIZE_ST,
} x86_size_e;

struct XmmReg {
    u64 data[2];
};
static_assert(sizeof(XmmReg) == 16);

struct PendingSignal {
    int sig;
    siginfo_t info;
};

// TODO: Please make me standard layout type? offsetof warnings...
struct ThreadState {
    explicit ThreadState(ThreadState* copy_state);

    u64 gprs[16]{};
    GuestAddress rip{0};
    u64 fp[8]{}; // we support 64-bit precision instead of 80-bit for speed and simplicity
    XmmReg xmm[16]{};
    bool cf{};
    bool pf{};
    bool af{};
    bool zf{};
    bool sf{};
    bool of{};
    bool df{};
    // Actual segment values
    u16 gs{};
    u16 fs{};
    u16 cs{};
    u16 ds{};
    u16 ss{};
    u16 es{};
    // Base addresses (either fsbase/gsbase on 64-bit mode or all of them set by ie. mov gs, ax & on set_thread_area in 32-bit mode)
    u64 gsbase{};
    u64 fsbase{};
    u64 csbase{};
    u64 dsbase{};
    u64 ssbase{};
    u64 esbase{};
    u32 mxcsr{0x1F80}; // default value
    RMode rmode{RMode::RNE};
    u16 fpu_cw{};
    u16 fpu_tw{};
    u16 fpu_sw{};
    u8 fpu_top{};

    // We use two separate stacks, one for jit code and one for cpp code -- this only happens when RSB optimization
    // is enabled to ensure that the C++ code doesn't trigger our guards for stack overflow on call/ret predictions
    u64 jit_stack{};
    u64 cpp_stack{};

    pid_t* clear_tid_address = nullptr;
    pthread_t thread{}; // The pthread this state belongs to
    u64 tid{};
    stack_t alt_stack{};
    bool signals_disabled{}; // some instructions would make it annoying to allow for signals to occur, be it because they have loops like rep, or use
                             // lr/sc instructions. So, this flag is set to true when we absolutely don't want a signal to be handled here.
    bool cpuid_bit{};        // stupid rflags bit that is modifiable when cpuid is present, so we need to store its state here. SDL2 modifies it to
                             // check presence of cpuid... on x86-64 processors... lol...

    std::vector<PendingSignal> pending_signals{}; // signals that were raised during an unsafe time, queued for later

    std::vector<HostAddress> calltrace{}; // used if g_calltrace is true

    // Two processes can share the same signal handler table
    SignalHandlerTable* signal_table{};

    sigset_t signal_mask{};

    ExitReason exit_reason{};

    u8 exit_code{}; // process exit code

    bool mode32 = false; // 32-bit execution mode, changes the behavior of some instructions and the decoder

    u32 gdt[3]{};

    u64 persona = 0;

    // We need a place to save execution frames so we can return from the JIT back to C code.
    // It can't be the stack, we use that for return stack buffer optimization.
    // This happens in two places:
    // - On JIT entry
    // - On signal handling
    // Note that signals can happen inside signals so we need enough space that this realistically never
    // overflows and we can return cleanly.
    u64 frame_pointer = 0;
    u8 frames[4096]{};

    u64 underflow_page = 0;
    u64 overflow_page = 0;

    std::unique_ptr<Recompiler> recompiler;

    biscuit::RMode GetRMode() {
        u8 rc = (mxcsr >> 13) & 3;
        return rounding_mode(x86RoundingMode(rc));
    }

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

    GuestAddress GetRip() const {
        return rip;
    }

    void SetRip(GuestAddress value) {
        rip = value;
    }

    u64 GetFlags() {
        u64 flags = 0;
        flags |= cf;
        flags |= pf << 2;
        flags |= af << 4;
        flags |= zf << 6;
        flags |= sf << 7;
        flags |= df << 10;
        flags |= of << 11;
        return flags;
    }

    static void InitializeKey();

    static ThreadState* Create(ThreadState* copy_state = nullptr);

    static ThreadState* Get();

    static void Destroy(ThreadState* state);
};
