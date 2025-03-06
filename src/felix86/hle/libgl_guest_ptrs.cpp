// See explanation in this header
#include "libgl_guest_ptrs.hpp"

// Bytes equivalent to invlpg[rax]; ret; -> see FAST_HANDLE(INVLPG) / generator.cpp for explanation
// This is the same thing we do for our generated libGLX-guest.so
// The reason we don't need to generate a libGL.so is because everything in libGL is just function pointers
// So we can allow the guest to use the guest libGL functions, which will call a function pointer that points
// exactly to these felix86_guest_*** functions below. Yada yada, the recompiler will see invlpg, make a trampoline
// to the host GL function, then a ret follows the invlpg so it will return as if the guest gl function was returning
#define INVLPG_RET "\x0F\x01\x38\xC3"

extern "C" { // I like them unmangled
// Each string will be the instructions and the name right after
#define X(libname, function, ...) const char* felix86_guest_##function = INVLPG_RET #function;
#include "gl_thunks.inc"
#undef X
}