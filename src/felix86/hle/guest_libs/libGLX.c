#include <X11/X.h>
#include <X11/Xutil.h>
#include <stdint.h>
#include <stdio.h>

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