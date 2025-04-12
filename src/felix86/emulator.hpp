#pragma once

#include <sys/mman.h>
#include "felix86/common/start_params.hpp"
#include "felix86/common/state.hpp"
#include "felix86/hle/filesystem.hpp"

struct TestConfig {
    u64 entrypoint;
    bool mode32;
};

struct Emulator {
    Filesystem& GetFilesystem() {
        return fs;
    }

    static void* CompileNext(ThreadState* state);

    static void LinkIndirect(u64 host_address, u64 guest_address, u8* link_address, ThreadState* state);

    static void UnlinkIndirect(ThreadState* state, u8* link_address);

    [[nodiscard]] static std::pair<ExitReason, int> Start(const StartParameters& config);

    static void StartTest(const TestConfig& config, u64 stack);

    // The exit dispatcher function also restores the stack pointer to what it was before
    // entering the dispatcher, so it can be called from anywhere
    [[noreturn]] static void ExitDispatcher(ThreadState* state);

private:
    [[nodiscard]] static std::pair<void*, size_t> setupMainStack(ThreadState* state);

    Filesystem fs;
    void* stack = nullptr;
    size_t stack_size = 0;
};
