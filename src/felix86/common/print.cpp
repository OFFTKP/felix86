#include <stdio.h>
#include "felix86/common/print.hpp"

void print_guest_register(x86_ref_e guest) {
    switch (guest) {
    case X86_REF_RAX:
        printf("rax");
        break;
    case X86_REF_RCX:
        printf("rcx");
        break;
    case X86_REF_RDX:
        printf("rdx");
        break;
    case X86_REF_RBX:
        printf("rbx");
        break;
    case X86_REF_RSP:
        printf("rsp");
        break;
    case X86_REF_RBP:
        printf("rbp");
        break;
    case X86_REF_RSI:
        printf("rsi");
        break;
    case X86_REF_RDI:
        printf("rdi");
        break;
    case X86_REF_R8:
        printf("r8");
        break;
    case X86_REF_R9:
        printf("r9");
        break;
    case X86_REF_R10:
        printf("r10");
        break;
    case X86_REF_R11:
        printf("r11");
        break;
    case X86_REF_R12:
        printf("r12");
        break;
    case X86_REF_R13:
        printf("r13");
        break;
    case X86_REF_R14:
        printf("r14");
        break;
    case X86_REF_R15:
        printf("r15");
        break;
    case X86_REF_CF:
        printf("cf");
        break;
    case X86_REF_PF:
        printf("pf");
        break;
    case X86_REF_AF:
        printf("af");
        break;
    case X86_REF_ZF:
        printf("zf");
        break;
    case X86_REF_SF:
        printf("sf");
        break;
    case X86_REF_OF:
        printf("of");
        break;
    case X86_REF_RIP:
        printf("rip");
        break;
    case X86_REF_FS:
        printf("fs");
        break;
    case X86_REF_GS:
        printf("gs");
        break;
    case X86_REF_XMM0 ... X86_REF_XMM15:
        printf("xmm%d", guest - X86_REF_XMM0);
        break;
    default:
        printf("Unknown guest");
        break;
    }
}

void print_state(x86_thread_state_t* state) {
    for (int i = 0; i < 16; i++) {
        print_guest_register(X86_REF_RAX + i);
        printf(" = %016lx\n", state->gprs[i]);
    }

    printf("cf = %d\n", state->cf);
    printf("pf = %d\n", state->pf);
    printf("af = %d\n", state->af);
    printf("zf = %d\n", state->zf);
    printf("sf = %d\n", state->sf);
    printf("of = %d\n", state->of);

    printf("rip = %016lx\n", state->rip);

    for (int i = 0; i < 16; i++) {
        printf("xmm%d = {", i);
        for (int j = 0; j < 2; j++) {
            printf("%016lx", state->xmm[i].data[j]);
            if (j != 1) {
                printf(", ");
            }
        }
        printf("}\n");
    }
}