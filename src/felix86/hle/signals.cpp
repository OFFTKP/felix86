#include <array>
#include <sys/mman.h>
#include "biscuit/decoder.hpp"
#include "felix86/common/state.hpp"
#include "felix86/hle/signals.hpp"
#include "felix86/v2/recompiler.hpp"

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

void reconstruct_state(ThreadState* state, BlockMetadata* current_block, u64 rip, uint64_t pc, const u64* gprs, const XmmReg* xmms) {
    const u64 start = current_block->address;
    const u64 end = pc;
    u64 current = start;
    bool valid = true;

    // Go through the instructions in this block, find the ones that modify our allocated registers, extract the values
    // from the registers and put them into `state`
    biscuit::Decoder decoder;
    DecodedInstruction instruction;
    DecodedOperand operands[4];

    // TODO: static method in recompiler for this conversion that doesn't do runtime computation
    static std::array<x86_ref_e, 32> gpr_to_x86 = {};
    static std::array<x86_ref_e, 32> vec_to_x86 = {};
    static std::atomic_flag initialized = ATOMIC_FLAG_INIT;
    if (!initialized.test_and_set()) {
        memset(gpr_to_x86.data(), X86_REF_COUNT, 32);
        memset(vec_to_x86.data(), X86_REF_COUNT, 32);

        for (int i = 0; i < 16; i++) {
            biscuit::GPR allocated_gpr = Recompiler::allocatedGPR((x86_ref_e)(X86_REF_RAX + i));
            biscuit::Vec allocated_vec = Recompiler::allocatedVec((x86_ref_e)(X86_REF_XMM0 + i));
            gpr_to_x86[allocated_gpr.Index()] = (x86_ref_e)(X86_REF_RAX + i);
            vec_to_x86[allocated_vec.Index()] = (x86_ref_e)(X86_REF_XMM0 + i);
        }

        gpr_to_x86[Recompiler::allocatedGPR(X86_REF_ZF).Index()] = X86_REF_ZF;
        gpr_to_x86[Recompiler::allocatedGPR(X86_REF_CF).Index()] = X86_REF_CF;
        gpr_to_x86[Recompiler::allocatedGPR(X86_REF_OF).Index()] = X86_REF_OF;
        gpr_to_x86[Recompiler::allocatedGPR(X86_REF_SF).Index()] = X86_REF_SF;
    }

    while (current < end) {
        DecoderStatus status = decoder.Decode((void*)current, 4, instruction, operands);

        if (status == DecoderStatus::UnknownInstruction) {
            u32 buffer = *(u32*)current;
            WARN("Couldn't decode: %08x", buffer);
            current += 4;
            continue;
        } else if (status == DecoderStatus::UnknownInstructionCompressed) {
            u16 buffer = *(u16*)current;
            WARN("Couldn't decode: %04x", buffer);
            current += 2;
            continue;
        } else {
            current += instruction.length;
        }

        // See Recompiler::invalidStateUntilJump, we use this NOP to mark regions of the block
        // that don't have valid register state and shouldn't be copied
        if (instruction.mnemonic == Mnemonic::SRLI && operands[0].GPR() == x0 && operands[1].GPR() == x0 && operands[2].Immediate() == 42) {
            valid = false;
        }

        if (valid) {
            if (instruction.operand_count >= 1) {
                bool write = operands[0].IsWrite();
                if (write && operands[0].IsGPR()) {
                    int gpr_index = operands[0].GPR().Index();
                    x86_ref_e ref = gpr_to_x86[gpr_index];
                    if (ref >= X86_REF_RAX && ref <= X86_REF_R15) {
                        u64 value = gprs[gpr_index];
                        u64 old_value = state->GetGpr(ref);
                        VERBOSE("Reconstructing state: x86 gpr %d gets value %lx (old: %lx) at RISC-V PC: %lx", ref - X86_REF_RAX, value, old_value,
                                current);
                        state->SetGpr(ref, value);
                    } else if (ref >= X86_REF_CF && ref <= X86_REF_OF) {
                        u64 value = gprs[gpr_index];
                        state->SetFlag(ref, value);
                    }
                } else if (write && operands[0].IsVec()) {
                    int vec_index = operands[0].Vec().Index();
                    x86_ref_e ref = vec_to_x86[vec_index];
                    if (ref != X86_REF_COUNT) {
                        XmmReg xmm = xmms[vec_index];
                        state->SetXmm(ref, xmm);
                    }
                }
            }
        } else {
            if (instruction.mnemonic == Mnemonic::JAL || instruction.mnemonic == Mnemonic::JALR) {
                valid = true;
            }
        }
    }

    // Finally also set the RIP
    state->SetRip(rip);
}

BlockMetadata* get_block_metadata(ThreadState* state, u64 host_pc) {
    auto& map = state->recompiler->getHostPcMap();
    auto it = map.lower_bound(host_pc);
    ASSERT(it != map.end());
    if (!(host_pc >= it->second->address && host_pc <= it->second->address_end)) {
        // Print all the blocks so we can see what is going on
        if (g_config.verbose) {
            for (auto& range : map) {
                printf("Block: %lx-%lx\n", range.second->address, range.second->address_end);
            }
        }
        ERROR("PC: %lx not inside range %lx-%lx?", host_pc, it->second->address, it->second->address_end);
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

// arch/x86/kernel/signal.c, get_sigframe function prepares the signal frame
void Signals::setupFrame(uint64_t pc, ThreadState* state, sigset_t new_mask, const u64* host_gprs, const XmmReg* host_vecs, bool use_altstack,
                         bool in_jit_code, siginfo_t* host_siginfo) {
    u64 rsp = use_altstack ? (u64)state->alt_stack.ss_sp : state->GetGpr(X86_REF_RSP);
    if (rsp == 0) {
        ERROR("RSP is null, use_altstack: %d", use_altstack);
    }

    rsp = rsp - 128; // red zone
    rsp = rsp - sizeof(x64_rt_sigframe);
    x64_rt_sigframe* frame = (x64_rt_sigframe*)rsp;

    frame->pretcode = (char*)Signals::magicSigreturnAddress();

    frame->uc.uc_mcontext.fpregs = &frame->uc.fpregs_mem;

    frame->uc.uc_flags = 0;
    frame->uc.uc_link = 0;
    frame->info = *host_siginfo;

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
        BlockMetadata* current_block = get_block_metadata(state, pc);
        u64 actual_rip = get_actual_rip(*current_block, pc);
        ASSERT(current_block);
        reconstruct_state(state, current_block, actual_rip, pc, host_gprs, host_vecs);
    } else {
        // State reconstruction isn't necessary, the state should be in some stable form
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

    state->SetGpr(X86_REF_RSP, rsp);               // set the new stack pointer
    state->SetGpr(X86_REF_RSI, (u64)&frame->info); // set the siginfo pointer
    state->SetGpr(X86_REF_RDX, (u64)&frame->uc);   // set the ucontext pointer
}

void Signals::sigreturn(ThreadState* state) {
    VERBOSE("------- sigreturn -------");
    ASSERT_MSG(state->exit_reason == EXIT_REASON_UNKNOWN, "State had exit reason when entering sigreturn?");
    state->exit_reason = EXIT_REASON_SIGRETURN;

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

riscv_v_state* get_riscv_vector_state(void* ctx) {
#ifdef __riscv
    ucontext_t* context = (ucontext_t*)ctx;
    mcontext_t* mcontext = &context->uc_mcontext;
    unsigned int* reserved = mcontext->__fpregs.__q.__glibc_reserved;

    // Normally the glibc should have better support for this, but this will be fine for now
    if (reserved[1] != 0x53465457) { // RISC-V V extension magic number that indicates the presence of vector state
        return nullptr;              // old kernel version, unsupported, we can't get the vector state and the vector regs may= be unstable
    }

    void* after_fpregs = reserved + 3;
    riscv_v_state* v_state = (riscv_v_state*)after_fpregs;
    return v_state;
#else
    return nullptr;
#endif
}

// Gets the vector state from the frame, only for recentish Linux kernels
std::optional<std::array<XmmReg, 32>> get_vector_state(void* ctx) {
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

bool handle_smc(ThreadState* current_state, siginfo_t* info, ucontext_t* context, u64 pc) {
    if (!is_in_jit_code(current_state, (u8*)pc)) {
        WARN("We hit a SIGSEGV ACCERR but PC is not in JIT code...");
        return false;
    }

    u64 write_address = (u64)info->si_addr & ~0xFFFull;
    Recompiler::invalidateRangeGlobal(write_address, write_address + 0x1000);
    ASSERT(::mprotect((void*)write_address, 0x1000, PROT_READ | PROT_WRITE) == 0);
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
    if (!g_config.capture_sigsegv) {
        return false;
    }

    int pid = getpid();
    PLAIN("I have been hit by a wild SIGSEGV! My PID is %d, you have 40 seconds to attach gdb using `gdb -p %d` to find out why! If you think this "
          "SIGSEGV was intended, disabled this mode by unsetting the `capture_sigsegv` option.",
          pid, pid);
    ::sleep(40);
    return true;
}

constexpr std::array<RegisteredHostSignal, 3> host_signals = {{
    {SIGSEGV, SEGV_ACCERR, handle_smc},
    {SIGILL, 0, handle_breakpoint},
    {SIGSEGV, 0, handle_wild_sigsegv}, // order matters, relevant sigsegvs are handled before this handler
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
        return true;
    }

    if (g_mode32) {
        WARN("WARN: Signals (%d) in 32-bit apps are currently not well supported", sig);
    }

    if (handler->func == (u64)SIG_IGN) {
        ERROR("Signal %d hit but signal handler is SIGIGN", sig);
        return true;
    }

    if (state->signals_disabled) {
        // Nothing we can do, the signals are disabled. Push it to the queue.
        state->pending_signals.push_back({sig, *info});

        if (state->pending_signals.size() > 5) {
            ERROR("More than 5 pending signals, something is probably wrong, exiting to avoid spam");
        }

        // Unlink the current block, making it certain that we will eventually return to the dispatcher to handle this signal
        // even if we are stuck in a loop, for example in a block that branches back to itself forever.
        state->recompiler->unlinkBlock(state, state->GetRip());
        return true;
    }

    if (g_config.print_signals || g_config.verbose) {
        PLAIN("------- Guest signal %s (%d) %s -------", sigdescr_np(sig), sig, in_jit_code ? "in jit code" : "not in jit code");
    }

    ASSERT(!g_mode32);

    XmmReg* xmms;

    u64* gprs = get_regs(ctx);
    auto host_vecs = get_vector_state(ctx);
    if (host_vecs) {
        // Xmms start at the first allocated register, xmm0-xmm15 are allocated to sequential host registers so we are fine
        xmms = &(*host_vecs)[Recompiler::allocatedVec(X86_REF_XMM0).Index()];
    } else {
        // In the chance that this is an old kernel and we couldn't get the vector state in the signal handler, let's at least
        // get the most recent state we are aware of, from before entering the block
        xmms = state->xmm;
    }

    bool use_altstack = handler->flags & SA_ONSTACK;
    if (use_altstack && state->alt_stack.ss_sp == 0) {
        // If there's no altstack set up, use the default stack instead
        use_altstack = false;
    }

    sigset_t mask_during_signal;
    mask_during_signal = *(sigset_t*)&handler->mask;

    if (!(handler->flags & SA_NODEFER)) {
        sigaddset(&mask_during_signal, sig);
    }

    // Prepares everything necessary to run the signal handler when we return from the host signal handler.
    // The stack is switched if necessary and filled with the frame that the signal handler expects.
    Signals::setupFrame(pc, state, mask_during_signal, gprs, xmms, use_altstack, in_jit_code, info);

    // RSI and RDX are set by setupFrame
    state->SetGpr(X86_REF_RDI, sig);

    // Now we just need to set RIP to the handler function
    state->SetRip(handler->func);

    // Block the signals specified in the sa_mask until the signal handler returns
    sigset_t new_mask;
    sigandset(&new_mask, &mask_during_signal, Signals::hostSignalMask());
    pthread_sigmask(SIG_BLOCK, &new_mask, nullptr);

    if (handler->flags & SA_RESETHAND) {
        handler->func = (u64)SIG_DFL;
    }

    // Eventually, this should return right after this call and have the correct state.
    // When entering the dispatcher, the host state is saved in ThreadState::frames. Including sp & ra.
    // sigreturn will call exitDispatcher, which will load the old frame and return back here after this call.
    // This way we can support signals inside signal handlers too.
    // The only problem would be longjmps out of signal handlers. This is evil but possible that a game or something does it
    // In that case the frames would eventually overflow and at least we'd gave an appropriate message.
    state->recompiler->enterDispatcher(state);

    if (state->exit_reason == EXIT_REASON_SIGRETURN) {
        // All went fine, we returned from the dispatcher normally
    } else {
        if (state->exit_reason == EXIT_REASON_EXIT_GROUP_SYSCALL || state->exit_reason == EXIT_REASON_EXIT_SYSCALL) {
            WARN("Exitting thread %d from inside a signal handler with error code: %d", gettid(), state->exit_code);
            _exit(state->exit_code);
        }
        ERROR("Something went wrong when returning from dispatcher on signal handler: %s", print_exit_reason(state->exit_reason));
    }

    // Reset the exit reason
    state->exit_reason = EXIT_REASON_UNKNOWN;

    return true;
}

// Main signal handler function, all signals come here
void signal_handler(int sig, siginfo_t* info, void* ctx) {
    // First, check if this is a host signal
    bool handled;

    handled = dispatch_host(sig, info, ctx);
    if (handled) {
        // Ok it was a host signal
        VERBOSE("Host signal %d was handled successfully", sig);
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

void Signals::registerSignalHandler(ThreadState* state, int sig, u64 handler, u64 mask, int flags) {
    ASSERT(sig >= 1 && sig <= 64);

    // Hopefully externally synchronized, no need for locks :cluegi:
    state->signal_table->registerSignal(sig, handler, mask, flags);

    // Start capturing at the first register of a signal handler and don't stop capturing even if it is disabled
    if (handler != 0) {
        struct real_sigaction sa;
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
    WARN("About to run sigsuspend");
    int result = ::sigsuspend(mask);
    if (result == -1) {
        return -errno;
    } else {
        return result;
    }
}
