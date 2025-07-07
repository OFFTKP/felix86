#pragma once

#include "biscuit/isa.hpp"
#include "felix86/common/global.hpp"
#include "felix86/common/log.hpp"
#include "felix86/common/signal_queue.hpp"
#include "felix86/common/types.hpp"
#include "felix86/common/utility.hpp"
#include "felix86/hle/cpuid.hpp"
#include "felix86/hle/guest_types.hpp"
#include "felix86/hle/syscall.hpp"

// We statically allocate 8 FPRs and 8 Vecs for x87 and MMX. But in x86 they share the same registers.
// For this reason we need to have a way to communicate to signal handlers which registers hold the correct values.
// For now, the dispatcher will always assume FPRs hold the correct values so blocks must restore state to x87 when finished.
enum class x87State {
    Unknown = 0,
    x87 = 1,
    MMX = 2,
};

#define C0_BIT (1 << 8)
#define C1_BIT (1 << 9)
#define C2_BIT (1 << 10)
#define C3_BIT (1 << 14)

struct Recompiler;
struct ThreadState;
struct SignalHandlerTable;

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
    X86_REF_MM0,
    X86_REF_MM1,
    X86_REF_MM2,
    X86_REF_MM3,
    X86_REF_MM4,
    X86_REF_MM5,
    X86_REF_MM6,
    X86_REF_MM7,
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
    X86_SIZE_XMM,
    X86_SIZE_YMM,
    X86_SIZE_BYTE_HIGH,
    X86_SIZE_ST,
} x86_size_e;

struct XmmReg {
    u64 data[2] = {0, 0};
};
static_assert(sizeof(XmmReg) == 16);

#define FUNCTION_POINTERS                                                                                                                            \
    X(f80_to_64)                                                                                                                                     \
    X(felix86_fsin)                                                                                                                                  \
    X(felix86_fcos)                                                                                                                                  \
    X(felix86_fpatan)                                                                                                                                \
    X(felix86_fptan)                                                                                                                                 \
    X(felix86_fprem)                                                                                                                                 \
    X(felix86_f2xm1)                                                                                                                                 \
    X(felix86_fscale)                                                                                                                                \
    X(felix86_fyl2x)                                                                                                                                 \
    X(felix86_fyl2xp1)                                                                                                                               \
    X(felix86_fxam)                                                                                                                                  \
    X(felix86_fstenv_16)                                                                                                                             \
    X(felix86_fstenv_32)                                                                                                                             \
    X(felix86_fldenv_16)                                                                                                                             \
    X(felix86_fldenv_32)                                                                                                                             \
    X(felix86_fsave_16)                                                                                                                              \
    X(felix86_fsave_32)                                                                                                                              \
    X(felix86_frstor_16)                                                                                                                             \
    X(felix86_frstor_32)                                                                                                                             \
    X(felix86_set_segment)                                                                                                                           \
    X(felix86_iret)                                                                                                                                  \
    X(felix86_divu128)                                                                                                                               \
    X(felix86_div128)                                                                                                                                \
    X(felix86_cpuid)                                                                                                                                 \
    X(felix86_syscall)                                                                                                                               \
    X(felix86_syscall32)                                                                                                                             \
    X(felix86_fxsave)                                                                                                                                \
    X(felix86_fxrstor)                                                                                                                               \
    X(felix86_pcmpxstrx)

// TODO: Please make me standard layout type? offsetof warnings...
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
    biscuit::RMode rmode_sse{biscuit::RMode::RNE};
    biscuit::RMode rmode_x87{biscuit::RMode::RNE};
    u16 fpu_cw{};
    u16 fpu_tw{};
    u16 fpu_sw{};
    u8 fpu_top{};

    // This is important so that we know whether the current values in the fp array are MMX registers or x87 registers
    // Because if they are x87 registers we need to f64_to_f80 them when saving using fsave or when reading from signal handlers
    x87State x87_state = x87State::MMX;

    // Whenever we writeback the state we set this bool so that the signal handler knows not to pull registers from ucontext
    // and instead pull them from ThreadState. If this is false then we haven't done a writeback so pull from ucontext.
    // This is useful because sometimes we are in JIT code but we also are thrashing registers (setting a0, a1 etc.) so
    // sometimes we want to pull from ThreadState even though we are in JIT code.
    bool state_is_correct = false;
    ExitReason exit_reason{};
    pid_t* clear_tid_address = nullptr;
    pthread_t thread{}; // The pthread this state belongs to
    u64 tid{};
    stack_t alt_stack{};
    bool cpuid_bit{}; // stupid rflags bit that is modifiable when cpuid is present, so we need to store its state here. SDL2 modifies it to
                      // check presence of cpuid... on x86-64 processors... lol...
    bool ac_bit{};    // this is checked by Java to see if it's i386 or not

    u32 robust_list{};

    // Counts the amount of atomic instructions that end up on a non-atomic path due to being unaligned
    // This is a cheap and quick way of checking if a program uses unaligned non-atomic atomics
    // Example: RMW on 16-bit data in the end of a 64-bit address (address & 0b111 == 0b111). Can't be atomically emulated with a lr.d/sc.d combo
    u64 unaligned_atomics_counter = 0;

    // Two processes can share the same signal handler table
    SignalHandlerTable* signal_table{};

    sigset_t signal_mask{};

    u8 exit_code{}; // process exit code

    bool mode32 = false; // 32-bit execution mode, changes the behavior of some instructions and the decoder

    u32 gdt[32]{};

    u64 persona = 0;

#define X(name) u64 name = (u64)::name;
    // We don't want our code to hardcode pointers in order to be reusable (cached)
    FUNCTION_POINTERS;
#undef X

    Recompiler* recompiler;

    // For storing generated risc-v or x86 code that needs to outlive code cache clears
    u8* riscv_trampoline_storage_start = nullptr;
    u8* x86_trampoline_storage_start = nullptr;
    u8* riscv_trampoline_storage = nullptr;
    u8* x86_trampoline_storage = nullptr;

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

    XmmReg GetXmm(x86_ref_e ref) const {
        if (ref < X86_REF_XMM0 || ref > X86_REF_XMM15) {
            ERROR("Invalid XMM register reference: %d", ref);
            return {};
        }

        return xmm[ref - X86_REF_XMM0];
    }

    void SetXmm(x86_ref_e ref, const XmmReg& value) {
        if (ref < X86_REF_XMM0 || ref > X86_REF_XMM15) {
            ERROR("Invalid XMM register reference: %d", ref);
            return;
        }

        xmm[ref - X86_REF_XMM0] = value;
    }

    u64 GetMm(x86_ref_e ref) const {
        if (ref < X86_REF_MM0 || ref > X86_REF_MM7) {
            ERROR("Invalid MM register reference: %d", ref);
            return {};
        }

        return fp[ref - X86_REF_MM0];
    }

    void SetXmm(x86_ref_e ref, u64 value) {
        if (ref < X86_REF_MM0 || ref > X86_REF_MM7) {
            ERROR("Invalid MM register reference: %d", ref);
            return;
        }

        fp[ref - X86_REF_XMM0] = value;
    }

    u64 GetRip() const {
        return rip;
    }

    void SetRip(u64 value) {
        rip = value;
    }

    void SetTLS(u64 address) {
        if (g_mode32) {
            ASSERT(SetUserDesc((x86_user_desc*)address) == 0);
        } else {
            fsbase = address;
        }
    }

    int SetUserDesc(x86_user_desc* udesc) {
        ASSERT(udesc);
        int index = udesc->entry_number;
        if (index == -1) {
            for (int i = 12; i < 15; i++) {
                if (gdt[i] == 0) {
                    index = i;
                    break;
                }
            }

            if (index == -1) {
                WARN("Failed search for empty gdt slot");
                return -ESRCH;
            }
        }

        gdt[index] = udesc->base_addr;
        udesc->entry_number = index;

#define CHECK_SEG(name)                                                                                                                              \
    if ((name >> 3) == index) {                                                                                                                      \
        name##base = udesc->base_addr;                                                                                                               \
    }
        CHECK_SEG(fs);
        CHECK_SEG(gs);
        CHECK_SEG(es);
        CHECK_SEG(ss);
        CHECK_SEG(cs);
        CHECK_SEG(ds);
#undef CHECK_SEG

        return 0;
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
