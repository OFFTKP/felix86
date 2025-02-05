#include <array>
#include "felix86/emulator.hpp"
#include "felix86/hle/filesystem.hpp"
#include "felix86/hle/signals.hpp"

using VectorState = std::array<XmmReg, 32>;

bool is_in_jit_code(ThreadState* state, uintptr_t ptr) {
    CodeBuffer& buffer = state->recompiler->getAssembler().GetCodeBuffer();
    uintptr_t start = buffer.GetOffsetAddress(0);
    uintptr_t end = buffer.GetCursorAddress();
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

void reconstruct_state(ThreadState* state, BlockMetadata* current_block, u64 rip, u64* host_gprs, std::optional<VectorState>& host_vecs) {
    // We were in the middle of a basic block when the signal hit
    // We need to fixup our state and get the correct values that may not have been written back to the state struct
    // First, for the GPRS
    for (int i = 0; i < 16; i++) {
        bool needs_copy = false;
        for (auto& access : current_block->register_accesses[i]) {
            if (access.address >= rip)
                break;
            needs_copy = access.valid;
        }

        // At the time of signal, the register was loaded from memory and potentially modified, so we need to copy it to the state struct
        // effectively doing a writeback to state.
        if (needs_copy) {
            int allocated_host_gpr = Recompiler::allocatedGPR((x86_ref_e)(X86_REF_RAX + i)).Index();
            state->SetGpr((x86_ref_e)(X86_REF_RAX + i), host_gprs[allocated_host_gpr]);
        }
    }

    // Do the same for XMMs
    if (host_vecs) {
        for (int i = 0; i < 16; i++) {
            bool needs_copy = false;
            // TODO: again get rid of magic number 16 + 5
            for (auto& access : current_block->register_accesses[16 + 5 + i]) {
                if (access.address >= rip)
                    break;
                needs_copy = access.valid;
            }

            if (needs_copy) {
                state->SetXmmReg((x86_ref_e)(X86_REF_XMM0 + i), host_vecs->at(i));
            }
        }
    } else {
        static bool warned = false;
        if (!warned) {
            WARN("Could not retrieve host vector registers, probably old Linux kernel version, may cause issues with signal handling");
            warned = true;
        }
    }

    // Check for CF
    {
        bool needs_copy = false;
        // TODO: get rid of magic number 16, 17, etc
        for (auto& access : current_block->register_accesses[16]) {
            if (access.address >= rip)
                break;
            needs_copy = access.valid;
        }

        if (needs_copy) {
            int allocated_host_gpr = Recompiler::allocatedGPR(X86_REF_CF).Index();
            state->SetFlag(X86_REF_CF, host_gprs[allocated_host_gpr] & 1);
        }
    }

    // Check for AF
    {
        bool needs_copy = false;
        for (auto& access : current_block->register_accesses[17]) {
            if (access.address >= rip)
                break;
            needs_copy = access.valid;
        }

        if (needs_copy) {
            int allocated_host_gpr = Recompiler::allocatedGPR(X86_REF_AF).Index();
            state->SetFlag(X86_REF_AF, host_gprs[allocated_host_gpr] & 1);
        }
    }

    // Check for ZF
    {
        bool needs_copy = false;
        for (auto& access : current_block->register_accesses[18]) {
            if (access.address >= rip)
                break;
            needs_copy = access.valid;
        }

        if (needs_copy) {
            int allocated_host_gpr = Recompiler::allocatedGPR(X86_REF_ZF).Index();
            state->SetFlag(X86_REF_ZF, host_gprs[allocated_host_gpr] & 1);
        }
    }

    // Check for SF
    {
        bool needs_copy = false;
        for (auto& access : current_block->register_accesses[19]) {
            if (access.address >= rip)
                break;
            needs_copy = access.valid;
        }

        if (needs_copy) {
            int allocated_host_gpr = Recompiler::allocatedGPR(X86_REF_SF).Index();
            state->SetFlag(X86_REF_SF, host_gprs[allocated_host_gpr] & 1);
        }
    }

    // Check for OF
    {
        bool needs_copy = false;
        for (auto& access : current_block->register_accesses[20]) {
            if (access.address >= rip)
                break;
            needs_copy = access.valid;
        }

        if (needs_copy) {
            int allocated_host_gpr = Recompiler::allocatedGPR(X86_REF_OF).Index();
            state->SetFlag(X86_REF_OF, host_gprs[allocated_host_gpr] & 1);
        }
    }

    // Finally also set the RIP
    state->SetRip(rip);
}

// arch/x86/kernel/signal.c, get_sigframe function prepares the signal frame
void setup(BlockMetadata* current_block, u64 rip, ThreadState* state, sigset_t new_mask, u64* host_gprs, std::optional<VectorState>& host_vecs,
           bool use_altstack, bool in_jit_code) {
    u64 rsp = use_altstack ? (u64)state->alt_stack.ss_sp : state->GetGpr(X86_REF_RSP);
    rsp -= 128; // red zone

    rsp -= sizeof(x64_rt_sigframe);
    x64_rt_sigframe* frame = (x64_rt_sigframe*)rsp;

    frame->pretcode = (char*)Signals::magicSigreturnAddress();

    frame->uc.uc_mcontext.fpregs = &frame->uc.fpregs_mem;

    frame->uc.uc_flags = 0;
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

    sigset_t* old_mask = &state->signal_mask;
    frame->uc.uc_sigmask = *old_mask;

    if (in_jit_code) {
        // We were in the middle of executing a basic block, the state up to that point needs to be written back to the state struct
        reconstruct_state(state, current_block, rip, host_gprs, host_vecs);
    } else {
        // State reconstruction isn't necessary, the state should be in some stable form
        // It's rare that a signal doesn't happen in jit code as we block signals during compilation, but it's possible
    }

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
    frame->uc.uc_mcontext.fpregs->xmm[0] = state->GetXmmReg(X86_REF_XMM0);
    frame->uc.uc_mcontext.fpregs->xmm[1] = state->GetXmmReg(X86_REF_XMM1);
    frame->uc.uc_mcontext.fpregs->xmm[2] = state->GetXmmReg(X86_REF_XMM2);
    frame->uc.uc_mcontext.fpregs->xmm[3] = state->GetXmmReg(X86_REF_XMM3);
    frame->uc.uc_mcontext.fpregs->xmm[4] = state->GetXmmReg(X86_REF_XMM4);
    frame->uc.uc_mcontext.fpregs->xmm[5] = state->GetXmmReg(X86_REF_XMM5);
    frame->uc.uc_mcontext.fpregs->xmm[6] = state->GetXmmReg(X86_REF_XMM6);
    frame->uc.uc_mcontext.fpregs->xmm[7] = state->GetXmmReg(X86_REF_XMM7);
    frame->uc.uc_mcontext.fpregs->xmm[8] = state->GetXmmReg(X86_REF_XMM8);
    frame->uc.uc_mcontext.fpregs->xmm[9] = state->GetXmmReg(X86_REF_XMM9);
    frame->uc.uc_mcontext.fpregs->xmm[10] = state->GetXmmReg(X86_REF_XMM10);
    frame->uc.uc_mcontext.fpregs->xmm[11] = state->GetXmmReg(X86_REF_XMM11);
    frame->uc.uc_mcontext.fpregs->xmm[12] = state->GetXmmReg(X86_REF_XMM12);
    frame->uc.uc_mcontext.fpregs->xmm[13] = state->GetXmmReg(X86_REF_XMM13);
    frame->uc.uc_mcontext.fpregs->xmm[14] = state->GetXmmReg(X86_REF_XMM14);
    frame->uc.uc_mcontext.fpregs->xmm[15] = state->GetXmmReg(X86_REF_XMM15);

    state->SetGpr(X86_REF_RSP, rsp);               // set the new stack pointer
    state->SetGpr(X86_REF_RSI, (u64)&frame->info); // set the siginfo pointer
    state->SetGpr(X86_REF_RDX, (u64)&frame->uc);   // set the ucontext pointer
}

BlockMetadata* get_block_metadata(ThreadState* state, u64 host_pc) {
    auto& map = state->recompiler->getBlockMap();

    for (auto& span : map) {
        if (host_pc >= (u64)span.second.address && host_pc < (u64)span.second.address_end) {
            return &span.second;
        }
    }

    UNREACHABLE();
    return nullptr;
}

u64 get_actual_rip(BlockMetadata& metadata, u64 host_pc) {
    u64 ret_value = 0;
    for (auto& span : metadata.instruction_spans) {
        if (host_pc >= span.second) {
            ret_value = span.first;
        } else { // if it's smaller it means that instruction isn't reached yet, return previous value
            ASSERT(ret_value != 0);
            return ret_value;
        }
    }

    ASSERT(ret_value != 0);
    return ret_value;
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
    state->SetFlag(X86_REF_CF, cf);
    state->SetFlag(X86_REF_PF, pf);
    state->SetFlag(X86_REF_AF, af);
    state->SetFlag(X86_REF_ZF, zf);
    state->SetFlag(X86_REF_SF, sf);
    state->SetFlag(X86_REF_OF, of);

    state->SetXmmReg(X86_REF_XMM0, frame->uc.uc_mcontext.fpregs->xmm[0]);
    state->SetXmmReg(X86_REF_XMM1, frame->uc.uc_mcontext.fpregs->xmm[1]);
    state->SetXmmReg(X86_REF_XMM2, frame->uc.uc_mcontext.fpregs->xmm[2]);
    state->SetXmmReg(X86_REF_XMM3, frame->uc.uc_mcontext.fpregs->xmm[3]);
    state->SetXmmReg(X86_REF_XMM4, frame->uc.uc_mcontext.fpregs->xmm[4]);
    state->SetXmmReg(X86_REF_XMM5, frame->uc.uc_mcontext.fpregs->xmm[5]);
    state->SetXmmReg(X86_REF_XMM6, frame->uc.uc_mcontext.fpregs->xmm[6]);
    state->SetXmmReg(X86_REF_XMM7, frame->uc.uc_mcontext.fpregs->xmm[7]);
    state->SetXmmReg(X86_REF_XMM8, frame->uc.uc_mcontext.fpregs->xmm[8]);
    state->SetXmmReg(X86_REF_XMM9, frame->uc.uc_mcontext.fpregs->xmm[9]);
    state->SetXmmReg(X86_REF_XMM10, frame->uc.uc_mcontext.fpregs->xmm[10]);
    state->SetXmmReg(X86_REF_XMM11, frame->uc.uc_mcontext.fpregs->xmm[11]);
    state->SetXmmReg(X86_REF_XMM12, frame->uc.uc_mcontext.fpregs->xmm[12]);
    state->SetXmmReg(X86_REF_XMM13, frame->uc.uc_mcontext.fpregs->xmm[13]);
    state->SetXmmReg(X86_REF_XMM14, frame->uc.uc_mcontext.fpregs->xmm[14]);
    state->SetXmmReg(X86_REF_XMM15, frame->uc.uc_mcontext.fpregs->xmm[15]);

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

#if defined(__x86_64__)
void signal_handler(int sig, siginfo_t* info, void* ctx) {
    UNREACHABLE();
}
#elif defined(__riscv)
riscv_v_state* get_riscv_vector_state(void* ctx) {
    ucontext_t* context = (ucontext_t*)ctx;
    mcontext_t* mcontext = &context->uc_mcontext;
    unsigned int* reserved = mcontext->__fpregs.__q.__glibc_reserved;

    // Normally the glibc should have better support for this, but this will be fine for now
    if (reserved[1] != 0x53465457) { // RISC-V V extension magic number that indicates the presence of vector state
        return nullptr;
    }

    void* after_fpregs = reserved + 3;
    riscv_v_state* v_state = (riscv_v_state*)after_fpregs;
    return v_state;
}

std::optional<VectorState> get_vector_state(void* ctx) {
    riscv_v_state* v_state = get_riscv_vector_state(ctx);
    if (!v_state) {
        return std::nullopt;
    }
    u8* datap = (u8*)v_state->datap;

    std::array<XmmReg, 32> xmm_regs;
    for (int i = 0; i < 32; i++) {
        xmm_regs[i] = *(XmmReg*)datap;
        datap += v_state->vlenb;
    }

    return xmm_regs;
}

void signal_handler(int sig, siginfo_t* info, void* ctx) {
    ucontext_t* context = (ucontext_t*)ctx;
    uintptr_t pc = context->uc_mcontext.__gregs[REG_PC];

    ThreadState* current_state = ThreadState::Get();
    ASSERT(current_state);
    Recompiler& recompiler = *current_state->recompiler;

    switch (sig) {
    case SIGBUS: {
        switch (info->si_code) {
        case BUS_ADRALN: {
            ASSERT(is_in_jit_code(current_state, pc));
            // TODO: assert it's a vector load/store
            u32 instruction = *(u32*)pc; // Read the faulting instruction

            // Go back one instruction, we are going to overwrite it with vsetivli.
            // It's guaranteed to be either a vsetivli or a nop.
            context->uc_mcontext.__gregs[REG_PC] = pc - 4;

            Assembler& as = recompiler.getAssembler();
            riscv_v_state* vstate = get_riscv_vector_state(ctx);

            SEW sew = (SEW)(vstate->vtype >> 3);
            u64 len = vstate->vl;

            // when are we gonna get a proper decoder...
            biscuit::Vec vd = biscuit::Vec((instruction >> 7) & 0b11111);
            biscuit::GPR address = biscuit::GPR((instruction >> 15) & 0b11111);
            bool is_load = !((instruction >> 5) & 1);

            // TODO: normally this needs to unlink the block, then modify, then relink to be safe
            void* start = as.GetCursorPointer();

            ptrdiff_t cursor = as.GetCodeBuffer().GetCursorOffset();
            as.RewindBuffer(pc - as.GetCodeBuffer().GetOffsetAddress(0) - 4); // go to vsetivli
            u32 vsetivli = *(u32*)(pc - 4);
            ASSERT(((vsetivli & 0b1111111) == 0b1010111) || vsetivli == 0b0010011); // vsetivli or nop
            switch (sew) {
            case SEW::E64: {
                as.VSETIVLI(x0, len * 8, SEW::E8);
                if (is_load) {
                    as.VLE8(vd, address);
                } else {
                    as.VSE8(vd, address);
                }
                as.VSETIVLI(x0, len, sew); // go back to old len + sew
                break;
            }
            case SEW::E32: {
                as.VSETIVLI(x0, len * 4, SEW::E8);
                if (is_load) {
                    as.VLE8(vd, address);
                } else {
                    as.VSE8(vd, address);
                }
                as.VSETIVLI(x0, len, sew); // go back to old len + sew
                break;
            }
            case SEW::E16: {
                as.VSETIVLI(x0, len * 2, SEW::E8);
                if (is_load) {
                    as.VLE8(vd, address);
                } else {
                    as.VSE8(vd, address);
                }
                as.VSETIVLI(x0, len, sew); // go back to old len + sew
                break;
            }
            default: {
                UNREACHABLE();
                break;
            }
            }
            void* end = as.GetCursorPointer();

            as.AdvanceBuffer(cursor);
            flush_icache(start, end);
            break;
        }
        default: {
            goto check_guest_signal;
        }
        }
        break;
    }
    case SIGILL: {
        bool found = false;
        if (is_in_jit_code(current_state, pc)) {
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

        if (found) {
            return;
        }

        goto check_guest_signal;
    }
    default: {
    check_guest_signal:
        // First we need to find the current ThreadState object
        SignalHandlerTable& handlers = *current_state->signal_handlers;
        RegisteredSignal& handler = handlers[sig - 1];
        if (!handler.func) {
            ERROR("Unhandled signal %d, no signal handler found", sig);
        }

        ASSERT(handler.func != SIG_IGN); // TODO: what does that even mean?
        if (g_strace) {
            STRACE("------- Guest signal %s -------", strsignal(sig));
        }

        // TODO: this could cause issues if it never jumps back to the dispatcher
        if (current_state->signals_disabled) {
            ERROR("Signal %d hit while signals are disabled", sig);

            // Signals are disabled! This might be because we are currently executing a disabled region of code, such as a rep instruction or an
            // atomic instruction that uses lr/sc. We need to queue the signal to be handled later.
            current_state->pending_signals.push(sig);

            // Also check that there's no more than 1000 signals in the queue. Arbitrary number, but the only case where this realistically happens
            // is when the signal keeps hitting over and over, which means it is a synchronous signal. We don't want to deadlock the thread and eat up
            // all the memory.
            if (current_state->pending_signals.size() > 1000) {
                ERROR("Too many pending signals, something is wrong");
            }
            return;
        }

        bool jit_code = is_in_jit_code(current_state, pc);
        auto vecs = get_vector_state(ctx);
        bool use_altstack = handler.flags & SA_ONSTACK;

        sigset_t mask_during_signal;
        mask_during_signal = handler.mask;
#ifndef SA_NODEFER
#define SA_NODEFER 0x40000000
#endif
        if (!(handler.flags & SA_NODEFER)) {
            sigaddset(&mask_during_signal, sig);
        }

        BlockMetadata* metadata = nullptr;
        u64 actual_rip = current_state->GetRip();
        if (jit_code) {
            metadata = get_block_metadata(current_state, pc);
            actual_rip = get_actual_rip(*metadata, pc);
        }

        // Prepares everything necessary to run the signal handler when we return from the host signal handler.
        // The stack is switched if necessary and filled with the frame that the signal handler expects.
        setup(metadata, actual_rip, current_state, mask_during_signal, (u64*)context->uc_mcontext.__gregs, vecs, use_altstack, jit_code);

        current_state->SetGpr(X86_REF_RDI, sig);

        // Now we just need to set RIP to the handler function
        current_state->SetRip((u64)handler.func);

        // Block the signals specified in the sa_mask until the signal handler returns
        sigset_t new_mask;
        sigandset(&new_mask, &mask_during_signal, Signals::hostSignalMask());
        pthread_sigmask(SIG_BLOCK, &new_mask, nullptr);

        if (handler.flags & SA_RESETHAND) {
            handler.func = nullptr;
        }

        if (jit_code) {
            // If in jit code, make it jump to the dispatcher immediately. If it's not in jit code, just let it naturally go to the dispatcher.
            context->uc_mcontext.__gregs[REG_PC] = (u64)recompiler.getCompileNext();
        }
        break;
    }
    }
}
#endif

void Signals::initialize() {
    struct sigaction sa;
    sa.sa_sigaction = signal_handler;
    sa.sa_flags = SA_SIGINFO;
    sigemptyset(&sa.sa_mask);

    sigaction(SIGILL, &sa, nullptr);
    sigaction(SIGBUS, &sa, nullptr);
}

void Signals::registerSignalHandler(ThreadState* state, int sig, void* handler, sigset_t mask, int flags) {
    ASSERT(sig >= 1 && sig <= 64);
    // TODO: atomic!!
    (*state->signal_handlers)[sig - 1] = {handler, mask, flags};

    // Start capturing at the first register of a signal handler and don't stop capturing even if it is disabled
    if (handler) {
        struct sigaction sa;
        sa.sa_sigaction = signal_handler;
        sa.sa_flags = SA_SIGINFO;
        sigemptyset(&sa.sa_mask);
        sigaction(sig, &sa, nullptr);
    }
}

RegisteredSignal Signals::getSignalHandler(ThreadState* state, int sig) {
    ASSERT(sig >= 1 && sig <= 64);
    // TODO: atomic!!
    return (*state->signal_handlers)[sig - 1];
}