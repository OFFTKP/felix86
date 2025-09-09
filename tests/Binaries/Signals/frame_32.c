#include <cstddef>
#include <stdint.h>
#include <stdio.h>
#include <sys/signal.h>
#include <unistd.h>

struct x64_fpxreg {
    unsigned short int significand[4];
    unsigned short int exponent;
    unsigned short int reserved[3];
};

struct Xmm128 {
    uint64_t val[2];
};

struct after_fpstate {
    uint16_t status;
    uint16_t magic; /* 0xffff: regular FPU data only */
    /* 0x0000: FXSR FPU data */

    /* FXSR FPU environment */
    uint32_t _fxsr_env[6]; /* FXSR FPU env is ignored */
    uint32_t mxcsr;
    uint32_t reserved;
    struct x64_fpxreg _fxsr_st[8]; /* FXSR FPU reg data is ignored */
    struct Xmm128 _xmm[8];         /* First 8 XMM registers */
    uint32_t padding[44];
    uint32_t padding2[12];
};

void signal_handler(int sig, siginfo_t* info, void* data) {
    ucontext_t* context = (ucontext_t*)data;
    printf("Sig: %d\n", sig);
    printf("addr: %x\n", (unsigned)info->si_addr);
    printf("fpstate: %x\n", (unsigned)context->uc_mcontext.fpregs);
    printf("errno: %x\n", (unsigned)info->si_errno);
    printf("code: %x\n", (unsigned)info->si_code);
    printf("Link: %p\n", context->uc_link);
    printf("    eax: %x\n", context->uc_mcontext.gregs[REG_EAX]);
    printf("    ecx: %x\n", context->uc_mcontext.gregs[REG_ECX]);
    printf("    ebx: %x\n", context->uc_mcontext.gregs[REG_EBX]);
    printf("    edx: %x\n", context->uc_mcontext.gregs[REG_EDX]);
    printf("    esp: %x\n", context->uc_mcontext.gregs[REG_ESP]);
    printf("    ebp: %x\n", context->uc_mcontext.gregs[REG_EBP]);
    printf("    esi: %x\n", context->uc_mcontext.gregs[REG_ESI]);
    printf("    edi: %x\n", context->uc_mcontext.gregs[REG_EDI]);
    printf("    uesp: %x\n", context->uc_mcontext.gregs[REG_UESP]);
    printf("    eip: %x\n", context->uc_mcontext.gregs[REG_EIP]);
    printf("    trapno: %x\n", context->uc_mcontext.gregs[REG_TRAPNO]);
    printf("    err: %x\n", context->uc_mcontext.gregs[REG_ERR]);
    printf("    flags: %x\n", context->uc_mcontext.gregs[REG_EFL]);
    printf("    cs: %x\n", context->uc_mcontext.gregs[REG_CS]);
    printf("    ds: %x\n", context->uc_mcontext.gregs[REG_DS]);
    printf("    es: %x\n", context->uc_mcontext.gregs[REG_ES]);
    printf("    fs: %x\n", context->uc_mcontext.gregs[REG_FS]);
    printf("    gs: %x\n", context->uc_mcontext.gregs[REG_GS]);
    printf("    ss: %x\n", context->uc_mcontext.gregs[REG_SS]);
    for (int i = 0; i < 8; i++) {
        printf("    st%d: %lx %x at %x\n", i, *(long*)context->uc_mcontext.fpregs->_st[i].significand, context->uc_mcontext.fpregs->_st[i].exponent,
               &context->uc_mcontext.fpregs->_st[i]);
    }
    printf("    tag %lx", context->uc_mcontext.fpregs->tag);
    printf("    cw %lx", context->uc_mcontext.fpregs->cw);
    printf("    sw %lx", context->uc_mcontext.fpregs->sw);
    printf("    status %lx\n", context->uc_mcontext.fpregs->status);
    printf("    offset %d\n", offsetof(_libc_fpstate, _st));

#define ASSERT(cond)                                                                                                                                 \
    if (!(cond)) {                                                                                                                                   \
        printf("Failed %s\n", #cond);                                                                                                                \
        _exit(1);                                                                                                                                    \
        __builtin_unreachable();                                                                                                                     \
    }

    ASSERT(sig == SIGILL);
    ASSERT(info->si_errno == 0);
    ASSERT(context->uc_mcontext.gregs[REG_EAX] == 0x12345678);
    ASSERT(context->uc_mcontext.gregs[REG_ECX] == 0x928139ab);
    ASSERT(context->uc_mcontext.gregs[REG_EBX] == 0xa1239518);
    ASSERT(context->uc_mcontext.gregs[REG_EDX] == 0xbfada921);
    ASSERT(context->uc_mcontext.gregs[REG_EBP] == 0xbf891290);
    ASSERT(context->uc_mcontext.gregs[REG_ESI] == 0x82b1e021);
    ASSERT(context->uc_mcontext.gregs[REG_EDI] == 0xa8c80123);

    for (int i = 0; i < 8; i++) {
        ASSERT(context->uc_mcontext.fpregs->_st[i].exponent == 0xFFFF);
    }

    ASSERT(*(long*)context->uc_mcontext.fpregs->_st[0].significand == 0x12345678);
    ASSERT(*(long*)context->uc_mcontext.fpregs->_st[1].significand == 0xa1239518);
    ASSERT(*(long*)context->uc_mcontext.fpregs->_st[2].significand == 0x928139ab);
    ASSERT(*(long*)context->uc_mcontext.fpregs->_st[3].significand == 0xbfada921);
    ASSERT(*(long*)context->uc_mcontext.fpregs->_st[4].significand == 0xbf891290);
    ASSERT(*(long*)context->uc_mcontext.fpregs->_st[5].significand == 0xa8c80123);
    ASSERT(*(long*)context->uc_mcontext.fpregs->_st[6].significand == 0x82b1e021);
    ASSERT(*(long*)context->uc_mcontext.fpregs->_st[7].significand == 0x12345678);

    after_fpstate* after = (after_fpstate*)((uint8_t*)&context->uc_mcontext.fpregs->status);
    printf("\nmagic: %lx\n", after->magic);

    ASSERT(after->magic == 0);
    ASSERT(after->_xmm[0].val[0] == 0x7F2C3D19B4A7D622);
    ASSERT(after->_xmm[0].val[1] == 0x1A93F4ECE15B78A4);
    ASSERT(after->_xmm[1].val[0] == 0x3C9B0F558D2A6BE1);
    ASSERT(after->_xmm[1].val[1] == 0x42F8719CAF014DEA);
    ASSERT(after->_xmm[2].val[0] == 0xD58C1B396BE7A321);
    ASSERT(after->_xmm[2].val[1] == 0x9F3CDA782B10F5C3);
    ASSERT(after->_xmm[3].val[0] == 0xF1E6479A03ACD1BE);
    ASSERT(after->_xmm[3].val[1] == 0x71C5E930D44B8E2F);
    ASSERT(after->_xmm[4].val[0] == 0xAEBD223F8C6E1499);
    ASSERT(after->_xmm[4].val[1] == 0xBB31A29E37CF48DA);
    ASSERT(after->_xmm[5].val[0] == 0xE8421B5F099D73A0);
    ASSERT(after->_xmm[5].val[1] == 0x16F4A7BB3E9C0842);
    ASSERT(after->_xmm[6].val[0] == 0x4D2A8F71C5B730AE);
    ASSERT(after->_xmm[6].val[1] == 0xA0E1DCCFF78E4563);
    ASSERT(after->_xmm[7].val[0] == 0xCB37824F1129BAD5);
    ASSERT(after->_xmm[7].val[1] == 0x5B89F03DAE3471BC);

    _exit(0x42);
}

__attribute__((naked)) void cause_signal() {
    asm(R"(
        .intel_syntax noprefix

        mov eax, 0x12345678
        mov ecx, 0x928139ab
        mov ebx, 0xa1239518
        mov edx, 0xbfada921
        mov ebp, 0xbf891290
        mov edi, 0xa8c80123
        lea esi, [nums]

        movups xmm0, [esi]
        movups xmm1, [esi+16*1]
        movups xmm2, [esi+16*2]
        movups xmm3, [esi+16*3]
        movups xmm4, [esi+16*4]
        movups xmm5, [esi+16*5]
        movups xmm6, [esi+16*6]
        movups xmm7, [esi+16*7]

        mov esi, 0x82b1e021

        emms
        movd mm0, eax        
        movd mm1, ebx        
        movd mm2, ecx        
        movd mm3, edx        
        movd mm4, ebp        
        movd mm5, edi        
        movd mm6, esi        
        movd mm7, eax        

        .byte 0x0f,0x0b
        hlt

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

int main() {
    struct sigaction sa;
    sa.sa_flags = SA_SIGINFO;
    sa.sa_sigaction = signal_handler;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGILL, &sa, NULL);
    cause_signal();
}