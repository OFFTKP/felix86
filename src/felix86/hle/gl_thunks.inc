// Read thunks.hpp -> signature format
// These are the functions that can be loaded by glXGetProcAddress or eglGetProcAddress
// X(lib name, func name, signature)
X("libGL.so", glGetString, "q_d")
X("libEGL.so", eglGetPlatformDisplayEXT, q_dqq)