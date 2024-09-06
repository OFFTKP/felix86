#include "runner.hpp"

#define TEST_ADD(x, y) FELIX86_TEST(add_##x##_##y) { \
    uint8_t ux = x, uy = y; \
    int8_t sx = ux, sy = uy; \
    mov(al, ux); mov(bl, uy); add(al, bl); \
    verify(X86_REF_RAX, (uint8_t)(x + y)); \
    uint8_t result; int8_t sresult; \
    bool overflow = __builtin_add_overflow(sx, sy, &sresult); \
    bool carry = __builtin_add_overflow(ux, uy, &result); \
    verify_c(carry); \
    verify_o(overflow); \
    int a_1 = ux & 0xF, a_2 = uy & 0xF; \
    int a = a_1 + a_2; \
    verify_a(a > 0xF); \
}

TEST_ADD(1, 5)
TEST_ADD(127, 1)
TEST_ADD(255, 1)
TEST_ADD(255, 255)
TEST_ADD(127, 127)
TEST_ADD(255, 0)
TEST_ADD(0, 255)
TEST_ADD(127, 0)
TEST_ADD(0, 127)
TEST_ADD(0x0F, 0x0F)
TEST_ADD(0x0F, 0x0E)
TEST_ADD(0x0E, 0x02)

FELIX86_TEST(add_sign_extend) {
    xor_(ebx, ebx);
    add(ebx, -1); // encoded as add rm32, imm8 - gets sign extended

    xor_(eax, eax);
    add(rax, -1337); // encoded as add r32, imm32 - gets sign extended

    verify(X86_REF_RBX, (uint32_t)-1ull);
    verify(X86_REF_RAX, (uint64_t)-1337);
}

FELIX86_TEST(add_eax_imm32) {
    mov(eax, 0x12345678);
    add(eax, 0x87654321);

    verify(X86_REF_RAX, 0x12345678 + 0x87654321);
}

FELIX86_TEST(add_dead_store_load) {
    mov(bl, 0x5);
    xor_(rax, rax);
    add(al, bl);
    add(al, bl);

    verify(X86_REF_RAX, 0x5 + 0x5);
}
