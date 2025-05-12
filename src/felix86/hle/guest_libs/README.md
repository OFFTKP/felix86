# Thunked libraries

Our approach to thunked libraries is perhaps a little hacky, but I think it's cute.

Basically we create some x86-64 libraries that have instructions no real userspace program should ever have.
They are followed by a ret (to return from the guest function) and a null-terminated string.

Example:

```
global glXChooseVisual
align 16
glXChooseVisual:
invlpg [rax]
ret
db "glXChooseVisual", 0
```

When the recompiler hits the invlpg instruction, it is configured to check that the operand is [rax] and that the string
that follows after the ret is some known function we wish to thunk. It will then create a trampoline instead of emitting
RISC-V code to emulate an invlpg. INVLPG is not an instruction any userspace program can run, it needs ring 0 access.
We can abuse this to encode more info. For example, an invlpg[rbx] could mean a different thing, etc.

For now:
```
invlpg [rax]
```
tells the recompiler "I am a thunked function, my name is right after the RET, please make a trampoline."

```
invlpg [rbx]
```
Exists in the constructor, called when a library is loaded. Essentially notifies our recompiler that a library
has been loaded, the name of the library follows after the RET (this time as a pointer for convenience), and after the name
exists a null terminated list of {const char*, void*}, where the const char* is the name of a guest function and the void* is the pointer
to the function itself. This is because some times we wanna call guest code from host code to do some stuff.
```
invlpg [rcx]
```
Similar to invlpg [rax], but instead of a name, a pointer and signature is provided. This is useful for GetProcAddress functions
that want to return a guest-callable pointer to a host function.
```
invlpg [rdx]
```
Special "ret" that returns to host code. Essentially calls ExitDispatcher with EXIT_REASON_GUEST_CODE_FINISHED. This is useful
for when returning from recompiled guest code that is called from a host (thunked) function. A trampoline is generated for guest
callbacks that basically enters the dispatcher and when the guest function returns it calls invlpg [rdx], which will call
ExitDispatcher. Due to how ExitDispatcher works, it will pop the frame and return to whatever called EnterDispatcher, which
was our trampoline.