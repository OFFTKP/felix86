#include <cstdio>
#include "felix86/common/log.hpp"
#include "felix86/common/print.hpp"

std::string print_guest_register(x86_ref_e guest) {
    switch (guest) {
    case X86_REF_RAX:
        return "rax";
    case X86_REF_RCX:
        return "rcx";
    case X86_REF_RDX:
        return "rdx";
    case X86_REF_RBX:
        return "rbx";
    case X86_REF_RSP:
        return "rsp";
    case X86_REF_RBP:
        return "rbp";
    case X86_REF_RSI:
        return "rsi";
    case X86_REF_RDI:
        return "rdi";
    case X86_REF_R8:
        return "r8";
    case X86_REF_R9:
        return "r9";
    case X86_REF_R10:
        return "r10";
    case X86_REF_R11:
        return "r11";
    case X86_REF_R12:
        return "r12";
    case X86_REF_R13:
        return "r13";
    case X86_REF_R14:
        return "r14";
    case X86_REF_R15:
        return "r15";
    case X86_REF_CF:
        return "cf";
    case X86_REF_PF:
        return "pf";
    case X86_REF_AF:
        return "af";
    case X86_REF_ZF:
        return "zf";
    case X86_REF_SF:
        return "sf";
    case X86_REF_OF:
        return "of";
    case X86_REF_RIP:
        return "rip";
    case X86_REF_FS:
        return "fsbase";
    case X86_REF_GS:
        return "gsbase";
    case X86_REF_XMM0 ... X86_REF_XMM15:
        return "xmm" + std::to_string(guest - X86_REF_XMM0);
    case X86_REF_ST0 ... X86_REF_ST7:
        return "st" + std::to_string(guest - X86_REF_ST0);
    default:
        UNREACHABLE();
    }
}

void print_state(ThreadState* state) {
    for (int i = 0; i < 16; i++) {
        std::string guest = print_guest_register((x86_ref_e)(X86_REF_RAX + i));
        printf("%s", guest.c_str());
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