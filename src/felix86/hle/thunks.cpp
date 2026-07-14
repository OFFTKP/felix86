#include "felix86/hle/thunks.hpp"

// TODO: this file is messy. Split it up to separate files per library once our thunking implementation is more concrete

// Thunks need libX11
#ifndef FELIX86_BUILD_THUNKING
#include <cstdlib>
#include <string>
#include "felix86/common/config.hpp"
#include "felix86/common/log.hpp"
#include "felix86/common/types.hpp"
#include "felix86/common/utility.hpp"

void Thunks::initialize() {
    const char* env = secure_getenv("FELIX86_ENABLED_THUNKS");
    if (env) {
        // Check if not the default and warn
        Config c;
        std::string def = c.enabled_thunks;
        if (std::string(env) != def) {
            WARN("You've set FELIX86_ENABLED_THUNKS but this felix86 was built without -DFELIX86_BUILD_THUNKING=ON");
        }
    }
}

void* Thunks::generateTrampoline(Recompiler&, const char*) {
    return nullptr;
}

void* Thunks::generateTrampoline(Recompiler&, const char*, const char*, u64) {
    return nullptr;
}

void Thunks::runConstructor(const char*, GuestPointers*) {}

#else
#include <cmath>
#include <mutex>
#include <dlfcn.h>
#include <sys/mman.h>
#include "felix86/common/overlay.hpp"
#include "felix86/common/state.hpp"
#include "felix86/hle/abi.hpp"
#include "felix86/hle/signals.hpp"
#include "felix86/v2/recompiler.hpp"

#include <vulkan/vulkan.h>
void* libGL = nullptr;
void* libGLX = nullptr;
void* libX11 = nullptr;
void* libEGL = nullptr;
void* libvulkan = nullptr;
void* libwayland = nullptr;
void* libluajit = nullptr;

// Definitions from libX11
typedef struct {
    void* visual;
    u64 visualid;
    int screen;
    int depth;
    int c_class; /* C++ */
    unsigned long red_mask;
    unsigned long green_mask;
    unsigned long blue_mask;
    int colormap_size;
    int bits_per_rgb;
} XVisualInfo64;

typedef struct {
    void* ext_data; /* hook for extension to hang data */
    void* private1;
    int fd; /* Network socket. */
    int private2;
    int proto_major_version; /* major version of server's X protocol */
    int proto_minor_version; /* minor version of servers X protocol */
    char* vendor;            /* vendor of the server hardware */
    u64 private3;
    u64 private4;
    u64 private5;
    int private6;
    u64 (*resource_alloc)(/* allocator function */
                          void*);
    int byte_order;       /* screen byte order, LSBFirst, MSBFirst */
    int bitmap_unit;      /* padding and data requirements */
    int bitmap_pad;       /* padding requirements on bitmaps */
    int bitmap_bit_order; /* LeastSignificant or MostSignificant */
    int nformats;         /* number of pixmap formats in list */
    void* pixmap_format;  /* pixmap format list */
    int private8;
    int release; /* release of the server */
    void *private9, *private10;
    int qlen;                        /* Length of input event queue */
    unsigned long last_request_read; /* seq number of last event read */
    unsigned long request;           /* sequence number of last request. */
    void* private11;
    void* private12;
    void* private13;
    void* private14;
    unsigned max_request_size; /* maximum number 32 bit words in request*/
    void* db;
    int (*private15)(void*);
    char* display_name;          /* "host:display" string used on this connect*/
    int default_screen;          /* default screen for operations */
    int nscreens;                /* number of screens on this server*/
    void* screens;               /* pointer to list of screens */
    unsigned long motion_buffer; /* size of motion buffer */
    unsigned long private16;
    int min_keycode; /* minimum defined keycode */
    int max_keycode; /* maximum defined keycode */
    void* private17;
    void* private18;
    int private19;
    char* xdefaults; /* contents of defaults from server */
                     /* there is more to this structure, but it is private to Xlib */
} Display64;

using glXChooseVisualType = XVisualInfo64* (*)(void*, int, int*);
using XGetVisualInfoType = XVisualInfo64* (*)(void*, long, void*, int*);
using XSyncType = int (*)(void*, int);
using XFreeType = int (*)(void*);
using mallocType = decltype(&malloc);

XGetVisualInfoType felix86_guest_XGetVisualInfo = nullptr;
XSyncType felix86_guest_XSync = nullptr;
mallocType felix86_guest_malloc = nullptr;

// We can't load it at function call time because vkGetInstanceProcAddr/vkGetDeviceProcAddr needs
// a VkInstance or VkDevice, so we need to load this before the function call
u32 (*host_vkGetPhysicalDeviceXlibPresentationSupportKHR)(void* physicalDevice, u32 queueFamilyIndex, void* dpy, u64 visualID) = nullptr;

// Ditto
u32 (*host_vkGetPhysicalDeviceXcbPresentationSupportKHR)(void* physicalDevice, u32 queueFamilyIndex, void* connection, void* visualID) = nullptr;

// Since we only need these from wayland-client.h, instead of including the file and requiring
// a dependency we just define them here
struct wl_interface {
    /** Interface name */
    const char* name;
    /** Interface version */
    int version;
    /** Number of methods (requests) */
    int method_count;
    /** Method (request) signatures */
    const struct wl_message* methods;
    /** Number of events */
    int event_count;
    /** Event signatures */
    const struct wl_message* events;
};

struct wl_message {
    /** Message name */
    const char* name;
    /** Message signature */
    const char* signature;
    /** Object argument interfaces */
    const struct wl_interface** types;
};

static std::mutex display_map_mutex;
static std::unordered_map<void*, void*> host_to_guest;
static std::unordered_map<void*, void*> guest_to_host;
static std::unordered_map<void*, void*> guest_to_host_connection;

void* host_XOpenDisplay(const char* name) {
    ASSERT(name);
    static void* (*xopendisplay_ptr)(const char*) = (decltype(xopendisplay_ptr))dlsym(libX11, "XOpenDisplay");
    ASSERT(xopendisplay_ptr);
    return xopendisplay_ptr(name);
}

int host_XFlush(void* display) {
    if (display == nullptr) {
        WARN("XFlush(nil) called?");
        return 0;
    }

    static int (*xflush_ptr)(void*) = (decltype(xflush_ptr))dlsym(libX11, "XFlush");
    ASSERT(xflush_ptr);
    return xflush_ptr(display);
}

void* host_XGetVisualInfo(void* display, long vinfo_mask, XVisualInfo64* vinfo_template, int* nitems_return) {
    static XGetVisualInfoType xvisualinfo_ptr = (XGetVisualInfoType)dlsym(libX11, "XGetVisualInfo");
    ASSERT(xvisualinfo_ptr);
    return xvisualinfo_ptr(display, vinfo_mask, vinfo_template, nitems_return);
}

int host_XFree(void* ptr) {
    static XFreeType xfree_ptr = (XFreeType)dlsym(libX11, "XFree");
    ASSERT(xfree_ptr);
    return xfree_ptr(ptr);
}

void* host_glXGetProcAddress(const char* name) {
    static auto glXGetProcAddress = (void* (*)(const char*))dlsym(libGLX, "glXGetProcAddress");
    return glXGetProcAddress(name);
}

void host_glDebugMessageCallback(void* callback, void* userParam) {
    static auto gldebugmessagecallback_ptr = (decltype(&host_glDebugMessageCallback))host_glXGetProcAddress("glDebugMessageCallback");
    return gldebugmessagecallback_ptr(callback, userParam);
}

XVisualInfo64* guest_XGetVisualInfo(void* display, long vinfo_mask, XVisualInfo64* vinfo_template, int* nitems_return) {
    ASSERT(felix86_guest_XGetVisualInfo);
    return felix86_guest_XGetVisualInfo(display, vinfo_mask, vinfo_template, nitems_return);
}

int guest_XSync(void* display, int discard) {
    ASSERT(felix86_guest_XSync);
    return felix86_guest_XSync(display, discard);
}

void* guest_malloc(size_t size) noexcept {
    ASSERT(felix86_guest_malloc);
    return felix86_guest_malloc(size);
}
static_assert(std::is_same_v<decltype(&guest_malloc), void*(*)(size_t) noexcept>);

void* guestToHostDisplay(void* guest_display) {
    if (guest_display == 0) {
        WARN("guestToHostDisplay(nil) called?");
        return nullptr;
    }

    guest_XSync(guest_display, 0);

    std::lock_guard<std::mutex> lock(display_map_mutex);

    if (guest_to_host.find(guest_display) != guest_to_host.end()) {
        return (void*)guest_to_host[guest_display];
    }

    const char* display_name = ((Display64*)guest_display)->display_name;
    void* host_display = host_XOpenDisplay(display_name);
    if (host_display) {
        guest_to_host[guest_display] = host_display;
        host_to_guest[host_display] = guest_display;
        return host_display;
    } else {
        WARN("Failed to XOpenDisplay: %s", display_name);
        return nullptr;
    }
}

void* guestToHostConnection(void* connection) {
    std::lock_guard<std::mutex> lock(display_map_mutex);

    if (guest_to_host_connection.find(connection) != guest_to_host_connection.end()) {
        return guest_to_host_connection[connection];
    }

    static void* libxcb = dlopen("libxcb.so.1", RTLD_LAZY);
    static auto host_xcb_connect = (void* (*)(void*, void*))dlsym(libxcb, "xcb_connect");
    static auto host_xcb_connection_has_error = (bool (*)(void*))dlsym(libxcb, "xcb_connection_has_error");
    void* host_connection = host_xcb_connect(nullptr, nullptr);
    if (host_xcb_connection_has_error(host_connection)) {
        ERROR("Couldn't open xcb connection");
    }

    guest_to_host_connection[connection] = host_connection;
    return host_connection;
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

XVisualInfo64* hostToGuestVisualInfo(void* guest_display, XVisualInfo64* host_info) {
    if (!host_info) {
        return nullptr;
    }

    XVisualInfo64 guest_info;
    guest_info.screen = host_info->screen;
    guest_info.visualid = host_info->visualid;
    host_XFree(host_info);

    int nitems_return = 0;
    XVisualInfo64* info = guest_XGetVisualInfo(guest_display, /* VisualScreenMask | VisualIDMask */ 3, &guest_info, &nitems_return);
    if (nitems_return != 1) {
        ERROR("Failed to find matching XVisualInfo64");
    }

    ASSERT(info);
    return info;
}

XVisualInfo64* guestToHostVisualInfo(void* host_display, XVisualInfo64* guest) {
    if (!host_display) {
        return nullptr;
    }

    XVisualInfo64 v;
    v.screen = guest->screen;
    v.visualid = guest->visualid;

    int c;
    void* info = host_XGetVisualInfo(host_display, /* VisualScreenMask | VisualIDMask */ 3, &v, &c);

    if (c >= 1 && info != nullptr) {
        return (XVisualInfo64*)info;
    } else {
        WARN("guestToHostVisualInfo returned null");
        return nullptr;
    }
}

template <class T, size_t size = sizeof(T)>
T* relocateArrayToGuest(T* host_ptr, size_t count) {
    void* guest_ptr = guest_malloc(size * count);
    ASSERT(guest_ptr);
    for (size_t i = 0; i < count; i++) {
        T* guest_data = (T*)((u8*)guest_ptr + i * size);
        T* host_data = (T*)((u8*)host_ptr + i * size);
        *guest_data = *host_data;
    }
    return (T*)guest_ptr;
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
#include "luajit_thunks.inc"
#include "vulkan_thunks.inc"
#include "wayland-client_thunks.inc"
};

#undef X

void unknown_thunk(const char* name) {
    ERROR("Unknown thunk with name: %s", name);
}

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
        u8* a_wasteful_page = (u8*)mmap(nullptr, 4096, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
        ASSERT(a_wasteful_page != MAP_FAILED);
        // 48 8d 3d 0f 00 00 00 ; lea rdi, [rip + 15], load pointer to string
        // 0f 01 39 ; invlpg [rcx] ; see handlers.cpp -- invlpg (magic instruction that generates jump to host code)
        // 00 00 00 00 00 00 00 00 ; pointer we jump to
        // ... 00 ; signature const char* = v_q
        // ... 00 ; name const char*
        // c3 ; ret
        a_wasteful_page[0] = 0x48;
        a_wasteful_page[1] = 0x8d;
        a_wasteful_page[2] = 0x3d;
        a_wasteful_page[3] = 0x0f;
        a_wasteful_page[4] = 0x00;
        a_wasteful_page[5] = 0x00;
        a_wasteful_page[6] = 0x00;
        a_wasteful_page[7] = 0x0f;
        a_wasteful_page[8] = 0x01;
        a_wasteful_page[9] = 0x39;
        u64 ptr = (u64)&unknown_thunk;
        memcpy(a_wasteful_page + 10, &ptr, 8);
        a_wasteful_page[18] = 'v';
        a_wasteful_page[19] = '_';
        a_wasteful_page[20] = 'q';
        a_wasteful_page[21] = '\0';
        size_t size = strlen(name);
        memcpy(a_wasteful_page + 22, name, size);
        a_wasteful_page[22 + size] = 0;
        a_wasteful_page[22 + size + 1] = 0xc3;
        a_wasteful_page[4095] = 0xf4; // hlt at the end of the page just in case
        return a_wasteful_page;
    }

    const char* signature = thunk->signature;
    size_t sigsize = strlen(signature);
    size_t namesize = strlen(name);
    ThreadState* state = ThreadState::Get();
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
    memcpy(&memory[3 + 8 + sigsize + 1], name, namesize);
    memory[3 + 8 + sigsize + 1 + namesize + 1] = 0xc3;
    state->x86_trampoline_storage += 3 + 8 + sigsize + 1 + namesize + 1 + 1;
    VERBOSE("Created guest-callable host pointer for %s: %p", name, host_ptr);
    return memory;
}

VkResult felix86_thunk_vkCreateInstance(const VkInstanceCreateInfo* pCreateInfo, const VkAllocationCallbacks*, VkInstance* pInstance) {
    // Remove debug callbacks from VkInstanceCreateInfo
    VkBaseInStructure* base = (VkBaseInStructure*)pCreateInfo;
    constexpr int skipped_type = 1000011000;
    while (base->pNext) {
        VkBaseInStructure* next = (VkBaseInStructure*)base->pNext;
        if (next->sType == skipped_type) {
            base->pNext = next->pNext;

            if (!base->pNext) {
                break;
            }
        }

        base = (VkBaseInStructure*)base->pNext;
    }

    static auto actual = (VkResult (*)(const VkInstanceCreateInfo*, const VkAllocationCallbacks*, VkInstance*))dlsym(libvulkan, "vkCreateInstance");
    return actual(pCreateInfo, nullptr, pInstance);
}

void* host_vkGetInstanceProcAddr(VkInstance instance, const char* name);
void* host_vkGetDeviceProcAddr(VkDevice device, const char* name);
void* host_eglGetProcAddress(const char* name);
void* get_custom_vk_thunk(const std::string& name);
void* get_custom_egl_thunk(const std::string& name);
void* get_custom_glx_thunk(const std::string& name);
void* get_custom_gl_thunk(const std::string& name);
void* get_custom_luajit_thunk(const std::string& name);

void* felix86_thunk_glXGetProcAddress(const char* name) {
    void* ptr = get_custom_glx_thunk(name);
    if (ptr == nullptr) {
        ptr = get_custom_gl_thunk(name);
        if (ptr == nullptr) {
            ptr = host_glXGetProcAddress(name);
        }
    }

    if (ptr) {
        return generate_guest_pointer(name, (u64)ptr);
    } else {
        return nullptr;
    }
}

// TODO: Kinda wasteful to code cache if this gets called more than once per name
void* felix86_thunk_vkGetInstanceProcAddr(VkInstance instance, const char* name) {
    VERBOSE("vkGetInstanceProcAddr: %s", name);
    void* ptr = get_custom_vk_thunk(name);
    if (ptr == nullptr) {
        ptr = host_vkGetInstanceProcAddr(instance, name);
    }

    // See comment above host_vkGetPhysicalDeviceXlibPresentationSupportKHR for justification
    if (strcmp(name, "vkGetPhysicalDeviceXlibPresentationSupportKHR") == 0) {
        host_vkGetPhysicalDeviceXlibPresentationSupportKHR =
            (decltype(host_vkGetPhysicalDeviceXlibPresentationSupportKHR))host_vkGetInstanceProcAddr(instance, name);
    } else if (strcmp(name, "vkGetPhysicalDeviceXcbPresentationSupportKHR")) {
        host_vkGetPhysicalDeviceXcbPresentationSupportKHR =
            (decltype(host_vkGetPhysicalDeviceXcbPresentationSupportKHR))host_vkGetInstanceProcAddr(instance, name);
    }

    if (ptr) {
        // We can't return `ptr` here because it's a host pointer
        // But we also can't return our own thunked pointers, we need to return the one
        // getprocaddr returned. So we generate an `] to create a proper guest pointer that will jump to our pointer
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
    void* ptr = get_custom_egl_thunk(name);
    if (ptr == nullptr) {
        ptr = get_custom_gl_thunk(name);
        if (ptr == nullptr) {
            ptr = host_eglGetProcAddress(name);
        }
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

struct felix86_VkXlibSurfaceCreateInfoKHR {
    VkStructureType sType;
    const void* pNext;
    u32 flags;
    void* dpy;
    u64 window;
};

VkResult felix86_thunk_vkCreateXlibSurfaceKHR(VkInstance instance, const felix86_VkXlibSurfaceCreateInfoKHR* pCreateInfo,
                                              const VkAllocationCallbacks* pAllocator, VkSurfaceKHR* pSurface) {
    void* guest_display = pCreateInfo->dpy;
    void* host_display = guestToHostDisplay(guest_display);
    felix86_VkXlibSurfaceCreateInfoKHR host_create_info = *pCreateInfo;
    host_create_info.dpy = host_display;
    static auto host_vkCreateXlibSurfaceKHR =
        (decltype(&felix86_thunk_vkCreateXlibSurfaceKHR))host_vkGetInstanceProcAddr(instance, "vkCreateXlibSurfaceKHR");
    VkResult result = host_vkCreateXlibSurfaceKHR(instance, &host_create_info, pAllocator, pSurface);
    host_XFlush(host_display);
    return result;
}

struct felix86_VkXcbSurfaceCreateInfoKHR {
    VkStructureType sType;
    const void* pNext;
    u32 flags;
    void* connection;
    void* window;
};

VkResult felix86_thunk_vkCreateXcbSurfaceKHR(VkInstance instance, const felix86_VkXcbSurfaceCreateInfoKHR* pCreateInfo,
                                             const VkAllocationCallbacks* pAllocator, VkSurfaceKHR* pSurface) {
    void* host_connection = guestToHostConnection(pCreateInfo->connection);
    felix86_VkXcbSurfaceCreateInfoKHR host_create_info = *pCreateInfo;
    host_create_info.connection = host_connection;
    static auto host_vkCreateXcbSurfaceKHR =
        (decltype(&felix86_thunk_vkCreateXcbSurfaceKHR))host_vkGetInstanceProcAddr(instance, "vkCreateXcbSurfaceKHR");
    VkResult result = host_vkCreateXcbSurfaceKHR(instance, &host_create_info, pAllocator, pSurface);
    return result;
}

VkBool32 felix86_thunk_vkGetPhysicalDeviceXlibPresentationSupportKHR(VkPhysicalDevice physicalDevice, uint32_t queueFamilyIndex, void* dpy,
                                                                     u64 visualID) {
    void* host_display = guestToHostDisplay(dpy);
    VkBool32 result = host_vkGetPhysicalDeviceXlibPresentationSupportKHR(physicalDevice, queueFamilyIndex, host_display, visualID);
    host_XFlush(host_display);
    return result;
}

VkBool32 felix86_thunk_vkGetPhysicalDeviceXcbPresentationSupportKHR(VkPhysicalDevice physicalDevice, uint32_t queueFamilyIndex, void* connection,
                                                                    void* visualID) {
    void* host_connection = guestToHostConnection(connection);
    VkBool32 result = host_vkGetPhysicalDeviceXcbPresentationSupportKHR(physicalDevice, queueFamilyIndex, host_connection, visualID);
    return result;
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
    for (int i = 0; i < interface->event_count; i++) {
        const char* signature = interface->events[i].signature;
        std::string f86_signature = wl_to_felix86_signature(signature);
        void* callback = callbacks[i];
        void* host_callback = ABIMadness::hostToGuestTrampoline(f86_signature.c_str(), callback);
        host_callable[i] = (u64)host_callback;
    }

    static auto host_wl_proxy_add_listener = (int (*)(struct wl_proxy*, void*, void*))dlsym(libwayland, "wl_proxy_add_listener");
    return host_wl_proxy_add_listener(proxy, host_callable, data);
}

#define PRINTME VERBOSE("Calling thunked %s", __PRETTY_FUNCTION__)

XVisualInfo64* felix86_thunk_glXChooseVisual(void* guest_display, int screen, int* attribList) {
    PRINTME;
    static auto host_glXChooseVisual = (glXChooseVisualType)dlsym(libGLX, "glXChooseVisual");
    return hostToGuestVisualInfo(guest_display, host_glXChooseVisual(guestToHostDisplay(guest_display), screen, attribList));
}

void* felix86_thunk_glXCreateContextAttribsARB(void* guest_display, void* config, void* share_context, int direct, const int* attrib_list) {
    PRINTME;
    static auto host_glXCreateContextAttribsARB =
        (decltype(&felix86_thunk_glXCreateContextAttribsARB))host_glXGetProcAddress("glXCreateContextAttribsARB");
    return host_glXCreateContextAttribsARB(guestToHostDisplay(guest_display), config, share_context, direct, attrib_list);
}

int felix86_thunk_glXQueryRendererIntegerMESA(void* guest_display, int screen, int renderer, int attribute, unsigned int* value) {
    PRINTME;
    static auto host_glXQueryRendererIntegerMESA =
        (decltype(&felix86_thunk_glXQueryRendererIntegerMESA))host_glXGetProcAddress("glXQueryRendererIntegerMESA");
    return host_glXQueryRendererIntegerMESA(guestToHostDisplay(guest_display), screen, renderer, attribute, value);
}

const char* felix86_thunk_glXQueryRendererStringMESA(void* guest_display, int screen, int renderer, int attribute) {
    PRINTME;
    static auto host_glXQueryRendererStringMESA =
        (decltype(&felix86_thunk_glXQueryRendererStringMESA))host_glXGetProcAddress("glXQueryRendererStringMESA");
    return host_glXQueryRendererStringMESA(guestToHostDisplay(guest_display), screen, renderer, attribute);
}

void* felix86_thunk_glXCreateContext(void* dpy, XVisualInfo64* visual, void* shareList, int direct) {
    PRINTME;
    static auto host_glXCreateContext = (decltype(&felix86_thunk_glXCreateContext))dlsym(libGLX, "glXCreateContext");
    void* host_dpy = guestToHostDisplay(dpy);
    XVisualInfo64* host_visual = guestToHostVisualInfo(host_dpy, visual);
    void* ret = host_glXCreateContext(host_dpy, host_visual, shareList, direct);
    host_XFree(host_visual);
    return ret;
}

void felix86_thunk_glXDestroyContext(void* dpy, void* ctx) {
    PRINTME;
    static auto host_glXDestroyContext = (decltype(&felix86_thunk_glXDestroyContext))dlsym(libGLX, "glXDestroyContext");
    return host_glXDestroyContext(guestToHostDisplay(dpy), ctx);
}

int felix86_thunk_glXMakeCurrent(void* dpy, u64 drawable, void* ctx) {
    PRINTME;
    static auto host_glXMakeCurrent = (decltype(&felix86_thunk_glXMakeCurrent))dlsym(libGLX, "glXMakeCurrent");
    return host_glXMakeCurrent(guestToHostDisplay(dpy), drawable, ctx);
}

void felix86_thunk_glXCopyContext(void* dpy, void* src, void* dst, unsigned long mask) {
    PRINTME;
    static auto host_glXCopyContext = (decltype(&felix86_thunk_glXCopyContext))dlsym(libGLX, "glXCopyContext");
    return host_glXCopyContext(guestToHostDisplay(dpy), src, dst, mask);
}

void felix86_thunk_glXSwapBuffers(void* dpy, u64 drawable) {
    PRINTME;
    static auto host_glXSwapBuffers = (decltype(&felix86_thunk_glXSwapBuffers))dlsym(libGLX, "glXSwapBuffers");
    return host_glXSwapBuffers(guestToHostDisplay(dpy), drawable);
}

u64 felix86_thunk_glXCreateGLXPixmap(void* dpy, XVisualInfo64* visual, u64 pixmap) {
    PRINTME;
    static auto host_glXCreateGLXPixmap = (decltype(&felix86_thunk_glXCreateGLXPixmap))dlsym(libGLX, "glXCreateGLXPixmap");
    void* host_dpy = guestToHostDisplay(dpy);
    XVisualInfo64* info = guestToHostVisualInfo(host_dpy, visual);
    u64 ret = host_glXCreateGLXPixmap(host_dpy, info, pixmap);
    host_XFree(info);
    return ret;
}

void felix86_thunk_glXDestroyGLXPixmap(void* dpy, u64 pixmap) {
    PRINTME;
    static auto host_glXDestroyGLXPixmap = (decltype(&felix86_thunk_glXDestroyGLXPixmap))dlsym(libGLX, "glXDestroyGLXPixmap");
    return host_glXDestroyGLXPixmap(guestToHostDisplay(dpy), pixmap);
}

int felix86_thunk_glXQueryExtension(void* dpy, int* errorb, int* event) {
    PRINTME;
    static auto host_glXQueryExtension = (decltype(&felix86_thunk_glXQueryExtension))dlsym(libGLX, "glXQueryExtension");
    return host_glXQueryExtension(guestToHostDisplay(dpy), errorb, event);
}

int felix86_thunk_glXQueryVersion(void* dpy, int* maj, int* min) {
    PRINTME;
    static auto host_glXQueryVersion = (decltype(&felix86_thunk_glXQueryVersion))dlsym(libGLX, "glXQueryVersion");
    return host_glXQueryVersion(guestToHostDisplay(dpy), maj, min);
}

int felix86_thunk_glXIsDirect(void* dpy, void* ctx) {
    PRINTME;
    static auto host_glXIsDirect = (decltype(&felix86_thunk_glXIsDirect))dlsym(libGLX, "glXIsDirect");
    return host_glXIsDirect(guestToHostDisplay(dpy), ctx);
}

int felix86_thunk_glXGetConfig(void* dpy, XVisualInfo64* visual, int attrib, int* value) {
    PRINTME;
    static auto host_glXGetConfig = (decltype(&felix86_thunk_glXGetConfig))dlsym(libGLX, "glXGetConfig");
    void* host_dpy = guestToHostDisplay(dpy);
    XVisualInfo64* info = guestToHostVisualInfo(host_dpy, visual);
    int ret = host_glXGetConfig(host_dpy, info, attrib, value);
    host_XFree(info);
    return ret;
}

const char* felix86_thunk_glXQueryExtensionsString(void* dpy, int screen) {
    PRINTME;
    static auto host_glXQueryExtensionsString = (decltype(&felix86_thunk_glXQueryExtensionsString))dlsym(libGLX, "glXQueryExtensionsString");
    return host_glXQueryExtensionsString(guestToHostDisplay(dpy), screen);
}

const char* felix86_thunk_glXQueryServerString(void* dpy, int screen, int name) {
    PRINTME;
    static auto host_glXQueryServerString = (decltype(&felix86_thunk_glXQueryServerString))dlsym(libGLX, "glXQueryServerString");
    return host_glXQueryServerString(guestToHostDisplay(dpy), screen, name);
}

const char* felix86_thunk_glXGetClientString(void* dpy, int name) {
    PRINTME;
    static auto host_glXGetClientString = (decltype(&felix86_thunk_glXGetClientString))dlsym(libGLX, "glXGetClientString");
    return host_glXGetClientString(guestToHostDisplay(dpy), name);
}

void** felix86_thunk_glXChooseFBConfig(void* guest_display, int screen, const int* attribList, int* nitems) {
    PRINTME;
    static auto host_glXChooseFBConfig = (decltype(&felix86_thunk_glXChooseFBConfig))dlsym(libGLX, "glXChooseFBConfig");
    void** ptr = host_glXChooseFBConfig(guestToHostDisplay(guest_display), screen, attribList, nitems);
    return relocateArrayToGuest(ptr, *nitems);
}

int felix86_thunk_glXGetFBConfigAttrib(void* dpy, void* config, int attribute, int* value) {
    PRINTME;
    static auto host_glXGetFBConfigAttrib = (decltype(&felix86_thunk_glXGetFBConfigAttrib))dlsym(libGLX, "glXGetFBConfigAttrib");
    return host_glXGetFBConfigAttrib(guestToHostDisplay(dpy), config, attribute, value);
}

void* felix86_thunk_glXGetCurrentDisplay() {
    PRINTME;
    static auto host_glXGetCurrentDisplay = (decltype(&felix86_thunk_glXGetCurrentDisplay))dlsym(libGLX, "glXGetCurrentDisplay");
    return hostToGuestDisplay(host_glXGetCurrentDisplay());
}

void** felix86_thunk_glXGetFBConfigs(void* dpy, int screen, int* nelements) {
    PRINTME;
    static auto host_glXGetFBConfigs = (decltype(&felix86_thunk_glXGetFBConfigs))dlsym(libGLX, "glXGetFBConfigs");
    void** ptr = host_glXGetFBConfigs(guestToHostDisplay(dpy), screen, nelements);
    return relocateArrayToGuest(ptr, *nelements);
}

XVisualInfo64* felix86_thunk_glXGetVisualFromFBConfig(void* guest_display, void* config) {
    PRINTME;
    static auto host_glXGetVisualFromFBConfig = (decltype(&felix86_thunk_glXGetVisualFromFBConfig))dlsym(libGLX, "glXGetVisualFromFBConfig");
    return hostToGuestVisualInfo(guest_display, host_glXGetVisualFromFBConfig(guestToHostDisplay(guest_display), config));
}

u64 felix86_thunk_glXCreateWindow(void* dpy, void* config, u64 win, const int* attribList) {
    PRINTME;
    static auto host_glXCreateWindow = (decltype(&felix86_thunk_glXCreateWindow))dlsym(libGLX, "glXCreateWindow");
    return host_glXCreateWindow(guestToHostDisplay(dpy), config, win, attribList);
}

void felix86_thunk_glXDestroyWindow(void* dpy, u64 window) {
    PRINTME;
    static auto host_glXDestroyWindow = (decltype(&felix86_thunk_glXDestroyWindow))dlsym(libGLX, "glXDestroyWindow");
    return host_glXDestroyWindow(guestToHostDisplay(dpy), window);
}

u64 felix86_thunk_glXCreatePixmap(void* dpy, void* config, u64 pixmap, const int* attribList) {
    PRINTME;
    static auto host_glXCreatePixmap = (decltype(&felix86_thunk_glXCreatePixmap))dlsym(libGLX, "glXCreatePixmap");
    return host_glXCreatePixmap(guestToHostDisplay(dpy), config, pixmap, attribList);
}

void felix86_thunk_glXDestroyPixmap(void* dpy, u64 pixmap) {
    PRINTME;
    static auto host_glXDestroyPixmap = (decltype(&felix86_thunk_glXDestroyPixmap))dlsym(libGLX, "glXDestroyPixmap");
    return host_glXDestroyPixmap(guestToHostDisplay(dpy), pixmap);
}

u64 felix86_thunk_glXCreatePbuffer(void* dpy, void* config, const int* attribList) {
    PRINTME;
    static auto host_glXCreatePbuffer = (decltype(&felix86_thunk_glXCreatePbuffer))dlsym(libGLX, "glXCreatePbuffer");
    return host_glXCreatePbuffer(guestToHostDisplay(dpy), config, attribList);
}

void felix86_thunk_glXDestroyPbuffer(void* dpy, u64 pbuf) {
    PRINTME;
    static auto host_glXDestroyPbuffer = (decltype(&felix86_thunk_glXDestroyPbuffer))dlsym(libGLX, "glXDestroyPbuffer");
    return host_glXDestroyPbuffer(guestToHostDisplay(dpy), pbuf);
}

void felix86_thunk_glXQueryDrawable(void* dpy, u64 draw, int attribute, unsigned int* value) {
    PRINTME;
    static auto host_glXQueryDrawable = (decltype(&felix86_thunk_glXQueryDrawable))dlsym(libGLX, "glXQueryDrawable");
    return host_glXQueryDrawable(guestToHostDisplay(dpy), draw, attribute, value);
}

void* felix86_thunk_glXCreateNewContext(void* dpy, void* config, int renderType, void* shareList, int direct) {
    PRINTME;
    static auto host_glXCreateNewContext = (decltype(&felix86_thunk_glXCreateNewContext))dlsym(libGLX, "glXCreateNewContext");
    return host_glXCreateNewContext(guestToHostDisplay(dpy), config, renderType, shareList, direct);
}

int felix86_thunk_glXMakeContextCurrent(void* dpy, u64 draw, u64 read, void* ctx) {
    PRINTME;
    static auto host_glXMakeContextCurrent = (decltype(&felix86_thunk_glXMakeContextCurrent))dlsym(libGLX, "glXMakeContextCurrent");
    return host_glXMakeContextCurrent(guestToHostDisplay(dpy), draw, read, ctx);
}

int felix86_thunk_glXQueryContext(void* dpy, void* ctx, int attribute, int* value) {
    PRINTME;
    static auto host_glXQueryContext = (decltype(&felix86_thunk_glXQueryContext))dlsym(libGLX, "glXQueryContext");
    return host_glXQueryContext(guestToHostDisplay(dpy), ctx, attribute, value);
}

void felix86_thunk_glXSelectEvent(void* dpy, u64 drawable, unsigned long mask) {
    PRINTME;
    static auto host_glXSelectEvent = (decltype(&felix86_thunk_glXSelectEvent))dlsym(libGLX, "glXSelectEvent");
    return host_glXSelectEvent(guestToHostDisplay(dpy), drawable, mask);
}

void felix86_thunk_glXGetSelectedEvent(void* dpy, u64 drawable, unsigned long* mask) {
    PRINTME;
    static auto host_glXGetSelectedEvent = (decltype(&felix86_thunk_glXGetSelectedEvent))dlsym(libGLX, "glXGetSelectedEvent");
    return host_glXGetSelectedEvent(guestToHostDisplay(dpy), drawable, mask);
}

void felix86_thunk_glXSwapIntervalEXT(void* guest_display, u64 drawable, int interval) {
    PRINTME;
    static auto host_glXSwapIntervalEXT = (decltype(&felix86_thunk_glXSwapIntervalEXT))host_glXGetProcAddress("glXSwapIntervalEXT");
    return host_glXSwapIntervalEXT(guestToHostDisplay(guest_display), drawable, interval);
}

void felix86_thunk_glDebugMessageCallback(void* callback, void* userParam) {
    LOG("Registered host callback with glDebugMessageCallback, ignoring");
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
    } else if (name == "vkCreateXlibSurfaceKHR") {
        return (void*)felix86_thunk_vkCreateXlibSurfaceKHR;
    } else if (name == "vkCreateXcbSurfaceKHR") {
        return (void*)felix86_thunk_vkCreateXcbSurfaceKHR;
    } else if (name == "vkGetPhysicalDeviceXlibPresentationSupportKHR") {
        return (void*)felix86_thunk_vkGetPhysicalDeviceXlibPresentationSupportKHR;
    } else if (name == "vkGetPhysicalDeviceXcbPresentationSupportKHR") {
        return (void*)felix86_thunk_vkGetPhysicalDeviceXcbPresentationSupportKHR;
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
#define MAP(n)                                                                                                                                       \
    if (name == #n)                                                                                                                                  \
    return (void*)felix86_thunk_##n

    MAP(glXGetProcAddress);
    MAP(glXGetProcAddress);
    MAP(glXChooseVisual);
    MAP(glXCreateContext);
    MAP(glXDestroyContext);
    MAP(glXMakeCurrent);
    MAP(glXCopyContext);
    MAP(glXSwapBuffers);
    MAP(glXCreateGLXPixmap);
    MAP(glXDestroyGLXPixmap);
    MAP(glXQueryExtension);
    MAP(glXQueryVersion);
    MAP(glXIsDirect);
    MAP(glXGetConfig);
    MAP(glXQueryExtensionsString);
    MAP(glXQueryServerString);
    MAP(glXGetClientString);
    MAP(glXChooseFBConfig);
    MAP(glXGetFBConfigAttrib);
    MAP(glXGetFBConfigs);
    MAP(glXGetVisualFromFBConfig);
    MAP(glXCreateWindow);
    MAP(glXDestroyWindow);
    MAP(glXCreatePixmap);
    MAP(glXDestroyPixmap);
    MAP(glXCreatePbuffer);
    MAP(glXDestroyPbuffer);
    MAP(glXQueryDrawable);
    MAP(glXCreateNewContext);
    MAP(glXMakeContextCurrent);
    MAP(glXQueryContext);
    MAP(glXSelectEvent);
    MAP(glXGetSelectedEvent);
    MAP(glXGetCurrentDisplay);
    MAP(glXCreateContextAttribsARB);
    MAP(glXQueryRendererIntegerMESA);
    MAP(glXQueryRendererStringMESA);
    MAP(glXSwapIntervalEXT);

    return nullptr;
#undef MAP
}

void* get_custom_gl_thunk(const std::string& name) {
    if (name == "glDebugMessageCallback" || name == "glDebugMessageCallbackAMD" || name == "glDebugMessageCallbackARB") {
        return (void*)felix86_thunk_glDebugMessageCallback;
    }

    return nullptr;
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
    std::filesystem::path thunks = g_config.thunks_path;
    ASSERT_MSG(std::filesystem::exists(thunks), "The thunks path set with FELIX86_THUNKS %s does not exist", thunks.c_str());
    std::string srootfs = g_config.rootfs_path.string();

#ifndef FELIX86_BUILD_THUNKING
    ERROR(
        "FELIX86_THUNKS is set, but this build of felix86 was not built with thunking support, enable FELIX86_BUILD_THUNKING in cmake configuration");
    return;
#endif

    bool thunk_vk = false;
    bool thunk_egl = false;
    bool thunk_glx = false;
    bool thunk_gl = false;
    bool thunk_wayland = false;
    bool thunk_luajit = false;
    std::string enabled_thunks = g_config.enabled_thunks;
    if (enabled_thunks != "none") {
        if (enabled_thunks == "all") {
            thunk_vk = true;
            thunk_egl = true;
            thunk_glx = true;
            thunk_gl = true;
            thunk_wayland = true;
            thunk_luajit = true;
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
                    thunk_gl = true;
                } else if (n == "libwayland-client" || n == "libwayland" || n == "wayland-client" || n == "wayland" || n == "wl") {
                    thunk_wayland = true;
                } else if (n == "libglx" || n == "glx") {
                    thunk_glx = true;
                    thunk_gl = true;
                } else if (n == "lua" || n == "luajit") {
                    thunk_luajit = true;
                } else {
                    WARN("Unknown option: %s in FELIX86_ENABLED_THUNKS", t.c_str());
                }
            }
        }
    }

    auto add_overlays = [&thunks](std::initializer_list<const char*> names) {
        std::filesystem::path thunk_path;
        for (const char* name : names) {
            std::filesystem::path path;
            if (!ThreadState::Get()->ctx.Mode32()) {
                path = thunks / "x86_64-linux-gnu" / name;
            } else {
                path = thunks / "i386-linux-gnu" / name;
            }
            if (std::filesystem::exists(path)) {
                thunk_path = path;
                break;
            }
        }

        if (!thunk_path.empty()) {
            for (const char* name : names) {
                Overlays::addOverlay(name, thunk_path);
            }
        } else {
            WARN("I couldn't find the thunked library for %s", names.begin());
        }
    };

    if (thunk_egl) {
        constexpr const char* egl_name = "libEGL.so.1";
        libEGL = dlopen(egl_name, RTLD_NOW | RTLD_LOCAL);
        if (!libEGL) {
            WARN("I couldn't open libEGL.so for thunking, error: %s", dlerror());
            thunk_egl = false;
        } else {
            add_overlays({"libEGL.so", "libEGL.so.1"});
        }
    }

    if (thunk_vk) {
        constexpr const char* vulkan_name = "libvulkan.so.1";
        libvulkan = dlopen(vulkan_name, RTLD_NOW | RTLD_LOCAL);
        if (!libvulkan) {
            WARN("I couldn't open libvulkan.so for thunking, error: %s", dlerror());
            thunk_vk = false;
        } else {
            add_overlays({"libvulkan.so", "libvulkan.so.1"});
        }
    }

    if (thunk_glx) {
        constexpr const char* glx_name = "libGLX.so.0";
        libGLX = dlopen(glx_name, RTLD_NOW | RTLD_LOCAL);
        if (!libGLX) {
            WARN("I couldn't open libGLX.so for thunking, error: %s", dlerror());
            thunk_glx = false;
        } else {
            add_overlays({"libGLX.so", "libGLX.so.0", "libGLX.so.0.0.0"});
        }

        constexpr const char* x11_name = "libX11.so";
        libX11 = dlopen(x11_name, RTLD_NOW | RTLD_LOCAL);
        if (!libX11) {
            WARN("I couldn't open libX11.so, error: %s", dlerror());
            thunk_glx = false;
        }
    }

    if (thunk_gl) {
        constexpr const char* gl_name = "libGL.so.1";
        libGL = dlopen(gl_name, RTLD_NOW | RTLD_LOCAL);
        if (!libGL) {
            WARN("I couldn't open libGL.so for thunking, error: %s", dlerror());
            thunk_gl = false;
        } else {
            add_overlays({"libGL.so", "libGL.so.1", "libGL.so.1.7.0"});
        }
    }

    if (thunk_wayland) {
        constexpr const char* wayland_name = "libwayland-client.so.0";
        libwayland = dlopen(wayland_name, RTLD_NOW | RTLD_LOCAL);
        if (!libwayland) {
            WARN("I couldn't open libwayland-client.so for thunking, error: %s", dlerror());
            thunk_wayland = false;
        } else {
            add_overlays({"libwayland-client.so", "libwayland-client.so.0"});
        }
    }

    if (thunk_luajit) {
        constexpr const char* luajit_name = "libluajit-5.1.so";
        libluajit = dlopen(luajit_name, RTLD_NOW | RTLD_LOCAL);
        if (!libluajit) {
            // Also try libluajit.so just in case
            libluajit = dlopen("lubluajit.so", RTLD_NOW | RTLD_LOCAL);
        }

        if (!libluajit) {
            WARN("I couldn't open libluajit-5.1.so for thunking, error: %s", dlerror());
        } else {
            add_overlays({"libluajit-5.1.so", "libluajit-5.1.so.2", "libluajit.so"});
        }
    }

    for (u32 i = 0; i < sizeof(thunk_metadata) / sizeof(Thunk); i++) {
        Thunk& metadata = thunk_metadata[i];
        void* ptr = nullptr;
        std::string lib_name = metadata.lib_name;
        if (lib_name == "libEGL.so" && thunk_egl && libEGL) {
            ptr = get_custom_egl_thunk(metadata.function_name);
            if (!ptr) {
                ptr = dlsym(libEGL, metadata.function_name);
            }
        } else if (lib_name == "libvulkan.so" && thunk_vk && libvulkan) {
            ptr = get_custom_vk_thunk(metadata.function_name);
            if (!ptr) {
                ptr = dlsym(libvulkan, metadata.function_name);
            }

            constexpr const char* x11_name = "libX11.so";
            libX11 = dlopen(x11_name, RTLD_NOW | RTLD_LOCAL);
            if (!libX11) {
                WARN("I couldn't open libX11.so, error: %s", dlerror());
                thunk_vk = false;
            }
        } else if (lib_name == "libwayland-client.so" && thunk_wayland && libwayland) {
            ptr = get_custom_wl_thunk(metadata.function_name);
            if (!ptr) {
                ptr = dlsym(libwayland, metadata.function_name);
            }
        } else if (lib_name == "libGLX.so" && thunk_glx && libGLX) {
            ptr = get_custom_glx_thunk(metadata.function_name);
            if (!ptr) {
                ptr = dlsym(libGLX, metadata.function_name);
            }
        } else if (lib_name == "libGL.so" && thunk_gl && libGL) {
            ptr = get_custom_gl_thunk(metadata.function_name);
            if (!ptr) {
                ptr = dlsym(libGL, metadata.function_name);
            }
        } else if (lib_name == "libluajit-5.1.so" && thunk_luajit && libluajit) {
            ptr = get_custom_luajit_thunk(metadata.function_name);
            if (!ptr) {
                ptr = dlsym(libluajit, metadata.function_name);
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

    // TODO: unordered_map
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
    rec.call(target);
    marshaller.emitEpilogue(as);

    return trampoline;
}

void* Thunks::generateTrampoline(Recompiler& rec, const char* name, const char* signature, u64 host_ptr) {
    ASSERT(signature);
    ASSERT(host_ptr);
    Assembler& as = rec.getAssembler();
    void* trampoline = as.GetCursorPointer();

    GuestToHostMarshaller marshaller(name, signature);
    marshaller.emitPrologue(as);
    rec.call(host_ptr);
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

            // The constructor may be called multiple times as GLX gets unloaded and reloaded so we need
            // to always make new trampolines (obligatory TODO: only generate trampolines per signature vs per pointer)
            if (name == "XGetVisualInfo") {
                felix86_guest_XGetVisualInfo = (XGetVisualInfoType)ABIMadness::hostToGuestTrampoline("q_qqqq", func);
            } else if (name == "XSync") {
                felix86_guest_XSync = (XSyncType)ABIMadness::hostToGuestTrampoline("d_qd", func);
            } else if (name == "malloc") {
                felix86_guest_malloc = (mallocType)ABIMadness::hostToGuestTrampoline("q_q", func);
            } else {
                ERROR("Unknown function name when trying to run constructor: %s", pointers->name);
            }

            pointers++;
        }

        ASSERT_MSG(felix86_guest_XGetVisualInfo, "Failed to find XGetVisualInfo in thunked libGLX");
        ASSERT_MSG(felix86_guest_XSync, "Failed to find XSync in thunked libGLX");
        ASSERT_MSG(felix86_guest_malloc, "Failed to find guest malloc");
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
    } else if (libname == "libvulkan.so") {
        while (pointers) {
            const void* func = pointers->func;
            if (!func) {
                break;
            }

            const std::string name = pointers->name;

            // The constructor may be called multiple times as Vulkan gets unloaded and reloaded so we need
            // to always make new trampolines (obligatory TODO: only generate trampolines per signature vs per pointer)
            if (name == "XGetVisualInfo") {
                felix86_guest_XGetVisualInfo = (XGetVisualInfoType)ABIMadness::hostToGuestTrampoline("q_qqqq", func);
            } else if (name == "XSync") {
                felix86_guest_XSync = (XSyncType)ABIMadness::hostToGuestTrampoline("d_qd", func);
            } else {
                ERROR("Unknown function name when trying to run constructor: %s", pointers->name);
            }

            pointers++;
        }

        ASSERT_MSG(felix86_guest_XGetVisualInfo, "Failed to find XGetVisualInfo in thunked libvulkan");
        ASSERT_MSG(felix86_guest_XSync, "Failed to find XSync in thunked libvulkan");
        VERBOSE("Constructor for %s finished!", lib);
        return; // everything ok!
    }

    ERROR("Unknown library name when trying to run constructor: %s", lib);
}
#endif
