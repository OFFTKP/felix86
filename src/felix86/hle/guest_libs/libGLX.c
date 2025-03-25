#include <GL/glx.h>
#include <X11/X.h>
#include <X11/Xutil.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

__attribute__((noinline)) XVisualInfo* __felix86_XGetVisualInfo(Display* display, long vinfo_mask, XVisualInfo* vinfo_template, int* nitems_return) {
    return XGetVisualInfo(display, vinfo_mask, vinfo_template, nitems_return);
}

__attribute__((noinline)) void __felix86_XSync(Display* display, Bool discard) {
    XSync(display, discard);
}

__attribute__((noinline)) XVisualInfo* __felix86_ConvertVisualInfo(Display* guest_display, XVisualInfo* host_info) {
    if (!guest_display || !host_info) {
        printf("libGLX-thunked.so: guest_display or host_info is null\n");
        return NULL;
    }

    XVisualInfo info;
    info.screen = host_info->screen;
    info.visualid = host_info->visualid;

    // TODO: free host_info

    int count;
    XVisualInfo* ret = XGetVisualInfo(guest_display, VisualScreenMask | VisualIDMask, &info, &count);

    if (count >= 1 && ret) {
        printf("libGLX-thunked.so: Converted visual info\n");
        return ret;
    } else {
        printf("libGLX-thunked.so: Visual info conversion failed\n");
        return NULL;
    }
}

typedef struct {
    GLXFBConfig* (*ptr_glXChooseFBConfig)(Display* dpy, int screen, const int* attrib_list, int* nelements);
    XVisualInfo* (*ptr_glXChooseVisual)(Display* dpy, int screen, int* attribList);
    void (*ptr_glXCopyContext)(Display* dpy, GLXContext src, GLXContext dst, unsigned long mask);
    GLXContext (*ptr_glXCreateContext)(Display* dpy, XVisualInfo* vis, GLXContext shareList, Bool direct);
    GLXPixmap (*ptr_glXCreateGLXPixmap)(Display* dpy, XVisualInfo* visual, Pixmap pixmap);
    GLXContext (*ptr_glXCreateNewContext)(Display* dpy, GLXFBConfig config, int render_type, GLXContext share_list, Bool direct);
    GLXPbuffer (*ptr_glXCreatePbuffer)(Display* dpy, GLXFBConfig config, const int* attrib_list);
    GLXPixmap (*ptr_glXCreatePixmap)(Display* dpy, GLXFBConfig config, Pixmap pixmap, const int* attrib_list);
    GLXWindow (*ptr_glXCreateWindow)(Display* dpy, GLXFBConfig config, Window win, const int* attrib_list);
    void (*ptr_glXDestroyContext)(Display* dpy, GLXContext ctx);
    void (*ptr_glXDestroyGLXPixmap)(Display* dpy, GLXPixmap pixmap);
    void (*ptr_glXDestroyPbuffer)(Display* dpy, GLXPbuffer pbuf);
    void (*ptr_glXDestroyPixmap)(Display* dpy, GLXPixmap pixmap);
    void (*ptr_glXDestroyWindow)(Display* dpy, GLXWindow win);
    const char* (*ptr_glXGetClientString)(Display* dpy, int name);
    int (*ptr_glXGetConfig)(Display* dpy, XVisualInfo* visual, int attrib, int* value);
    GLXContext (*ptr_glXGetCurrentContext)(void);
    GLXDrawable (*ptr_glXGetCurrentDrawable)(void);
    GLXDrawable (*ptr_glXGetCurrentReadDrawable)(void);
    int (*ptr_glXGetFBConfigAttrib)(Display* dpy, GLXFBConfig config, int attribute, int* value);
    GLXFBConfig* (*ptr_glXGetFBConfigs)(Display* dpy, int screen, int* nelements);
    __GLXextFuncPtr (*ptr_glXGetProcAddress)(const GLubyte* procName);
    __GLXextFuncPtr (*ptr_glXGetProcAddressARB)(const GLubyte* procName);
    void (*ptr_glXGetSelectedEvent)(Display* dpy, GLXDrawable draw, unsigned long* event_mask);
    XVisualInfo* (*ptr_glXGetVisualFromFBConfig)(Display* dpy, GLXFBConfig config);
    Bool (*ptr_glXIsDirect)(Display* dpy, GLXContext ctx);
    Bool (*ptr_glXMakeContextCurrent)(Display* dpy, GLXDrawable draw, GLXDrawable read, GLXContext ctx);
    Bool (*ptr_glXMakeCurrent)(Display* dpy, GLXDrawable drawable, GLXContext ctx);
    int (*ptr_glXQueryContext)(Display* dpy, GLXContext ctx, int attribute, int* value);
    void (*ptr_glXQueryDrawable)(Display* dpy, GLXDrawable draw, int attribute, unsigned int* value);
    Bool (*ptr_glXQueryExtension)(Display* dpy, int* errorb, int* event);
    const char* (*ptr_glXQueryExtensionsString)(Display* dpy, int screen);
    const char* (*ptr_glXQueryServerString)(Display* dpy, int screen, int name);
    Bool (*ptr_glXQueryVersion)(Display* dpy, int* maj, int* min);
    void (*ptr_glXSelectEvent)(Display* dpy, GLXDrawable draw, unsigned long event_mask);
    void (*ptr_glXSwapBuffers)(Display* dpy, GLXDrawable drawable);
    void (*ptr_glXUseXFont)(Font font, int first, int count, int list);
    void (*ptr_glXWaitGL)(void);
    void (*ptr_glXWaitX)(void);
} __glXGLCoreFunctions;

extern const __glXGLCoreFunctions __GLXGL_CORE_FUNCTIONS;

__attribute__((noinline)) void* __felix86_glXGetProcAddressSelf(const char* name) {
#define CASE(func)                                                                                                                                   \
    if (strcmp(name, #func) == 0) {                                                                                                                  \
        printf("Resolved %s to %p\n", name, __GLXGL_CORE_FUNCTIONS.ptr_##func);                                                                      \
        return __GLXGL_CORE_FUNCTIONS.ptr_##func;                                                                                                    \
    }

    CASE(glXChooseVisual);
    CASE(glXCreateContext);
    CASE(glXDestroyContext);
    CASE(glXMakeCurrent);
    CASE(glXCopyContext);
    CASE(glXSwapBuffers);
    CASE(glXCreateGLXPixmap);
    CASE(glXDestroyGLXPixmap);
    CASE(glXQueryExtension);
    CASE(glXQueryVersion);
    CASE(glXIsDirect);
    CASE(glXGetConfig);
    CASE(glXGetCurrentContext);
    CASE(glXGetCurrentDrawable);
    CASE(glXWaitGL);
    CASE(glXWaitX);
    CASE(glXUseXFont);
    CASE(glXChooseFBConfig);
    CASE(glXCreateNewContext);
    CASE(glXCreatePbuffer);
    CASE(glXCreatePixmap);
    CASE(glXCreateWindow);
    CASE(glXDestroyPbuffer);
    CASE(glXDestroyPixmap);
    CASE(glXDestroyWindow);
    CASE(glXGetClientString);
    CASE(glXGetCurrentReadDrawable);
    CASE(glXGetFBConfigAttrib);
    CASE(glXGetFBConfigs);
    CASE(glXGetProcAddress);
    CASE(glXGetProcAddressARB);
    CASE(glXGetSelectedEvent);
    CASE(glXGetVisualFromFBConfig);
    CASE(glXMakeContextCurrent);
    CASE(glXQueryContext);
    CASE(glXQueryDrawable);
    CASE(glXQueryExtensionsString);
    CASE(glXQueryServerString);
    CASE(glXSelectEvent);

    return NULL; // not one of the glX functions, will search using host function
}