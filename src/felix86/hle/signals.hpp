#pragma once

#include <array>
#include <csignal>
#include "felix86/common/utility.hpp"

struct RegisteredSignal {
    void* handler = (void*)SIG_DFL; // handler function of signal
    sigset_t mask = {};             // blocked during execution of this handler
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
};