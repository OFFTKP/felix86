#pragma once

#include <string>
#include <biscuit/assembler.hpp>

/*
    We use a custom signature format to describe the function.
    return type, _, arguments.

    void -> v
    integer -> q, d, w, b with x86 naming convention (qword, dword, word, byte)
    float, double -> F, D
    add others here when we need them (will we?)

    `x` means zero this argument, useful for zeroing specific arguments
    For example zeroing the allocation callbacks in Vulkan

    example:
    v_dwF -> void my_func(int a, short b, float c)

    We only thunk simple functions so this should be fine.

    x86-64 ABI:
    If the class is INTEGER, the next available register of the sequence %rdi, %rsi, %rdx,
    %rcx, %r8 and %r9 is used. Return value goes in %rax.

    If the class is SSE, the next available vector register is used, the registers are taken
    in the order from %xmm0 to %xmm7. Return value goes in %xmm0.

    Note: When x86-64 functions return they zero the upper 96 or 64 bits of xmm0.

    RISC-V ABI:
    Uses a0-a7, fa0-fa7 and the rest goes in the stack
    Return value goes in a0 or fa0. a1 can also be used but we don't have 128-bit return values.
*/

// For when guest recompiled code calls a host function and we need to do argument marshalling
struct GuestToHostMarshaller {
    explicit GuestToHostMarshaller(const std::string& name, const std::string& signature);

    void emitPrologue(biscuit::Assembler& as);
    void emitEpilogue(biscuit::Assembler& as);

private:
    std::string name, signature;
    int stack_size;
};

struct ABIMadness {
    static void* hostToGuestTrampoline(const char* signature, void* guest_function);
};