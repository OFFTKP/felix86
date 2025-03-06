#include <cmath>
#include <dlfcn.h>
#include "felix86/common/state.hpp"
#include "felix86/hle/thunks.hpp"
#include "felix86/v2/recompiler.hpp"

void* libGLX = nullptr;

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

// Function pointers, obtained with dlopen+dlsym
// u64's as we don't really care about the type here,
// these are just pointers for the assembler
namespace thunkptr {
#define X(libname, name, ...) u64 name = 0;
#include "glx_thunks.inc"
#undef X
} // namespace thunkptr

struct Thunk {
    const char* lib_name;
    const char* function_name;
    const char* signature;
    u64* host_function = 0;
    u64 constructor_function = 0;
    u64 destructor_function = 0;
};

#define X(lib_name, function_name, signature, constructor_function, destructor_function)                                                             \
    {lib_name, #function_name, #signature, &thunkptr::function_name, (u64)constructor_function, (u64)destructor_function},

static Thunk thunk_metadata[] = {
#include "glx_thunks.inc"
};

#undef X

void* glXGetProcAddressAndPrint(const char* name) {
    printf("glXGetProcAddress: %s\n", name);
    static void* actual = dlsym(libGLX, "glXGetProcAddress");
    return ((void* (*)(const char*))actual)(name);
}

// Load the host function pointers in the thunkptr namespace with pointers using dlopen + dlsym
void Thunks::initialize() {
    thunkptr::glXGetProcAddress = (u64)glXGetProcAddressAndPrint;
    thunkptr::glXGetProcAddressARB = (u64)glXGetProcAddressAndPrint;

    constexpr const char* path = "/felix86/lib/libGLX.so";
    libGLX = dlopen(path, RTLD_LAZY);
    if (!libGLX) {
        ERROR("I couldn't open libGLX at %s, error: %s", path, dlerror());
    } // having an else here would be uglier than a goto

#define X(libname, name, ...)                                                                                                                        \
    if (thunkptr::name == 0) {                                                                                                                       \
        thunkptr::name = (u64)dlsym(libGLX, #name);                                                                                                  \
        if (thunkptr::name == 0) {                                                                                                                   \
            ERROR("Failed to find symbol %s in %s, error: %s", #name, "libGLX.so", dlerror());                                                       \
        }                                                                                                                                            \
    }
#include "glx_thunks.inc"
#undef X
}

void call(Assembler& as, u64 target) {
    i64 offset = target - (u64)as.GetCursorPointer();
    if (IsValidJTypeImm(offset)) {
        as.JAL(offset);
    } else if (IsValid2GBImm(offset)) {
        const auto hi20 = static_cast<int32_t>(((static_cast<uint32_t>(offset) + 0x800) >> 12) & 0xFFFFF);
        const auto lo12 = static_cast<int32_t>(offset << 20) >> 20;
        as.AUIPC(t0, hi20);
        as.JALR(ra, lo12, t0);
    } else {
        as.LI(t0, target);
        as.JALR(t0);
    }
}

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
void* Thunks::generateTrampoline(Recompiler& rec, Assembler& as, const char* name) {
    if (!name) {
        return nullptr;
    }

    const Thunk* thunk = nullptr;
    std::string sname = name;
    for (auto& meta : thunk_metadata) {
        if (meta.function_name == sname) {
            thunk = &meta;
            break;
        }
    }

    if (!thunk) {
        return nullptr;
    }

    const std::string& signature = thunk->signature;
    const u64 target = *thunk->host_function;
    const u64 constructor = thunk->constructor_function;
    const u64 destructor = thunk->destructor_function;

    ASSERT(signature.size() > 0);
    ASSERT_MSG(target != 0, "Symbol has nullptr address: %s", name);

    void* trampoline = as.GetCursorPointer();
    char return_type = signature[0];

    ASSERT(signature[1] == '_'); // maybe in the future separating arguments and return type will be useful (it won't)

    // Push return address
    as.ADDI(sp, sp, -8);
    as.SD(ra, 0, sp);

    if (constructor) {
        as.MV(a0, Recompiler::threadStatePointer());
        call(as, constructor);
    }

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
            as.LD(gprarg(current_int_arg), x86offset(current_int_arg), Recompiler::threadStatePointer());
            current_int_arg++;
            ASSERT(current_int_arg <= 6);
            break;
        case 'd':
            as.LWU(gprarg(current_int_arg), x86offset(current_int_arg), Recompiler::threadStatePointer());
            current_int_arg++;
            ASSERT(current_int_arg <= 6);
            break;
        case 'w':
            as.LHU(gprarg(current_int_arg), x86offset(current_int_arg), Recompiler::threadStatePointer());
            current_int_arg++;
            ASSERT(current_int_arg <= 6);
            break;
        case 'b':
            as.LBU(gprarg(current_int_arg), x86offset(current_int_arg), Recompiler::threadStatePointer());
            current_int_arg++;
            ASSERT(current_int_arg <= 6);
            break;
        case 'F':
            as.FLW(fprarg(current_float_arg), offsetof(ThreadState, xmm) + (sizeof(XmmReg) * current_float_arg), Recompiler::threadStatePointer());
            current_float_arg++;
            ASSERT(current_float_arg <= 8);
            break;
        case 'D':
            as.FLD(fprarg(current_float_arg), offsetof(ThreadState, xmm) + (sizeof(XmmReg) * current_float_arg), Recompiler::threadStatePointer());
            current_float_arg++;
            ASSERT(current_float_arg <= 8);
            break;
        default:
            ERROR("Unknown argument type: %c", arguments[i]);
            break;
        }
    }

    call(as, target);

    // Save return value to the correct x86-64 register
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
    case 'F':
        as.FSW(fa0, offsetof(ThreadState, xmm) + 0, Recompiler::threadStatePointer());
        as.SW(x0, offsetof(ThreadState, xmm) + 4, Recompiler::threadStatePointer()); // store 0 into bits 32-63
        for (int i = 1; i < Recompiler::maxVlen() / 64; i++) {
            as.SD(x0, offsetof(ThreadState, xmm) + (i * 8), Recompiler::threadStatePointer());
        }
        break;
    case 'D':
        as.FSD(fa0, offsetof(ThreadState, xmm) + 0, Recompiler::threadStatePointer());
        for (int i = 1; i < Recompiler::maxVlen() / 64; i++) {
            as.SD(x0, offsetof(ThreadState, xmm) + (i * 8), Recompiler::threadStatePointer());
        }
        break;
    case 'v':
        // No return value
        break;
    default:
        ERROR("Unknown return type: %c", return_type);
    }

    if (destructor) {
        as.MV(a0, Recompiler::threadStatePointer());
        call(as, destructor);
    }

    // Pop return address
    as.LD(ra, 0, sp);
    as.ADDI(sp, sp, 8);

    rec.backToDispatcher();

    return trampoline;
}