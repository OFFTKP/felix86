#pragma once

#include <sys/mman.h>
#include "felix86/common/config.hpp"
#include "felix86/common/log.hpp"
#include "felix86/common/state.hpp"
#include "felix86/hle/filesystem.hpp"

struct TestConfig {
    HostAddress entrypoint;
    bool mode32;
};

struct Emulator {
    Filesystem& GetFilesystem() {
        return fs;
    }

    static void* CompileNext(ThreadState* state);

    [[nodiscard]] static std::pair<ExitReason, int> Start(const Config& config);

    static void StartTest(const TestConfig& config, GuestAddress stack);

    // The exit dispatcher function also restores the stack pointer to what it was before
    // entering the dispatcher, so it can be called from anywhere
    [[noreturn]] static void ExitDispatcher(ThreadState* state);

private:
    [[nodiscard]] static std::pair<void*, size_t> setupMainStack(ThreadState* state);

    static void initialize32BitAddressSpace();
    static void uninitialize32BitAddressSpace();

    Filesystem fs;
    void* stack = nullptr;
    size_t stack_size = 0;
};
