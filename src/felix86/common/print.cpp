#include <cstdio>
#include "felix86/common/log.hpp"
#include "felix86/common/print.hpp"
#include "felix86/common/utility.hpp"

const char* print_guest_register(x86_ref_e guest) {
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
    case X86_REF_DF:
        return "df";
    case X86_REF_OF:
        return "of";
    case X86_REF_RIP:
        return "rip";
    case X86_REF_FS:
        return "fsbase";
    case X86_REF_GS:
        return "gsbase";
    case X86_REF_CS:
        return "cs";
    case X86_REF_ES:
        return "es";
    case X86_REF_DS:
        return "ds";
    case X86_REF_SS:
        return "ss";
#define CASE(name)                                                                                                                                   \
    case X86_REF_XMM##name:                                                                                                                          \
        return "xmm" #name;                                                                                                                          \
    case X86_REF_MM##name:                                                                                                                           \
        return "mm" #name;                                                                                                                           \
    case X86_REF_YMM##name:                                                                                                                          \
        return "ymm" #name;                                                                                                                          \
    case X86_REF_ST##name:                                                                                                                           \
        return "st" #name;
        CASE(0)
        CASE(1)
        CASE(2)
        CASE(3)
        CASE(4)
        CASE(5)
        CASE(6)
        CASE(7)
#undef CASE
#define CASE(name)                                                                                                                                   \
    case X86_REF_XMM##name:                                                                                                                          \
        return "xmm" #name;                                                                                                                          \
    case X86_REF_YMM##name:                                                                                                                          \
        return "ymm" #name;
        CASE(8)
        CASE(9)
        CASE(10)
        CASE(11)
        CASE(12)
        CASE(13)
        CASE(14)
        CASE(15)
#undef CASE
    case X86_REF_COUNT:
        UNREACHABLE();
        break;
    }

    UNREACHABLE();
    return "";
}

extern "C" __attribute__((visibility("default"))) void print_gprs(ThreadState* state) {
    for (int i = 0; i < 16; i++) {
        const char* guest = print_guest_register((x86_ref_e)(X86_REF_RAX + i));
        PLAIN("%s = %lx", guest, state->gprs[i]);
    }

    PLAIN("rip = %lx", state->rip);
    PLAIN("cf = %d", state->cf);
    PLAIN("pf = %d", state->pf);
    PLAIN("af = %d", state->af);
    PLAIN("zf = %d", state->zf);
    PLAIN("sf = %d", state->sf);
    PLAIN("df = %d", state->df);
    PLAIN("of = %d", state->of);
}

extern "C" __attribute__((visibility("default"))) void print_state(ThreadState* state) {
    print_gprs(state);

    for (int i = 0; i < 16; i++) {
        if (state->xmm[i].data[1] == 0) {
            PLAIN("xmm%d = %lx", i, state->xmm[i].data[0]);
        } else {
            PLAIN("xmm%d = %lx%lx", i, state->xmm[i].data[1], state->xmm[i].data[0]);
        }
    }

    update_symbols();
    dump_states();
}