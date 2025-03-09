#include <cmath>
#include <dlfcn.h>
#include "felix86/common/state.hpp"
#include "felix86/hle/thunks.hpp"
#include "felix86/v2/recompiler.hpp"

#include <X11/Xlibint.h>
#include <X11/Xutil.h>

static void* libGLX = nullptr;
static void* libX11 = nullptr;
static void* libEGL = nullptr;

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

Display* guestToHostDisplay(Display* guest) {
    if (guest == 0) {
        WARN("guestToHostDisplay(nil) called?");
        return nullptr;
    }

    std::lock_guard<std::mutex> lock(display_map_mutex);

    if (guest_to_host.find(guest) != guest_to_host.end()) {
        return (Display*)guest_to_host[guest];
    }

    _XDisplay* guest_display = (_XDisplay*)guest;
    const char* display_name = guest_display->display_name;
    Display* host_display = felix86_XOpenDisplay(display_name);
    if (host_display) {
        guest_to_host[guest_display] = host_display;
        host_to_guest[host_display] = guest_display;
        LOG("XOpenDisplay creating new mapping %p (guest) -> %p (host)", guest_display, host_display);
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
        LOG("getHostVisualInfo(%p, %p) has created an XVisualInfo: %p", host_display, guest, info);
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
#include "egl_thunks.inc" // <- these are loaded on Thunks::Initialize
#include "gl_thunks.inc"  // <- these are loaded on felix86_guest_glXGetProcAddress, as they are requested
#include "glx_thunks.inc" // <- these are loaded on Thunks::Initialize
#undef X
} // namespace thunkptr

struct Thunk {
    const char* lib_name;
    const char* function_name;
    const char* signature;
    u64* host_function = 0;
};

#define X(lib_name, function_name, signature) {lib_name, #function_name, #signature, &thunkptr::function_name},

static Thunk thunk_metadata[] = {
#include "egl_thunks.inc"
#include "gl_thunks.inc"
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

void* felix86_guest_GetProcAddressCommon(void* (*getProcAddress)(const char* name), const char* name) {
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

void* felix86_guest_glXGetProcAddress(const char* name) {
    printf("glXGetProcAddress: %s\n", name);
    static auto actual = (void* (*)(const char*))dlsym(libGLX, "glXGetProcAddress");
    ASSERT_MSG(actual, "Couldn't find glXGetProcAddress?");
    return felix86_guest_GetProcAddressCommon(actual, name);
}

void* felix86_guest_eglGetProcAddress(const char* name) {
    printf("eglGetProcAddress: %s\n", name);
    static auto actual = (void* (*)(const char*))dlsym(libEGL, "eglGetProcAddress");
    ASSERT_MSG(actual, "Couldn't find eglGetProcAddress?");
    return felix86_guest_GetProcAddressCommon(actual, name);
}

// We don't care about the internals
using GLXContext = void*;
using GLXDrawable = void*;
using GLXPixmap = void*;
using GLXFBConfig = void*;
using GLXWindow = void*;
using GLXPbuffer = void*;

// TODO: Implement the XVisualInfo back and forth stuff
XVisualInfo* felix86_guest_glXChooseVisual(Display* dpy, int screen, int* attribList) {
    ERROR("TODO: VisualInfo mapping stuff");
    ERROR("Don't forget to add XFlush");
    static auto host_glXChooseVisual = (decltype(&felix86_guest_glXChooseVisual))dlsym(libGLX, "glXChooseVisual");
    return host_glXChooseVisual(guestToHostDisplay(dpy), screen, attribList);
}

GLXContext felix86_guest_glXCreateContext(Display* dpy, XVisualInfo* vis, GLXContext shareList, Bool direct) {
    ERROR("TODO: VisualInfo mapping stuff");
    static auto host_glXCreateContext = (decltype(&felix86_guest_glXCreateContext))dlsym(libGLX, "glXCreateContext");
    return host_glXCreateContext(guestToHostDisplay(dpy), vis, shareList, direct);
}

void felix86_guest_glXDestroyContext(Display* dpy, GLXContext ctx) {
    static auto host_glXDestroyContext = (decltype(&felix86_guest_glXDestroyContext))dlsym(libGLX, "glXDestroyContext");
    return host_glXDestroyContext(guestToHostDisplay(dpy), ctx);
}

Bool felix86_guest_glXMakeCurrent(Display* dpy, GLXDrawable drawable, GLXContext ctx) {
    static auto host_glXMakeCurrent = (decltype(&felix86_guest_glXMakeCurrent))dlsym(libGLX, "glXMakeCurrent");
    return host_glXMakeCurrent(guestToHostDisplay(dpy), drawable, ctx);
}

void felix86_guest_glXCopyContext(Display* dpy, GLXContext src, GLXContext dst, unsigned long mask) {
    static auto host_glXCopyContext = (decltype(&felix86_guest_glXCopyContext))dlsym(libGLX, "glXCopyContext");
    return host_glXCopyContext(guestToHostDisplay(dpy), src, dst, mask);
}

void felix86_guest_glXSwapBuffers(Display* dpy, GLXDrawable drawable) {
    static auto host_glXSwapBuffers = (decltype(&felix86_guest_glXSwapBuffers))dlsym(libGLX, "glXSwapBuffers");
    return host_glXSwapBuffers(guestToHostDisplay(dpy), drawable);
}

GLXPixmap felix86_guest_glXCreateGLXPixmap(Display* dpy, XVisualInfo* visual, Pixmap pixmap) {
    static auto host_glXCreateGLXPixmap = (decltype(&felix86_guest_glXCreateGLXPixmap))dlsym(libGLX, "glXCreateGLXPixmap");
    return host_glXCreateGLXPixmap(guestToHostDisplay(dpy), visual, pixmap);
}

void felix86_guest_glXDestroyGLXPixmap(Display* dpy, GLXPixmap pixmap) {
    static auto host_glXDestroyGLXPixmap = (decltype(&felix86_guest_glXDestroyGLXPixmap))dlsym(libGLX, "glXDestroyGLXPixmap");
    return host_glXDestroyGLXPixmap(guestToHostDisplay(dpy), pixmap);
}

Bool felix86_guest_glXQueryExtension(Display* dpy, int* errorb, int* event) {
    static auto host_glXQueryExtension = (decltype(&felix86_guest_glXQueryExtension))dlsym(libGLX, "glXQueryExtension");
    return host_glXQueryExtension(guestToHostDisplay(dpy), errorb, event);
}

Bool felix86_guest_glXQueryVersion(Display* dpy, int* maj, int* min) {
    static auto host_glXQueryVersion = (decltype(&felix86_guest_glXQueryVersion))dlsym(libGLX, "glXQueryVersion");
    return host_glXQueryVersion(guestToHostDisplay(dpy), maj, min);
}

Bool felix86_guest_glXIsDirect(Display* dpy, GLXContext ctx) {
    static auto host_glXIsDirect = (decltype(&felix86_guest_glXIsDirect))dlsym(libGLX, "glXIsDirect");
    return host_glXIsDirect(guestToHostDisplay(dpy), ctx);
}

int felix86_guest_glXGetConfig(Display* dpy, XVisualInfo* visual, int attrib, int* value) {
    static auto host_glXGetConfig = (decltype(&felix86_guest_glXGetConfig))dlsym(libGLX, "glXGetConfig");
    return host_glXGetConfig(guestToHostDisplay(dpy), visual, attrib, value);
}

const char* felix86_guest_glXQueryExtensionsString(Display* dpy, int screen) {
    static auto host_glXQueryExtensionsString = (decltype(&felix86_guest_glXQueryExtensionsString))dlsym(libGLX, "glXQueryExtensionsString");
    return host_glXQueryExtensionsString(guestToHostDisplay(dpy), screen);
}

const char* felix86_guest_glXQueryServerString(Display* dpy, int screen, int name) {
    static auto host_glXQueryServerString = (decltype(&felix86_guest_glXQueryServerString))dlsym(libGLX, "glXQueryServerString");
    return host_glXQueryServerString(guestToHostDisplay(dpy), screen, name);
}

const char* felix86_guest_glXGetClientString(Display* dpy, int name) {
    static auto host_glXGetClientString = (decltype(&felix86_guest_glXGetClientString))dlsym(libGLX, "glXGetClientString");
    return host_glXGetClientString(guestToHostDisplay(dpy), name);
}

GLXFBConfig* felix86_guest_glXChooseFBConfig(Display* dpy, int screen, const int* attribList, int* nitems) {
    static auto host_glXChooseFBConfig = (decltype(&felix86_guest_glXChooseFBConfig))dlsym(libGLX, "glXChooseFBConfig");
    return host_glXChooseFBConfig(guestToHostDisplay(dpy), screen, attribList, nitems);
}

int felix86_guest_glXGetFBConfigAttrib(Display* dpy, GLXFBConfig config, int attribute, int* value) {
    static auto host_glXGetFBConfigAttrib = (decltype(&felix86_guest_glXGetFBConfigAttrib))dlsym(libGLX, "glXGetFBConfigAttrib");
    return host_glXGetFBConfigAttrib(guestToHostDisplay(dpy), config, attribute, value);
}

GLXFBConfig* felix86_guest_glXGetFBConfigs(Display* dpy, int screen, int* nelements) {
    static auto host_glXGetFBConfigs = (decltype(&felix86_guest_glXGetFBConfigs))dlsym(libGLX, "glXGetFBConfigs");
    return host_glXGetFBConfigs(guestToHostDisplay(dpy), screen, nelements);
}

XVisualInfo* felix86_guest_glXGetVisualFromFBConfig(Display* dpy, GLXFBConfig config) {
    static auto host_glXGetVisualFromFBConfig = (decltype(&felix86_guest_glXGetVisualFromFBConfig))dlsym(libGLX, "glXGetVisualFromFBConfig");
    return host_glXGetVisualFromFBConfig(guestToHostDisplay(dpy), config);
}

GLXWindow felix86_guest_glXCreateWindow(Display* dpy, GLXFBConfig config, Window win, const int* attribList) {
    static auto host_glXCreateWindow = (decltype(&felix86_guest_glXCreateWindow))dlsym(libGLX, "glXCreateWindow");
    return host_glXCreateWindow(guestToHostDisplay(dpy), config, win, attribList);
}

void felix86_guest_glXDestroyWindow(Display* dpy, GLXWindow window) {
    static auto host_glXDestroyWindow = (decltype(&felix86_guest_glXDestroyWindow))dlsym(libGLX, "glXDestroyWindow");
    return host_glXDestroyWindow(guestToHostDisplay(dpy), window);
}

GLXPixmap felix86_guest_glXCreatePixmap(Display* dpy, GLXFBConfig config, Pixmap pixmap, const int* attribList) {
    static auto host_glXCreatePixmap = (decltype(&felix86_guest_glXCreatePixmap))dlsym(libGLX, "glXCreatePixmap");
    return host_glXCreatePixmap(guestToHostDisplay(dpy), config, pixmap, attribList);
}

void felix86_guest_glXDestroyPixmap(Display* dpy, GLXPixmap pixmap) {
    static auto host_glXDestroyPixmap = (decltype(&felix86_guest_glXDestroyPixmap))dlsym(libGLX, "glXDestroyPixmap");
    return host_glXDestroyPixmap(guestToHostDisplay(dpy), pixmap);
}

GLXPbuffer felix86_guest_glXCreatePbuffer(Display* dpy, GLXFBConfig config, const int* attribList) {
    static auto host_glXCreatePbuffer = (decltype(&felix86_guest_glXCreatePbuffer))dlsym(libGLX, "glXCreatePbuffer");
    return host_glXCreatePbuffer(guestToHostDisplay(dpy), config, attribList);
}

void felix86_guest_glXDestroyPbuffer(Display* dpy, GLXPbuffer pbuf) {
    static auto host_glXDestroyPbuffer = (decltype(&felix86_guest_glXDestroyPbuffer))dlsym(libGLX, "glXDestroyPbuffer");
    return host_glXDestroyPbuffer(guestToHostDisplay(dpy), pbuf);
}

void felix86_guest_glXQueryDrawable(Display* dpy, GLXDrawable draw, int attribute, unsigned int* value) {
    static auto host_glXQueryDrawable = (decltype(&felix86_guest_glXQueryDrawable))dlsym(libGLX, "glXQueryDrawable");
    return host_glXQueryDrawable(guestToHostDisplay(dpy), draw, attribute, value);
}

GLXContext felix86_guest_glXCreateNewContext(Display* dpy, GLXFBConfig config, int renderType, GLXContext shareList, Bool direct) {
    static auto host_glXCreateNewContext = (decltype(&felix86_guest_glXCreateNewContext))dlsym(libGLX, "glXCreateNewContext");
    return host_glXCreateNewContext(guestToHostDisplay(dpy), config, renderType, shareList, direct);
}

Bool felix86_guest_glXMakeContextCurrent(Display* dpy, GLXDrawable draw, GLXDrawable read, GLXContext ctx) {
    static auto host_glXMakeContextCurrent = (decltype(&felix86_guest_glXMakeContextCurrent))dlsym(libGLX, "glXMakeContextCurrent");
    return host_glXMakeContextCurrent(guestToHostDisplay(dpy), draw, read, ctx);
}

int felix86_guest_glXQueryContext(Display* dpy, GLXContext ctx, int attribute, int* value) {
    static auto host_glXQueryContext = (decltype(&felix86_guest_glXQueryContext))dlsym(libGLX, "glXQueryContext");
    return host_glXQueryContext(guestToHostDisplay(dpy), ctx, attribute, value);
}

void felix86_guest_glXSelectEvent(Display* dpy, GLXDrawable drawable, unsigned long mask) {
    static auto host_glXSelectEvent = (decltype(&felix86_guest_glXSelectEvent))dlsym(libGLX, "glXSelectEvent");
    return host_glXSelectEvent(guestToHostDisplay(dpy), drawable, mask);
}

void felix86_guest_glXGetSelectedEvent(Display* dpy, GLXDrawable drawable, unsigned long* mask) {
    static auto host_glXGetSelectedEvent = (decltype(&felix86_guest_glXGetSelectedEvent))dlsym(libGLX, "glXGetSelectedEvent");
    return host_glXGetSelectedEvent(guestToHostDisplay(dpy), drawable, mask);
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
    thunkptr::eglGetProcAddress = (u64)felix86_guest_eglGetProcAddress;

    // These need to be handled specially to map Display* and a couple other things
    // For these we don't dlsym
    thunkptr::glXChooseVisual = (u64)felix86_guest_glXChooseVisual;
    thunkptr::glXCreateContext = (u64)felix86_guest_glXCreateContext;
    thunkptr::glXDestroyContext = (u64)felix86_guest_glXDestroyContext;
    thunkptr::glXMakeCurrent = (u64)felix86_guest_glXMakeCurrent;
    thunkptr::glXCopyContext = (u64)felix86_guest_glXCopyContext;
    thunkptr::glXSwapBuffers = (u64)felix86_guest_glXSwapBuffers;
    thunkptr::glXCreateGLXPixmap = (u64)felix86_guest_glXCreateGLXPixmap;
    thunkptr::glXDestroyGLXPixmap = (u64)felix86_guest_glXDestroyGLXPixmap;
    thunkptr::glXQueryExtension = (u64)felix86_guest_glXQueryExtension;
    thunkptr::glXQueryVersion = (u64)felix86_guest_glXQueryVersion;
    thunkptr::glXIsDirect = (u64)felix86_guest_glXIsDirect;
    thunkptr::glXGetConfig = (u64)felix86_guest_glXGetConfig;
    thunkptr::glXQueryExtensionsString = (u64)felix86_guest_glXQueryExtensionsString;
    thunkptr::glXQueryServerString = (u64)felix86_guest_glXQueryServerString;
    thunkptr::glXGetClientString = (u64)felix86_guest_glXGetClientString;
    thunkptr::glXChooseFBConfig = (u64)felix86_guest_glXChooseFBConfig;
    thunkptr::glXGetFBConfigAttrib = (u64)felix86_guest_glXGetFBConfigAttrib;
    thunkptr::glXGetFBConfigs = (u64)felix86_guest_glXGetFBConfigs;
    thunkptr::glXGetVisualFromFBConfig = (u64)felix86_guest_glXGetVisualFromFBConfig;
    thunkptr::glXCreateWindow = (u64)felix86_guest_glXCreateWindow;
    thunkptr::glXDestroyWindow = (u64)felix86_guest_glXDestroyWindow;
    thunkptr::glXCreatePixmap = (u64)felix86_guest_glXCreatePixmap;
    thunkptr::glXDestroyPixmap = (u64)felix86_guest_glXDestroyPixmap;
    thunkptr::glXCreatePbuffer = (u64)felix86_guest_glXCreatePbuffer;
    thunkptr::glXDestroyPbuffer = (u64)felix86_guest_glXDestroyPbuffer;
    thunkptr::glXQueryDrawable = (u64)felix86_guest_glXQueryDrawable;
    thunkptr::glXCreateNewContext = (u64)felix86_guest_glXCreateNewContext;
    thunkptr::glXMakeContextCurrent = (u64)felix86_guest_glXMakeContextCurrent;
    thunkptr::glXQueryContext = (u64)felix86_guest_glXQueryContext;
    thunkptr::glXSelectEvent = (u64)felix86_guest_glXSelectEvent;
    thunkptr::glXGetSelectedEvent = (u64)felix86_guest_glXGetSelectedEvent;

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

    constexpr const char* egl_name = "libEGL.so.1";
    const std::filesystem::path egl_path = find_lib(egl_name);
    if (egl_path.empty()) {
        ERROR("I couldn't find %s in /felix86/lib, is it mounted correctly?", egl_name);
    }

    libEGL = dlopen(egl_path.c_str(), RTLD_LAZY);
    if (!libEGL) {
        ERROR("I couldn't open libEGL at %s, error: %s", egl_path.c_str(), dlerror());
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
#define X(libname, name, ...)                                                                                                                        \
    if (thunkptr::name == 0) {                                                                                                                       \
        thunkptr::name = (u64)dlsym(libEGL, #name);                                                                                                  \
        if (thunkptr::name == 0) {                                                                                                                   \
            ERROR("Failed to find symbol %s in %s, error: %s", #name, "libEGL.so", dlerror());                                                       \
        }                                                                                                                                            \
    }
#include "egl_thunks.inc"
#undef X
    // gl_thunks are loaded from the getprocaddress functions
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

    Recompiler::call(as, target);

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

    return trampoline;
}