#include <array>
#include "felix86/emulator.hpp"
#include "felix86/hle/filesystem.hpp"
#include "felix86/hle/signals.hpp"

bool is_in_jit_code(uintptr_t ptr) {
    uintptr_t start = g_emulator->GetAssembler().GetCodeBuffer().GetOffsetAddress(0);
    uintptr_t end = g_emulator->GetAssembler().GetCodeBuffer().GetCursorAddress();
    return ptr >= start && ptr < end;
}

struct x64_fpxreg {
    unsigned short int significand[4];
    unsigned short int exponent;
    unsigned short int reserved[3];
};

struct x64_libc_fpstate {
    /* 64-bit fxsave format. Also the legacy part of xsave, which is the one we use as we don't support AVX  */
    u16 cwd;
    u16 swd;
    u16 ftw;
    u16 fop;
    u64 rip;
    u64 rdp;
    u32 mxcsr;
    u32 mxcr_mask;
    x64_fpxreg _st[8];
    XmmReg xmm[16];
    u32 reserved[24]; // Bytes 464...511 are for the implementation to do whatever it wants.
                      // Linux kernel uses them in _fpx_sw_bytes for magic numbers and xsave size and other stuff
};
static_assert(sizeof(x64_libc_fpstate) == 512);

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
    u64 gregs[23];            // using the indices in the enum above
    x64_libc_fpstate* fpregs; // it's a pointer, points to after the end of x64_rt_sigframe in stack
    u64 reserved[8];
};
static_assert(sizeof(x64_mcontext) == 256);

struct x64_ucontext {
    u64 uc_flags;
    x64_ucontext* uc_link;
    stack_t uc_stack;
    x64_mcontext uc_mcontext;
    sigset_t uc_sigmask;
    struct x64_libc_fpstate __fpregs_mem;
    u64 ssp[4]; // unused
};
static_assert(sizeof(x64_ucontext) == 968);

// https://github.com/torvalds/linux/blob/master/arch/x86/include/asm/sigframe.h#L59
struct x64_rt_sigframe {
    char* pretcode; // return address
    x64_ucontext uc;
    siginfo_t info;
    // fp state follows here
};
static_assert(sizeof(siginfo_t) == 128);
static_assert(sizeof(x64_rt_sigframe) == 1104);

// arch/x86/kernel/signal.c, get_sigframe function prepares the signal frame
void setup_frame(u64& rsp, ThreadState* state, sigset_t mask, bool use_altstack, bool fetch_state) {
    u64 initial_rsp = rsp;
    rsp -= 128; // red zone

    rsp -= sizeof(x64_libc_fpstate);
    static_assert(sizeof(x64_libc_fpstate) == 512);
    x64_libc_fpstate* fpstate = (x64_libc_fpstate*)rsp;

    rsp -= sizeof(x64_rt_sigframe);
    x64_rt_sigframe* frame = (x64_rt_sigframe*)rsp;

    // TODO: setup frame->pretcode

    // TODO: setup uc_flags
    frame->uc.uc_link = 0;

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

    frame->uc.uc_sigmask = mask;

    if (fetch_state) {
    }
}

void Signals::sigreturn(ThreadState* state) {
    auto lock = g_emulator->Lock();

    u64 rsp = state->GetGpr(X86_REF_RSP);

    // When the signal handler returned, it popped the return address, which is the 8 bytes "pretcode" field in the sigframe
    // We need to adjust the rsp back before reading the entire struct.
    rsp += 8;

    x64_rt_sigframe* frame = (x64_rt_sigframe*)rsp;
    rsp += sizeof(x64_rt_sigframe);

    // The registers need to be restored to what they were before the signal handler was called.
    state->SetGpr(X86_REF_RAX, frame->uc.uc_mcontext.gregs[REG_RAX]);
}

// #if defined(__x86_64__)
// void signal_handler(int sig, siginfo_t* info, void* ctx) {
//     UNREACHABLE();
// }
// #elif defined(__riscv)
void signal_handler(int sig, siginfo_t* info, void* ctx) {
    ucontext_t* context = (ucontext_t*)ctx;
    uintptr_t pc = context->uc_mcontext.__gregs[REG_PC];

    Recompiler& recompiler = g_emulator->GetRecompiler();

    switch (sig) {
    case SIGBUS: {
        switch (info->si_code) {
        case BUS_ADRALN: {
            auto lock = g_emulator->Lock();
            ASSERT(is_in_jit_code(pc));
            // Go back one instruction, we are going to overwrite it with vsetivli.
            // It's guaranteed to be either a vsetivli or a nop.
            context->uc_mcontext.__gregs[REG_PC] = pc - 4;

            Assembler& as = recompiler.getAssembler();
            VectorMemoryAccess vma = recompiler.getVectorMemoryAccess(pc - 4);

            ptrdiff_t cursor = as.GetCodeBuffer().GetCursorOffset();
            as.RewindBuffer(pc - as.GetCodeBuffer().GetOffsetAddress(0) - 4); // go to vsetivli
            switch (vma.sew) {
            case SEW::E64: {
                as.VSETIVLI(x0, vma.len * 8, SEW::E8);
                if (vma.load) {
                    as.VLE8(vma.dest, vma.address);
                } else {
                    as.VSE8(vma.dest, vma.address);
                }
                as.VSETIVLI(x0, vma.len, vma.sew);
                break;
            }
            case SEW::E32: {
                as.VSETIVLI(x0, vma.len * 4, SEW::E8);
                if (vma.load) {
                    as.VLE8(vma.dest, vma.address);
                } else {
                    as.VSE8(vma.dest, vma.address);
                }
                as.VSETIVLI(x0, vma.len, vma.sew);
                break;
            }
            default: {
                UNREACHABLE();
                break;
            }
            }
            as.AdvanceBuffer(cursor);
            flush_icache();
            break;
        }
        default: {
            ERROR("Unhandled SIGBUS code: %d", info->si_code);
            break;
        }
        }
        break;
    }
    case SIGILL: {
        auto lock = g_emulator->Lock();
        bool found = false;
        if (is_in_jit_code(pc)) {
            // Search to see if it is our breakpoint
            // Note the we don't use EBREAK as gdb refuses to continue when it hits that if it doesn't have a breakpoint,
            // and also refuses to call our signal handler.
            // So we use illegal instructions to emulate breakpoints.
            for (auto& bp : g_breakpoints) {
                for (u64 location : bp.second) {
                    if (location == pc) {
                        // Skip the breakpoint and continue
                        printf("Guest breakpoint %016lx hit at %016lx\n", bp.first, pc);
                        context->uc_mcontext.__gregs[REG_PC] = pc + 4;
                        found = true;
                        break;
                    }
                }

                if (found) {
                    break;
                }
            }
        }

        if (!found) {
            ERROR("Unhandled SIGILL (%d) at PC: %016lx", info->si_code, pc);
        }
        break;
    }
    default: {
        // ThreadState* state = g_emulator->GetThreadState();
        // if ((*state->signal_handlers)[sig - 1].handler) {
        //     // There's a guest signal handler for this signal.
        //     // If the signal happened inside the JIT code, we need to do some sort of state reconstruction at the end
        //     // of the guest signal handler.
        //     // Otherwise I think we're good to recompile and run it :cluegi:
        //     ERROR("implme");
        // } else {
        //     ERROR("Unhandled signal %d", sig);
        // }
        ERROR("Unhandled signal %d", sig);

        auto lock = g_emulator->Lock();

        // First we need to find the current ThreadState object
        ThreadState* current_state = g_emulator->GetThreadState();

        SignalHandlerTable& handlers = *current_state->signal_handlers;

        if (handlers[sig - 1].handler) {
            uintptr_t return_pc = 0xDEADBEEF;
            if (is_in_jit_code(pc)) {

            } else {
            }
        } else {
            ERROR("Unhandled signal %d, no signal handler found", sig);
        }
        break;
    }
    }
}
// #endif

void Signals::initialize() {
    struct sigaction sa;
    sa.sa_sigaction = signal_handler;
    sa.sa_flags = SA_SIGINFO;
    sigemptyset(&sa.sa_mask);

    if (!g_testing) { // we don't need to install guest handlers for tests
        for (int i = 1; i <= 64; i++) {
            sigaction(i, &sa, nullptr);
        }
    } else {
        // Only install the signal handler for SIGILL and SIGBUS
        sigaction(SIGILL, &sa, nullptr);
        sigaction(SIGBUS, &sa, nullptr);
    }
}

void Signals::registerSignalHandler(ThreadState* state, int sig, void* handler, sigset_t mask, int flags) {
    ASSERT(sig >= 1 && sig <= 64);
    (*state->signal_handlers)[sig - 1] = {handler, mask, flags};
}

RegisteredSignal Signals::getSignalHandler(ThreadState* state, int sig) {
    ASSERT(sig >= 1 && sig <= 64);
    return (*state->signal_handlers)[sig - 1];
}