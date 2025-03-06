// These pointers are going to be passed from glXGetProcAddress to the guest.
// They contain the invlpg instruction which signals "hey we are jumping to a thunk" and a ret.
// They also contain the name of the function right after.

// This is what happens:
// - Guest calls glXGetProcAddress
// - That is thunked, the trampoline leads us to felix86_glXGetProcAddress
// - This function will then:
//     - Return one of the pointers here to the guest, so that when it is called
//       our recompiler sees invlpg and goes "oh it's a thunk" and creates a trampoline
//     - Actually fill the thunkptr::glFunction with the equivalent host OpenGL function
#pragma once

#define X(libname, function, ...) extern "C" const char* felix86_guest_##function;
#include "gl_thunks.inc"
#undef X
