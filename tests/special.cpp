#include "runner.hpp"

FELIX86_TEST(push_pop64) {
    mov(rax, 0x123456789bcdef0);
    push(rax);
    mov(rbx, 0);
    pop(rbx);

    mov(rcx, 0x123456789bcdef0);
    push(cx);
    mov(rdx, 0);
    pop(dx);

    verify(X86_REF_RBX, 0x123456789bcdef0);
    verify(X86_REF_RDX, 0xdef0);
}

FELIX86_TEST(stosq) {
    Xbyak::Label memory, end;
    mov(rdi, memory);
    mov(rax, 0x123456789bcdef0);
    stosq();
    jmp(end);
    L(memory);
    dq(0xdeadbeefdeadbeef);
    L(end);

    verify_memory((u64*)memory.getAddress(), 0x123456789bcdef0, sizeof(u64));
    verify(X86_REF_RDI, (u64)memory.getAddress() + 8);
}

FELIX86_TEST(stosw) {
    Xbyak::Label memory, end;
    mov(rdi, memory);
    mov(rax, 0x1234);
    stosw();
    jmp(end);
    L(memory);
    dw(0xdead);
    L(end);

    verify_memory((u16*)memory.getAddress(), 0x1234, sizeof(u16));
    verify(X86_REF_RDI, (u64)memory.getAddress() + 2);
}

FELIX86_TEST(stosd) {
    Xbyak::Label memory, end;
    mov(rdi, memory);
    mov(rax, 0x12345678);
    stosd();
    jmp(end);
    L(memory);
    dd(0xdeadbeef);
    L(end);

    verify_memory((u32*)memory.getAddress(), 0x12345678, sizeof(u32));
    verify(X86_REF_RDI, (u64)memory.getAddress() + 4);
}

FELIX86_TEST(rep_stosq) {
    Xbyak::Label memory, end;
    mov(rcx, 6);
    mov(rdi, memory);
    mov(rax, 0x123456789bcdef0);
    rep();
    stosq();
    jmp(end);
    L(memory);
    dq(0xdeadbeefdeadbeef);
    dq(0xdeadbeefdeadbeef);
    dq(0xdeadbeefdeadbeef);
    dq(0xdeadbeefdeadbeef);
    dq(0xdeadbeefdeadbeef);
    dq(0xdeadbeefdeadbeef);
    L(end);

    for (int i = 0; i < 6; i++) {
        verify_memory((u64*)memory.getAddress() + i, 0x123456789bcdef0, sizeof(u64));
    }

    verify(X86_REF_RDI, (u64)memory.getAddress() + 8 * 6);
}