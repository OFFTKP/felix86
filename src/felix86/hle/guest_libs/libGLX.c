#include <X11/X.h>
#include <X11/Xutil.h>
#include <stdint.h>

__attribute__((noinline)) XVisualInfo* __felix86_XGetVisualInfo(Display* display, long vinfo_mask, XVisualInfo* vinfo_template, int* nitems_return) {
    return XGetVisualInfo(display, vinfo_mask, vinfo_template, nitems_return);
}

__attribute__((noinline)) void __felix86_XSync(Display* display, Bool discard) {
    XSync(display, discard);
}
