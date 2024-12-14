#pragma once

#include <csignal>
#include <vector>
#include "felix86/common/utility.hpp"

struct RegisteredSignal {
    void* handler = (void*)SIG_DFL; // handler function of signal
    sigset_t mask = {};             // blocked during execution of this handler
    int flags = 0;
};

struct SignalHandler {
    SignalHandler();
    ~SignalHandler();

    void RegisterSignalHandler(int sig, void* handler, sigset_t mask, int flags);
    [[nodiscard]] RegisteredSignal GetSignalHandler(int sig);

private:
    std::vector<RegisteredSignal> handlers;
};