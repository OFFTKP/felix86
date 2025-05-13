#include "felix86/hle/thunks.hpp"

// TODO: this file is messy. Split it up to separate files per library once our thunking implementation is more concrete

// Thunks need libX11
#ifndef BUILD_THUNKING
void Thunks::initialize() {}

void* Thunks::generateTrampoline(Recompiler&, const char*) {
    return nullptr;
}

void* Thunks::generateTrampoline(Recompiler&, const char*, u64) {
    return nullptr;
}

void Thunks::runConstructor(const char*, GuestPointers*) {}

#else
#include <cmath>
#include <dlfcn.h>
#include <sys/mman.h>
#include "felix86/common/overlay.hpp"
#include "felix86/common/state.hpp"
#include "felix86/hle/abi.hpp"
#include "felix86/v2/recompiler.hpp"

#include <X11/Xlibint.h>
#include <X11/Xutil.h>
#include <vulkan/vulkan.h>
#include <wayland-client.h>

static void* libGLX = nullptr;
static void* libX11 = nullptr;
static void* libEGL = nullptr;
static void* libvulkan = nullptr;
static void* libwayland = nullptr;

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

struct Thunk {
    const char* lib_name;
    const char* function_name;
    const char* signature;
    u64 pointer = 0;
};

#define X(lib_name, function_name, signature) {lib_name, #function_name, #signature, 0},

static Thunk thunk_metadata[] = {
#include "egl_thunks.inc"
#include "gl_thunks.inc"
#include "glx_thunks.inc"
#include "vulkan_thunks.inc"
#include "wayland-client_thunks.inc"
};

#undef X

// We don't care about the internals
using GLXContext = void*;
using GLXDrawable = void*;
using GLXPixmap = void*;
using GLXFBConfig = void*;
using GLXWindow = void*;
using GLXPbuffer = void*;

void* generate_guest_pointer(const char* name, u64 host_ptr) {
    const Thunk* thunk = nullptr;
    std::string sname = name;
    for (auto& meta : thunk_metadata) { // TODO: speed it up? only search specific lib
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
    ThreadState* state = ThreadState::Get();
    state->signals_disabled = true;
    // We can't put this code in code cache, because it needs to outlive potential code cache clears
    u8* memory = state->x86_trampoline_storage;
    // Our recompiler marks guest code as PROT_READ, we need to undo this as it may have marked previous trampolines
    mprotect((u8*)((u64)memory & ~0xFFFull), 4096, PROT_READ | PROT_WRITE);

    // 0f 01 39 ; invlpg [rcx] ; see handlers.cpp -- invlpg (magic instruction that generates jump to host code)
    // 00 00 00 00 00 00 00 00 ; pointer we jump to
    // ... 00 ; signature const char*
    // c3 ; ret
    memory[0] = 0x0f;
    memory[1] = 0x01;
    memory[2] = 0x39;
    memcpy(&memory[3], &host_ptr, sizeof(u64));
    memcpy(&memory[3 + 8], signature, sigsize);
    memory[3 + 8 + sigsize + 1] = 0xc3;
    state->x86_trampoline_storage += 3 + 8 + sigsize + 2;
    state->signals_disabled = false;
    VERBOSE("Created guest-callable host pointer for %s: %p", name, host_ptr);
    return memory;
}

VkResult felix86_thunk_vkCreateInstance(const VkInstanceCreateInfo* pCreateInfo, const VkAllocationCallbacks*, VkInstance* pInstance) {
    // Remove debug callbacks from VkInstanceCreateInfo
    VkBaseInStructure* base = (VkBaseInStructure*)pCreateInfo;
    while (base->pNext) {
        VkBaseInStructure* next = (VkBaseInStructure*)base->pNext;
        if (next->sType == VK_STRUCTURE_TYPE_DEBUG_REPORT_CREATE_INFO_EXT) {
            base->pNext = next->pNext;

            if (!base->pNext) {
                break;
            }
        }

        base = (VkBaseInStructure*)base->pNext;
    }

    static auto actual = (VkResult(*)(const VkInstanceCreateInfo*, const VkAllocationCallbacks*, VkInstance*))dlsym(libvulkan, "vkCreateInstance");
    return actual(pCreateInfo, nullptr, pInstance);
}

void* host_vkGetInstanceProcAddr(VkInstance instance, const char* name);
void* host_vkGetDeviceProcAddr(VkDevice device, const char* name);
void* host_eglGetProcAddress(const char* name);
void* get_custom_vk_thunk(const std::string& name);
void* get_custom_egl_thunk(const std::string& name);

void* felix86_thunk_glXGetProcAddress(const char* name) {
    UNIMPLEMENTED();
    return nullptr;
}

// TODO: Kinda wasteful to code cache if this gets called more than once per name
void* felix86_thunk_vkGetInstanceProcAddr(VkInstance instance, const char* name) {
    VERBOSE("vkGetInstanceProcAddr: %s", name);
    void* ptr = get_custom_vk_thunk(name);
    if (ptr == nullptr) {
        ptr = host_vkGetInstanceProcAddr(instance, name);
    }

    if (ptr) {
        // We can't return `ptr` here because it's a host pointer
        // But we also can't return our own thunked pointers, we need to return the one
        // getprocaddr returned. So we generate an invlpg [rcx] to create a proper guest pointer that will jump to our pointer
        return generate_guest_pointer(name, (u64)ptr);
    } else {
        return nullptr;
    }
}

void* felix86_thunk_vkGetDeviceProcAddr(VkDevice device, const char* name) {
    VERBOSE("vkGetDeviceProcAddr: %s", name);
    void* ptr = get_custom_vk_thunk(name);
    if (ptr == nullptr) {
        ptr = host_vkGetDeviceProcAddr(device, name);
    }

    if (ptr) {
        return generate_guest_pointer(name, (u64)ptr);
    } else {
        return nullptr;
    }
}

void* felix86_thunk_eglGetProcAddress(const char* name) {
    VERBOSE("eglGetProcAddress: %s", name);
    void* ptr = get_custom_egl_thunk(name);
    if (ptr == nullptr) {
        ptr = host_eglGetProcAddress(name);
    }

    if (ptr) {
        return generate_guest_pointer(name, (u64)ptr);
    } else {
        return nullptr;
    }
}

VkResult felix86_thunk_vkCreateDebugReportCallbackEXT(VkInstance instance, const VkDebugReportCallbackCreateInfoEXT* pCreateInfo,
                                                      const VkAllocationCallbacks* pAllocator, VkDebugReportCallbackEXT* pCallback) {
    // TODO: implement one day, needs callback support
    return VK_SUCCESS;
}

void felix86_thunk_vkDestroyDebugReportCallbackEXT(VkInstance instance, VkDebugReportCallbackEXT callback, const VkAllocationCallbacks* pAllocator) {
    // See vkCreateDebugReportCallbackEXT above
}

#define WL_CLOSURE_MAX_ARGS 20

// Convert the wayland callback signature to a felix86 thunk signature to generate a host->guest trampoline
std::string wl_to_felix86_signature(const std::string& wayland_signature) {
    std::string ret = "v_qq"; // wayland callbacks return void and take void*, wl_proxy* as the first two args
    for (auto c : wayland_signature) {
        switch (c) {
        case 's': // const char*
        case 'o': // wl_proxy*
        case 'n': // wl_proxy*
        case 'a': // wl_array*
        {
            ret += 'q';
            break;
        }
        case 'u': // u32
        case 'i': // i32
        case 'f': // wl_fixed_t ie. i32
        case 'h': {
            ret += 'd';
            break;
        }
        case '?': {
            continue;
        }
        case '0' ... '9': {
            continue;
        }
        default: {
            ERROR("Unknown wayland signature character: %c", c);
            break;
        }
        }
    }
    return ret;
}

void* host_wl_proxy_get_listener(struct wl_proxy* proxy) {
    static auto host_wl_proxy_get_listener = (void* (*)(struct wl_proxy*))dlsym(libwayland, "wl_proxy_get_listener");
    return host_wl_proxy_get_listener(proxy);
}

int felix86_thunk_wl_proxy_add_listener(struct wl_proxy* proxy, void** callbacks, void* data) {
    void* old_listener = host_wl_proxy_get_listener(proxy);
    delete[] (u64*)old_listener;

    struct wl_interface* interface = *(struct wl_interface**)proxy;
    u64* host_callable = new u64[WL_CLOSURE_MAX_ARGS];
    for (u32 i = 0; i < interface->event_count; i++) {
        const char* signature = interface->events[i].signature;
        std::string f86_signature = wl_to_felix86_signature(signature);
        void* callback = callbacks[i];
        void* host_callback = ABIMadness::hostToGuestTrampoline(f86_signature.c_str(), callback);
        host_callable[i] = (u64)host_callback;
    }

    static auto host_wl_proxy_add_listener = (int (*)(struct wl_proxy*, void*, void*))dlsym(libwayland, "wl_proxy_add_listener");
    return host_wl_proxy_add_listener(proxy, host_callable, data);
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

void* get_custom_vk_thunk(const std::string& name) {
    if (name == "vkGetInstanceProcAddr") {
        return (void*)felix86_thunk_vkGetInstanceProcAddr;
    } else if (name == "vkGetDeviceProcAddr") {
        return (void*)felix86_thunk_vkGetDeviceProcAddr;
    } else if (name == "vkCreateInstance") {
        return (void*)felix86_thunk_vkCreateInstance;
    } else if (name == "vkCreateDebugReportCallbackEXT") {
        return (void*)felix86_thunk_vkCreateDebugReportCallbackEXT;
    } else if (name == "vkDestroyDebugReportCallbackEXT") {
        return (void*)felix86_thunk_vkDestroyDebugReportCallbackEXT;
    } else {
        return nullptr;
    }
}

void* get_custom_egl_thunk(const std::string& name) {
    if (name == "eglGetProcAddress") {
        return (void*)felix86_thunk_eglGetProcAddress;
    } else {
        return nullptr;
    }
}

void* get_custom_wl_thunk(const std::string& name) {
    if (name == "wl_proxy_add_listener") {
        return (void*)felix86_thunk_wl_proxy_add_listener;
    } else {
        return nullptr;
    }
}

void* get_custom_glx_thunk(const std::string& name) {
    if (name == "glXGetProcAddress") {
        return (void*)felix86_thunk_glXGetProcAddress;
    } else if (name == "glXGetProcAddressARB") {
        return (void*)felix86_thunk_glXGetProcAddress;
    } else if (name == "glXChooseVisual") {
        return (void*)felix86_thunk_glXChooseVisual;
    } else if (name == "glXCreateContext") {
        return (void*)felix86_thunk_glXCreateContext;
    } else if (name == "glXDestroyContext") {
        return (void*)felix86_thunk_glXDestroyContext;
    } else if (name == "glXMakeCurrent") {
        return (void*)felix86_thunk_glXMakeCurrent;
    } else if (name == "glXCopyContext") {
        return (void*)felix86_thunk_glXCopyContext;
    } else if (name == "glXSwapBuffers") {
        return (void*)felix86_thunk_glXSwapBuffers;
    } else if (name == "glXCreateGLXPixmap") {
        return (void*)felix86_thunk_glXCreateGLXPixmap;
    } else if (name == "glXDestroyGLXPixmap") {
        return (void*)felix86_thunk_glXDestroyGLXPixmap;
    } else if (name == "glXQueryExtension") {
        return (void*)felix86_thunk_glXQueryExtension;
    } else if (name == "glXQueryVersion") {
        return (void*)felix86_thunk_glXQueryVersion;
    } else if (name == "glXIsDirect") {
        return (void*)felix86_thunk_glXIsDirect;
    } else if (name == "glXGetConfig") {
        return (void*)felix86_thunk_glXGetConfig;
    } else if (name == "glXQueryExtensionsString") {
        return (void*)felix86_thunk_glXQueryExtensionsString;
    } else if (name == "glXQueryServerString") {
        return (void*)felix86_thunk_glXQueryServerString;
    } else if (name == "glXGetClientString") {
        return (void*)felix86_thunk_glXGetClientString;
    } else if (name == "glXChooseFBConfig") {
        return (void*)felix86_thunk_glXChooseFBConfig;
    } else if (name == "glXGetFBConfigAttrib") {
        return (void*)felix86_thunk_glXGetFBConfigAttrib;
    } else if (name == "glXGetFBConfigs") {
        return (void*)felix86_thunk_glXGetFBConfigs;
    } else if (name == "glXGetVisualFromFBConfig") {
        return (void*)felix86_thunk_glXGetVisualFromFBConfig;
    } else if (name == "glXCreateWindow") {
        return (void*)felix86_thunk_glXCreateWindow;
    } else if (name == "glXDestroyWindow") {
        return (void*)felix86_thunk_glXDestroyWindow;
    } else if (name == "glXCreatePixmap") {
        return (void*)felix86_thunk_glXCreatePixmap;
    } else if (name == "glXDestroyPixmap") {
        return (void*)felix86_thunk_glXDestroyPixmap;
    } else if (name == "glXCreatePbuffer") {
        return (void*)felix86_thunk_glXCreatePbuffer;
    } else if (name == "glXDestroyPbuffer") {
        return (void*)felix86_thunk_glXDestroyPbuffer;
    } else if (name == "glXQueryDrawable") {
        return (void*)felix86_thunk_glXQueryDrawable;
    } else if (name == "glXCreateNewContext") {
        return (void*)felix86_thunk_glXCreateNewContext;
    } else if (name == "glXMakeContextCurrent") {
        return (void*)felix86_thunk_glXMakeContextCurrent;
    } else if (name == "glXQueryContext") {
        return (void*)felix86_thunk_glXQueryContext;
    } else if (name == "glXSelectEvent") {
        return (void*)felix86_thunk_glXSelectEvent;
    } else if (name == "glXGetSelectedEvent") {
        return (void*)felix86_thunk_glXGetSelectedEvent;
    } else {
        return nullptr;
    }
}

void* host_vkGetInstanceProcAddr(VkInstance instance, const char* name) {
    static auto vkGetInstanceProcAddr = (void* (*)(VkInstance, const char*))dlsym(libvulkan, "vkGetInstanceProcAddr");
    return vkGetInstanceProcAddr(instance, name);
}

void* host_vkGetDeviceProcAddr(VkDevice device, const char* name) {
    static auto vkGetDeviceProcAddr = (void* (*)(VkDevice, const char*))dlsym(libvulkan, "vkGetDeviceProcAddr");
    return vkGetDeviceProcAddr(device, name);
}

void* host_eglGetProcAddress(const char* name) {
    static auto eglGetProcAddress = (void* (*)(const char*))dlsym(libEGL, "eglGetProcAddress");
    return eglGetProcAddress(name);
}

// Load the host function pointers in the thunkptr namespace with pointers using dlopen + dlsym
void Thunks::initialize() {
#if 0
    constexpr const char* glx_name = "libGLX.so";
    libGLX = dlopen(glx_name, RTLD_NOW | RTLD_LOCAL);
    if (!libGLX) {
        ERROR("I couldn't open libGLX.so, error: %s", dlerror());
    }

    constexpr const char* x11_name = "libX11.so";
    libX11 = dlopen(x11_name, RTLD_NOW | RTLD_LOCAL);
    if (!libX11) {
        ERROR("I couldn't open libX11.so, error: %s", dlerror());
    }
#endif
    std::filesystem::path thunks = g_config.thunks_path;
    ASSERT_MSG(std::filesystem::exists(thunks), "The thunks path set with FELIX86_THUNKS %s does not exist", thunks.c_str());
    std::string srootfs = g_config.rootfs_path.string();

#ifndef BUILD_THUNKING
    ERROR("FELIX86_THUNKS is set, but this build of felix86 was not built with thunking support, enable BUILD_THUNKING in cmake configuration");
    return;
#endif

    bool thunk_vk = false;
    bool thunk_egl = false;
    bool thunk_wayland = false;
    std::string enabled_thunks = g_config.enabled_thunks;
    if (enabled_thunks == "all") {
        thunk_vk = true;
        thunk_egl = true;
        thunk_wayland = true;
    } else if (!enabled_thunks.empty()) {
        std::vector<std::string> list = split_string(enabled_thunks, ',');
        for (const auto& t : list) {
            std::string n = t;
            for (auto& c : n) {
                c = tolower(c);
            }

            if (n == "libvulkan" || n == "vulkan" || n == "vk") {
                thunk_vk = true;
            } else if (n == "libegl" || n == "egl") {
                thunk_egl = true;
            } else if (n == "libwayland-client" || n == "libwayland" || n == "wayland-client" || n == "wayland" || n == "wl") {
                thunk_wayland = true;
            } else {
                ERROR("Unknown option: %s in FELIX86_ENABLED_THUNKS", t.c_str());
            }
        }
    }

    if (thunk_egl) {
        std::filesystem::path egl_thunk;
        bool found_egl = false;

        auto check_egl = [&](const char* path) {
            if (!found_egl && std::filesystem::exists(thunks / path)) {
                egl_thunk = thunks / path;
                found_egl = true;
            }
        };

        check_egl("libEGL.so.1");
        check_egl("libEGL.so");
        check_egl("libEGL-thunked.so");

        if (!egl_thunk.empty()) {
            Overlays::addOverlay("libEGL.so.1", egl_thunk);
            Overlays::addOverlay("libEGL.so", egl_thunk);
        } else {
            WARN("I couldn't find libEGL-thunked.so in %s", thunks.c_str());
        }
    }

    if (thunk_vk) {
        std::filesystem::path vulkan_thunk;
        bool found_vulkan = false;

        auto check_vulkan = [&](const char* path) {
            if (!found_vulkan && std::filesystem::exists(thunks / path)) {
                vulkan_thunk = thunks / path;
                found_vulkan = true;
            }
        };

        check_vulkan("libvulkan.so.1");
        check_vulkan("libvulkan.so");
        check_vulkan("libvulkan-thunked.so");

        if (!vulkan_thunk.empty()) {
            Overlays::addOverlay("libvulkan.so.1", vulkan_thunk);
            Overlays::addOverlay("libvulkan.so", vulkan_thunk);
        } else {
            WARN("I couldn't find libvulkan.so in %s", thunks.c_str());
        }
    }

    if (thunk_wayland) {
        std::filesystem::path wayland_thunk;
        bool found_wayland = false;

        auto check_wayland = [&](const char* path) {
            if (!found_wayland && std::filesystem::exists(thunks / path)) {
                wayland_thunk = thunks / path;
                found_wayland = true;
            }
        };

        check_wayland("libwayland-client.so.0");
        check_wayland("libwayland-client.so");
        check_wayland("libwayland-client-thunked.so");

        if (!wayland_thunk.empty()) {
            Overlays::addOverlay("libwayland-client.so.0", wayland_thunk);
            Overlays::addOverlay("libwayland-client.so", wayland_thunk);
        } else {
            WARN("I couldn't find libwayland-client.so in %s", thunks.c_str());
        }
    }

    constexpr const char* egl_name = "libEGL.so.1";
    libEGL = dlopen(egl_name, RTLD_NOW | RTLD_LOCAL);
    if (!libEGL) {
        ERROR("I couldn't open libEGL.so, error: %s", dlerror());
    }

    constexpr const char* vulkan_name = "libvulkan.so.1";
    libvulkan = dlopen(vulkan_name, RTLD_NOW | RTLD_LOCAL);
    if (!libvulkan) {
        ERROR("I couldn't open libvulkan.so, error: %s", dlerror());
    }

    constexpr const char* wayland_name = "libwayland-client.so.0";
    libwayland = dlopen(wayland_name, RTLD_NOW | RTLD_LOCAL);
    if (!libwayland) {
        ERROR("I couldn't open libwayland-client.so, error: %s", dlerror());
    }

    for (int i = 0; i < sizeof(thunk_metadata) / sizeof(Thunk); i++) {
        Thunk& metadata = thunk_metadata[i];
        void* ptr = nullptr;
        std::string lib_name = metadata.lib_name;
        if (lib_name == "libEGL.so") {
            ptr = get_custom_egl_thunk(metadata.function_name);
            if (!ptr) {
                ptr = dlsym(libEGL, metadata.function_name);
            }
        } else if (lib_name == "libvulkan.so") {
            ptr = get_custom_vk_thunk(metadata.function_name);
            if (!ptr) {
                ptr = dlsym(libvulkan, metadata.function_name);
            }
        } else if (lib_name == "libwayland-client.so") {
            ptr = get_custom_wl_thunk(metadata.function_name);
            if (!ptr) {
                ptr = dlsym(libwayland, metadata.function_name);
            }
        } else {
            continue;
        }

        if (ptr == nullptr) {
            VERBOSE("Failed to find %s in thunked library %s", metadata.function_name, metadata.lib_name);
        }
        metadata.pointer = (u64)ptr;
    }
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
    const u64 target = thunk->pointer;

    ASSERT(signature.size() > 0);
    ASSERT_MSG(target != 0, "Symbol has nullptr address: %s", name);

    void* trampoline = as.GetCursorPointer();

    GuestToHostMarshaller marshaller(name, signature);
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

    GuestToHostMarshaller marshaller(std::string("ptr_") + signature, signature);
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
    } else if (libname == "libwayland-client.so") {
        // The job of this constructor is to copy the host pointers to the interface objects like wl_keyboard_interface
        while (pointers) {
            u64* ptr = pointers->func;
            if (!ptr) {
                break;
            }

            const char* name = pointers->name;
            u64 host_ptr = (u64)dlsym(libwayland, name);
            ASSERT_MSG(host_ptr != 0, "Could not find host libwayland-client pointer for %s", host_ptr);
            // Interfaces are placed in RO memory but we need to change their values to match our host library
            // So hack away the protection
            mprotect((void*)((u64)ptr & ~0xFFFull), 4096, PROT_READ | PROT_WRITE);
            memcpy((void*)ptr, (void*)host_ptr, sizeof(wl_interface));
            VERBOSE("libwayland-client thunk: %s set to %p (guest ptr: %p)", name, host_ptr, ptr);

            pointers++;
        }
        VERBOSE("Constructor for %s finished!", lib);
        return; // everything ok!
    }

    ERROR("Unknown library name when trying to run constructor: %s", lib);
}
#endif