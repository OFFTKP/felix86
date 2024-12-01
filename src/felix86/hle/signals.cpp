#include "felix86/hle/filesystem.hpp"
#include "felix86/hle/signals.hpp"

void signal_handler(int sig, siginfo_t* info, void* ctx) {
    WARN("Received signal: %d", sig);
}

SignalHandler::SignalHandler() : handlers(32) {
    // struct sigaction act;
    // act.sa_sigaction = signal_handler;
    // act.sa_flags = SA_SIGINFO;
    // sigemptyset(&act.sa_mask);
    // for (int i = 1; i < 32; i++) {
    //     sigaction(i, &act, nullptr);
    // }
}

SignalHandler::~SignalHandler() {
    // struct sigaction act;
    // act.sa_handler = SIG_DFL;
    // act.sa_flags = 0;
    // sigemptyset(&act.sa_mask);
    // for (int i = 1; i < 32; i++) {
    //     sigaction(i, &act, nullptr);
    // }
}

void SignalHandler::RegisterSignalHandler(int sig, void* handler, sigset_t mask, int flags) {
    ASSERT(sig > 0 && sig < 32);
    handlers[sig] = {handler, mask, flags};
    WARN("Registering signal handler for signal %d", sig);
}

RegisteredSignal SignalHandler::GetSignalHandler(int sig) {
    ASSERT(sig > 0 && sig < 32);
    return handlers[sig];
}