#include <sys/mman.h>
#include "felix86/common/log.hpp"
#include "felix86/common/state.hpp"
#include "felix86/common/types.hpp"
#include "felix86/common/utility.hpp"
#include "felix86/hle/abi.hpp"
#include "felix86/hle/signals.hpp"
#include "felix86/v2/recompiler.hpp"

biscuit::GPR gprarg(int i) {
    switch (i) {
    case 0:
        return a0;
    case 1:
        return a1;
    case 2:
        return a2;
    case 3:
        return a3;
    case 4:
        return a4;
    case 5:
        return a5;
    case 6:
        return a6;
    case 7:
        return a7;
    default:
        ERROR("Invalid GPR argument index: %d", i);
        return x0;
    }
}

biscuit::FPR fprarg(int i) {
    switch (i) {
    case 0:
        return fa0;
    case 1:
        return fa1;
    case 2:
        return fa2;
    case 3:
        return fa3;
    case 4:
        return fa4;
    case 5:
        return fa5;
    case 6:
        return fa6;
    case 7:
        return fa7;
    default:
        ERROR("Invalid FPR argument index: %d", i);
        return fa0;
    }
}

x86_ref_e x86arg(int i) {
    switch (i) {
    case 0:
        return X86_REF_RDI;
    case 1:
        return X86_REF_RSI;
    case 2:
        return X86_REF_RDX;
    case 3:
        return X86_REF_RCX;
    case 4:
        return X86_REF_R8;
    case 5:
        return X86_REF_R9;
    default:
        ERROR("Invalid x86 offset index: %d", i);
        return X86_REF_COUNT;
    }
}

int get_size(char c) {
    switch (c) {
    case 'x':
        return 0;
    case 'q':
        return 8;
    case 'd':
        return 4;
    case 'w':
        return 2;
    case 'b':
        return 1;
    case 'F':
        return 4;
    case 'D':
        return 8;
    default: {
        ERROR("Unknown type: %c", c);
        return 0;
    }
    }
}

struct x86_location {
    enum {
        reg,
        stack,
    } type;
    union {
        x86_ref_e reg;
        u32 stack_position;
    } value;
};

struct riscv_location {
    enum {
        gpr,
        fpr,
        stack,
    } type;
    union {
        int reg_index;
        u32 stack_position;
    } value;
};

struct Marshalling {
    x86_location x86;
    riscv_location riscv;
    int size;
};

void my_printer(ThreadState* state, const char* name) {
    int len = strlen(name);
    const char* signature = name + len + 1;
    int sig_len = strlen(signature);
    char buffer[4096];
    int size = 0;
    size += sprintf(buffer + size, "Calling function %s (%s) {", name, signature);
    for (int i = 2; i < sig_len; i++) {
        if (i >= 8) {
            size += sprintf(buffer + size, "too big...");
            break;
        }
        x86_ref_e ref = x86arg(i - 2);
        u64 gpr = state->gprs[ref];
        size += sprintf(buffer + size, "arg%d = %lx, ", i - 2, gpr);
    }
    size += sprintf(buffer + size, "}");
    PLAIN("%s", buffer);
}

GuestToHostMarshaller::GuestToHostMarshaller(const std::string& name, const std::string& signature) : name(name), signature(signature) {}

void GuestToHostMarshaller::emitPrologue(biscuit::Assembler& as) {
    ASSERT(signature.size() >= 2);
    ASSERT(signature[1] == '_');

    if (g_config.print_thunks) {
        biscuit::Label after;
        as.MV(a0, s11);
        as.LI(t0, (u64)my_printer);
        as.AUIPC(a1, 0);
        as.ADDI(a1, a1, 16);
        as.JALR(t0);
        as.J(&after);
        for (int i = 0; i < name.size(); i++) {
            u8 c = name[i];
            as.GetCodeBuffer().Emit(c);
        }
        as.GetCodeBuffer().Emit((u8)0);
        for (int i = 0; i < signature.size(); i++) {
            u8 c = signature[i];
            as.GetCodeBuffer().Emit(c);
        }
        as.GetCodeBuffer().Emit((u8)0);
        while ((u64)as.GetCursorPointer() & 0b11) {
            as.GetCodeBuffer().Emit((u8)0);
        }
        as.Bind(&after);
    }

    int gprcount = 0;
    int fprcount = 0;
    int guest_stackpos = 8; // [rsp + 0] has the return address, arguments start at [rsp + 8]
    int host_stackpos = 0;  // riscv starts at 0 since there's the ra reg
    std::vector<Marshalling> marshallings;
    for (size_t i = 2; i < signature.size(); i++) {
        char type = signature[i];
        switch (type) {
        case 'x':
        case 'q':
        case 'd':
        case 'w':
        case 'b': {
            Marshalling marshalling;
            marshalling.size = get_size(type);
            if (gprcount < 6) {
                // x86 ABI has 6 GPR argument registers
                x86_ref_e x86_reg = x86arg(gprcount);
                marshalling.x86.type = x86_location::reg;
                marshalling.x86.value.reg = x86_reg;

                biscuit::GPR riscv_reg = gprarg(gprcount);
                marshalling.riscv.type = riscv_location::gpr;
                marshalling.riscv.value.reg_index = riscv_reg.Index();
            } else if (gprcount == 6 || gprcount == 7) {
                // Since RISC-V ABI has 8 GPR argument registers some x86 stack variables go to registers
                int x86_pos = guest_stackpos;
                marshalling.x86.type = x86_location::stack;
                marshalling.x86.value.stack_position = x86_pos;

                biscuit::GPR riscv_reg = gprarg(gprcount);
                marshalling.riscv.type = riscv_location::gpr;
                marshalling.riscv.value.reg_index = riscv_reg.Index();
                guest_stackpos += 8;
            } else {
                int x86_pos = guest_stackpos;
                marshalling.x86.type = x86_location::stack;
                marshalling.x86.value.stack_position = x86_pos;

                int riscv_pos = host_stackpos;
                marshalling.riscv.type = riscv_location::stack;
                marshalling.riscv.value.stack_position = riscv_pos;
                guest_stackpos += 8;
                host_stackpos += 8;
            }
            marshallings.push_back(marshalling);
            gprcount++;
            break;
        }
        case 'F':
        case 'D': {
            // Same as GPR but with FPRs
            Marshalling marshalling;
            marshalling.size = get_size(type);
            if (fprcount < 8) {
                // x86 ABI has 8 FPR argument registers, just like RISC-V
                x86_ref_e x86_reg = (x86_ref_e)(X86_REF_XMM0 + fprcount);
                marshalling.x86.type = x86_location::reg;
                marshalling.x86.value.reg = x86_reg;

                biscuit::FPR riscv_reg = fprarg(fprcount);
                marshalling.riscv.type = riscv_location::fpr;
                marshalling.riscv.value.reg_index = riscv_reg.Index();
            } else {
                int x86_pos = guest_stackpos;
                marshalling.x86.type = x86_location::stack;
                marshalling.x86.value.stack_position = x86_pos;

                int riscv_pos = host_stackpos;
                marshalling.riscv.type = riscv_location::stack;
                marshalling.riscv.value.stack_position = riscv_pos;
                guest_stackpos += 8;
                host_stackpos += 8;
            }
            marshallings.push_back(marshalling);
            fprcount++;
            break;
        }
        default:
            ERROR("Unknown argument type: %c", type);
            break;
        }
    }

    // Align stack to 16 bytes
    host_stackpos += 15;
    host_stackpos &= ~15;

    stack_size = host_stackpos;

    if (stack_size != 0) {
        as.ADDI(sp, sp, -stack_size);
    }

    for (auto& marshalling : marshallings) {
        biscuit::GPR address_reg;
        int offset;
        if (marshalling.x86.type == x86_location::stack) {
            // State is written back before jumping to trampoline but it should still hold
            // the correct value here
            address_reg = Recompiler::allocatedGPR(X86_REF_RSP);
            offset = marshalling.x86.value.stack_position;
        } else {
            address_reg = Recompiler::threadStatePointer();
            if (marshalling.x86.value.reg >= X86_REF_RAX && marshalling.x86.value.reg <= X86_REF_R15) {
                offset = offsetof(ThreadState, gprs) + (marshalling.x86.value.reg - X86_REF_RAX) * 8;
            } else if (marshalling.x86.value.reg >= X86_REF_XMM0 && marshalling.x86.value.reg <= X86_REF_XMM15) {
                offset = offsetof(ThreadState, xmm) + (marshalling.x86.value.reg - X86_REF_XMM0) * sizeof(XmmReg);
            } else {
                UNREACHABLE();
            }
        }

        if (marshalling.riscv.type == riscv_location::gpr || marshalling.riscv.type == riscv_location::stack) {
            biscuit::GPR dest_reg;
            bool is_stack = false;
            if (marshalling.riscv.type == riscv_location::stack) {
                dest_reg = x7;
                is_stack = true;
                static_assert(Recompiler::isScratch(x7));
            } else {
                dest_reg = biscuit::GPR(marshalling.riscv.value.reg_index);
            }

            switch (marshalling.size) {
            case 8: {
                as.LD(dest_reg, offset, address_reg);
                break;
            }
            case 4: {
                as.LW(dest_reg, offset, address_reg);
                break;
            }
            case 2: {
                as.LH(dest_reg, offset, address_reg);
                break;
            }
            case 1: {
                as.LB(dest_reg, offset, address_reg);
                break;
            }
            case 0: { // size 0 means zero out this register, see justification in comments in emitPrologue func
                as.MV(dest_reg, x0);
                break;
            }
            default: {
                UNREACHABLE();
                break;
            }
            }

            if (is_stack) {
                switch (marshalling.size) {
                case 8: {
                    as.SD(dest_reg, marshalling.riscv.value.stack_position, sp);
                    break;
                }
                case 4: {
                    as.SW(dest_reg, marshalling.riscv.value.stack_position, sp);
                    break;
                }
                case 2: {
                    as.SH(dest_reg, marshalling.riscv.value.stack_position, sp);
                    break;
                }
                case 1: {
                    as.SB(dest_reg, marshalling.riscv.value.stack_position, sp);
                    break;
                }
                case 0: {
                    as.SD(x0, marshalling.riscv.value.stack_position, sp);
                    break;
                }
                default: {
                    UNREACHABLE();
                    break;
                }
                }
            }
        } else if (marshalling.riscv.type == riscv_location::fpr) {
            biscuit::FPR dest_reg = biscuit::FPR(marshalling.riscv.value.reg_index);
            ASSERT(marshalling.x86.type == x86_location::reg);
            ASSERT(address_reg == Recompiler::threadStatePointer());
            switch (marshalling.size) {
            case 8: {
                as.FLD(dest_reg, offset, address_reg);
                break;
            }
            case 4: {
                as.FLW(dest_reg, offset, address_reg);
                break;
            }
            }
        } else {
            UNREACHABLE();
        }
    }
}

void GuestToHostMarshaller::emitEpilogue(biscuit::Assembler& as) {
    char return_type = signature[0];

    if (return_type != 'v') {
        // Save return type directly to ThreadState struct
        switch (return_type) {
        case 'b':
            // Preserves top bits in x86-64
            as.SB(a0, offsetof(ThreadState, gprs) + 0, Recompiler::threadStatePointer());
            break;
        case 'w':
            // Preserves top bits in x86-64
            as.SH(a0, offsetof(ThreadState, gprs) + 0, Recompiler::threadStatePointer());
            break;
        case 'd':
            as.SW(a0, offsetof(ThreadState, gprs) + 0, Recompiler::threadStatePointer());
            as.SW(x0, offsetof(ThreadState, gprs) + 4, Recompiler::threadStatePointer()); // store 0 into bits 32-63
            break;
        case 'q':
            as.SD(a0, offsetof(ThreadState, gprs) + 0, Recompiler::threadStatePointer());
            break;
        case 'F': {
            as.FSW(fa0, offsetof(ThreadState, xmm) + 0, Recompiler::threadStatePointer());
            as.SW(x0, offsetof(ThreadState, xmm) + 4, Recompiler::threadStatePointer()); // store 0 into bits 32-63
            for (u32 i = 1; i < sizeof(XmmReg) / 8; i++) {
                as.SD(x0, offsetof(ThreadState, xmm) + (i * 8), Recompiler::threadStatePointer());
            }
            break;
        }
        case 'D': {
            as.FSD(fa0, offsetof(ThreadState, xmm) + 0, Recompiler::threadStatePointer());
            for (u32 i = 1; i < sizeof(XmmReg) / 8; i++) {
                as.SD(x0, offsetof(ThreadState, xmm) + (i * 8), Recompiler::threadStatePointer());
            }
            break;
        }
        default: {
            UNREACHABLE();
            break;
        }
        }
    }

    if (stack_size != 0) {
        as.ADDI(sp, sp, stack_size);
    }
}

void enter_dispatcher_for_callback(ThreadState* state) {
    u64 rip = state->rip;
    VERBOSE("Entering dispatcher for callback at %p", rip);
    state->recompiler->enterDispatcher(state);
    ASSERT(state->exit_reason == EXIT_REASON_GUEST_CODE_FINISHED);
    state->exit_reason = EXIT_REASON_UNKNOWN;
    VERBOSE("Finished callback %p", rip);
}

void* ABIMadness::hostToGuestTrampoline(const char* signature, const void* guest_function) {
    // We need custom guest code and custom host code
    ThreadState* state = ThreadState::Get();
    SignalGuard guard;
    u8* const x86_code = state->x86_trampoline_storage;
    u8* curr = x86_code;
    // Our recompiler marks guest code as PROT_READ, we need to undo this as it may have marked previous trampolines
    mprotect((u8*)((u64)x86_code & ~0xFFFull), 4096, PROT_READ | PROT_WRITE);
    // mov rax, u64
    curr[0] = 0x48;
    curr[1] = 0xb8;
    memcpy(&curr[2], &guest_function, sizeof(u64));
    curr += 10;

    // call rax
    curr[0] = 0xff;
    curr[1] = 0xd0;
    curr += 2;

    // invlpg [rdx], exit dispatcher
    curr[0] = 0x0f;
    curr[1] = 0x01;
    curr[2] = 0x3a;
    curr += 3;

    // hlt to stop scanAhead and make sure it doesn't get here
    curr[0] = 0xf4;
    curr += 1;

    state->x86_trampoline_storage = curr;

    u8* riscv_code = state->riscv_trampoline_storage;

    // Now create our RISC-V portion later in a separate page because the
    // x86 code is going to become read-only when it gets recompiled
    biscuit::Assembler as(riscv_code, 4096);
    void* trampoline = as.GetCursorPointer();
    as.ADDI(sp, sp, -32);
    as.SD(ra, 24, sp);
    as.SD(s11, 0, sp);
    as.SD(s10, 8, sp);

    biscuit::GPR thread_state_pointer = s11;
    biscuit::GPR guest_stack_pointer = t1;

    // ThreadState* in s11, RSP in t1
    // This is yucky... we need to store all of our saved registers to get the ThreadState at runtime
    // We can't just call ThreadState::Get because it will ruin our arguments.
    as.ADDI(sp, sp, -16 * 8);
    as.SD(a0, 0 * 8, sp);
    as.SD(a1, 1 * 8, sp);
    as.SD(a2, 2 * 8, sp);
    as.SD(a3, 3 * 8, sp);
    as.SD(a4, 4 * 8, sp);
    as.SD(a5, 5 * 8, sp);
    as.SD(a6, 6 * 8, sp);
    as.SD(a7, 7 * 8, sp);
    as.FSD(fa0, 8 * 8, sp);
    as.FSD(fa1, 9 * 8, sp);
    as.FSD(fa2, 10 * 8, sp);
    as.FSD(fa3, 11 * 8, sp);
    as.FSD(fa4, 12 * 8, sp);
    as.FSD(fa5, 13 * 8, sp);
    as.FSD(fa6, 14 * 8, sp);
    as.FSD(fa7, 15 * 8, sp);
    as.LI(t1, (u64)ThreadState::Get);
    as.JALR(t1);
    as.MV(thread_state_pointer, a0);
    as.LD(a0, 0 * 8, sp);
    as.LD(a1, 1 * 8, sp);
    as.LD(a2, 2 * 8, sp);
    as.LD(a3, 3 * 8, sp);
    as.LD(a4, 4 * 8, sp);
    as.LD(a5, 5 * 8, sp);
    as.LD(a6, 6 * 8, sp);
    as.LD(a7, 7 * 8, sp);
    as.FLD(fa0, 8 * 8, sp);
    as.FLD(fa1, 9 * 8, sp);
    as.FLD(fa2, 10 * 8, sp);
    as.FLD(fa3, 11 * 8, sp);
    as.FLD(fa4, 12 * 8, sp);
    as.FLD(fa5, 13 * 8, sp);
    as.FLD(fa6, 14 * 8, sp);
    as.FLD(fa7, 15 * 8, sp);
    as.ADDI(sp, sp, 16 * 8);

    as.LD(guest_stack_pointer, offsetof(ThreadState, gprs) + (X86_REF_RSP - X86_REF_RAX) * 8, thread_state_pointer);

    // Marshal host arguments to guest arguments
    size_t size = strlen(signature);
    ASSERT(size >= 2);
    ASSERT(signature[1] == '_');

    int x86_stack_size = 0;
    {
        int x86_gpr_count = 0;
        int x86_fpr_count = 0;
        for (u64 i = 2; i < size; i++) {
            switch (signature[i]) {
            case 'b':
            case 'w':
            case 'd':
            case 'q':
            case 'x': {
                if (x86_gpr_count >= 6) {
                    x86_stack_size += 8;
                } else {
                    x86_gpr_count++;
                }
                break;
            }
            case 'F':
            case 'D': {
                if (x86_fpr_count >= 8) {
                    x86_stack_size += 8;
                } else {
                    x86_fpr_count++;
                }
                break;
            }
            default: {
                UNREACHABLE();
                break;
            }
            }
        }
    }

    int x86_stack_offset = 0;
    int gpr_count = 0;
    int riscv_stack_offset = 32; // we decremented sp to push arguments, add it back here when pulling stack args
    if (x86_stack_size > 0) {
        as.ADDI(guest_stack_pointer, guest_stack_pointer, -x86_stack_size);
        as.SD(guest_stack_pointer, offsetof(ThreadState, gprs) + (X86_REF_RSP - X86_REF_RAX) * 8, thread_state_pointer);
    }

    for (size_t i = 2; i < size; i++) {
        switch (signature[i]) {
        case 'q':
        case 'd': {
            if (gpr_count >= 8) {
                biscuit::GPR temp = t0;
                // We decremented sp by 32 previously, we need to add it again here when loading stack arguments
                if (signature[i] == 'd') {
                    as.LWU(temp, riscv_stack_offset, sp);
                } else {
                    as.LD(temp, riscv_stack_offset, sp);
                }
                as.SD(temp, x86_stack_offset, guest_stack_pointer);
                riscv_stack_offset += 8;
                x86_stack_offset += 8;
            } else if (gpr_count >= 6) {
                biscuit::GPR riscv_reg = gprarg(gpr_count);
                if (signature[i] == 'd') {
                    as.SLLI(riscv_reg, riscv_reg, 32);
                    as.SRLI(riscv_reg, riscv_reg, 32);
                }
                as.SD(riscv_reg, x86_stack_offset, guest_stack_pointer);
                x86_stack_offset += 8;
            } else {
                biscuit::GPR riscv_reg = gprarg(gpr_count);
                if (signature[i] == 'd') {
                    as.SLLI(riscv_reg, riscv_reg, 32);
                    as.SRLI(riscv_reg, riscv_reg, 32);
                }
                x86_ref_e arg = x86arg(gpr_count);
                as.SD(riscv_reg, offsetof(ThreadState, gprs) + (arg - X86_REF_RAX) * 8, thread_state_pointer);
            }
            gpr_count++;
            break;
        }
        case 'F':
        case 'D': {
            UNREACHABLE();
            break;
        }
        default: {
            // Currently we only have `q` or `d` in callback arguments
            // TODO: Support all characters in the future
            ERROR("Unknown signature argument: %c", signature[i]);
            break;
        }
        }
    }

    ASSERT(x86_stack_size == x86_stack_offset);

    // Save old RIP, set new RIP -- TODO: why do we even save the old rip? delete this
    as.LD(s10, offsetof(ThreadState, rip), thread_state_pointer);
    as.LI(t0, (u64)x86_code);
    as.SD(t0, offsetof(ThreadState, rip), thread_state_pointer);

    as.MV(a0, s11);
    as.LI(t2, (u64)enter_dispatcher_for_callback);

    // Finally we "jump" to the guest code
    // In reality we enter the dispatcher and it's gonna compile it and yada yada
    as.JALR(t2);

    // Eventually we will return here when ExitDispatcher is called due to invlpg [rdx]
    // Restore old RIP (is this saving/restoring even necessary?)
    as.SD(s10, offsetof(ThreadState, rip), thread_state_pointer);

    if (x86_stack_size > 0) {
        // Restore the old stack
        as.LD(guest_stack_pointer, offsetof(ThreadState, gprs) + (X86_REF_RSP - X86_REF_RAX) * 8, thread_state_pointer);
        as.ADDI(guest_stack_pointer, guest_stack_pointer, x86_stack_size);
        as.SD(guest_stack_pointer, offsetof(ThreadState, gprs) + (X86_REF_RSP - X86_REF_RAX) * 8, thread_state_pointer);
    }

    // Load the return value from the state struct to a RISC-V register
    char return_type = signature[0];
    switch (return_type) {
    case 'd': {
        // RAX
        as.LW(a0, offsetof(ThreadState, gprs), thread_state_pointer);
        break;
    }
    case 'q': {
        // RAX
        as.LD(a0, offsetof(ThreadState, gprs), thread_state_pointer);
        break;
    }
    case 'D': {
        // XMM0
        as.FLD(fa0, offsetof(ThreadState, xmm), thread_state_pointer);
        break;
    }
    case 'v': {
        break;
    }
    default: {
        UNIMPLEMENTED();
    }
    }

    as.LD(s11, 0, sp);
    as.LD(s10, 8, sp);
    as.LD(ra, 24, sp);
    as.ADDI(sp, sp, 32);
    as.RET();

    state->riscv_trampoline_storage = as.GetCursorPointer();

    flush_icache();
    return trampoline;
}