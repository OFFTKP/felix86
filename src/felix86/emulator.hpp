#pragma once

#include <list>
#include "felix86/common/log.hpp"
#include "felix86/common/x86.hpp"
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
    Emulator(const Config& config) : config(config), recompiler(*this) {
        g_emulator = this;
        fs.LoadRootFS(config.rootfs_path);
        fs.LoadExecutable(config.executable_path);
        ThreadState* main_state = CreateThreadState();
        setupMainStack(main_state);
        main_state->brk_current_address = fs.GetBRK();
        main_state->SetRip((u64)fs.GetEntrypoint());
    }

    Emulator(const TestConfig& config) : recompiler(*this) {
        g_emulator = this;
        ThreadState* main_state = CreateThreadState();
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

    ThreadState* GetTestState() {
        ASSERT(testing);
        ASSERT(thread_states.size() == 1);
        return &thread_states.front();
    }

    Assembler& GetAssembler() {
        return recompiler.getAssembler();
    }

    void Run();

    void StartThread(ThreadState* state);

    static void* CompileNext(Emulator* emulator, ThreadState* state);

    static void CompileFunction(Emulator* emulator, u64 rip) {
        emulator->compileFunction(rip);
    }

    std::pair<void*, size_t> GetAuxv() {
        return {auxv_base, auxv_size};
    }

    Recompiler& GetRecompiler() {
        return recompiler;
    }

    ThreadState* CreateThreadState();

private:
    void setupMainStack(ThreadState* state);

    void* compileFunction(u64 rip);

    std::mutex compilation_mutex; // to synchronize compilation and function lookup
    std::list<ThreadState> thread_states;
    Config config;
    Filesystem fs;
    Recompiler recompiler;
    bool testing = false;
    void* auxv_base = nullptr;
    size_t auxv_size = 0;
};
