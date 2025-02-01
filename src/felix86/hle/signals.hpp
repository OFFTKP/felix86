#pragma once

#include <array>
#include <csignal>
#include "felix86/common/utility.hpp"

struct RegisteredSignal {
    void* func = (void*)SIG_DFL; // handler function of signal
    sigset_t mask = {};          // blocked during execution of this handler
    int flags = 0;
};

using SignalHandlerTable = std::array<RegisteredSignal, 64>;

struct Signals {
    static void initialize();
    static void registerSignalHandler(ThreadState* state, int sig, void* handler, sigset_t mask, int flags);
    [[nodiscard]] static RegisteredSignal getSignalHandler(ThreadState* state, int sig);
    static constexpr u64 hostSignalMask() {
        return ~((1ULL << SIGBUS) | (1ULL << SIGILL));
    }

    // Once the custom guest signal handler is ran, it needs to sigreturn to get the correct state. But we don't really
    // want to run a sigreturn, we want to do what we are supposed to do in this function (reconstruct the state) and jump there.
    // So we want the signal handler to return here. So the address we give it is to a thunk that jumps here.
    static void sigreturn(ThreadState* state);

    // Our recompiler checks guest addresses using an unordered_map to get the host address with the recompiled piece of code
    // Signal handlers return to an address the kernel pushes to the stack, which calls ra_sigreturn and all that.
    // We obviously can't call sigreturn because x86-64 and RISC-V are different. So we need a function that achieves the same result.
    // We are going to make a custom mapping with this magic address pointing to our sigreturn function (well, a thunk that jumps there, initialized
    // in emitSigreturnThunk). This address uses more than 56 bits, so it's normally not a valid x86-64 address, which means it will never collide
    // with a real address.
    static constexpr u64 magicSigreturnAddress() {
        return 0x1F00'0000'0000'0000;
    }
};