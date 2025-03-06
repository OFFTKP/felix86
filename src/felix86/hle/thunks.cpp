#include <cmath>
#include <dlfcn.h>
#include "felix86/common/state.hpp"
#include "felix86/hle/thunks.hpp"
#include "felix86/v2/recompiler.hpp"

#include <X11/Xlibint.h>
#include <X11/Xutil.h>

static void* libGLX = nullptr;
static void* libX11 = nullptr;

static std::mutex display_map_mutex;
static std::unordered_map<void*, void*> host_to_guest;
static std::unordered_map<void*, void*> guest_to_host;

Display* felix86_XOpenDisplay(const char* name) {
    ASSERT(name);
    static Display* (*xopendisplay_ptr)(const char*) = (decltype(xopendisplay_ptr))dlsym(libX11, "XOpenDisplay");
    return xopendisplay_ptr(name);
}

int felix86_XFlush(Display* display) {
    if (display == nullptr) {
        WARN("XFlush(nil) called?");
        return 0;
    }

    static int (*xflush_ptr)(Display*) = (decltype(xflush_ptr))dlsym(libX11, "XFlush");
    return xflush_ptr(display);
}

using XVisualInfoPtr = XVisualInfo* (*)(Display*, long, XVisualInfo*, int*);

XVisualInfo* felix86_XGetVisualInfo(Display* display, long vinfo_mask, XVisualInfo* vinfo_template, int* nitems_return) {
    static XVisualInfoPtr xvisualinfo_ptr = (XVisualInfoPtr)dlsym(libX11, "XGetVisualInfo");
    ASSERT(xvisualinfo_ptr);
    return xvisualinfo_ptr(display, vinfo_mask, vinfo_template, nitems_return);
}

void* guestToHostDisplay(void* guest) {
    if (guest == 0) {
        WARN("guestToHostDisplay(nil) called?");
        return nullptr;
    }

    std::lock_guard<std::mutex> lock(display_map_mutex);

    if (guest_to_host.find(guest) != guest_to_host.end()) {
        return guest_to_host[guest];
    }

    _XDisplay* guest_display = (_XDisplay*)guest;
    const char* display_name = guest_display->display_name;
    Display* host_display = felix86_XOpenDisplay(display_name);
    if (host_display) {
        guest_to_host[guest_display] = host_display;
        host_to_guest[host_display] = guest_display;
        WARN("XOpenDisplay creating new mapping %p (guest) -> %p (host)", guest_display, host_display);
        return host_display;
    } else {
        WARN("Failed to XOpenDisplay: %s", display_name);
        return nullptr;
    }
}

void* hostToGuestDisplay(void* host) {
    if (host == 0) {
        WARN("hostToGuestDisplay(nil) called?\n");
        return nullptr;
    }

    std::lock_guard<std::mutex> lock(display_map_mutex);

    if (host_to_guest.find(host) != host_to_guest.end()) {
        return host_to_guest[host];
    } else {
        WARN("hostToGuestDisplay couldn't find guest display matching %p?", host);
        return nullptr;
    }
}

// NOTE: due to RISC-V ABI, returning void* void* like this is perfect, as they will be returned
// directly into a0 and a1, which is exactly where we wanted these pointers
std::pair<void*, void*> getHostVisualInfo(Display* host_display, XVisualInfo* guest) {
    if (!host_display) {
        WARN("getHostVisualInfo with nil display?");
        return {host_display, nullptr};
    }

    XVisualInfo v;
    v.screen = guest->screen;
    v.visualid = guest->visualid;

    int c;
    XVisualInfo* info = felix86_XGetVisualInfo(host_display, VisualScreenMask | VisualIDMask, &v, &c);

    if (c >= 1 && info != nullptr) {
        return {host_display, info};
    } else {
        WARN("getHostVisualInfo returned null?");
        return {host_display, nullptr};
    }
}

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

// Actual host function pointers
// u64's as we don't really care about the type here,
// these are just pointers for the assembler to create trampolines
namespace thunkptr {
#define X(libname, name, ...) u64 name = 0;
#include "gl_thunks.inc"  // <- these are loaded on felix86_guest_glXGetProcAddress, as they are requested
#include "glx_thunks.inc" // <- these are loaded on Thunks::Initialize
#undef X
} // namespace thunkptr

// TODO: obviously this sort of argument fiddling is a bad and rushed implementation. Make it better!
enum IHateThisEnumProperties {
    NoneThankfully = 0,

    // This function takes a Display* as argument 0, this needs to be replaced with a host Display*
    // due to struct internal differences. The alternative would be thunking X11 which is hell.
    Arg0GuestToHostDisplay = 1 << 0,

    // This gets a host XVisualInfo* from the data in the guest XVisualInfo*
    Arg1GetHostVisualInfo = 1 << 1,

    // Run XFlush on return from thunked function
    FlushOnReturn = 1 << 2,

    // Change the return value from a host display to a guest display
    ReturnsDisplay = 1 << 3,
};

struct Thunk {
    const char* lib_name;
    const char* function_name;
    const char* signature;
    u64* host_function = 0;
    u64 properties = NoneThankfully;
};

#define X(lib_name, function_name, signature, properties) {lib_name, #function_name, #signature, &thunkptr::function_name, properties},

static Thunk thunk_metadata[] = {
#include "glx_thunks.inc"
};

#undef X

constexpr unsigned long hashstr(const std::string_view& str, int h = 0) {
    return !str[h] ? 55 : (hashstr(str, h + 1) * 33) + (unsigned char)(str[h]);
}

void* felix86_host_glXGetProcAddress(const char* name) {
    static void* (*getprocaddress)(const char*) = (void* (*)(const char*))dlsym(libGLX, "glXGetProcAddress");
    return getprocaddress(name);
}

void* felix86_guest_glXGetProcAddress(const char* name) {
    printf("glXGetProcAddress: %s\n", name);
    static void* actual = dlsym(libGLX, "glXGetProcAddress");
    ASSERT_MSG(actual, "Couldn't find glXGetProcAddress?");

    // Get the host pointer, return a pointer from libgl_guest_ptrs.hpp for the recompiler to generate a trampoline
    // when it is actually called.
    switch (hashstr(name)) {
#define X(libname, function, ...)                                                                                                                    \
    case hashstr(function):                                                                                                                          \
        thunkptr::function = felix86_host_glXGetProcAddress(name);                                                                                   \
        return felix86_guest_##function;

    default: {
        ERROR("felix86_glXGetProcAddress could not find %s in thunked functions", name);
        return nullptr;
    }
    }
#undef X
}

std::filesystem::path find_lib(const std::filesystem::path& lib) {
#define CHECK(dir)                                                                                                                                   \
    if (std::filesystem::exists(dir / lib)) {                                                                                                        \
        return dir / lib;                                                                                                                            \
    }

    CHECK("/felix86/lib")
    CHECK("/felix86/lib/riscv64-linux-gnu")
    // Is there any more we need to check?

    return "";
}

// Load the host function pointers in the thunkptr namespace with pointers using dlopen + dlsym
void Thunks::initialize() {
    thunkptr::glXGetProcAddress = (u64)felix86_guest_glXGetProcAddress;
    thunkptr::glXGetProcAddressARB = (u64)felix86_guest_glXGetProcAddress;

    constexpr const char* glx_name = "libGLX.so";
    const std::filesystem::path glx_path = find_lib(glx_name);

    const char* ld_lib_path = getenv("LD_LIBRARY_PATH");
    const char* ld_lib_expected = "/felix86/lib:/felix86/lib/riscv64-linux-gnu";
    if (!ld_lib_path || std::string(ld_lib_path) != ld_lib_expected) {
        ERROR("When initializing thunks, LD_LIBRARY_PATH had an unexpected value (not %s), so dlopen would not find the libraries", ld_lib_expected);
    }

    if (glx_path.empty()) {
        ERROR("I couldn't find %s in /felix86/lib, is it mounted correctly?", glx_name);
    }

    libGLX = dlopen(glx_path.c_str(), RTLD_LAZY);
    if (!libGLX) {
        ERROR("I couldn't open libGLX at %s, error: %s", glx_path.c_str(), dlerror());
    }

    constexpr const char* x11_name = "libX11.so";
    const std::filesystem::path x11_path = find_lib(x11_name);
    if (x11_path.empty()) {
        ERROR("I couldn't find %s in /felix86/lib, is it mounted correctly?", x11_name);
    }

    libX11 = dlopen(x11_path.c_str(), RTLD_LAZY);
    if (!libX11) {
        ERROR("I couldn't open libX11 at %s, error: %s", x11_path.c_str(), dlerror());
    }

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
    const u64 properties = thunk->properties;

    ASSERT(signature.size() > 0);
    ASSERT_MSG(target != 0, "Symbol has nullptr address: %s", name);

    void* trampoline = as.GetCursorPointer();
    char return_type = signature[0];

    ASSERT(signature[1] == '_'); // maybe in the future separating arguments and return type will be useful (it won't)

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

            if (current_int_arg == 0 && properties & Arg0GuestToHostDisplay) {
                ASSERT(i == 0);
                // With a0 loaded, call guestToHostDisplay, which returns a pointer in a0.
                // No other registers are loaded at this point (since i == 0, and every function that has Display*
                // it's always the first argument thankfully) so we don't need to save/restore any regs
                call(as, (u64)guestToHostDisplay);

                // Put the host display in s0, a saved register, because we'll need it later on XFlush
                // Whenever FlushOnReturn is present, Arg0GuestToHostDisplay is also guaranteed to be present
                as.MV(s0, a0);
            }

            if (current_int_arg == 1 && properties & Arg1GetHostVisualInfo) {
                ASSERT(i == 1);
                // Luckily, XVisualInfo is always the second argument
                // a0 has XDisplay* and a1 has XVisualInfo (guest) at this point
                // Calling this function will return our XDisplay* and a XVisualInfo* (host) in a1
                call(as, (u64)getHostVisualInfo);
                // a0 and a1 are returned fine
            }

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

    if (properties & ReturnsDisplay) {
        // Transform the return value in a0 to a guest Display*
        call(as, (u64)hostToGuestDisplay);
        // Returns in a0, store it to RAX as usual after this
    }

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

    if (properties & FlushOnReturn) {
        // We need to call XFlush
        ASSERT(properties & Arg0GuestToHostDisplay); // host display exists in s0
        as.MV(a0, s0);
        call(as, (u64)felix86_XFlush);
    }

    return trampoline;
}