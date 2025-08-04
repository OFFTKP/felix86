#include <array>
#include <sys/mman.h>
#include "felix86/common/state.hpp"
#include "felix86/common/types.hpp"
#include "felix86/common/utility.hpp"
#include "felix86/hle/signals.hpp"
#include "felix86/v2/recompiler.hpp"
#undef si_pid

struct RegisteredHostSignal {
    int sig;                                                                            // ie SIGILL etc
    int code;                                                                           // stuff like BUS_ADRALN, 0 if all
    bool (*func)(ThreadState* current_state, siginfo_t* info, ucontext_t* ctx, u64 pc); // the function to call
};

bool is_in_jit_code(ThreadState* state, u8* ptr) {
    CodeBuffer& buffer = state->recompiler->getAssembler().GetCodeBuffer();
    u8* start = state->recompiler->getStartOfCodeCache();
    u8* end = (u8*)buffer.GetCursorAddress();
    return ptr >= start && ptr < end;
}

struct x64_fpxreg {
    unsigned short int significand[4];
    unsigned short int exponent;
    unsigned short int reserved[3];
};

struct Xmm128 {
    u64 val[2];

    Xmm128(const XmmReg& other) {
        val[0] = other.data[0];
        val[1] = other.data[1];
    }

    operator XmmReg() const {
        XmmReg ret{};
        ret.data[0] = val[0];
        ret.data[1] = val[1];
        return ret;
    }
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
    Xmm128 xmm[16];
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
    alignas(16) struct x64_libc_fpstate fpregs_mem; // fpregs points here
    u64 ssp[4];                                     // unused
};
static_assert(sizeof(x64_ucontext) == 976);

// https://github.com/torvalds/linux/blob/master/arch/x86/include/asm/sigframe.h#L59
struct x64_rt_sigframe {
    char* pretcode; // return address
    x64_ucontext uc;
    siginfo_t info;
    // fp state follows here
};
static_assert(sizeof(siginfo_t) == 128);
static_assert(sizeof(x64_rt_sigframe) == 1120);

void reconstruct_state(ThreadState* state, const u64* gprs, const u64* fprs, const XmmReg* xmms) {
    if (state->state_is_correct) {
        // The ThreadState struct already contains the correct values, don't pull them out
        // This can happen if we are inside JIT code but already wrote the state when we hit the signal
    } else {
        for (int i = 0; i < 16; i++) {
            biscuit::GPR allocated_gpr = Recompiler::allocatedGPR((x86_ref_e)(X86_REF_RAX + i));
            state->gprs[i] = gprs[allocated_gpr.Index()];

            biscuit::Vec allocated_vec = Recompiler::allocatedVec((x86_ref_e)(X86_REF_XMM0 + i));
            state->xmm[i] = xmms[allocated_vec.Index()];
        }

        if (state->x87_state == x87State::MMX) {
            for (int i = 0; i < 8; i++) {
                biscuit::Vec allocated_vec = Recompiler::allocatedVec((x86_ref_e)(X86_REF_MM0 + i));
                state->fp[i] = xmms[allocated_vec.Index()].data[0];
            }
        } else {
            for (int i = 0; i < 8; i++) {
                biscuit::FPR allocated_fpr = Recompiler::allocatedFPR((x86_ref_e)(X86_REF_ST0 + i));
                state->fp[i] = fprs[allocated_fpr.Index()];
            }
        }

        state->cf = gprs[Recompiler::allocatedGPR(X86_REF_CF).Index()];
        state->zf = gprs[Recompiler::allocatedGPR(X86_REF_ZF).Index()];
        state->sf = gprs[Recompiler::allocatedGPR(X86_REF_SF).Index()];
        state->of = gprs[Recompiler::allocatedGPR(X86_REF_OF).Index()];
    }
}

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

// arch/x86/kernel/signal.c, get_sigframe function prepares the signal frame
x64_rt_sigframe* setupFrame(RegisteredSignal& signal, int sig, ThreadState* state, const u64* host_gprs, const u64* host_fprs,
                            const XmmReg* host_vecs, siginfo_t* guest_info) {
    bool use_altstack = signal.flags & SA_ONSTACK;
    if (!state->state_is_correct) {
        // We were in the middle of executing a basic block, the state up to that point needs to be written back to the state struct
        u64 pc = host_gprs[REG_PC];
        BlockMetadata* current_block = get_block_metadata(state, pc);
        if (current_block) {
            u64 actual_rip = get_actual_rip(*current_block, pc);
            reconstruct_state(state, host_gprs, host_fprs, host_vecs);
            state->SetRip(actual_rip);
        } else {
            // Assume RIP is correct. This can happen if we are in the address cache code for example, state_is_correct is 0
            // but we aren't inside a block. In this case the REG_GP holds the correct RIP
            u64 actual_rip = host_gprs[3]; // <- REG_GP
            state->SetRip(actual_rip);
        }
    } else {
        // State reconstruction isn't necessary, the state should be in some stable form
    }

    u64 rsp = use_altstack ? ((u64)state->alt_stack.ss_sp + state->alt_stack.ss_size) : state->GetGpr(X86_REF_RSP);
    if (rsp == 0) {
        WARN("RSP is null, use_altstack: %d... using original stack", use_altstack);
        rsp = state->GetGpr(X86_REF_RSP);
        ASSERT(rsp != 0);
    }

    rsp = rsp - 128; // red zone
    rsp = rsp - 8;
    rsp = rsp - (rsp % 8);
    rsp = rsp - sizeof(x64_rt_sigframe);
    x64_rt_sigframe* frame = (x64_rt_sigframe*)rsp;

    ASSERT(signal.restorer);
    frame->pretcode = (char*)signal.restorer;

    frame->uc.uc_mcontext.fpregs = &frame->uc.fpregs_mem;

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

    sigset_t* old_mask = &state->signal_mask;
    frame->uc.uc_sigmask = *old_mask;

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
    frame->uc.uc_mcontext.fpregs->xmm[0] = state->GetXmm(X86_REF_XMM0);
    frame->uc.uc_mcontext.fpregs->xmm[1] = state->GetXmm(X86_REF_XMM1);
    frame->uc.uc_mcontext.fpregs->xmm[2] = state->GetXmm(X86_REF_XMM2);
    frame->uc.uc_mcontext.fpregs->xmm[3] = state->GetXmm(X86_REF_XMM3);
    frame->uc.uc_mcontext.fpregs->xmm[4] = state->GetXmm(X86_REF_XMM4);
    frame->uc.uc_mcontext.fpregs->xmm[5] = state->GetXmm(X86_REF_XMM5);
    frame->uc.uc_mcontext.fpregs->xmm[6] = state->GetXmm(X86_REF_XMM6);
    frame->uc.uc_mcontext.fpregs->xmm[7] = state->GetXmm(X86_REF_XMM7);
    frame->uc.uc_mcontext.fpregs->xmm[8] = state->GetXmm(X86_REF_XMM8);
    frame->uc.uc_mcontext.fpregs->xmm[9] = state->GetXmm(X86_REF_XMM9);
    frame->uc.uc_mcontext.fpregs->xmm[10] = state->GetXmm(X86_REF_XMM10);
    frame->uc.uc_mcontext.fpregs->xmm[11] = state->GetXmm(X86_REF_XMM11);
    frame->uc.uc_mcontext.fpregs->xmm[12] = state->GetXmm(X86_REF_XMM12);
    frame->uc.uc_mcontext.fpregs->xmm[13] = state->GetXmm(X86_REF_XMM13);
    frame->uc.uc_mcontext.fpregs->xmm[14] = state->GetXmm(X86_REF_XMM14);
    frame->uc.uc_mcontext.fpregs->xmm[15] = state->GetXmm(X86_REF_XMM15);

    bool is_mmx = (x87State)state->x87_state == x87State::MMX;
    for (int i = 0; i < 8; i++) {
        // TODO: verify that these aren't saved relative to TOP when using x87
        x64_fpxreg* reg = &frame->uc.uc_mcontext.fpregs->_st[i];
        if (is_mmx) {
            memcpy(reg, &state->fp[i], sizeof(u64));
            reg->exponent = 0xFFFF; // according to Intel manual MMX instructions set these to 1's
        } else {
            Float80 f80 = f64_to_80(state->fp[i]);
            memcpy(reg, &f80, sizeof(Float80));
            static_assert(sizeof(Float80) == 10);
        }
    }

    state->SetGpr(X86_REF_RSP, (u64)frame);        // set the new stack pointer
    state->SetGpr(X86_REF_RDI, sig);               // set the signal
    state->SetGpr(X86_REF_RSI, (u64)&frame->info); // set the siginfo pointer
    state->SetGpr(X86_REF_RDX, (u64)&frame->uc);   // set the ucontext pointer
    state->SetGpr(X86_REF_RAX, 0);
    state->SetRip(signal.func);

    state->SetFlag(X86_REF_DF, 0);

    return frame;
}

void Signals::sigreturn(ThreadState* state) {
    u64 rsp = state->GetGpr(X86_REF_RSP);

    // When the signal handler returned, it popped the return address, which is the 8 bytes "pretcode" field in the sigframe
    // We need to adjust the rsp back before reading the entire struct.
    // Now technically a "malicious" sighandler could jump to memory instead of `ret` but that would probably lead to problems in the programs
    // execution anyway
    rsp -= 8;

    x64_rt_sigframe* frame = (x64_rt_sigframe*)rsp;
    rsp += sizeof(x64_rt_sigframe);

    SIGLOG("------- sigreturn TID: %d -------", gettid());

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

    if (!g_no_riscv_v_state) {
        state->SetXmm(X86_REF_XMM0, frame->uc.uc_mcontext.fpregs->xmm[0]);
        state->SetXmm(X86_REF_XMM1, frame->uc.uc_mcontext.fpregs->xmm[1]);
        state->SetXmm(X86_REF_XMM2, frame->uc.uc_mcontext.fpregs->xmm[2]);
        state->SetXmm(X86_REF_XMM3, frame->uc.uc_mcontext.fpregs->xmm[3]);
        state->SetXmm(X86_REF_XMM4, frame->uc.uc_mcontext.fpregs->xmm[4]);
        state->SetXmm(X86_REF_XMM5, frame->uc.uc_mcontext.fpregs->xmm[5]);
        state->SetXmm(X86_REF_XMM6, frame->uc.uc_mcontext.fpregs->xmm[6]);
        state->SetXmm(X86_REF_XMM7, frame->uc.uc_mcontext.fpregs->xmm[7]);
        state->SetXmm(X86_REF_XMM8, frame->uc.uc_mcontext.fpregs->xmm[8]);
        state->SetXmm(X86_REF_XMM9, frame->uc.uc_mcontext.fpregs->xmm[9]);
        state->SetXmm(X86_REF_XMM10, frame->uc.uc_mcontext.fpregs->xmm[10]);
        state->SetXmm(X86_REF_XMM11, frame->uc.uc_mcontext.fpregs->xmm[11]);
        state->SetXmm(X86_REF_XMM12, frame->uc.uc_mcontext.fpregs->xmm[12]);
        state->SetXmm(X86_REF_XMM13, frame->uc.uc_mcontext.fpregs->xmm[13]);
        state->SetXmm(X86_REF_XMM14, frame->uc.uc_mcontext.fpregs->xmm[14]);
        state->SetXmm(X86_REF_XMM15, frame->uc.uc_mcontext.fpregs->xmm[15]);

        for (int i = 0; i < 8; i++) {
            x64_fpxreg* reg = &frame->uc.uc_mcontext.fpregs->_st[i];
            if (reg->exponent == 0xFFFF) {
                memcpy(&state->fp[i], reg->significand, sizeof(u64));
            } else {
                double f64 = f80_to_64((Float80*)reg);
                memcpy(&state->fp[i], &f64, sizeof(u64));
            }
        }
    } else {
        // Don't set the state, because the frame isn't going to have correct
        // values. Most things shouldn't modify the values of registers in signal handlers.
        // But if they do, and you need support for that, update your kernel.
    }

    // Restore signal mask to what it was supposed to be outside of signal handler
    sigset_t host_mask;
    sigandset(&host_mask, &state->signal_mask, Signals::hostSignalMask());
    pthread_sigmask(SIG_SETMASK, &host_mask, nullptr);
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

u64* get_regs(void* ctx) {
#ifdef __riscv
    return (u64*)((ucontext_t*)ctx)->uc_mcontext.__gregs;
#else
    UNREACHABLE();
    return nullptr;
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

bool handle_smc(ThreadState* current_state, siginfo_t* info, ucontext_t* context, u64 pc) {
    if (!is_in_jit_code(current_state, (u8*)pc)) {
        WARN("We hit a SIGSEGV ACCERR but PC is not in JIT code...");
        return false;
    }

    u64 write_address = (u64)info->si_addr & ~0xFFFull;
    Recompiler::invalidateRangeGlobal(write_address, write_address + 0x1000);
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
    // In many cases it's annoying to attach a debugger at the start of a program, because it may be spawning many processes which
    // can trip up gdb and it won't know which fork to follow. The "don't detach forks" mode is also kind of jittery as far as I can see.
    // The capture_sigsegv mode can help us sleep the process for a while to attach gdb and get a proper backtrace.
    bool in_jit_code = is_in_jit_code(current_state, (u8*)pc);
    bool capture_it = g_config.capture_sigsegv;
    if (capture_it) {
        int pid = gettid();
        PLAIN("I have been hit by a wild SIGSEGV%s! My TID is %d, you have 40 seconds to attach gdb using `gdb -p %d` to find out why! If you think "
              "this SIGSEGV was intended, disabled this mode by unsetting the `capture_sigsegv` option.",
              !in_jit_code ? ANSI_BOLD " in emulator code" ANSI_COLOR_RESET : "", pid, pid);

        LOG("Current RIP:");
        if (in_jit_code) {
            BlockMetadata* current_block = get_block_metadata(current_state, pc);
            if (current_block) {
                u64 actual_rip = get_actual_rip(*current_block, pc);
                print_address(actual_rip);
            } else {
                WARN("Failed to get actual RIP"); // <- TODO: get it from REG_GP
            }
        } else {
            print_address(current_state->rip);
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

bool handle_wild_sigabrt(ThreadState* current_state, siginfo_t* info, ucontext_t* context, u64 pc) {
    // Similar to SIGSEGV sleeping, SIGABRT can be nice to capture because it's called when guest errors like
    // stack smashing happen.
    bool in_jit_code = is_in_jit_code(current_state, (u8*)pc);
    if (g_config.capture_sigabrt) {
        int pid = gettid();
        PLAIN("I have been hit by a wild SIGABRT%s! My TID is %d, you have 40 seconds to attach gdb using `gdb -p %d` to find out why! If you "
              "think this SIGABRT was intended, disabled this mode by unsetting the `capture_sigabrt` option.",
              !in_jit_code ? ANSI_BOLD " in emulator code" ANSI_COLOR_RESET : "", pid, pid);

        LOG("Current RIP:");
        if (in_jit_code) {
            BlockMetadata* current_block = get_block_metadata(current_state, pc);
            if (current_block) {
                u64 actual_rip = get_actual_rip(*current_block, pc);
                print_address(actual_rip);
            } else {
                WARN("Failed to get actual RIP"); // <- TODO: get it from REG_GP
            }
        } else {
            print_address(current_state->rip);
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

constexpr std::array<RegisteredHostSignal, 4> host_signals = {{
    {SIGSEGV, SEGV_ACCERR, handle_smc},
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

// Will just start executing a guest signal inside the host signal handler immediately.
bool dispatch_guest(int sig, siginfo_t* info, void* ctx) {
    ThreadState* state = ThreadState::Get();
    u64 pc = get_pc(ctx);
    bool in_jit_code = is_in_jit_code(state, (u8*)pc);
    RegisteredSignal* handler = state->signal_table->getRegisteredSignal(sig);
    if (!handler) {
        return false;
    }

    if ((void*)handler->func == SIG_DFL) {
        switch (sig) {
        case SIGHUP:
        case SIGINT:
        case SIGQUIT:
        case SIGILL:
        case SIGABRT:
        case SIGBUS:
        case SIGFPE:
        case SIGUSR1:
        case SIGSEGV:
        case SIGUSR2:
        case SIGPIPE:
        case SIGALRM:
        case SIGTERM:
        case SIGSTKFLT:
        case SIGVTALRM:
        case SIGPROF:
        case SIGIO:
        case SIGPWR:
        case SIGSYS: {
            ERROR("Hit signal %s (%d) but signal handler is SIG_DFL, and the default behavior is terminate. Probably a bug.", strsignal(sig), sig);
        }
        }

        if (g_config.paranoid) {
            WARN("Signal %s is going through default handler", strsignal(sig));
        }

        return true;
    }

    if (handler->func == (u64)SIG_IGN) {
        ERROR("Signal %d hit but signal handler is SIGIGN", sig);
        return true;
    }

    ASSERT(sig > 0);

    SIGLOG("------- Guest signal %s (%d) %s TID: %d -------", sigdescr_np(sig), sig, in_jit_code ? "in jit code" : "not in jit code", gettid());

    ASSERT_MSG(!g_mode32, "Got signal %d", sig);

    XmmReg* xmms;

    u64* gprs = get_regs(ctx);
    u64* fprs = get_fprs(ctx);
    std::array<XmmReg, 32> xmm_regs;

    riscv_v_state* v_state = get_riscv_vector_state(ctx);
    if (v_state) {
        u8* datap = (u8*)v_state->datap;
        for (int i = 0; i < 32; i++) {
            xmm_regs[i] = *(XmmReg*)datap;
            datap += v_state->vlenb;
        }
    } else {
        // In the chance that this is an old kernel and we couldn't get the vector state in the signal handler,
        // the xmm values in the signal handler are going to be wrong. Most thing shouldn't care about this
        // so we aren't going to do anything here
        g_no_riscv_v_state = true;
        WARN_ONCE("You have an old kernel with no vector state in signal handlers, this may cause problems in some programs");
    }

    xmms = xmm_regs.data();

    bool use_altstack = handler->flags & SA_ONSTACK;
    if (use_altstack && state->alt_stack.ss_sp == 0) {
        // If there's no altstack set up, use the default stack instead
        use_altstack = false;
    }

    siginfo_t guest_info = *info;

    // Prepares everything necessary to run the signal handler when we return from the host signal handler.
    // The stack is switched if necessary and filled with the frame that the signal handler expects.
    x64_rt_sigframe* frame = setupFrame(*handler, sig, state, gprs, fprs, xmms, &guest_info);

    // Block the signals specified in the sa_mask until the signal handler returns
    sigset_t new_mask;
    sigset_t mask_during_signal;
    mask_during_signal = *(sigset_t*)&handler->mask;

    // Combine with the current signal mask
    sigorset(&new_mask, &mask_during_signal, &state->signal_mask);

    if (handler->flags & SA_NODEFER) {
        sigdelset(&new_mask, sig);
    } else {
        sigaddset(&new_mask, sig);
    }

    sigandset(&new_mask, &new_mask, Signals::hostSignalMask());

    pthread_sigmask(SIG_SETMASK, &new_mask, nullptr);

    if (handler->flags & SA_RESETHAND) {
        handler->func = (u64)SIG_DFL;
    }

    u64 old_rip = frame->uc.uc_mcontext.gregs[REG_RIP];
#if 0
    print_address(old_rip);
    print_address(handler->func);
#endif

    // Eventually, this should return right after this call and have the correct state.
    // When entering the dispatcher, the host state is saved in the host stack
    // sigreturn will call exitDispatcher, which will load the old frame and return back here after this call.
    // This way we can support signals inside signal handlers too.
    // The only problem would be longjmps out of signal handlers. This is evil but possible that a game or something does it
    // In that case the frames would eventually overflow and at least we'd gave an appropriate message.
    state->recompiler->enterDispatcher(state);

    u64 new_rip = state->GetRip();
    if (in_jit_code) {
        // We are returning to JIT code. We need to set the host registers from the ucontext accordingly,
        // as they may have been changed in the signal handler.
        // TODO: we also need to set xmms, sts, flags too...
        u64* regs = get_regs(ctx);
        for (int i = 0; i < 16; i++) {
            x86_ref_e ref = (x86_ref_e)(X86_REF_RAX + i);
            u64 new_value = state->GetGpr(ref);
            regs[Recompiler::allocatedGPR(ref).Index()] = new_value;
        }

        // If the signal handler changed our RIP we need to go back to the dispatcher to compile a new block
        // Otherwise we will continue where we left off when the signal happened
        if (new_rip != old_rip) {
#ifdef __riscv
            regs[3] = new_rip;
            static_assert(Recompiler::allocatedGPR(X86_REF_RIP) == biscuit::gp);
            static_assert(biscuit::gp.Index() == 3);
            WARN("Signal handler changed RIP from %lx to %lx", old_rip, new_rip);
#endif
            set_pc(ctx, state->recompiler->getCompileNext());
        }
    }

    return true;
}

// Main signal handler function, all signals come here
void signal_handler(int sig, siginfo_t* info, void* ctx) {
    // First, check if this is a host signal
    bool handled;

    handled = dispatch_host(sig, info, ctx);
    if (handled) {
        // Ok it was a host signal
        return;
    }

    handled = dispatch_guest(sig, info, ctx);
    if (handled) {
        VERBOSE("Guest signal %d was handled successfully", sig);
        return;
    }

    // Uncaught signal even though we have a signal handler?
    ERROR("Couldn't find host or guest signal handler for %s", strsignal(sig));
}

void Signals::initialize() {
    struct sigaction sa;
    sa.sa_sigaction = signal_handler;
    sa.sa_flags = SA_SIGINFO;
    sigemptyset(&sa.sa_mask);

    for (auto& handler : host_signals) {
        ASSERT(sigaction(handler.sig, &sa, nullptr) == 0);
    }
}

void Signals::registerSignalHandler(ThreadState* state, int sig, u64 handler, u64 mask, int flags, u64 restorer) {
    ASSERT(sig >= 1 && sig <= 64);

    // Hopefully externally synchronized, no need for locks :cluegi:
    state->signal_table->registerSignal(sig, handler, mask, flags, restorer);

    // Start capturing at the first register of a signal handler and don't stop capturing even if it is disabled
    if (handler != 0) {
        struct riscv_sigaction sa;
        sa.sigaction = signal_handler;
        sa.sa_flags = SA_SIGINFO;
        sa.restorer = nullptr;
        sa.sa_mask = 0;

        // The libc `sigaction` function fails when you try to modify handlers for SIG33 for example
        if (syscall(SYS_rt_sigaction, sig, &sa, nullptr, 8) != 0) {
            WARN("Failed when setting signal handler for signal: %d (%s)", sig, strsignal(sig));
        }
    }
}

RegisteredSignal Signals::getSignalHandler(ThreadState* state, int sig) {
    ASSERT(sig >= 1 && sig <= 64);
    return *state->signal_table->getRegisteredSignal(sig);
}

int Signals::sigsuspend(ThreadState* state, sigset_t* mask) {
    sigset_t old_mask = state->signal_mask;
    memcpy(&state->signal_mask, mask, sizeof(u64));
    int result = ::sigsuspend(mask);
    memcpy(&state->signal_mask, &old_mask, sizeof(u64));
    if (result == -1) {
        return -errno;
    } else {
        return result;
    }
}

SignalGuard::SignalGuard() {
    static sigset_t full_mask = []() {
        sigset_t t;
        sigfillset(&t);
        sigandset(&t, &t, Signals::hostSignalMask());
        return t;
    }();

    pthread_sigmask(SIG_SETMASK, &full_mask, &old_mask);
}

SignalGuard::~SignalGuard() {
    if (!killed) {
        pthread_sigmask(SIG_SETMASK, &old_mask, nullptr);
    }
}

int Signals::sigprocmask(ThreadState* state, int how, sigset_t* set, sigset_t* oldset) {
    sigset_t old_host_set = state->signal_mask;
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

        sigset_t host_mask;
        sigandset(&host_mask, &state->signal_mask, Signals::hostSignalMask());
        result = pthread_sigmask(SIG_SETMASK, &host_mask, nullptr);
        ASSERT(result == 0);
    }

    if (oldset) {
        memcpy(oldset, &old_host_set, sizeof(u64));
    }

    return result;
}