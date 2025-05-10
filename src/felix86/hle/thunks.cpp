#include "felix86/hle/thunks.hpp"

// Thunks need libX11
#ifndef BUILD_THUNKING
void Thunks::initialize() {}

void* Thunks::generateTrampoline(Recompiler&, const char*) {
    return nullptr;
}

void Thunks::runConstructor(const char*, GuestPointers*) {}

#else
#include <cmath>
#include <dlfcn.h>
#include "felix86/common/state.hpp"
#include "felix86/hle/abi.hpp"
#include "felix86/hle/libgl_guest_ptrs.hpp"
#include "felix86/v2/recompiler.hpp"

#include <X11/Xlibint.h>
#include <X11/Xutil.h>

static void* libGLX = nullptr;
static void* libX11 = nullptr;
static void* libEGL = nullptr;
static void* libvulkan = nullptr;

using XGetVisualInfoType = decltype(&XGetVisualInfo);
using XSyncType = decltype(&XSync);

static XGetVisualInfoType felix86__x86_64__XGetVisualInfo = nullptr;
static XSyncType felix86__x86_64__XSync = nullptr;

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

XVisualInfo* getHostVisualInfo(Display* host_display, XVisualInfo* guest) {
    if (!host_display) {
        WARN("getHostVisualInfo with nil display?");
        return nullptr;
    }

    XVisualInfo v;
    v.screen = guest->screen;
    v.visualid = guest->visualid;

    int c;
    XVisualInfo* info = felix86_XGetVisualInfo(host_display, VisualScreenMask | VisualIDMask, &v, &c);

    if (c >= 1 && info != nullptr) {
        PLAIN("getHostVisualInfo(%p, %p) has created an XVisualInfo: %p", host_display, guest, info);
        return info;
    } else {
        WARN("getHostVisualInfo returned null?");
        return nullptr;
    }
}

// Actual host function pointers
// u64's as we don't really care about the type here,
// these are just pointers for the assembler to create trampolines
namespace thunkptr {
#define X(libname, name, ...) u64 name = 0;
#include "egl_thunks.inc"    // <- these are loaded on Thunks::Initialize
#include "gl_thunks.inc"     // <- these are loaded on felix86_thunk_glXGetProcAddress, as they are requested
#include "glx_thunks.inc"    // <- these are loaded on Thunks::Initialize
#include "vulkan_thunks.inc" // <- these are loaded on Thunks::Initialize
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
#include "vulkan_thunks.inc"
};

#undef X

// We don't care about the internals
using GLXContext = void*;
using GLXDrawable = void*;
using GLXPixmap = void*;
using GLXFBConfig = void*;
using GLXWindow = void*;
using GLXPbuffer = void*;
using VkInstance = void*;

constexpr unsigned long hashstr(const char* str, int h = 0) {
    return !str[h] ? 55 : (hashstr(str, h + 1) * 33) + (unsigned char)(str[h]);
}

void* felix86_thunk_GetProcAddressCommon(void* (*getProcAddress)(const char* name), const char* name) {
    // Get the host pointer, return a pointer from libgl_guest_ptrs.hpp for the recompiler to generate a trampoline
    // when it is actually called.
    // TODO: bad, do what we do for vulkan proc address, remove libgl_guest_ptrs
    switch (hashstr(name)) {
#define X(libname, function, ...)                                                                                                                    \
    case hashstr(#function):                                                                                                                         \
        thunkptr::function = (u64)getProcAddress(name);                                                                                              \
        return (void*)felix86_guest_##function;
#include "gl_thunks.inc"
    default: {
        VERBOSE("felix86_thunk_GetProcAddressCommon could not find %s in thunked functions", name);
        return nullptr;
    }
    }
#undef X
}

void* felix86_thunk_glXGetProcAddress(const char* name) {
    PLAIN("glXGetProcAddress: %s", name);
    static auto actual = (void* (*)(const char*))dlsym(libGLX, "glXGetProcAddress");
    ASSERT_MSG(actual, "Couldn't find glXGetProcAddress?");
    return felix86_thunk_GetProcAddressCommon(actual, name);
}

void* felix86_thunk_eglGetProcAddress(const char* name) {
    PLAIN("eglGetProcAddress: %s", name);
    static auto actual = (void* (*)(const char*))dlsym(libEGL, "eglGetProcAddress");
    ASSERT_MSG(actual, "Couldn't find eglGetProcAddress?");
    return felix86_thunk_GetProcAddressCommon(actual, name);
}

void* generate_guest_pointer(const char* name, u64 host_ptr) {
    // We can't put this code in code cache, because it needs to outlive potential code cache clears
    // So, we are going to leak memory! In the future we should cache these...
    // 0f 01 39 ; invlpg [rcx] ; see handlers.cpp -- invlpg (magic instruction that generates jump to host code)
    // 00 00 00 00 00 00 00 00 ; pointer we jump to
    // ... 00 ; signature const char*
    // c3 ; ret
    const Thunk* thunk = nullptr;
    std::string sname = name;
    for (auto& meta : thunk_metadata) { // TODO: speed it up? only search vulkan
        if (meta.function_name == sname) {
            thunk = &meta;
            break;
        }
    }

    if (!thunk) {
        WARN("Couldn't find signature for %s", name);
        return nullptr;
    }

    const char* signature = thunk->signature;
    size_t sigsize = strlen(signature);
    u8* memory = new u8[3 + 8 + sigsize + 1 + 1](); // zeroed out for null byte
    memory[0] = 0x0f;
    memory[1] = 0x01;
    memory[2] = 0x39;
    memcpy(&memory[3], &host_ptr, sizeof(u64));
    memcpy(&memory[3 + 8], signature, sigsize);
    memory[3 + 8 + sigsize + 1] = 0xc1;
    printf("mem: %p\n", memory);
    return memory;
}

// TODO: Kinda wasteful to code cache if this gets called more than once per name
void* felix86_thunk_vkGetInstanceProcAddr(VkInstance instance, const char* name) {
    PLAIN("vkGetInstanceProcAddr: %s", name);
    static auto actual = (void* (*)(VkInstance, const char*))dlsym(libvulkan, "vkGetInstanceProcAddr");
    void* ptr = actual(instance, name);
    if (ptr) {
        // We can't return `ptr` here because it's a host pointer
        // But we also can't return our own thunked pointers, we need to return the one
        // getprocaddr returned. So we generate an invlpg [rcx] to create a proper guest pointer that will jump to our pointer
        return generate_guest_pointer(name, (u64)ptr);
    } else {
        WARN("Host vkGetInstanceProcAddr returned null for %s", name);
        return nullptr;
    }
}

void* felix86_thunk_vkGetDeviceProcAddr(VkInstance instance, const char* name) {
    PLAIN("vkGetDeviceProcAddr: %s", name);
    static auto actual = (void* (*)(VkInstance, const char*))dlsym(libvulkan, "vkGetDeviceProcAddr");
    void* ptr = actual(instance, name);
    if (ptr) {
        return generate_guest_pointer(name, (u64)ptr);
    } else {
        WARN("Host vkGetDeviceProcAddr returned null for %s", name);
        return nullptr;
    }
}

#define PRINTME PLAIN("Calling thunked %s", __PRETTY_FUNCTION__)

XVisualInfo* felix86_thunk_glXChooseVisual(Display* dpy, int screen, int* attribList) {
    PRINTME;
    static auto host_glXChooseVisual = (decltype(&felix86_thunk_glXChooseVisual))dlsym(libGLX, "glXChooseVisual");
    return host_glXChooseVisual(guestToHostDisplay(dpy), screen, attribList);
}

GLXContext felix86_thunk_glXCreateContext(Display* dpy, XVisualInfo* visual, GLXContext shareList, Bool direct) {
    PRINTME;
    static auto host_glXCreateContext = (decltype(&felix86_thunk_glXCreateContext))dlsym(libGLX, "glXCreateContext");
    Display* host_dpy = guestToHostDisplay(dpy);
    return host_glXCreateContext(host_dpy, getHostVisualInfo(host_dpy, visual), shareList, direct);
}

void felix86_thunk_glXDestroyContext(Display* dpy, GLXContext ctx) {
    PRINTME;
    static auto host_glXDestroyContext = (decltype(&felix86_thunk_glXDestroyContext))dlsym(libGLX, "glXDestroyContext");
    return host_glXDestroyContext(guestToHostDisplay(dpy), ctx);
}

Bool felix86_thunk_glXMakeCurrent(Display* dpy, GLXDrawable drawable, GLXContext ctx) {
    PRINTME;
    static auto host_glXMakeCurrent = (decltype(&felix86_thunk_glXMakeCurrent))dlsym(libGLX, "glXMakeCurrent");
    return host_glXMakeCurrent(guestToHostDisplay(dpy), drawable, ctx);
}

void felix86_thunk_glXCopyContext(Display* dpy, GLXContext src, GLXContext dst, unsigned long mask) {
    PRINTME;
    static auto host_glXCopyContext = (decltype(&felix86_thunk_glXCopyContext))dlsym(libGLX, "glXCopyContext");
    return host_glXCopyContext(guestToHostDisplay(dpy), src, dst, mask);
}

void felix86_thunk_glXSwapBuffers(Display* dpy, GLXDrawable drawable) {
    PRINTME;
    static auto host_glXSwapBuffers = (decltype(&felix86_thunk_glXSwapBuffers))dlsym(libGLX, "glXSwapBuffers");
    return host_glXSwapBuffers(guestToHostDisplay(dpy), drawable);
}

GLXPixmap felix86_thunk_glXCreateGLXPixmap(Display* dpy, XVisualInfo* visual, Pixmap pixmap) {
    PRINTME;
    static auto host_glXCreateGLXPixmap = (decltype(&felix86_thunk_glXCreateGLXPixmap))dlsym(libGLX, "glXCreateGLXPixmap");
    Display* host_dpy = guestToHostDisplay(dpy);
    return host_glXCreateGLXPixmap(host_dpy, getHostVisualInfo(host_dpy, visual), pixmap);
}

void felix86_thunk_glXDestroyGLXPixmap(Display* dpy, GLXPixmap pixmap) {
    PRINTME;
    static auto host_glXDestroyGLXPixmap = (decltype(&felix86_thunk_glXDestroyGLXPixmap))dlsym(libGLX, "glXDestroyGLXPixmap");
    return host_glXDestroyGLXPixmap(guestToHostDisplay(dpy), pixmap);
}

Bool felix86_thunk_glXQueryExtension(Display* dpy, int* errorb, int* event) {
    PRINTME;
    static auto host_glXQueryExtension = (decltype(&felix86_thunk_glXQueryExtension))dlsym(libGLX, "glXQueryExtension");
    return host_glXQueryExtension(guestToHostDisplay(dpy), errorb, event);
}

Bool felix86_thunk_glXQueryVersion(Display* dpy, int* maj, int* min) {
    PRINTME;
    static auto host_glXQueryVersion = (decltype(&felix86_thunk_glXQueryVersion))dlsym(libGLX, "glXQueryVersion");
    return host_glXQueryVersion(guestToHostDisplay(dpy), maj, min);
}

Bool felix86_thunk_glXIsDirect(Display* dpy, GLXContext ctx) {
    PRINTME;
    static auto host_glXIsDirect = (decltype(&felix86_thunk_glXIsDirect))dlsym(libGLX, "glXIsDirect");
    return host_glXIsDirect(guestToHostDisplay(dpy), ctx);
}

int felix86_thunk_glXGetConfig(Display* dpy, XVisualInfo* visual, int attrib, int* value) {
    PRINTME;
    static auto host_glXGetConfig = (decltype(&felix86_thunk_glXGetConfig))dlsym(libGLX, "glXGetConfig");
    Display* host_dpy = guestToHostDisplay(dpy);
    return host_glXGetConfig(host_dpy, getHostVisualInfo(host_dpy, visual), attrib, value);
}

const char* felix86_thunk_glXQueryExtensionsString(Display* dpy, int screen) {
    PRINTME;
    static auto host_glXQueryExtensionsString = (decltype(&felix86_thunk_glXQueryExtensionsString))dlsym(libGLX, "glXQueryExtensionsString");
    return host_glXQueryExtensionsString(guestToHostDisplay(dpy), screen);
}

const char* felix86_thunk_glXQueryServerString(Display* dpy, int screen, int name) {
    PRINTME;
    static auto host_glXQueryServerString = (decltype(&felix86_thunk_glXQueryServerString))dlsym(libGLX, "glXQueryServerString");
    return host_glXQueryServerString(guestToHostDisplay(dpy), screen, name);
}

const char* felix86_thunk_glXGetClientString(Display* dpy, int name) {
    PRINTME;
    static auto host_glXGetClientString = (decltype(&felix86_thunk_glXGetClientString))dlsym(libGLX, "glXGetClientString");
    return host_glXGetClientString(guestToHostDisplay(dpy), name);
}

GLXFBConfig* felix86_thunk_glXChooseFBConfig(Display* dpy, int screen, const int* attribList, int* nitems) {
    PRINTME;
    static auto host_glXChooseFBConfig = (decltype(&felix86_thunk_glXChooseFBConfig))dlsym(libGLX, "glXChooseFBConfig");
    return host_glXChooseFBConfig(guestToHostDisplay(dpy), screen, attribList, nitems);
}

int felix86_thunk_glXGetFBConfigAttrib(Display* dpy, GLXFBConfig config, int attribute, int* value) {
    PRINTME;
    static auto host_glXGetFBConfigAttrib = (decltype(&felix86_thunk_glXGetFBConfigAttrib))dlsym(libGLX, "glXGetFBConfigAttrib");
    return host_glXGetFBConfigAttrib(guestToHostDisplay(dpy), config, attribute, value);
}

GLXFBConfig* felix86_thunk_glXGetFBConfigs(Display* dpy, int screen, int* nelements) {
    PRINTME;
    static auto host_glXGetFBConfigs = (decltype(&felix86_thunk_glXGetFBConfigs))dlsym(libGLX, "glXGetFBConfigs");
    return host_glXGetFBConfigs(guestToHostDisplay(dpy), screen, nelements);
}

XVisualInfo* felix86_thunk_glXGetVisualFromFBConfig(Display* dpy, GLXFBConfig config) {
    PRINTME;
    static auto host_glXGetVisualFromFBConfig = (decltype(&felix86_thunk_glXGetVisualFromFBConfig))dlsym(libGLX, "glXGetVisualFromFBConfig");
    return host_glXGetVisualFromFBConfig(guestToHostDisplay(dpy), config);
}

GLXWindow felix86_thunk_glXCreateWindow(Display* dpy, GLXFBConfig config, Window win, const int* attribList) {
    PRINTME;
    static auto host_glXCreateWindow = (decltype(&felix86_thunk_glXCreateWindow))dlsym(libGLX, "glXCreateWindow");
    return host_glXCreateWindow(guestToHostDisplay(dpy), config, win, attribList);
}

void felix86_thunk_glXDestroyWindow(Display* dpy, GLXWindow window) {
    PRINTME;
    static auto host_glXDestroyWindow = (decltype(&felix86_thunk_glXDestroyWindow))dlsym(libGLX, "glXDestroyWindow");
    return host_glXDestroyWindow(guestToHostDisplay(dpy), window);
}

GLXPixmap felix86_thunk_glXCreatePixmap(Display* dpy, GLXFBConfig config, Pixmap pixmap, const int* attribList) {
    PRINTME;
    static auto host_glXCreatePixmap = (decltype(&felix86_thunk_glXCreatePixmap))dlsym(libGLX, "glXCreatePixmap");
    return host_glXCreatePixmap(guestToHostDisplay(dpy), config, pixmap, attribList);
}

void felix86_thunk_glXDestroyPixmap(Display* dpy, GLXPixmap pixmap) {
    PRINTME;
    static auto host_glXDestroyPixmap = (decltype(&felix86_thunk_glXDestroyPixmap))dlsym(libGLX, "glXDestroyPixmap");
    return host_glXDestroyPixmap(guestToHostDisplay(dpy), pixmap);
}

GLXPbuffer felix86_thunk_glXCreatePbuffer(Display* dpy, GLXFBConfig config, const int* attribList) {
    PRINTME;
    static auto host_glXCreatePbuffer = (decltype(&felix86_thunk_glXCreatePbuffer))dlsym(libGLX, "glXCreatePbuffer");
    return host_glXCreatePbuffer(guestToHostDisplay(dpy), config, attribList);
}

void felix86_thunk_glXDestroyPbuffer(Display* dpy, GLXPbuffer pbuf) {
    PRINTME;
    static auto host_glXDestroyPbuffer = (decltype(&felix86_thunk_glXDestroyPbuffer))dlsym(libGLX, "glXDestroyPbuffer");
    return host_glXDestroyPbuffer(guestToHostDisplay(dpy), pbuf);
}

void felix86_thunk_glXQueryDrawable(Display* dpy, GLXDrawable draw, int attribute, unsigned int* value) {
    PRINTME;
    static auto host_glXQueryDrawable = (decltype(&felix86_thunk_glXQueryDrawable))dlsym(libGLX, "glXQueryDrawable");
    return host_glXQueryDrawable(guestToHostDisplay(dpy), draw, attribute, value);
}

GLXContext felix86_thunk_glXCreateNewContext(Display* dpy, GLXFBConfig config, int renderType, GLXContext shareList, Bool direct) {
    PRINTME;
    static auto host_glXCreateNewContext = (decltype(&felix86_thunk_glXCreateNewContext))dlsym(libGLX, "glXCreateNewContext");
    return host_glXCreateNewContext(guestToHostDisplay(dpy), config, renderType, shareList, direct);
}

Bool felix86_thunk_glXMakeContextCurrent(Display* dpy, GLXDrawable draw, GLXDrawable read, GLXContext ctx) {
    PRINTME;
    static auto host_glXMakeContextCurrent = (decltype(&felix86_thunk_glXMakeContextCurrent))dlsym(libGLX, "glXMakeContextCurrent");
    return host_glXMakeContextCurrent(guestToHostDisplay(dpy), draw, read, ctx);
}

int felix86_thunk_glXQueryContext(Display* dpy, GLXContext ctx, int attribute, int* value) {
    PRINTME;
    static auto host_glXQueryContext = (decltype(&felix86_thunk_glXQueryContext))dlsym(libGLX, "glXQueryContext");
    return host_glXQueryContext(guestToHostDisplay(dpy), ctx, attribute, value);
}

void felix86_thunk_glXSelectEvent(Display* dpy, GLXDrawable drawable, unsigned long mask) {
    PRINTME;
    static auto host_glXSelectEvent = (decltype(&felix86_thunk_glXSelectEvent))dlsym(libGLX, "glXSelectEvent");
    return host_glXSelectEvent(guestToHostDisplay(dpy), drawable, mask);
}

void felix86_thunk_glXGetSelectedEvent(Display* dpy, GLXDrawable drawable, unsigned long* mask) {
    PRINTME;
    static auto host_glXGetSelectedEvent = (decltype(&felix86_thunk_glXGetSelectedEvent))dlsym(libGLX, "glXGetSelectedEvent");
    return host_glXGetSelectedEvent(guestToHostDisplay(dpy), drawable, mask);
}

// Load the host function pointers in the thunkptr namespace with pointers using dlopen + dlsym
void Thunks::initialize() {
    thunkptr::glXGetProcAddress = (u64)felix86_thunk_glXGetProcAddress;
    thunkptr::glXGetProcAddressARB = (u64)felix86_thunk_glXGetProcAddress;
    thunkptr::eglGetProcAddress = (u64)felix86_thunk_eglGetProcAddress;

    // These need to be handled specially to map Display* and a couple other things
    // For these we don't dlsym
    thunkptr::glXChooseVisual = (u64)felix86_thunk_glXChooseVisual;
    thunkptr::glXCreateContext = (u64)felix86_thunk_glXCreateContext;
    thunkptr::glXDestroyContext = (u64)felix86_thunk_glXDestroyContext;
    thunkptr::glXMakeCurrent = (u64)felix86_thunk_glXMakeCurrent;
    thunkptr::glXCopyContext = (u64)felix86_thunk_glXCopyContext;
    thunkptr::glXSwapBuffers = (u64)felix86_thunk_glXSwapBuffers;
    thunkptr::glXCreateGLXPixmap = (u64)felix86_thunk_glXCreateGLXPixmap;
    thunkptr::glXDestroyGLXPixmap = (u64)felix86_thunk_glXDestroyGLXPixmap;
    thunkptr::glXQueryExtension = (u64)felix86_thunk_glXQueryExtension;
    thunkptr::glXQueryVersion = (u64)felix86_thunk_glXQueryVersion;
    thunkptr::glXIsDirect = (u64)felix86_thunk_glXIsDirect;
    thunkptr::glXGetConfig = (u64)felix86_thunk_glXGetConfig;
    thunkptr::glXQueryExtensionsString = (u64)felix86_thunk_glXQueryExtensionsString;
    thunkptr::glXQueryServerString = (u64)felix86_thunk_glXQueryServerString;
    thunkptr::glXGetClientString = (u64)felix86_thunk_glXGetClientString;
    thunkptr::glXChooseFBConfig = (u64)felix86_thunk_glXChooseFBConfig;
    thunkptr::glXGetFBConfigAttrib = (u64)felix86_thunk_glXGetFBConfigAttrib;
    thunkptr::glXGetFBConfigs = (u64)felix86_thunk_glXGetFBConfigs;
    thunkptr::glXGetVisualFromFBConfig = (u64)felix86_thunk_glXGetVisualFromFBConfig;
    thunkptr::glXCreateWindow = (u64)felix86_thunk_glXCreateWindow;
    thunkptr::glXDestroyWindow = (u64)felix86_thunk_glXDestroyWindow;
    thunkptr::glXCreatePixmap = (u64)felix86_thunk_glXCreatePixmap;
    thunkptr::glXDestroyPixmap = (u64)felix86_thunk_glXDestroyPixmap;
    thunkptr::glXCreatePbuffer = (u64)felix86_thunk_glXCreatePbuffer;
    thunkptr::glXDestroyPbuffer = (u64)felix86_thunk_glXDestroyPbuffer;
    thunkptr::glXQueryDrawable = (u64)felix86_thunk_glXQueryDrawable;
    thunkptr::glXCreateNewContext = (u64)felix86_thunk_glXCreateNewContext;
    thunkptr::glXMakeContextCurrent = (u64)felix86_thunk_glXMakeContextCurrent;
    thunkptr::glXQueryContext = (u64)felix86_thunk_glXQueryContext;
    thunkptr::glXSelectEvent = (u64)felix86_thunk_glXSelectEvent;
    thunkptr::glXGetSelectedEvent = (u64)felix86_thunk_glXGetSelectedEvent;

    thunkptr::vkGetInstanceProcAddr = (u64)felix86_thunk_vkGetInstanceProcAddr;
    thunkptr::vkGetDeviceProcAddr = (u64)felix86_thunk_vkGetDeviceProcAddr;

#if 0
    constexpr const char* glx_name = "libGLX.so";
    libGLX = dlopen(glx_name, RTLD_LAZY);
    if (!libGLX) {
        ERROR("I couldn't open libGLX.so, error: %s", dlerror());
    }

    constexpr const char* x11_name = "libX11.so";
    libX11 = dlopen(x11_name, RTLD_LAZY);
    if (!libX11) {
        ERROR("I couldn't open libX11.so, error: %s", dlerror());
    }

    constexpr const char* egl_name = "libEGL.so.1";
    libEGL = dlopen(egl_name, RTLD_LAZY);
    if (!libEGL) {
        ERROR("I couldn't open libEGL.so, error: %s", dlerror());
    }
#endif

    constexpr const char* vulkan_name = "libvulkan.so.1";
    libvulkan = dlopen(vulkan_name, RTLD_LAZY);
    if (!libvulkan) {
        ERROR("I couldn't open libvulkan.so, error: %s", dlerror());
    }

#if 0
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
#endif
#define X(libname, name, ...)                                                                                                                        \
    if (thunkptr::name == 0) {                                                                                                                       \
        thunkptr::name = (u64)dlsym(libvulkan, #name);                                                                                               \
        if (thunkptr::name == 0) {                                                                                                                   \
            ERROR("Failed to find symbol %s in %s, error: %s", #name, "libvulkan.so", dlerror());                                                    \
        }                                                                                                                                            \
    }
#include "vulkan_thunks.inc"
#undef X
    // gl_thunks are loaded from the getprocaddress functions
}

void* Thunks::generateTrampoline(Recompiler& rec, const char* name) {
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

    Assembler& as = rec.getAssembler();
    const std::string& signature = thunk->signature;
    const u64 target = *thunk->host_function;

    ASSERT(signature.size() > 0);
    ASSERT_MSG(target != 0, "Symbol has nullptr address: %s", name);

    void* trampoline = as.GetCursorPointer();

    ABIMarshaller marshaller(signature);
    marshaller.emitPrologue(as);
    Recompiler::call(as, target);
    marshaller.emitEpilogue(as);

    return trampoline;
}

void* Thunks::generateTrampoline(Recompiler& rec, const char* signature, u64 host_ptr) {
    ASSERT(signature);
    ASSERT(host_ptr);
    Assembler& as = rec.getAssembler();
    void* trampoline = as.GetCursorPointer();

    ABIMarshaller marshaller(signature);
    marshaller.emitPrologue(as);
    Recompiler::call(as, host_ptr);
    marshaller.emitEpilogue(as);

    return trampoline;
}

void Thunks::runConstructor(const char* lib, GuestPointers* pointers) {
    VERBOSE("Constructor for %s with pointers at %p", lib, (void*)pointers);
    std::string libname = lib;

    if (libname == "libGLX.so") {
        while (pointers) {
            const void* func = pointers->func;
            if (!func) {
                break;
            }

            const std::string name = pointers->name;

            if (name == "XGetVisualInfo") {
                felix86__x86_64__XGetVisualInfo = (XGetVisualInfoType)func;
            } else if (name == "XSync") {
                felix86__x86_64__XSync = (XSyncType)func;
            } else {
                ERROR("Unknown function name when trying to run constructor: %s", pointers->name);
            }

            pointers++;
        }

        ASSERT_MSG(felix86__x86_64__XGetVisualInfo, "Failed to find XGetVisualInfo in thunked libGLX");
        ASSERT_MSG(felix86__x86_64__XSync, "Failed to find XSync in thunked libGLX");
        VERBOSE("Constructor for %s finished!", lib);
        return; // everything ok!
    }

    ERROR("Unknown library name when trying to run constructor: %s", lib);
}
#endif