#pragma once

#include <sys/mman.h>
#include "felix86/common/log.hpp"
#include "felix86/common/state.hpp"
#include "felix86/hle/filesystem.hpp"
#include "felix86/hle/signals.hpp"

struct Config {
    std::filesystem::path rootfs_path;
    std::filesystem::path executable_path;
    std::vector<std::string> argv;
    std::vector<std::string> envp;
};

struct TestConfig {
    void* entrypoint;
    bool mode32;
};

struct Emulator {
    Emulator(const Config& config);

    Emulator(const TestConfig& config) {
        g_emulator = this;
        g_mode32 = config.mode32;
        auto main_state = ThreadState::Create(nullptr);
        VERBOSE("Created thread state with tid %ld", main_state->tid);
        main_state->SetRip((u64)config.entrypoint);
        testing = true;

        if (g_mode32) {
            initialize32BitAddressSpace();
        }
    }

    ~Emulator() {
        if (stack) {
            munmap(stack, stack_size);
        }
    }

    Filesystem& GetFilesystem() {
        return fs;
    }

    Config& GetConfig() {
        return config;
    }

    void Run();

    void StartThread(ThreadState* state);

    static void* CompileNext(ThreadState* state);

    std::pair<void*, size_t> GetAuxv() {
        return {auxv_base, auxv_size};
    }

    void CleanExit(ThreadState* state);

    void UnlinkBlock(ThreadState* state, u64 rip);

private:
    [[nodiscard]] std::pair<void*, size_t> setupMainStack(ThreadState* state);

    void initialize32BitAddressSpace();

    Config config;
    Filesystem fs;
    bool testing = false;
    void* auxv_base = nullptr;
    size_t auxv_size = 0;
    void* stack = nullptr;
    size_t stack_size = 0;
};
