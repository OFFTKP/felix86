#pragma once

#include <csignal>
#include "felix86/common/log.hpp"
#include "felix86/common/utility.hpp"

#ifndef SA_NODEFER
#define SA_NODEFER 0x40000000
#endif

struct RegisteredSignal {
    u64 func = {}; // handler function of signal
    u64 mask = {}; // blocked during execution of this handler
    int flags = 0;
};

struct real_sigaction {
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

    void registerSignal(int sig, u64 func, u64 mask, int flags) {
        sig -= 1;
        ASSERT(sig >= 0 && sig <= 63);
        table[sig].flags = flags;
        table[sig].mask = mask;
        table[sig].func = func;
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
    static void registerSignalHandler(ThreadState* state, int sig, u64 handler, u64 mask, int flags);
    [[nodiscard]] static RegisteredSignal getSignalHandler(ThreadState* state, int sig);

    // To AND with a mask because these signals are necessary for the emulator to work
    static sigset_t* hostSignalMask() {
        static sigset_t mask;
        static bool initialized = false;
        if (!initialized) {
            sigfillset(&mask);
            sigdelset(&mask, SIGILL);
            sigdelset(&mask, SIGSEGV);
            initialized = true;
        }
        return &mask;
    }

    // Once the custom guest signal handler is ran, it needs to sigreturn to get the correct state. But we don't really
    // want to run a sigreturn, we want to do what we are supposed to do in this function (reconstruct the state) and jump there.
    // So we want the signal handler to return here. So the address we give it is to a thunk that jumps here.
    static void sigreturn(ThreadState* state);

    static int sigsuspend(ThreadState* state, sigset_t* mask);

    // Hack explanation ahead!!!
    // Our recompiler checks guest addresses using an unordered_map to get the host address with the recompiled piece of code
    // Signal handlers return to an address the kernel pushes to the stack, which calls ra_sigreturn and all that.
    // We obviously can't call sigreturn because x86-64 and RISC-V are different. So we need a function that achieves the same result.
    // We are going to make a custom mapping with this magic address pointing to our sigreturn function (well, a thunk that jumps there, initialized
    // in emitSigreturnThunk). This address uses more than 56 bits, so it's normally not a valid x86-64 address, which means it will never collide
    // with a real address.
    static constexpr u64 magicSigreturnAddress() {
        return 0x1F00'0000'0000'0000;
    }

    static void setupFrame(uint64_t pc, ThreadState* state, sigset_t new_mask, const u64* host_gprs, const XmmReg* host_vecs, bool use_altstack,
                           bool in_jit_code, siginfo_t* host_siginfo);
};