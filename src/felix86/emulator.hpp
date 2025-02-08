#pragma once

#include <list>
#include <semaphore.h>
#include "felix86/common/log.hpp"
#include "felix86/common/state.hpp"
#include "felix86/hle/filesystem.hpp"
#include "felix86/hle/signals.hpp"
#include "felix86/v2/recompiler.hpp"

struct Config {
    std::filesystem::path rootfs_path;
    std::filesystem::path executable_path;
    std::vector<std::string> argv;
    std::vector<std::string> envp;
};

struct TestConfig {
    void* entrypoint;
};

struct Emulator {
    Emulator(const Config& config) : config(config) {
        g_emulator = this;
        fs.LoadRootFS(config.rootfs_path);
        fs.LoadExecutable(config.executable_path);
        auto main_state = ThreadState::Create();
        VERBOSE("Created thread state with tid %ld", main_state->tid);
        setupMainStack(main_state);
        main_state->signal_handlers = std::make_shared<SignalHandlerTable>();
        g_current_brk = fs.GetBRK();
        g_initial_brk = g_current_brk;
        main_state->SetRip((u64)fs.GetEntrypoint());
    }

    Emulator(const TestConfig& config) {
        g_emulator = this;
        auto main_state = ThreadState::Create();
        VERBOSE("Created thread state with tid %ld", main_state->tid);
        main_state->SetRip((u64)config.entrypoint);
        testing = true;
    }

    ~Emulator() = default;

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

private:
    void setupMainStack(ThreadState* state);

    Config config;
    Filesystem fs;
    bool testing = false;
    void* auxv_base = nullptr;
    size_t auxv_size = 0;
};
