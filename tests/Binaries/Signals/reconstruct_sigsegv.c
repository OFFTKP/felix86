#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/ucontext.h>
#include <unistd.h>
#include "common.h"

enum {
    REG_R8 = 0,
#define REG_R8 REG_R8
    REG_R9,
#define REG_R9 REG_R9
    REG_R10,
#define REG_R10 REG_R10
    REG_R11,
#define REG_R11 REG_R11
    REG_R12,
#define REG_R12 REG_R12
    REG_R13,
#define REG_R13 REG_R13
    REG_R14,
#define REG_R14 REG_R14
    REG_R15,
#define REG_R15 REG_R15
    REG_RDI,
#define REG_RDI REG_RDI
    REG_RSI,
#define REG_RSI REG_RSI
    REG_RBP,
#define REG_RBP REG_RBP
    REG_RBX,
#define REG_RBX REG_RBX
    REG_RDX,
#define REG_RDX REG_RDX
    REG_RAX,
#define REG_RAX REG_RAX
    REG_RCX,
#define REG_RCX REG_RCX
    REG_RSP,
#define REG_RSP REG_RSP
    REG_RIP,
#define REG_RIP REG_RIP
    REG_EFL,
#define REG_EFL REG_EFL
    REG_CSGSFS, /* Actually short cs, gs, fs, __pad0.  */
#define REG_CSGSFS REG_CSGSFS
    REG_ERR,
#define REG_ERR REG_ERR
    REG_TRAPNO,
#define REG_TRAPNO REG_TRAPNO
    REG_OLDMASK,
#define REG_OLDMASK REG_OLDMASK
    REG_CR2
#define REG_CR2 REG_CR2
};

volatile int success = 0;
volatile int ok = 0;

void signal_handler(int sig, siginfo_t* info, void* ctx) {
    ucontext_t* ucontext = (ucontext_t*)ctx;
    mcontext_t* mcontext = &ucontext->uc_mcontext;

    uint64_t expected_rax = 0xffffffffff8eac25;
    uint64_t expected_rdx = 0xffffffffffffffff;
    uint64_t expected_rcx = 5555;

    printf("rax: %016llx\n", mcontext->gregs[REG_RAX]);
    printf("rdx: %016llx\n", mcontext->gregs[REG_RDX]);
    printf("rcx: %016llx\n", mcontext->gregs[REG_RCX]);
    printf("bad address: %p\n", info->si_addr);
    if (mcontext->gregs[REG_RAX] == expected_rax && mcontext->gregs[REG_RDX] == expected_rdx && mcontext->gregs[REG_RCX] == expected_rcx &&
        info->si_addr == (void*)0x7badbeef) {
        ok = 1;
    }

    // Set rdi to a valid pointer so the SIGSEGV returns
    mcontext->gregs[REG_RDI] = (long long)&success;
}

int main() {
    struct sigaction act;
    act.sa_sigaction = signal_handler;
    act.sa_flags = SA_SIGINFO;
    sigaction(SIGSEGV, &act, 0);

    asm(".intel_syntax noprefix");
    asm("mov rax, -1337");
    asm("cqo");
    asm("mov rcx, 5555");
    asm("imul rcx");

    // Cause a SIGSEGV
    asm("mov rdi, 0x7badbeef");
    asm("mov [rdi], rcx");
    asm(".att_syntax prefix");

    // After signal handler rdi will be changed to `success` variable
    if (success == 5555 && ok) {
        return FELIX86_BTEST_SUCCESS;
    } else {
        return 1;
    }
}