#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/ucontext.h>
#include <unistd.h>
#include "common.h"

enum {
    REG_R8 = 0,
    REG_R9,
    REG_R10,
    REG_R11,
    REG_R12,
    REG_R13,
    REG_R14,
    REG_R15,
    REG_RDI,
    REG_RSI,
    REG_RBP,
    REG_RBX,
    REG_RDX,
    REG_RAX,
    REG_RCX,
    REG_RSP,
    REG_RIP,
    REG_EFL,
    REG_CSGSFS, /* Actually short cs, gs, fs, __pad0.  */
    REG_ERR,
    REG_TRAPNO,
    REG_OLDMASK,
    REG_CR2
};

volatile int is_ok = 0;

__attribute__((naked)) void* get_rsp() {
    asm(R"(
        .intel_syntax noprefix
        mov rax, rsp
        ret
        .att_syntax prefix
    )");
}

__attribute__((naked)) void cause_signal() {
    asm(R"(
        .intel_syntax noprefix
        lea rsp, [rsp - 48]
        mov [rsp + 0], rbx
        mov [rsp + 8], rbp
        mov [rsp + 16], r12
        mov [rsp + 24], r13
        mov [rsp + 32], r14
        mov [rsp + 40], r15

        mov rax, 0x1234567890abcdef
        mov rbx, 0xa123951821bcdea9
        mov rcx, 0x928139abc9a0a082
        mov rdx, 0xbfada92108320a91
        mov rbp, 0xbf891290a0128392
        mov rdi, 0xa8c8012398a0b012
        mov rsi, 0x19231009b9d80102
        mov r8,  0xabbcdebabdabaefb
        mov r9,  0x921318ac8aabada8
        mov r10, 0x92928a9acbabda97
        mov r11, 0x7217983182389189
        mov r12, 0xabcabdabdea92829
        mov r13, 0xbeef98a89ad89a8d
        mov r14, 0xa9008a9291189302
        lea r15, [nums]

        movups xmm0, [r15]
        movups xmm1, [r15+16*1]
        movups xmm2, [r15+16*2]
        movups xmm3, [r15+16*3]
        movups xmm4, [r15+16*4]
        movups xmm5, [r15+16*5]
        movups xmm6, [r15+16*6]
        movups xmm7, [r15+16*7]
        movups xmm8, [r15+16*8]
        movups xmm9, [r15+16*9]
        movups xmm10, [r15+16*10]
        movups xmm11, [r15+16*11]
        movups xmm12, [r15+16*12]
        movups xmm13, [r15+16*13]
        movups xmm14, [r15+16*14]
        movups xmm15, [r15+16*15]

        emms
        movq mm0, r8        
        movq mm1, r9        
        movq mm2, r10        
        movq mm3, r11        
        movq mm4, r12        
        movq mm5, r13        
        movq mm6, r14        
        movq mm7, r15        

        .byte 0x0f,0x0b

        mov rbx, [rsp + 0]
        mov rbp, [rsp + 8]
        mov r12, [rsp + 16]
        mov r13, [rsp + 24]
        mov r14, [rsp + 32]
        mov r15, [rsp + 40]
        lea rsp, [rsp + 48]
        ret

        nums:
        .quad 0x7F2C3D19B4A7D622
        .quad 0x1A93F4ECE15B78A4
        .quad 0x3C9B0F558D2A6BE1
        .quad 0x42F8719CAF014DEA
        .quad 0xD58C1B396BE7A321
        .quad 0x9F3CDA782B10F5C3
        .quad 0xF1E6479A03ACD1BE
        .quad 0x71C5E930D44B8E2F
        .quad 0xAEBD223F8C6E1499
        .quad 0xBB31A29E37CF48DA
        .quad 0xE8421B5F099D73A0
        .quad 0x16F4A7BB3E9C0842
        .quad 0x4D2A8F71C5B730AE
        .quad 0xA0E1DCCFF78E4563
        .quad 0xCB37824F1129BAD5
        .quad 0x5B89F03DAE3471BC
        .quad 0xC9D1736EA24987B1
        .quad 0x13AEB5F02CCEF4C8
        .quad 0x7A8B4E913C502A6F
        .quad 0x8041DFCE9983ACB2
        .quad 0x3E527A17CB41F99A
        .quad 0x2F1C7B9D8AE0C434
        .quad 0xE39D58A70C4E3F22
        .quad 0x991ACB453DAE8720
        .quad 0xA5F8E3B42C9A7B01
        .quad 0x6D4B3C91B884D34E
        .quad 0xF92E8DC5137B2E7A
        .quad 0x2C3D49F0071EB58D
        .quad 0xA71DEEF6C244F0B3
        .quad 0x8F5A0B317E2493DE
        .quad 0xBD41985A3B3C7FA6
        .quad 0x53F17D6E9A1054D9
        .att_syntax prefix
    )");
}

bool compare_xmm(uint32_t* ptr, uint64_t low, uint64_t high) {
    uint64_t low_actual = *(uint64_t*)ptr;
    uint64_t high_actual = *(uint64_t*)(ptr + 2);
    if (low != low_actual || high != high_actual) {
        return false;
    }
    return true;
}

void signal_handler(int sig, siginfo_t* info, void* ucontext) {
#define MUST(reg, value)                                                                                                                             \
    if ((uc->uc_mcontext.gregs[reg] != value)) {                                                                                                     \
        is_ok = 0;                                                                                                                                   \
        printf("GPR %s mismatch: %lx vs %lx\n", #reg, (uint64_t)uc->uc_mcontext.gregs[reg], value);                                                  \
    }
#define MUST_XMM(xmm, num0, num1)                                                                                                                    \
    if (!(compare_xmm(uc->uc_mcontext.fpregs->_xmm[xmm].element, num0, num1))) {                                                                     \
        is_ok = 0;                                                                                                                                   \
        uint64_t low = *(uint64_t*)uc->uc_mcontext.fpregs->_xmm[xmm].element;                                                                        \
        uint64_t high = *(uint64_t*)(uc->uc_mcontext.fpregs->_xmm[xmm].element + 2);                                                                 \
        printf("XMM%d mismatch: %lx%lx vs %lx%lx\n", xmm, high, low, num1, num0);                                                                    \
    }
    is_ok = 1;
    void* rsp = get_rsp();
    ucontext_t* uc = (ucontext_t*)ucontext;

    MUST(REG_RAX, 0x1234567890abcdef);
    MUST(REG_RBX, 0xa123951821bcdea9);
    MUST(REG_RCX, 0x928139abc9a0a082);
    MUST(REG_RDX, 0xbfada92108320a91);
    MUST(REG_RBP, 0xbf891290a0128392);
    MUST(REG_RDI, 0xa8c8012398a0b012);
    MUST(REG_RSI, 0x19231009b9d80102);
    MUST(REG_R8, 0xabbcdebabdabaefb);
    MUST(REG_R9, 0x921318ac8aabada8);
    MUST(REG_R10, 0x92928a9acbabda97);
    MUST(REG_R11, 0x7217983182389189);
    MUST(REG_R12, 0xabcabdabdea92829);
    MUST(REG_R13, 0xbeef98a89ad89a8d);
    MUST(REG_R14, 0xa9008a9291189302);

    MUST_XMM(0, 0x7f2c3d19b4a7d622, 0x1a93f4ece15b78a4);
    MUST_XMM(1, 0x3c9b0f558d2a6be1, 0x42f8719caf014dea);
    MUST_XMM(2, 0xd58c1b396be7a321, 0x9f3cda782b10f5c3);
    MUST_XMM(3, 0xf1e6479a03acd1be, 0x71c5e930d44b8e2f);
    MUST_XMM(4, 0xaebd223f8c6e1499, 0xbb31a29e37cf48da);
    MUST_XMM(5, 0xe8421b5f099d73a0, 0x16f4a7bb3e9c0842);
    MUST_XMM(6, 0x4d2a8f71c5b730ae, 0xa0e1dccff78e4563);
    MUST_XMM(7, 0xcb37824f1129bad5, 0x5b89f03dae3471bc);
    MUST_XMM(8, 0xc9d1736ea24987b1, 0x13aeb5f02ccef4c8);
    MUST_XMM(9, 0x7a8b4e913c502a6f, 0x8041dfce9983acb2);
    MUST_XMM(10, 0x3e527a17cb41f99a, 0x2f1c7b9d8ae0c434);
    MUST_XMM(11, 0xe39d58a70c4e3f22, 0x991acb453dae8720);
    MUST_XMM(12, 0xa5f8e3b42c9a7b01, 0x6d4b3c91b884d34e);
    MUST_XMM(13, 0xf92e8dc5137b2e7a, 0x2c3d49f0071eb58d);
    MUST_XMM(14, 0xa71deef6c244f0b3, 0x8f5a0b317e2493de);
    MUST_XMM(15, 0xbd41985a3b3c7fa6, 0x53f17d6e9a1054d9);

    for (int i = 0; i < 8; i++) {
        uint64_t mm;
        memcpy(&mm, &uc->uc_mcontext.fpregs->_st[i], sizeof(uint64_t));

        uint16_t ones;
        memcpy(&ones, (uint8_t*)&uc->uc_mcontext.fpregs->_st[i] + 8, sizeof(uint16_t));

        uint64_t reg;
        memcpy(&reg, &uc->uc_mcontext.gregs[REG_R8 + i], sizeof(uint64_t));

        if (mm != reg) {
            is_ok = 0;
            printf("%lx %lx\n", mm, reg);
        }

        if (ones != 0xFFFF) {
            is_ok = 0;
        }
    }

    if (sig != SIGILL) {
        is_ok = 0;
    }

    uc->uc_mcontext.gregs[REG_RIP] += 2;
}

int main() {
    struct sigaction act;
    act.sa_sigaction = signal_handler;
    act.sa_flags = SA_SIGINFO;
    sigemptyset(&act.sa_mask);
    sigaction(SIGILL, &act, 0);

    cause_signal();

    if (is_ok) {
        return FELIX86_BTEST_SUCCESS;
    } else {
        return 1;
    }
}