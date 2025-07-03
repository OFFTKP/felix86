#pragma once

#include <csignal>
#include "felix86/common/global.hpp"
#include "felix86/common/log.hpp"
#include "felix86/common/state.hpp"
#include "felix86/common/types.hpp"

#ifndef SA_NODEFER
#define SA_NODEFER 0x40000000
#endif

#ifndef SA_RESTORER
#define SA_RESTORER 0x04000000
#endif

struct RegisteredSignal {
    u64 func = {}; // handler function of signal
    u64 mask = {}; // blocked during execution of this handler
    int flags = 0;
    u64 restorer = {}; // for 32-bit apps
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
    SignalHandlerTable(const SignalHandlerTable& other) = delete;
    SignalHandlerTable& operator=(const SignalHandlerTable& other) = delete;
    SignalHandlerTable(SignalHandlerTable&& other) = delete;
    SignalHandlerTable& operator=(SignalHandlerTable&& other) = delete;

    // Allocate the signal handler table in shared memory and return a pointer
    static SignalHandlerTable* Create(SignalHandlerTable* copy) {
        SignalHandlerTable* table = new SignalHandlerTable;
        if (copy) {
            table->copy(copy);
        }
        return table;
    }

    RegisteredSignal* getRegisteredSignal(int sig) {
        sig -= 1;
        ASSERT(sig >= 0 && sig <= 63);
        return &table[sig];
    }

    void registerSignal(int sig, u64 func, u64 mask, int flags, u64 restorer) {
        sig -= 1;
        ASSERT(sig >= 0 && sig <= 63);
        table[sig].flags = flags;
        table[sig].mask = mask;
        table[sig].func = func;
        table[sig].restorer = restorer;
    }

private:
    SignalHandlerTable() = default;

    void copy(SignalHandlerTable* copy) {
        for (int i = 0; i < 64; i++) {
            table[i] = copy->table[i];
        }
    }

    RegisteredSignal table[64];
};

struct BlockMetadata;

struct XmmReg;

struct Signals {
    static void initialize();
    static void registerSignalHandler(ThreadState* state, int sig, u64 handler, u64 mask, int flags, u64 restorer);
    [[nodiscard]] static RegisteredSignal getSignalHandler(ThreadState* state, int sig);

    // To AND with a mask because these signals are necessary for the emulator to work
    static sigset_t* hostSignalMask() {
        static sigset_t mask;
        static bool initialized = false;
        if (!initialized) {
            sigfillset(&mask);
            sigdelset(&mask, SIGILL);
            sigdelset(&mask, SIGSEGV);
            sigdelset(&mask, SIGABRT);
            initialized = true;
        }
        return &mask;
    }

    // Once the custom guest signal handler is ran, it needs to sigreturn to get the correct state. But we don't really
    // want to run a sigreturn, we want to do what we are supposed to do in this function (reconstruct the state) and jump there.
    // So we want the signal handler to return here. So the address we give it is to a thunk that jumps here.
    static void sigreturn(ThreadState* state);

    static int sigsuspend(ThreadState* state, sigset_t* mask);

    static int sigprocmask(ThreadState* state, int how, sigset_t* new_set, sigset_t* old_set);
};

struct SignalGuard {
    SignalGuard();
    ~SignalGuard();
    SignalGuard(const SignalGuard&) = delete;
    SignalGuard& operator=(const SignalGuard&) = delete;

    void kill() {
        pthread_sigmask(SIG_SETMASK, &old_mask, nullptr);
        killed = true;
    }

private:
    sigset_t old_mask;
    bool killed = false;
};