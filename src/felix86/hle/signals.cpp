#include <array>
#include <csignal>
#include <cstring>
#include <bits/types/sigset_t.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <sys/ucontext.h>
#include "felix86/common/config.hpp"
#include "felix86/common/feature.hpp"
#include "felix86/common/global.hpp"
#include "felix86/common/log.hpp"
#include "felix86/common/print.hpp"
#include "felix86/common/state.hpp"
#include "felix86/common/types.hpp"
#include "felix86/common/utility.hpp"
#include "felix86/common/xsave.hpp"
#include "felix86/hle/signals.hpp"
#include "felix86/v2/recompiler.hpp"
#undef si_pid

#define FP_XSTATE_MAGIC1 0x46505853U
#define FP_XSTATE_MAGIC2 0x46505845U
#define SA_IA32_ABI 0x02000000u
#define SA_X32_ABI 0x01000000u

#ifndef SS_AUTODISARM
#define SS_AUTODISARM (1U << 31)
#endif

u64* get_regs(void* ctx) {
#ifdef __riscv
    return (u64*)((ucontext_t*)ctx)->uc_mcontext.__gregs;
#else
    UNREACHABLE();
    return nullptr;
#endif
}

struct RegisteredHostSignal {
    int sig;                                                                            // ie SIGILL etc
    int code;                                                                           // stuff like BUS_ADRALN, 0 if all
    bool (*func)(ThreadState* current_state, siginfo_t* info, ucontext_t* ctx, u64 pc); // the function to call
};

const char* signal_to_name(int sig) {
#define CASE(name)                                                                                                                                   \
    case name:                                                                                                                                       \
        return #name;
    switch (sig) {
        CASE(SIGHUP);
        CASE(SIGINT);
        CASE(SIGQUIT);
        CASE(SIGILL);
        CASE(SIGTRAP);
        CASE(SIGABRT);
        CASE(SIGBUS);
        CASE(SIGFPE);
        CASE(SIGKILL);
        CASE(SIGUSR1);
        CASE(SIGSEGV);
        CASE(SIGUSR2);
        CASE(SIGPIPE);
        CASE(SIGALRM);
        CASE(SIGTERM);
        CASE(SIGSTKFLT);
        CASE(SIGCHLD);
        CASE(SIGCONT);
        CASE(SIGSTOP);
        CASE(SIGTSTP);
        CASE(SIGTTIN);
        CASE(SIGTTOU);
        CASE(SIGURG);
        CASE(SIGXCPU);
        CASE(SIGXFSZ);
        CASE(SIGVTALRM);
        CASE(SIGPROF);
        CASE(SIGWINCH);
        CASE(SIGIO);
        CASE(SIGPWR);
        CASE(SIGSYS);
        CASE(32);
        CASE(33);
        CASE(34);
        CASE(35);
        CASE(36);
        CASE(37);
        CASE(38);
        CASE(39);
        CASE(40);
        CASE(41);
        CASE(42);
        CASE(43);
        CASE(44);
        CASE(45);
        CASE(46);
        CASE(47);
        CASE(48);
        CASE(49);
        CASE(50);
        CASE(51);
        CASE(52);
        CASE(53);
        CASE(54);
        CASE(55);
        CASE(56);
        CASE(57);
        CASE(58);
        CASE(59);
        CASE(60);
        CASE(61);
        CASE(62);
        CASE(63);
        CASE(64);
    default: {
        return "Unknown";
    }
    }
#undef CASE
}

bool is_in_jit_code(ThreadState* state, u8* ptr) {
    CodeBuffer& buffer = state->recompiler->getAssembler().GetCodeBuffer();
    u8* start = state->recompiler->getStartOfCodeCache();
    u8* end = (u8*)buffer.GetCursorAddress();
    return ptr >= start && ptr < end;
}

struct x64_fpstate {
    fxsave_frame fxsave;
    // xsave frame follows here...
};
static_assert(sizeof(x64_fpstate) == 512);

struct x86_fpstate {
    fsave_frame_32 fsave;
    u16 status;
    u16 magic; /* 0xffff: regular FPU data only */
    /* 0x0000: FXSR FPU data */
    fxsave_frame fxsave;
};
static_assert(sizeof(x86_fpstate) == sizeof(fsave_frame_32) + sizeof(fxsave_frame) + 4 /* status and magic */);

#ifndef __x86_64__
enum {
    REG_R8 = 0,
    REG_R9,
    REG_R10,
    REG_R11,
    REG_R12,
    REG_R13,
    REG_R14,
    REG_R15,
    REG_RDI,
    REG_RSI,
    REG_RBP,
    REG_RBX,
    REG_RDX,
    REG_RAX,
    REG_RCX,
    REG_RSP,
    REG_RIP,
    REG_EFL,
    REG_CSGSFS, /* Actually short cs, gs, fs, __pad0.  */
    REG_ERR,
    REG_TRAPNO,
    REG_OLDMASK,
    REG_CR2
};
#endif

struct x64_mcontext {
    u64 gregs[23];       // using the indices in the enum above
    x64_fpstate* fpregs; // it's a pointer, points to after the end of x64_rt_sigframe in stack
    u64 reserved[8];
};
static_assert(sizeof(x64_mcontext) == 256);

struct x64_ucontext {
    u64 uc_flags;
    x64_ucontext* uc_link;
    stack_t uc_stack;
    x64_mcontext uc_mcontext;
    sigset_t uc_sigmask;
};
static_assert(sizeof(x64_ucontext) == 424);

// https://github.com/torvalds/linux/blob/master/arch/x86/include/asm/sigframe.h#L59
struct x64_rt_sigframe {
    char* pretcode; // return address
    x64_ucontext uc;
    siginfo_t info;
    // fp state follows here
};

struct x86_sigcontext_32 {
    u16 gs, __gsh;
    u16 fs, __fsh;
    u16 es, __esh;
    u16 ds, __dsh;
    u32 di;
    u32 si;
    u32 bp;
    u32 sp;
    u32 bx;
    u32 dx;
    u32 cx;
    u32 ax;
    u32 trapno;
    u32 err;
    u32 ip;
    u16 cs, __csh;
    u32 flags;
    u32 sp_at_signal;
    u16 ss, __ssh;

    /*
     * fpstate is really (struct _fpstate *) or (struct _xstate *)
     * depending on the FP_XSTATE_MAGIC1 encoded in the SW reserved
     * bytes of (struct _fpstate) and FP_XSTATE_MAGIC2 present at the end
     * of extended memory layout. See comments at the definition of
     * (struct _fpx_sw_bytes)
     */
    u32 fpstate; /* Zero when no FPU/extended context */
    u32 oldmask;
    u32 cr2;
};

struct __attribute__((packed)) x86_ucontext {
    unsigned int uc_flags;
    unsigned int uc_link;
    x86_stack_t uc_stack;
    struct x86_sigcontext_32 uc_mcontext;
    sigset_t uc_sigmask; /* mask last for extensibility */
};

static_assert(offsetof(x86_ucontext, uc_mcontext) == 20);
static_assert(offsetof(x86_ucontext, uc_sigmask) == 108);

struct x86_legacy_sigframe {
    u32 pretcode;
    int sig;
    struct x86_sigcontext_32 sc;
    struct x86_fpstate fpstate_unused; // unused but we need the padding
    unsigned int extramask;
    char retcode[8];
    /* fp state follows here */
};

struct x86_rt_sigframe {
    u32 pretcode;
    int sig;
    u32 pinfo;
    u32 puc;
    x86_siginfo_t info;
    struct x86_ucontext uc;
    char retcode[8];
    /* fp state follows here */
};

BlockMetadata* get_block_metadata(ThreadState* state, u64 host_pc) {
    auto& map = state->recompiler->getHostPcMap();
    auto it = map.lower_bound(host_pc);
    ASSERT(it != map.end());
    if (!(host_pc >= it->second->address && host_pc <= it->second->address_end)) {
        WARN("PC: %lx not inside range %lx-%lx?", host_pc, it->second->address, it->second->address_end);
        return nullptr;
    }
    return it->second;
}

u64 get_actual_rip(BlockMetadata& metadata, u64 host_pc) {
    u64 ret_value{};
    for (auto& span : metadata.instruction_spans) {
        if (host_pc >= span.second) {
            ret_value = span.first;
        } else { // if it's smaller it means that instruction isn't reached yet, return previous value
            ASSERT_MSG(ret_value != 0, "First PC: %lx, Our PC: %lx, Block: %lx-%lx", metadata.instruction_spans[0].second, host_pc, metadata.address,
                       metadata.address_end);
            return ret_value;
        }
    }

    ASSERT(ret_value != 0);
    return ret_value;
}

#ifndef REG_PC
#define REG_PC 0 // risc-v stores it in gpr 0
#endif

bool sa_ss_flags(ThreadState* state) {
    u64 rsp = state->GetGpr(X86_REF_RSP);
    if (!state->alt_stack.ss_size) {
        return SS_DISABLE;
    }
    if (state->alt_stack.ss_flags & SS_AUTODISARM) {
        return 0;
    }
    bool is_on_altstack = rsp > (u64)state->alt_stack.ss_sp && rsp - (u64)state->alt_stack.ss_sp <= state->alt_stack.ss_size;
    return is_on_altstack ? SS_ONSTACK : 0;
}

// arch/x86/kernel/signal.c, get_sigframe function prepares the signal frame
void setupFrame_x64(RegisteredSignal& signal, int sig, ThreadState* state, siginfo_t* guest_info, ucontext_t* host_context) {
    u64 rsp = state->GetGpr(X86_REF_RSP);
    rsp = rsp - 128; // red zone
    bool use_altstack = signal.flags & SA_ONSTACK;
    if (use_altstack) {
        if (sa_ss_flags(state) == 0) {
            rsp = (u64)state->alt_stack.ss_sp + state->alt_stack.ss_size;
        }
    }

    if (rsp == 0) {
        WARN("RSP is null, use_altstack: %d... using original stack", use_altstack);
        rsp = state->GetGpr(X86_REF_RSP);
        ASSERT(rsp != 0);
    } else if (use_altstack) {
        VERBOSE("Altstack was established");
    }
    rsp = rsp - (rsp % 16);

    if (felix86_xsave_contains_ymms()) {
        // Make room for extra xsave data
        rsp = rsp - sizeof(ymm_hi) - sizeof(xsave_header);
        static_assert(sizeof(ymm_hi) % 16 == 0);
        static_assert(sizeof(xsave_header) % 16 == 0);
    }

    rsp = rsp - sizeof(x64_fpstate);
    static_assert(sizeof(x64_fpstate) % 16 == 0);
    x64_fpstate* fpstate = (x64_fpstate*)rsp;

    rsp = rsp - sizeof(x64_rt_sigframe);
    static_assert(sizeof(x64_rt_sigframe) % 16 == 0);
    x64_rt_sigframe* frame = (x64_rt_sigframe*)rsp;

    ASSERT(signal.restorer);
    frame->pretcode = (char*)signal.restorer;

    frame->uc.uc_mcontext.fpregs = fpstate;

    frame->uc.uc_flags = 0;
    frame->uc.uc_link = 0;
    frame->info = *guest_info;

    // After some testing, this is set to the altstack if it exists and is valid (which we don't check here, but on sigaltstack)
    // Otherwise it is zero, it's not set to the actual stack
    if (use_altstack) {
        frame->uc.uc_stack.ss_sp = state->alt_stack.ss_sp;
        frame->uc.uc_stack.ss_size = state->alt_stack.ss_size;
        frame->uc.uc_stack.ss_flags = state->alt_stack.ss_flags;
    } else {
        frame->uc.uc_stack.ss_sp = 0;
        frame->uc.uc_stack.ss_size = 0;
        frame->uc.uc_stack.ss_flags = 0;
    }

    if (state->alt_stack.ss_flags & SS_AUTODISARM) {
        state->alt_stack.ss_sp = 0;
        state->alt_stack.ss_size = 0;
        state->alt_stack.ss_flags = SS_DISABLE;
    }

    sigset_t old_mask;
    Signals::sigprocmask(state, SIG_SETMASK, nullptr, &old_mask);
    frame->uc.uc_sigmask = old_mask;

    // Now we need to copy the state to the frame
    frame->uc.uc_mcontext.gregs[REG_RAX] = state->GetGpr(X86_REF_RAX);
    frame->uc.uc_mcontext.gregs[REG_RCX] = state->GetGpr(X86_REF_RCX);
    frame->uc.uc_mcontext.gregs[REG_RDX] = state->GetGpr(X86_REF_RDX);
    frame->uc.uc_mcontext.gregs[REG_RBX] = state->GetGpr(X86_REF_RBX);
    frame->uc.uc_mcontext.gregs[REG_RSP] = state->GetGpr(X86_REF_RSP);
    frame->uc.uc_mcontext.gregs[REG_RBP] = state->GetGpr(X86_REF_RBP);
    frame->uc.uc_mcontext.gregs[REG_RSI] = state->GetGpr(X86_REF_RSI);
    frame->uc.uc_mcontext.gregs[REG_RDI] = state->GetGpr(X86_REF_RDI);
    frame->uc.uc_mcontext.gregs[REG_R8] = state->GetGpr(X86_REF_R8);
    frame->uc.uc_mcontext.gregs[REG_R9] = state->GetGpr(X86_REF_R9);
    frame->uc.uc_mcontext.gregs[REG_R10] = state->GetGpr(X86_REF_R10);
    frame->uc.uc_mcontext.gregs[REG_R11] = state->GetGpr(X86_REF_R11);
    frame->uc.uc_mcontext.gregs[REG_R12] = state->GetGpr(X86_REF_R12);
    frame->uc.uc_mcontext.gregs[REG_R13] = state->GetGpr(X86_REF_R13);
    frame->uc.uc_mcontext.gregs[REG_R14] = state->GetGpr(X86_REF_R14);
    frame->uc.uc_mcontext.gregs[REG_R15] = state->GetGpr(X86_REF_R15);
    frame->uc.uc_mcontext.gregs[REG_RIP] = state->GetRip();
    frame->uc.uc_mcontext.gregs[REG_EFL] = state->GetFlags();

    felix86_xsave(state, &frame->uc.uc_mcontext.fpregs->fxsave);

    state->SetGpr(X86_REF_RSP, (u64)frame);        // set the new stack pointer
    state->SetGpr(X86_REF_RDI, sig);               // set the signal
    state->SetGpr(X86_REF_RSI, (u64)&frame->info); // set the siginfo pointer
    state->SetGpr(X86_REF_RDX, (u64)&frame->uc);   // set the ucontext pointer
    state->SetGpr(X86_REF_RAX, 0);
    state->SetRip(signal.func);
    state->SetFlag(X86_REF_DF, 0);

    // Also update the allocatable state in the registers
    u64* regs = get_regs(host_context);
    regs[Recompiler::allocatedGPR(X86_REF_RSP).Index()] = state->GetGpr(X86_REF_RSP);
    regs[Recompiler::allocatedGPR(X86_REF_RDI).Index()] = state->GetGpr(X86_REF_RDI);
    regs[Recompiler::allocatedGPR(X86_REF_RSI).Index()] = state->GetGpr(X86_REF_RSI);
    regs[Recompiler::allocatedGPR(X86_REF_RDX).Index()] = state->GetGpr(X86_REF_RDX);
    regs[Recompiler::allocatedGPR(X86_REF_RAX).Index()] = state->GetGpr(X86_REF_RAX);
    regs[Recompiler::allocatedGPR(X86_REF_RIP).Index()] = state->GetRip();
}

void setupFrame_x86_rt(RegisteredSignal& signal, int sig, ThreadState* state, siginfo_t* guest_info, ucontext_t* host_context) {
    // sigreturn trampoline as it exists in the kernel
    // In x86_64 this doesn't exist and instead the user specifies a restorer
    static const struct {
        u8 movl;
        u32 val;
        u16 int80;
        u8 pad;
    } __attribute__((packed)) code = {
        0xb8,
        felix86_x86_32_rt_sigreturn,
        0x80cd,
        0,
    };

    if (!(signal.flags & SA_RESTORER) && signal.restorer) {
        WARN("Legacy altstack switching detected");
    }

    bool use_altstack = signal.flags & SA_ONSTACK;
    u64 rsp = state->GetGpr(X86_REF_RSP);
    if (use_altstack) {
        if (sa_ss_flags(state) == 0) {
            rsp = (u64)state->alt_stack.ss_sp + state->alt_stack.ss_size;
        }
    }

    if (rsp == 0) {
        WARN("RSP is null, use_altstack: %d... using original stack", use_altstack);
        rsp = state->GetGpr(X86_REF_RSP);
        ASSERT(rsp != 0);
    } else if (use_altstack) {
        VERBOSE("Altstack was established");
    }

    rsp = rsp - (rsp % 8);

    if (felix86_xsave_contains_ymms()) {
        rsp = rsp - sizeof(ymm_hi) - sizeof(xsave_header);
    }

    rsp -= sizeof(x86_fpstate);
    x86_fpstate* fpstate = (x86_fpstate*)rsp;
    ASSERT((u64)fpstate < UINT32_MAX);

    felix86_fsave_32(state, &fpstate->fsave);

    fpstate->magic = 0; // extended state

    felix86_xsave(state, &fpstate->fxsave);

    rsp -= sizeof(x86_rt_sigframe);

    rsp = ((rsp + 4) & -16ul) - 4;

    x86_rt_sigframe* frame = (x86_rt_sigframe*)rsp;
    ASSERT((u64)frame < UINT32_MAX);
    memcpy(frame->retcode, &code, sizeof(code));
    frame->pretcode = (u32)(u64)(char*)frame->retcode;
    if (signal.flags & SA_RESTORER) {
        frame->pretcode = signal.restorer;
    }

    frame->info = *guest_info;

    frame->uc.uc_mcontext.ax = state->GetGpr(X86_REF_RAX);
    frame->uc.uc_mcontext.cx = state->GetGpr(X86_REF_RCX);
    frame->uc.uc_mcontext.dx = state->GetGpr(X86_REF_RDX);
    frame->uc.uc_mcontext.bx = state->GetGpr(X86_REF_RBX);
    frame->uc.uc_mcontext.sp = state->GetGpr(X86_REF_RSP);
    frame->uc.uc_mcontext.bp = state->GetGpr(X86_REF_RBP);
    frame->uc.uc_mcontext.si = state->GetGpr(X86_REF_RSI);
    frame->uc.uc_mcontext.di = state->GetGpr(X86_REF_RDI);
    frame->uc.uc_mcontext.sp_at_signal = state->GetGpr(X86_REF_RSP);
    frame->uc.uc_mcontext.ip = state->GetRip();
    frame->uc.uc_mcontext.flags = state->GetFlags();
    frame->uc.uc_mcontext.fs = state->fs;
    frame->uc.uc_mcontext.gs = state->gs;
    frame->uc.uc_mcontext.cs = state->cs;
    frame->uc.uc_mcontext.ds = state->ds;
    frame->uc.uc_mcontext.ss = state->ss;
    frame->uc.uc_mcontext.es = state->es;
    frame->uc.uc_mcontext.__fsh = 0;
    frame->uc.uc_mcontext.__gsh = 0;
    frame->uc.uc_mcontext.__csh = 0;
    frame->uc.uc_mcontext.__dsh = 0;
    frame->uc.uc_mcontext.__ssh = 0;
    frame->uc.uc_mcontext.__esh = 0;
    if (use_altstack) {
        frame->uc.uc_stack.ss_sp = (u32)(u64)state->alt_stack.ss_sp;
        frame->uc.uc_stack.ss_size = state->alt_stack.ss_size;
        frame->uc.uc_stack.ss_flags = state->alt_stack.ss_flags;
    } else {
        frame->uc.uc_stack.ss_sp = 0;
        frame->uc.uc_stack.ss_size = 0;
        frame->uc.uc_stack.ss_flags = 0;
    }
    if (state->alt_stack.ss_flags & SS_AUTODISARM) {
        state->alt_stack.ss_sp = 0;
        state->alt_stack.ss_size = 0;
        state->alt_stack.ss_flags = SS_DISABLE;
    }
    sigset_t old_mask;
    Signals::sigprocmask(state, SIG_SETMASK, nullptr, &old_mask);
    frame->uc.uc_sigmask = old_mask;
    frame->uc.uc_mcontext.fpstate = (u32)(u64)fpstate;

    // These are laid out in the frame in the argument order, we don't need to push any arguments
    frame->sig = sig;
    frame->pinfo = (u32)(u64)&frame->info;
    frame->puc = (u32)(u64)&frame->uc;

    state->SetGpr(X86_REF_RSP, (u64)frame); // set the new stack pointer
    state->SetGpr(X86_REF_RAX, 0);
    state->SetRip(signal.func);
    state->SetFlag(X86_REF_DF, 0);

    // Also update the allocatable state in the registers
    u64* regs = get_regs(host_context);
    regs[Recompiler::allocatedGPR(X86_REF_RSP).Index()] = state->GetGpr(X86_REF_RSP);
    regs[Recompiler::allocatedGPR(X86_REF_RAX).Index()] = state->GetGpr(X86_REF_RAX);
    regs[Recompiler::allocatedGPR(X86_REF_RIP).Index()] = state->GetRip();
}

void setupFrame_x86(RegisteredSignal& signal, int sig, ThreadState* state, siginfo_t* guest_info, ucontext_t* host_context) {
    static const struct {
        u16 poplmovl;
        u32 val;
        u16 int80;
    } __attribute__((packed)) code = {
        0xb858, /* popl %eax ; movl $...,%eax */
        felix86_x86_32_sigreturn,
        0x80cd, /* int $0x80 */
    };

    // Kernel mentions some legacy altstack switching method, ensure it's not used until we find a program that does
    ASSERT(!(state->ss != state->ds && !(signal.flags & SA_RESTORER) && signal.restorer));
    u64 rsp = state->GetGpr(X86_REF_RSP);
    bool use_altstack = signal.flags & SA_ONSTACK;
    if (use_altstack) {
        if (sa_ss_flags(state) == 0) {
            rsp = (u64)state->alt_stack.ss_sp + state->alt_stack.ss_size;
        }
    }

    if (rsp == 0) {
        WARN("RSP is null, use_altstack: %d... using original stack", use_altstack);
        rsp = state->GetGpr(X86_REF_RSP);
        ASSERT(rsp != 0);
    } else if (use_altstack) {
        VERBOSE("Altstack was established");
    }

    rsp = rsp - (rsp % 8);
    if (felix86_xsave_contains_ymms()) {
        rsp = rsp - sizeof(ymm_hi) - sizeof(xsave_header);
    }
    rsp -= sizeof(x86_fpstate);
    x86_fpstate* fpstate = (x86_fpstate*)rsp;
    rsp = rsp - sizeof(x86_legacy_sigframe);
    rsp = ((rsp + 4) & -16ul) - 4;

    x86_legacy_sigframe* frame = (x86_legacy_sigframe*)rsp;
    frame->sc.ax = state->GetGpr(X86_REF_RAX);
    frame->sc.cx = state->GetGpr(X86_REF_RCX);
    frame->sc.dx = state->GetGpr(X86_REF_RDX);
    frame->sc.bx = state->GetGpr(X86_REF_RBX);
    frame->sc.sp = state->GetGpr(X86_REF_RSP);
    frame->sc.bp = state->GetGpr(X86_REF_RBP);
    frame->sc.si = state->GetGpr(X86_REF_RSI);
    frame->sc.di = state->GetGpr(X86_REF_RDI);
    frame->sc.sp_at_signal = state->GetGpr(X86_REF_RSP);
    frame->sc.ip = state->GetRip();
    frame->sc.flags = state->GetFlags();
    frame->sc.fs = state->fs;
    frame->sc.gs = state->gs;
    frame->sc.cs = state->cs;
    frame->sc.ds = state->ds;
    frame->sc.ss = state->ss;
    frame->sc.es = state->es;
    frame->sc.__fsh = 0;
    frame->sc.__gsh = 0;
    frame->sc.__csh = 0;
    frame->sc.__dsh = 0;
    frame->sc.__ssh = 0;
    frame->sc.__esh = 0;
    sigset_t old_mask;
    Signals::sigprocmask(state, SIG_SETMASK, nullptr, &old_mask);
    frame->sc.oldmask = old_mask.__val[0];
    ASSERT((u64)frame < UINT32_MAX);
    memcpy(frame->retcode, &code, sizeof(code));
    frame->pretcode = (u32)(u64)(char*)frame->retcode;
    if (signal.flags & SA_RESTORER) {
        frame->pretcode = signal.restorer;
    }
    frame->sig = sig;
    frame->extramask = old_mask.__val[1];
    felix86_fsave_32(state, &fpstate->fsave);
    frame->fpstate_unused.magic = 0; // extended state
    felix86_xsave(state, &fpstate->fxsave);
    frame->sc.fpstate = (u32)(u64)fpstate;

    state->SetGpr(X86_REF_RSP, (u64)frame); // set the new stack pointer
    state->SetGpr(X86_REF_RAX, sig);
    state->SetGpr(X86_REF_RDX, 0);
    state->SetGpr(X86_REF_RCX, 0);
    state->SetRip(signal.func);
    state->SetFlag(X86_REF_DF, 0);

    // Also update the allocatable state in the registers
    u64* regs = get_regs(host_context);
    regs[Recompiler::allocatedGPR(X86_REF_RSP).Index()] = state->GetGpr(X86_REF_RSP);
    regs[Recompiler::allocatedGPR(X86_REF_RAX).Index()] = state->GetGpr(X86_REF_RAX);
    regs[Recompiler::allocatedGPR(X86_REF_RDX).Index()] = state->GetGpr(X86_REF_RDX);
    regs[Recompiler::allocatedGPR(X86_REF_RCX).Index()] = state->GetGpr(X86_REF_RCX);
    regs[Recompiler::allocatedGPR(X86_REF_RIP).Index()] = state->GetRip();
}

void setupFrame(RegisteredSignal& signal, int sig, ThreadState* state, siginfo_t* guest_info, ucontext_t* host_context) {
    if (!g_mode32) {
        return setupFrame_x64(signal, sig, state, guest_info, host_context);
    } else {
        if (signal.flags & SA_SIGINFO) {
            return setupFrame_x86_rt(signal, sig, state, guest_info, host_context);
        } else {
            return setupFrame_x86(signal, sig, state, guest_info, host_context);
        }
    }
}

void restore_sigcontext_32(ThreadState* state, x86_sigcontext_32* ctx) {
    state->SetGpr(X86_REF_RAX, ctx->ax);
    state->SetGpr(X86_REF_RCX, ctx->cx);
    state->SetGpr(X86_REF_RDX, ctx->dx);
    state->SetGpr(X86_REF_RBX, ctx->bx);
    state->SetGpr(X86_REF_RSP, ctx->sp);
    state->SetGpr(X86_REF_RBP, ctx->bp);
    state->SetGpr(X86_REF_RSI, ctx->si);
    state->SetGpr(X86_REF_RDI, ctx->di);
    state->SetRip(ctx->ip);

    u64 flags = ctx->flags;
    bool cf = (flags >> 0) & 1;
    bool pf = (flags >> 2) & 1;
    bool af = (flags >> 4) & 1;
    bool zf = (flags >> 6) & 1;
    bool sf = (flags >> 7) & 1;
    bool of = (flags >> 11) & 1;
    bool df = (flags >> 10) & 1;
    state->SetFlag(X86_REF_CF, cf);
    state->SetFlag(X86_REF_PF, pf);
    state->SetFlag(X86_REF_AF, af);
    state->SetFlag(X86_REF_ZF, zf);
    state->SetFlag(X86_REF_SF, sf);
    state->SetFlag(X86_REF_OF, of);
    state->SetFlag(X86_REF_DF, df);

    state->cs = ctx->cs;
    state->ss = ctx->ss;
    state->ds = ctx->ds;
    state->es = ctx->es;
    state->fs = ctx->fs;
    state->gs = ctx->gs;
}

void Signals::sigreturn(ThreadState* state, bool rt) {
    u64 rsp = state->GetGpr(X86_REF_RSP);

    if (g_mode32) {
        // In 32-bit mode, rt_sigreturn signals pop just the return address while legacy signals pop the sig value too, see trampoline
        rsp -= rt ? 4 : 8;
        x86_rt_sigframe* rt_frame = (x86_rt_sigframe*)rsp;
        x86_legacy_sigframe* legacy_frame = (x86_legacy_sigframe*)rsp;

        SIGLOG("------- 32-bit rt_sigreturn TID: %d returning to RIP=%lx -------", gettid(), state->GetRip());
        x86_sigcontext_32* ctx;
        if (rt) {
            ctx = &rt_frame->uc.uc_mcontext;
        } else {
            ctx = &legacy_frame->sc;
        }
        restore_sigcontext_32(state, ctx);
        felix86_xrstor(state, &((x86_fpstate*)(u64)ctx->fpstate)->fxsave);

        // Restore signal mask to what it was supposed to be outside of signal handler
        sigset_t new_set;
        if (rt) {
            new_set = rt_frame->uc.uc_sigmask;
        } else {
            sigemptyset(&new_set);
            new_set.__val[0] = legacy_frame->sc.oldmask;
            new_set.__val[1] = legacy_frame->extramask;
        }
        Signals::sigprocmask(state, SIG_SETMASK, &new_set, nullptr);
    } else {
        // When the signal handler returned, it popped the return address, which is the 8 bytes "pretcode" field in the sigframe
        // We need to adjust the rsp back before reading the entire struct.
        rsp -= 8;
        x64_rt_sigframe* frame = (x64_rt_sigframe*)rsp;
        rsp += sizeof(x64_rt_sigframe);

        // The registers need to be restored to what they were before the signal handler was called, or what the signal handler changed them to.
        state->SetGpr(X86_REF_RAX, frame->uc.uc_mcontext.gregs[REG_RAX]);
        state->SetGpr(X86_REF_RCX, frame->uc.uc_mcontext.gregs[REG_RCX]);
        state->SetGpr(X86_REF_RDX, frame->uc.uc_mcontext.gregs[REG_RDX]);
        state->SetGpr(X86_REF_RBX, frame->uc.uc_mcontext.gregs[REG_RBX]);
        state->SetGpr(X86_REF_RSP, frame->uc.uc_mcontext.gregs[REG_RSP]);
        state->SetGpr(X86_REF_RBP, frame->uc.uc_mcontext.gregs[REG_RBP]);
        state->SetGpr(X86_REF_RSI, frame->uc.uc_mcontext.gregs[REG_RSI]);
        state->SetGpr(X86_REF_RDI, frame->uc.uc_mcontext.gregs[REG_RDI]);
        state->SetGpr(X86_REF_R8, frame->uc.uc_mcontext.gregs[REG_R8]);
        state->SetGpr(X86_REF_R9, frame->uc.uc_mcontext.gregs[REG_R9]);
        state->SetGpr(X86_REF_R10, frame->uc.uc_mcontext.gregs[REG_R10]);
        state->SetGpr(X86_REF_R11, frame->uc.uc_mcontext.gregs[REG_R11]);
        state->SetGpr(X86_REF_R12, frame->uc.uc_mcontext.gregs[REG_R12]);
        state->SetGpr(X86_REF_R13, frame->uc.uc_mcontext.gregs[REG_R13]);
        state->SetGpr(X86_REF_R14, frame->uc.uc_mcontext.gregs[REG_R14]);
        state->SetGpr(X86_REF_R15, frame->uc.uc_mcontext.gregs[REG_R15]);
        state->SetRip(frame->uc.uc_mcontext.gregs[REG_RIP]);

        SIGLOG("------- 64-bit rt_sigreturn TID: %d returning to RIP=%lx -------", gettid(), state->GetRip());

        u64 flags = frame->uc.uc_mcontext.gregs[REG_EFL];
        bool cf = (flags >> 0) & 1;
        bool pf = (flags >> 2) & 1;
        bool af = (flags >> 4) & 1;
        bool zf = (flags >> 6) & 1;
        bool sf = (flags >> 7) & 1;
        bool of = (flags >> 11) & 1;
        bool df = (flags >> 10) & 1;
        state->SetFlag(X86_REF_CF, cf);
        state->SetFlag(X86_REF_PF, pf);
        state->SetFlag(X86_REF_AF, af);
        state->SetFlag(X86_REF_ZF, zf);
        state->SetFlag(X86_REF_SF, sf);
        state->SetFlag(X86_REF_OF, of);
        state->SetFlag(X86_REF_DF, df);

        felix86_xrstor(state, &frame->uc.uc_mcontext.fpregs->fxsave);

        // Restore signal mask to what it was supposed to be outside of signal handler
        Signals::sigprocmask(state, SIG_SETMASK, &frame->uc.uc_sigmask, nullptr);
    }
}

struct riscv_v_state {
    unsigned long vstart;
    unsigned long vl;
    unsigned long vtype;
    unsigned long vcsr;
    unsigned long vlenb;
    void* datap;
};

u64 get_pc(void* ctx) {
#ifdef __riscv
    return (u64)((ucontext_t*)ctx)->uc_mcontext.__gregs[REG_PC];
#else
    UNREACHABLE();
    return 0;
#endif
}

void set_pc(void* ctx, u64 new_pc) {
#ifdef __riscv
    ((ucontext_t*)ctx)->uc_mcontext.__gregs[REG_PC] = new_pc;
#else
    UNREACHABLE();
#endif
}

u64* get_fprs(void* ctx) {
#ifdef __riscv
    return (u64*)((ucontext_t*)ctx)->uc_mcontext.__fpregs.__d.__f;
#else
    UNREACHABLE();
    return nullptr;
#endif
}

riscv_v_state* get_riscv_vector_state(void* ctx) {
#ifdef __riscv
    ucontext_t* context = (ucontext_t*)ctx;
    mcontext_t* mcontext = &context->uc_mcontext;
    unsigned int* reserved = mcontext->__fpregs.__q.__glibc_reserved;

    // Normally the glibc should have better support for this, but this will be fine for now
    if (reserved[1] != 0x53465457) { // RISC-V V extension magic number that indicates the presence of vector state
        return nullptr;              // old kernel version, unsupported, we can't get the vector state and the vector regs may be unstable
    }

    void* after_fpregs = reserved + 3;
    riscv_v_state* v_state = (riscv_v_state*)after_fpregs;
    return v_state;
#else
    return nullptr;
#endif
}

void pull_registers_from_context(ThreadState* state, ucontext_t* uctx) {
    // Pull registers out from statically allocated registers and commit them to memory
    riscv_v_state* vector_state = get_riscv_vector_state(uctx);
    if (!vector_state) {
        ERROR("Kernel version too old, can't recover vector state");
    }

    std::array<XmmReg, 32> xmm_regs;
    u8* datap = (u8*)vector_state->datap;
    for (int i = 0; i < 32; i++) {
        xmm_regs[i] = *(XmmReg*)datap;
        datap += vector_state->vlenb;
    }

    for (int i = 0; i < 16; i++) {
        x86_ref_e ref = (x86_ref_e)(X86_REF_XMM0 + i);
        state->SetXmm(ref, xmm_regs[Recompiler::allocatedXMM(ref).Index()]);
    }

    u64* regs = get_regs(uctx);
    for (int i = 0; i < 16; i++) {
        x86_ref_e ref = (x86_ref_e)(X86_REF_RAX + i);
        state->SetGpr(ref, regs[Recompiler::allocatedGPR(ref).Index()]);
    }

    state->SetFlag(X86_REF_OF, regs[Recompiler::allocatedGPR(X86_REF_OF).Index()]);
    state->SetFlag(X86_REF_CF, regs[Recompiler::allocatedGPR(X86_REF_CF).Index()]);
    state->SetFlag(X86_REF_ZF, regs[Recompiler::allocatedGPR(X86_REF_ZF).Index()]);
    state->SetFlag(X86_REF_SF, regs[Recompiler::allocatedGPR(X86_REF_SF).Index()]);
    state->SetRip(regs[Recompiler::allocatedGPR(X86_REF_RIP).Index()]);
    // The rest of the state is never statically allocated, so at safepoints it is always correct in memory
}

// Set ucontext_t register values in preparation of signal and setup the stack
// Afterwards we return from the signal handler normally. The RISC-V PC will be set to the
// dispatcher and the x86 RIP to the signal handler.
void prepare_guest_signal(int sig, siginfo_t* guest_info, ucontext_t* uctx) {
    ThreadState* state = ThreadState::Get();
    u64 rip = state->GetRip();
    set_pc(uctx, state->recompiler->getCompileNext());

    RegisteredSignal* handler = state->signal_table->getRegisteredSignal(sig);

    // While we *could* just jump to the handler and let the registers be
    // we pull them to ThreadState so we can construct the signal context using ThreadState instead of ucontext_t
    // as it makes the code cleaner
    pull_registers_from_context(state, uctx);

    setupFrame(*handler, sig, state, guest_info, uctx);

    sigset_t set;
    sigemptyset(&set);
    if (handler->flags & SA_NODEFER) {
        WARN("Preparing signal with SA_NODEFER");
    } else {
        sigaddset(&set, sig);
    }
    sigorset(&set, &set, (const sigset_t*)&handler->mask);
    int result = Signals::sigprocmask(state, SIG_BLOCK, &set, nullptr);
    ASSERT(result == 0);

    // Sigprocmask sets the mask inside the signal handler, set it outside too
    sigset_t host_new_mask = state->signal_mask;
    sigandset(&host_new_mask, &state->signal_mask, Signals::hostSignalMask());
    uctx->uc_sigmask = host_new_mask;

    SIGLOG("Preparing signal %s (%d) during RIP=%lx, RSP=%lx, handler at %lx", sigdescr_np(sig), sig, rip, state->gprs[X86_REF_RSP], handler->func);

    if (handler->flags & SA_RESETHAND) {
        Signals::registerSignalHandler(state, sig, (u64)SIG_DFL, handler->mask, handler->flags, handler->restorer);
    }
}

void prepare_synchronous_signal(ThreadState* state, int sig, siginfo_t* info, void* ctx, u64 rip) {
    // Set GP to actual instruction that faulted so that prepare_guest_signal picks it up for the context
    get_regs(ctx)[Recompiler::allocatedGPR(X86_REF_RIP).Index()] = rip;
    prepare_guest_signal(sig, info, (ucontext_t*)ctx);
}

bool handle_safepoint(ThreadState* current_state, siginfo_t* info, ucontext_t* context, u64 pc) {
    // First we need to check if we are a safepoint
    // Safepoint instructions perform SD(x0, -8, Recompiler::threadStatePointer())
    u32 expected_instruction;
    {
        Assembler tas((u8*)&expected_instruction, sizeof(u32));
        tas.SD(x0, -8, Recompiler::threadStatePointer());
    }

    u32 current_instruction = *(u32*)pc;
    if (current_instruction != expected_instruction) {
        // Not in a safepoint, don't handle signal now
        // Return. If no host signal handler picks up this signal, then it will be deferred
        return false;
    }

    if (!is_in_jit_code(current_state, (u8*)pc)) {
        // Not in JIT code, rare but possible
        return false;
    }

    if (info->si_addr != (u8*)current_state - 8) {
        // Sanity check. An SD(x0, -8, Recompiler::threadStatePointer()) that is inside
        // the JIT code will always be a safe point but we can never have enough checks
        WARN("Faulting address expected to be safepoint page but it's not?");
        return false;
    }

    if (current_state->should_restart_syscall) {
        // Check if previous instruction is FELIX86_HINT_SAFEPOINT_SYSCALL, i.e. if this safepoint is right after a syscall
        u32 expected_previous_instruction;
        {
            Assembler tas((u8*)&expected_previous_instruction, sizeof(u32));
            tas.SLTIU(x0, x0, FELIX86_HINT_SAFEPOINT_SYSCALL);
        }

        u32 actual_previous_instruction = *(u32*)(pc - 4);
        u64* regs = get_regs(context);
        if (expected_previous_instruction == actual_previous_instruction) {
            // Set RIP to RIP - 2 to go back to the syscall instruction and restore the RAX
            // We do this in the context and then prepare_guest_signal will pick it up
            regs[Recompiler::allocatedGPR(X86_REF_RIP).Index()] -= 2;
            u8* data = (u8*)regs[Recompiler::allocatedGPR(X86_REF_RIP).Index()];
            ASSERT((data[0] == 0x0f && data[1] == 0x05) || (data[0] == 0xcd && data[1] == 0x80));
            regs[Recompiler::allocatedGPR(X86_REF_RAX).Index()] = current_state->restarted_syscall_original_rax;
        } else {
            ASSERT_MSG(false, "state->should_restart_syscall set but not at a syscall safepoint?");
        }
        current_state->should_restart_syscall = false;
    }

    // Mask signals until we are done messing with these
    u64 full = -1ull, old;
    ASSERT(syscall(SYS_rt_sigprocmask, SIG_SETMASK, &full, &old, sizeof(u64)) == 0);

    ASSERT_MSG(current_state->effective_deferred_signals, "Faulted at safepoint, but no effective deferred signals, this shouldn't happen");

    // While the order of standard signals is unspecified, if standard and real-time signals are queued
    // standard signals are serviced first, and real-time signals follow a least to most order
    // Thus we just get the least significant set bit
    int sig = __builtin_ctzll(current_state->effective_deferred_signals);

    // Since we block signals, race here due to another signal being deferred is not possible
    siginfo_t guest_info;
    if (sig <= 30) {
        guest_info = current_state->deferred_standard_info[sig];
        current_state->deferred_signals = current_state->deferred_signals & ~(1ull << sig);
        current_state->effective_deferred_signals = current_state->effective_deferred_signals & ~(1ull << sig);
    } else {
        SignalQueueNode* node = current_state->deferred_realtime_info[sig - 31];
        ASSERT(node);
        guest_info = node->info;
        if (node->next == nullptr) {
            current_state->deferred_signals = current_state->deferred_signals & ~(1ull << sig);
            current_state->effective_deferred_signals = current_state->effective_deferred_signals & ~(1ull << sig);
        } else {
            current_state->deferred_realtime_info[sig - 31] = node->next;
        }
        ASSERT(munmap(node, 4096) == 0);
    }

    // Safe to restore our mask now
    ASSERT(syscall(SYS_rt_sigprocmask, SIG_SETMASK, &old, nullptr, sizeof(u64)) == 0);

    // This gives us a 0-indexed signal, but the actual signal number starts from 1
    sig += 1;

    // Handle signal...
    // In order to handle the signal, we need to set the context in such a way that the JIT will jump
    // to the dispatcher after returning, the stack will be well prepared. When finished, the code will naturally return to where it was supposed
    // to return because sigreturn will set the PC according to the ucontext. Or it will longjmp out, but we don't care because we are out of the
    // host signal handler. Being in a safepoint means we can do this PC manipulation without worrying about potential locks or stack overflows
    prepare_guest_signal(sig, &guest_info, context);
    return true;
}

bool handle_smc(ThreadState* current_state, siginfo_t* info, ucontext_t* context, u64 pc) {
    if (!is_in_jit_code(current_state, (u8*)pc)) {
        WARN("We hit a SIGSEGV ACCERR but PC is not in JIT code...");
        return false;
    }

    SMCLOG("Handling SMC on %lx during PC: %lx", info->si_addr, pc);
    u64 write_address = (u64)info->si_addr & ~0xFFFull;
    Recompiler::invalidateRangeGlobal(write_address, write_address + 1, "self-modifying code");
    ASSERT_MSG(::mprotect((void*)write_address, 0x1000, PROT_READ | PROT_WRITE) == 0, "mprotect failed on address %lx", write_address);
    return true;
}

bool handle_breakpoint(ThreadState* current_state, siginfo_t* info, ucontext_t* context, u64 pc) {
    if (is_in_jit_code(current_state, (u8*)pc)) {
        // Search to see if it is our breakpoint
        // Note the we don't use EBREAK as gdb refuses to continue when it hits that if it doesn't have a breakpoint,
        // and also refuses to call our signal handler.
        // So we use illegal instructions to emulate breakpoints.
        // GDB *can* be configured to do what we want, but that would also require configuring, which I don't like,
        // I prefer it when it just works out of the box
        for (auto& bp : g_breakpoints) {
            for (u64 location : bp.second) {
                if (location == pc) {
                    printf("Guest breakpoint %016lx hit at %016lx, continuing...\n", bp.first, pc);
                    set_pc(context, pc + 4); // skip the unimp instruction
                    return true;
                }
            }
        }
    }

    return false;
}

bool handle_wild_sigsegv(ThreadState* current_state, siginfo_t* info, ucontext_t* context, u64 pc) {
    if (g_config.abort_sigsegv) {
        // Re-raise as SIGABRT so that a core dump is generated
        struct sigaction sa{};
        sa.sa_handler = SIG_DFL;
        sigaction(SIGABRT, &sa, nullptr); // nop out old signal handler
        raise(SIGABRT);
    }

    // In many cases it's annoying to attach a debugger at the start of a program, because it may be spawning many processes which
    // can trip up gdb and it won't know which fork to follow. The "don't detach forks" mode is also kind of jittery as far as I can see.
    // The capture_sigsegv mode can help us sleep the process for a while to attach gdb and get a proper backtrace.
    bool in_jit_code = is_in_jit_code(current_state, (u8*)pc);
    bool capture_it = g_config.capture_sigsegv;
    if (capture_it) {
        int pid = gettid();
        PLAIN("I have been hit by a wild SIGSEGV%s! My TID is %d, you have 40 seconds to attach gdb using `gdb -p %d` to find out why! If you "
              "think "
              "this SIGSEGV was intended, disabled this mode by unsetting the `capture_sigsegv` option.",
              !in_jit_code ? ANSI_BOLD " in emulator code" ANSI_COLOR_RESET : "", pid, pid);

        if (g_config.calltrace) {
            LOG("Current RIP:");
            if (in_jit_code) {
                BlockMetadata* current_block = get_block_metadata(current_state, pc);
                if (current_block) {
                    u64 actual_rip = get_actual_rip(*current_block, pc);
                    print_address(actual_rip);
                } else {
#ifdef __riscv
                    WARN("Failed to get actual RIP, block: %lx", context->uc_mcontext.__gregs[Recompiler::allocatedGPR(X86_REF_RIP).Index()]);
#endif
                }
            } else {
                print_address(current_state->rip);
            }
        }

        if (g_config.calltrace) {
            dump_states();
        }
        ::sleep(40);
        return true;
    } else {
        // Check if signal handler is SIG_DFL or SIG_IGN
        RegisteredSignal* handler = current_state->signal_table->getRegisteredSignal(SIGSEGV);
        if (handler->func == (u64)SIG_DFL) {
            // We need to die from a SIGSEGV, set the handler to default and unmask it
            WARN("SIGSEGV with default behavior, terminating...");
            ASSERT((i64)signal(SIGSEGV, SIG_DFL) > 0);
            sigset_t set;
            sigemptyset(&set);
            sigaddset(&set, SIGSEGV);
            ASSERT(sigprocmask(SIG_UNBLOCK, &set, nullptr) == 0);
            raise(SIGSEGV);
            UNREACHABLE();
        } else if (handler->func == (u64)SIG_IGN) {
            // Uhh... warn and return true?
            WARN("SIGSEGV with ignore behavior");
            return true;
        }
        return false;
    }
}

bool handle_wild_sigabrt(ThreadState* current_state, siginfo_t* info, ucontext_t* context, u64 pc) {
    // Similar to SIGSEGV sleeping, SIGABRT can be nice to capture because it's called when guest errors like
    // stack smashing happen.
    bool in_jit_code = is_in_jit_code(current_state, (u8*)pc);
    if (g_config.capture_sigabrt) {
        int pid = gettid();
        PLAIN("I have been hit by a wild SIGABRT%s! My TID is %d, you have 40 seconds to attach gdb using `gdb -p %d` to find out why! If you "
              "think this SIGABRT was intended, disabled this mode by unsetting the `capture_sigabrt` option.",
              !in_jit_code ? ANSI_BOLD " in emulator code" ANSI_COLOR_RESET : "", pid, pid);

        if (g_config.calltrace) {
            LOG("Current RIP:");
            if (in_jit_code) {
                BlockMetadata* current_block = get_block_metadata(current_state, pc);
                if (current_block) {
                    u64 actual_rip = get_actual_rip(*current_block, pc);
                    print_address(actual_rip);
                } else {
#ifdef __riscv
                    WARN("Failed to get actual RIP, block: %lx", context->uc_mcontext.__gregs[Recompiler::allocatedGPR(X86_REF_RIP).Index()]);
#endif
                }
            } else {
                print_address(current_state->rip);
            }
        }

        if (g_config.calltrace) {
            dump_states();
        }
        ::sleep(40);
        return true;
    } else {
        return false;
    }
}

bool handle_synchronous(ThreadState* current_state, siginfo_t* info, ucontext_t* context, u64 pc) {
    // We can't cause a SIGSEGV SI_KERNEL from RISC-V, so fix up info->si_code to match x86 behavior
    if (!is_in_jit_code(current_state, (u8*)pc)) {
        return false;
    }

    u32 faulting_instruction = *(u32*)pc;
    u32 expected_instruction;
    Assembler tas((u8*)&expected_instruction, sizeof(u32));
    tas.SD(x0, 0, x0);
    if (faulting_instruction != expected_instruction) {
        return false;
    }

    // Check the hint right after to make sure this is a hlt
    u32 next_instruction = *(((u32*)pc) + 1);

    u32 expected_hlt, expected_divzero, expected_int3, expected_ud2;
    {
        Assembler tas2((u8*)&expected_hlt, sizeof(u32));
        tas2.SLTIU(x0, x0, FELIX86_HINT_HLT);
    }
    {
        Assembler tas2((u8*)&expected_divzero, sizeof(u32));
        tas2.SLTIU(x0, x0, FELIX86_HINT_DIVZERO);
    }
    {
        Assembler tas2((u8*)&expected_int3, sizeof(u32));
        tas2.SLTIU(x0, x0, FELIX86_HINT_INT3);
    }
    {
        Assembler tas2((u8*)&expected_ud2, sizeof(u32));
        tas2.SLTIU(x0, x0, FELIX86_HINT_UD2);
    }

    BlockMetadata* current_block = get_block_metadata(current_state, pc);
    ASSERT_MSG(current_block, "Failed to get current block during synchronous signal with PC=%lx, RIP=%lx", pc, current_state->rip);
    u64 actual_rip = get_actual_rip(*current_block, pc);

    int sig;
    if (next_instruction == expected_hlt) {
        sig = SIGSEGV;
        info->si_code = SI_KERNEL;
        info->si_addr = nullptr;
    } else if (next_instruction == expected_divzero) {
        sig = SIGFPE;
        info->si_code = FPE_INTDIV;
        info->si_addr = (void*)actual_rip;
    } else if (next_instruction == expected_int3) {
        sig = SIGTRAP;
        info->si_code = SI_KERNEL;
        info->si_addr = nullptr;
    } else if (next_instruction == expected_ud2) {
        sig = SIGILL;
        info->si_code = ILL_ILLOPN;
        info->si_addr = (void*)actual_rip;
    } else {
        return false;
    }

    prepare_synchronous_signal(current_state, sig, info, context, actual_rip);
    return true;
}

constexpr std::array<RegisteredHostSignal, 6> host_signals = {{
    {SIGSEGV, SEGV_ACCERR, handle_safepoint},
    {SIGSEGV, SEGV_ACCERR, handle_smc},
    {SIGSEGV, SEGV_MAPERR, handle_synchronous},
    {SIGILL, 0, handle_breakpoint},
    {SIGSEGV, 0, handle_wild_sigsegv}, // order matters, relevant sigsegvs are handled before this handler
    {SIGABRT, 0, handle_wild_sigabrt},
}};

bool dispatch_host(int sig, siginfo_t* info, void* ctx) {
    ThreadState* state = ThreadState::Get();
    u64 pc = get_pc(ctx);
    int code = info->si_code;
    for (auto& handler : host_signals) {
        if (handler.sig == sig && (handler.code == code || handler.code == 0)) {
            // The host signal handler matches what we want, attempt it
            if (handler.func(state, info, (ucontext_t*)ctx, pc)) {
                return true;
            }
        }
    }

    return false;
}

// Main signal handler function, all signals come here
void signal_handler(int sig, siginfo_t* info, void* ctx) {
    if (g_config.print_all_signals) {
        SIGLOG("------- Signal %s (%d) with code %d -------", sigdescr_np(sig), sig, info->si_code);
    }

    // First, check if this is a host signal
    bool handled;

    handled = dispatch_host(sig, info, ctx);
    if (handled) {
        // Ok it was a host signal
        return;
    }

    u64 pc = get_pc(ctx);
    ThreadState* state = ThreadState::Get();
    // Note: It's not enough to just check si_code > 0, for example SIGCHLD can be sent at any moment but it has a positive code
    // But we do need a si_code > 0 check, because si_code <= 0 means asynchronous and we can (unfortunately) be sent e.g. SIGSEGV from another
    // process via kill or whatever other method
    if (info->si_code > 0 && (sig == SIGSEGV || sig == SIGBUS || sig == SIGILL || sig == SIGFPE || sig == SIGTRAP)) {
        // Synchronous signal, handle immediately
        if (is_in_jit_code(state, (u8*)pc)) {
            BlockMetadata* current_block = get_block_metadata(state, pc);
            ASSERT_MSG(current_block, "Failed to get current block during synchronous signal with PC=%lx, RIP=%lx", pc, state->rip);
            u64 actual_rip = get_actual_rip(*current_block, pc);
            return prepare_synchronous_signal(state, sig, info, ctx, actual_rip);
        } else {
            ERROR("Synchronous signal %s with code %d but not in JIT code during RIP=%lx, PC=%lx", sigdescr_np(sig), info->si_code, state->rip, pc);
        }
    } else {
        // Asynchronous signal, defer
        // If we were in a safepoint, the signal would've been handled
        ASSERT(sig >= 1 && sig <= 64);
        int index = sig - 1;
        u32 ecall;
        {
            Assembler tas((u8*)&ecall, sizeof(u32));
            tas.ECALL();
        }
        RegisteredSignal* signal = state->signal_table->getRegisteredSignal(sig);
        if (state->in_restartable_syscall && (signal->flags & SA_RESTART) && *((u32*)(pc - 4)) == ecall &&
            get_regs(ctx)[biscuit::a0.Index()] == -EINTR) {
            state->should_restart_syscall = true;
        }
        state->deferred_signals |= 1ull << index;
        if (index <= 30) {
            state->deferred_standard_info[index] = *info;
        } else {
            // NOTE: This is wasteful. However, we can't use e.g. std::vector::push_back or similar functions that depend
            // on malloc here, as we are in a signal handler that can happen whenever. A smarter implementation might allocate
            // arenas for these and save quite a bit of memory per signal, but it is quite rare we get many queued signals anyway
            // and this is a significantly simpler and foolproof implementation
            // TODO: one day consider using arenas for this
            void* mem = mmap(nullptr, 4096, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
            ASSERT(mem != MAP_FAILED);
            SignalQueueNode* new_node = (SignalQueueNode*)mem;
            new_node->info = *info;
            new_node->next = nullptr;
            // Since the host signal handler is always registered without SA_NODEFER, there's no
            // possibility of a race here as the same signal can't happen here
            SignalQueueNode* node = state->deferred_realtime_info[index - 31];
            if (!node) {
                state->deferred_realtime_info[index - 31] = new_node;
            } else {
                size_t count = 0;
                while (node->next) {
                    node = node->next;
                    count++;
                }
                ASSERT_MSG(count <= 32, "Too many realtime signals of SIGRT%d", sig);
                node->next = new_node;
            }
        }
        // Make our safepoints fault
        u64 effective_deferred_signals = state->deferred_signals & ~state->signal_mask.__val[0];
        if (effective_deferred_signals != 0) {
            ASSERT(mprotect(state->deferred_fault_page, 4096, PROT_NONE) == 0);
        }
        state->effective_deferred_signals = effective_deferred_signals;
        SIGLOG("Deferring signal %s (%d) during RIP=%lx", sigdescr_np(sig), sig, state->GetRip());
    }
}

void Signals::initialize() {
    struct sigaction sa;
    sa.sa_sigaction = signal_handler;
    sa.sa_flags = SA_SIGINFO;
    sigemptyset(&sa.sa_mask);

    for (auto& handler : host_signals) {
        if ((u64)handler.func == (u64)&handle_wild_sigsegv && !g_config.capture_sigsegv && !g_config.abort_sigsegv) {
            continue;
        }

        if ((u64)handler.func == (u64)&handle_wild_sigabrt && !g_config.capture_sigabrt) {
            continue;
        }

        ASSERT(sigaction(handler.sig, &sa, nullptr) == 0);
    }
}

void Signals::registerSignalHandler(ThreadState* state, int sig, u64 handler, u64 mask, int flags, u64 restorer) {
    ASSERT(sig >= 1 && sig <= 64);

    state->signal_table->registerSignal(sig, handler, mask, flags, restorer);

    // If not SIG_DFL or SIG_IGN, register the host signal handler to capture the signal
    // If SIG_DFL or SIG_IGN, we want to passthrough the handler to the kernel
    // However, we can't do that for signals used by the emulator
    u64 bit = 1ull << (sig - 1);
    struct riscv_sigaction sa;
    if ((handler != (u64)SIG_DFL && handler != (u64)SIG_IGN) || (bit & ~hostSignalMask()->__val[0])) {
        sa.sigaction = signal_handler;
        sa.sa_flags = SA_SIGINFO;
        if (flags & SA_RESTART) {
            WARN("Installing signal handler for %s (%d) with SA_RESTART", sigdescr_np(sig), sig);
        }
    } else {
        sa.sigaction = (decltype(sa.sigaction))handler;
        sa.sa_flags = 0;
    }

    sa.restorer = nullptr;
    sa.sa_mask = 0;

    // The libc `sigaction` function fails when you try to modify handlers for SIG33 for example
    if (syscall(SYS_rt_sigaction, sig, &sa, nullptr, 8) != 0) {
        WARN("Failed when setting signal handler for signal: %d (%s)", sig, strsignal(sig));
    }
}

RegisteredSignal Signals::getSignalHandler(ThreadState* state, int sig) {
    ASSERT(sig >= 1 && sig <= 64);
    return *state->signal_table->getRegisteredSignal(sig);
}

int Signals::sigprocmask(ThreadState* state, int how, sigset_t* set, sigset_t* oldset) {
    sigset_t old_guest_set = state->signal_mask;
    int result = 0;
    if (set) {
        if (how == SIG_BLOCK) {
            sigorset(&state->signal_mask, &state->signal_mask, set);
        } else if (how == SIG_UNBLOCK) {
            sigset_t not_set;
            sigfillset(&not_set);
            u16 bit_size = sizeof(sigset_t) * 8;
            for (u16 i = 0; i < bit_size; i++) {
                if (sigismember(set, i)) {
                    sigdelset(&state->signal_mask, i);
                }
            }
            sigandset(&state->signal_mask, &state->signal_mask, &not_set);
        } else if (how == SIG_SETMASK) {
            memcpy(&state->signal_mask, set, sizeof(u64)); // copying the entire struct segfaults sometimes
        } else {
            return -EINVAL;
        }

        // Host mask needs to not block some signals used by the emulator
        sigset_t host_mask;
        sigandset(&host_mask, &state->signal_mask, Signals::hostSignalMask());

        sigdelset(&state->signal_mask, SIGKILL);
        sigdelset(&state->signal_mask, SIGSTOP);

        // Temporarily block all signals so we can safely check state->deferred_signals and set the page
        // without worrying of a signal happening
        u64 full = -1ull;
        ASSERT(syscall(SYS_rt_sigprocmask, SIG_BLOCK, &full, nullptr, sizeof(full)) == 0);

        // If any deferred signals become unmasked, make the page PROT_NONE
        // Otherwise, make it read/write
        state->effective_deferred_signals = state->deferred_signals & ~state->signal_mask.__val[0];
        if (state->effective_deferred_signals != 0) {
            ASSERT(mprotect(state->deferred_fault_page, 4096, PROT_NONE) == 0);
        } else {
            ASSERT(mprotect(state->deferred_fault_page, 4096, PROT_READ | PROT_WRITE) == 0);
        }

        // Finally, set our new host mask
        result = syscall(SYS_rt_sigprocmask, SIG_SETMASK, &host_mask.__val[0], nullptr, sizeof(u64));
        ASSERT(result == 0);
    }

    if (oldset) {
        memcpy(oldset, &old_guest_set, sizeof(u64));
    }

    return result;
}