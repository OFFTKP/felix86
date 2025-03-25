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

extern void* glXChooseVisual;
extern void* glXCreateContext;
extern void* glXDestroyContext;
extern void* glXMakeCurrent;
extern void* glXCopyContext;
extern void* glXSwapBuffers;
extern void* glXCreateGLXPixmap;
extern void* glXDestroyGLXPixmap;
extern void* glXQueryExtension;
extern void* glXQueryVersion;
extern void* glXIsDirect;
extern void* glXGetConfig;
extern void* glXGetCurrentContext;
extern void* glXGetCurrentDrawable;
extern void* glXWaitGL;
extern void* glXWaitX;
extern void* glXUseXFont;
extern void* glXChooseFBConfig;
extern void* glXCreateNewContext;
extern void* glXCreatePbuffer;
extern void* glXCreatePixmap;
extern void* glXCreateWindow;
extern void* glXDestroyPbuffer;
extern void* glXDestroyPixmap;
extern void* glXDestroyWindow;
extern void* glXGetClientString;
extern void* glXGetCurrentDisplay;
extern void* glXGetCurrentReadDrawable;
extern void* glXGetFBConfigAttrib;
extern void* glXGetFBConfigs;
extern void* glXGetProcAddress;
extern void* glXGetProcAddressARB;
extern void* glXGetSelectedEvent;
extern void* glXGetVisualFromFBConfig;
extern void* glXMakeContextCurrent;
extern void* glXQueryContext;
extern void* glXQueryDrawable;
extern void* glXQueryExtensionsString;
extern void* glXQueryServerString;
extern void* glXSelectEvent;

__attribute__((noinline)) void* __felix86_glXGetProcAddressSelf(const char* name) {
#define CASE(func)                                                                                                                                   \
    if (strcmp(name, #func) == 0) {                                                                                                                  \
        return func;                                                                                                                                 \
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
    CASE(glXGetCurrentDisplay);
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