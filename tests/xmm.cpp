#include "runner.hpp"

FELIX86_TEST(movdqa) {
    static u64 mem[2] = {0x123456789abcdef0, 0xfedcba9876543210};
    mov(rax, (u64)mem);
    movdqa(xmm0, xword[rax]);

    verify_xmm(X86_REF_XMM0, {0x123456789abcdef0, 0xfedcba9876543210});
}

FELIX86_TEST(punpckldq) {
    #define TEST(dst, src, v1, v2, vres0, vres1) \
        mov(rax, (u64)v1); \
        movq(dst, rax); \
        mov(rax, (u64)v2); \
        movq(src, rax); \
        punpckldq(dst, src); \
        verify_xmm((x86_ref_e)(X86_REF_XMM0 + dst.getIdx()), {vres0, vres1})
    
    TEST(xmm0, xmm1, 0x6b8b4567327b23c6, 0x643c986966334873, 0x66334873327b23c6, 0x643c98616b8b4567);
}