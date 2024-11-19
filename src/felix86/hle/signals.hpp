#pragma once

#include <csignal>
#include <vector>
#include "felix86/common/utility.hpp"

struct RegisteredSignal {
    void* handler = (void*)SIG_DFL;
    sigset_t mask = {};
    int flags = 0;
};

struct SignalHandler {
    SignalHandler();
    ~SignalHandler();

    [[nodiscard]] RegisteredSignal RegisterSignalHandler(int sig, void* handler, sigset_t mask, int flags);

private:
    std::vector<RegisteredSignal> handlers;
};