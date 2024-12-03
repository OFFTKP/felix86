#pragma once

#include "felix86/aot/aot.hpp"
#include "felix86/backend/backend.hpp"
#include "felix86/common/log.hpp"
#include "felix86/common/x86.hpp"
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
};

struct Emulator {
    Emulator(const Config& config) : config(config), backend(*this) {
        g_emulator = this;
        fs.LoadRootFS(config.rootfs_path);
        fs.LoadExecutable(config.executable_path);
        ThreadState* main_state = createThreadState();
        setupMainStack(main_state);
        main_state->brk_current_address = fs.GetBRK();
        main_state->SetRip((u64)fs.GetEntrypoint());

        if (g_aot) {
            AOT aot(*this, fs.GetExecutable());
            aot.CompileAll();
        }
    }

    Emulator(const TestConfig& config) : backend(*this) {
        g_emulator = this;
        ThreadState* main_state = createThreadState();
        main_state->SetRip((u64)config.entrypoint);
        testing = true;
    }

    ~Emulator() = default;

    Filesystem& GetFilesystem() {
        return fs;
    }

    SignalHandler& GetSignalHandler() {
        return signal_handler;
    }

    Config& GetConfig() {
        return config;
    }

    ThreadState* GetTestState() {
        ASSERT(testing);
        ASSERT(thread_states.size() == 1);
        return &thread_states.front();
    }

    std::pair<void*, u64> GetCodeAt(u64 rip) {
        return backend.GetCodeAt(rip);
    }

    void Run();

    static void* CompileNext(Emulator* emulator, ThreadState* state);

    static void CompileFunction(Emulator* emulator, u64 rip) {
        emulator->compileFunction(rip);
    }

private:
    void setupMainStack(ThreadState* state);

    void* compileFunction(u64 rip);

    ThreadState* createThreadState();

    std::mutex compilation_mutex; // to synchronize compilation and function lookup
    std::list<ThreadState> thread_states;
    Config config;
    Backend backend;
    Filesystem fs;
    SignalHandler signal_handler;
    bool testing = false;
};
