#pragma once

#include <csignal>
#include "felix86/common/global.hpp"
#include "felix86/common/log.hpp"
#include "felix86/common/state.hpp"
#include "felix86/common/types.hpp"
#include "felix86/hle/ptrace.hpp"

#ifndef SA_NODEFER
#define SA_NODEFER 0x40000000
#endif

#ifndef SA_RESTORER
#define SA_RESTORER 0x04000000
#endif

struct RegisteredSignal {
    u64 func; // handler function of signal
    u64 mask; // blocked during execution of this handler
    int flags;
    u64 restorer; // for 32-bit apps
};

struct riscv_sigaction {
    union {
        void (*handler)(int);
        void (*sigaction)(int, siginfo_t*, void*);
    };

    uint64_t sa_flags;

    void (*restorer)();
    uint64_t sa_mask;
};

struct SignalHandlerTable {
    // Allocate the signal handler table in shared memory and return a pointer
    static SignalHandlerTable* create(SignalHandlerTable* copy) {
        SignalHandlerTable* table = new SignalHandlerTable;
        if (copy) {
            for (int i = 0; i < 64; i++) {
                table->table[i] = copy->table[i];
            }
        } else {
            memset(table->table, 0, sizeof(table->table));
        }
        return table;
    }

    static RegisteredSignal* getRegisteredSignal(SignalHandlerTable* table, int sig) {
        sig -= 1;
        ASSERT(sig >= 0 && sig <= 63);
        return &table->table[sig];
    }

    static void registerSignal(SignalHandlerTable* table, int sig, u64 func, u64 mask, int flags, u64 restorer) {
        ASSERT(sig != SIGKILL && sig != SIGSTOP);
        sig -= 1;
        ASSERT(sig >= 0 && sig <= 63);
        table->table[sig].flags = flags;
        table->table[sig].mask = mask;
        table->table[sig].func = func;
        table->table[sig].restorer = restorer;
    }

    RegisteredSignal table[64];
};
static_assert(std::is_trivially_constructible_v<SignalHandlerTable>);
static_assert(std::is_standard_layout_v<SignalHandlerTable>);
static_assert(offsetof(RegisteredSignal, func) == 0);
static_assert(offsetof(RegisteredSignal, mask) == 8);
static_assert(offsetof(RegisteredSignal, flags) == 16);
static_assert(offsetof(RegisteredSignal, restorer) == 24);
static_assert(sizeof(RegisteredSignal) == 32);
static_assert(sizeof(SignalHandlerTable) == sizeof(RegisteredSignal) * 64);

struct BlockMetadata;

struct XmmReg;

struct Signals {
    static void initialize();
    static void registerSignalHandler(ThreadState* state, int sig, u64 handler, u64 mask, int flags, u64 restorer);

    // To AND with a mask because these signals are necessary for the emulator to work
    static sigset_t* hostSignalMask() {
        static sigset_t mask;
        static bool initialized = false;
        if (!initialized) {
            sigfillset(&mask);
            sigdelset(&mask, SIGILL);
            sigdelset(&mask, SIGSEGV);
            sigdelset(&mask, SIGABRT);
            sigdelset(&mask, FELIX86_PTRACE_SIGNAL);
            initialized = true;
        }
        return &mask;
    }

    static void sigreturn(ThreadState* state, bool rt);

    static int sigprocmask(ThreadState* state, int how, sigset_t* new_set, sigset_t* old_set);
};
