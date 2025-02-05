#pragma once

#include <array>
#include <queue>
#include "felix86/common/log.hpp"
#include "felix86/common/utility.hpp"
#include "felix86/hle/signals.hpp"

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
    X86_SIZE_BYTE,
    X86_SIZE_WORD,
    X86_SIZE_DWORD,
    X86_SIZE_QWORD,
    X86_SIZE_MM,
    X86_SIZE_XMM,
    X86_SIZE_BYTE_HIGH,
} x86_size_e;

struct XmmReg {
    u64 data[2];
};
static_assert(sizeof(XmmReg) == 16);

struct ThreadState {
    explicit ThreadState(ThreadState* state);

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

    pid_t* clear_tid_address = nullptr;
    pthread_t thread{}; // The pthread this state belongs to
    u64 tid{};
    stack_t alt_stack{};
    bool signals_disabled{}; // some instructions would make it annoying to allow for signals to occur, be it because they have loops like rep, or use
                             // lr/sc instructions. So, this flag is set to true when we absolutely don't want a signal to be handled here.

    std::queue<int> pending_signals{}; // queue for signals that are pending to be handled because they were disabled when they happened
                                       // This doesn't quite work if a signal is "synchronous", meaning if an instruction purposefully triggered it
                                       // but those instructions should not overlap with ones that would disable signals.

    std::vector<u64> calltrace{}; // used if g_calltrace is true

    // Two processes can share the same signal handler table
    std::shared_ptr<SignalHandlerTable> signal_handlers{};
    sigset_t signal_mask{};

    void* compile_next_handler{};

    u8 exit_reason{};

    std::array<u64, 16> saved_host_gprs;

    std::unique_ptr<Recompiler> recompiler;

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
};
