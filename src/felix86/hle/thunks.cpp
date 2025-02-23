#include <cmath>
#include "felix86/common/log.hpp"
#include "felix86/common/state.hpp"
#include "felix86/hle/thunks.hpp"
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

int x86offset(int i) {
    switch (i) {
    case 0:
        return offsetof(ThreadState, gprs) + (8 * (X86_REF_RDI - X86_REF_RAX));
    case 1:
        return offsetof(ThreadState, gprs) + (8 * (X86_REF_RSI - X86_REF_RAX));
    case 2:
        return offsetof(ThreadState, gprs) + (8 * (X86_REF_RDX - X86_REF_RAX));
    case 3:
        return offsetof(ThreadState, gprs) + (8 * (X86_REF_RCX - X86_REF_RAX));
    case 4:
        return offsetof(ThreadState, gprs) + (8 * (X86_REF_R8 - X86_REF_RAX));
    case 5:
        return offsetof(ThreadState, gprs) + (8 * (X86_REF_R9 - X86_REF_RAX));
    default:
        ERROR("Invalid x86 offset index: %d", i);
        return 0;
    }
}

struct Thunk {
    const char* lib_name;
    const char* function_name;
    const char* signature;
    u64 host_function;
};

#define X(lib_name, function_name, signature, host_function) {lib_name, function_name, signature, (u64)host_function},
#define GLIBC(function_name, signature) X("libc.so.6", #function_name, signature, ::function_name)

static Thunk thunks[] = {
#include "glibc_thunks.inc"
};

#undef GLIBC
#undef X

static constexpr u64 trampoline_code_size = 1024 * 1024; // 1MB of trampoline code cache
static u8 trampoline_code_cache[trampoline_code_size];   // on BSS so that it may be closer to the host functions, may change in the future

biscuit::Assembler Thunks::tas(trampoline_code_cache, trampoline_code_size); // The assembler for the thunks

/*
    We use a custom signature format to describe the function.
    return type, _, arguments.

    void -> v
    integer -> q, d, w, b with x86 naming convention (qword, dword, word, byte)
    float, double -> F, D
    add others here when we need them (will we?)

    example:
    v_iif -> void my_func(int a, short b, float c)

    We only thunk simple functions so this should be fine.
*/
/*
    x86-64 ABI:
    If the class is INTEGER, the next available register of the sequence %rdi, %rsi, %rdx,
    %rcx, %r8 and %r9 is used. Return value goes in %rax.

    If the class is SSE, the next available vector register is used, the registers are taken
    in the order from %xmm0 to %xmm7. Return value goes in %xmm0.

    Note: When x86-64 functions return they zero the upper 96 or 64 bits of xmm0.

    RISC-V ABI:
    Uses a0-a7, fa0-fa7. This is enough for our purposes.
    Return value goes in a0 or fa0.
*/
void* Thunks::generateTrampoline(const std::string& signature, u64 target) {
    ASSERT(signature.size() > 0);

    void* trampoline = tas.GetCursorPointer();
    char return_type = signature[0];

    ASSERT(signature[1] == '_'); // maybe in the future separating arguments and return type will be useful (it won't)

    // Push return address
    tas.ADDI(sp, sp, -8);
    tas.SD(ra, 0, sp);

    // Check if we have arguments
    std::vector<char> arguments;
    if (signature.size() > 1) {
        arguments = std::vector<char>(signature.begin() + 2, signature.end());
    }

    int current_int_arg = 0;
    int current_float_arg = 0;
    for (size_t i = 0; i < arguments.size(); i++) {
        switch (arguments[i]) {
        case 'q':
            tas.LD(gprarg(current_int_arg), x86offset(current_int_arg), Recompiler::threadStatePointer());
            current_int_arg++;
            ASSERT(current_int_arg <= 6);
            break;
        case 'd':
            tas.LWU(gprarg(current_int_arg), x86offset(current_int_arg), Recompiler::threadStatePointer());
            current_int_arg++;
            ASSERT(current_int_arg <= 6);
            break;
        case 'w':
            tas.LHU(gprarg(current_int_arg), x86offset(current_int_arg), Recompiler::threadStatePointer());
            current_int_arg++;
            ASSERT(current_int_arg <= 6);
            break;
        case 'b':
            tas.LBU(gprarg(current_int_arg), x86offset(current_int_arg), Recompiler::threadStatePointer());
            current_int_arg++;
            ASSERT(current_int_arg <= 6);
            break;
        case 'F':
            tas.FLW(fprarg(current_float_arg), offsetof(ThreadState, xmm) + (sizeof(XmmReg) * current_float_arg), Recompiler::threadStatePointer());
            current_float_arg++;
            ASSERT(current_float_arg <= 8);
            break;
        case 'D':
            tas.FLD(fprarg(current_float_arg), offsetof(ThreadState, xmm) + (sizeof(XmmReg) * current_float_arg), Recompiler::threadStatePointer());
            current_float_arg++;
            ASSERT(current_float_arg <= 8);
            break;
        default:
            ERROR("Unknown argument type: %c", arguments[i]);
            break;
        }
    }

    i64 offset = target - (u64)tas.GetCursorPointer();

    // Call the host function
    if (IsValidJTypeImm(offset)) {
        tas.JAL(offset);
    } else if (IsValid2GBImm(offset)) {
        const auto hi20 = static_cast<int32_t>(((static_cast<uint32_t>(offset) + 0x800) >> 12) & 0xFFFFF);
        const auto lo12 = static_cast<int32_t>(offset << 20) >> 20;
        tas.AUIPC(t0, hi20);
        tas.JALR(x1, lo12, t0);
    } else {
        tas.LI(t0, target);
        tas.JALR(t0);
    }

    // Save return value to the correct x86-64 register
    switch (return_type) {
    case 'b':
        // Preserves top bits in x86-64
        tas.SB(a0, offsetof(ThreadState, gprs) + 0, Recompiler::threadStatePointer());
        break;
    case 'w':
        // Preserves top bits in x86-64
        tas.SH(a0, offsetof(ThreadState, gprs) + 0, Recompiler::threadStatePointer());
        break;
    case 'd':
        tas.SW(a0, offsetof(ThreadState, gprs) + 0, Recompiler::threadStatePointer());
        tas.SW(x0, offsetof(ThreadState, gprs) + 4, Recompiler::threadStatePointer()); // store 0 into bits 32-63
        break;
    case 'q':
        tas.SD(a0, offsetof(ThreadState, gprs) + 0, Recompiler::threadStatePointer());
        break;
    case 'F':
        tas.FSW(fa0, offsetof(ThreadState, xmm) + 0, Recompiler::threadStatePointer());
        tas.SW(x0, offsetof(ThreadState, xmm) + 4, Recompiler::threadStatePointer()); // store 0 into bits 32-63
        for (int i = 1; i < Recompiler::maxVlen() / 64; i++) {
            tas.SD(x0, offsetof(ThreadState, xmm) + (i * 8), Recompiler::threadStatePointer());
        }
        break;
    case 'D':
        tas.FSD(fa0, offsetof(ThreadState, xmm) + 0, Recompiler::threadStatePointer());
        for (int i = 1; i < Recompiler::maxVlen() / 64; i++) {
            tas.SD(x0, offsetof(ThreadState, xmm) + (i * 8), Recompiler::threadStatePointer());
        }
        break;
    case 'v':
        // No return value
        break;
    default:
        ERROR("Unknown return type: %c", return_type);
    }

    // Pop return address
    tas.LD(ra, 0, sp);
    tas.ADDI(sp, sp, 8);

    tas.RET();

    return trampoline;
}