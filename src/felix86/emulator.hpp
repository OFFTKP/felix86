#pragma once

#include <list>
#include <semaphore.h>
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
    Emulator(const Config& config) : config(config), recompiler() {
        g_emulator = this;
        fs.LoadRootFS(config.rootfs_path);
        fs.LoadExecutable(config.executable_path);
        ThreadState* main_state = CreateThreadState();
        VERBOSE("Created thread state with tid %ld", main_state->tid);
        setupMainStack(main_state);
        main_state->signal_handlers = std::make_shared<SignalHandlerTable>();
        g_current_brk = fs.GetBRK();
        main_state->SetRip((u64)fs.GetEntrypoint());
    }

    Emulator(const TestConfig& config) : recompiler() {
        g_emulator = this;
        ThreadState* main_state = CreateThreadState();
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

    auto& GetStates() {
        return thread_states;
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

    static void* CompileNext(ThreadState* state);

    static void Sigreturn();

    ThreadState* GetThreadState();

    std::pair<void*, size_t> GetAuxv() {
        return {auxv_base, auxv_size};
    }

    Recompiler& GetRecompiler() {
        return recompiler;
    }

    ThreadState* CreateThreadState(ThreadState* copy_state = nullptr);

    void RemoveState(ThreadState* state);

    void CleanExit(ThreadState* state);

private:
    void setupMainStack(ThreadState* state);

    sem_t* semaphore; // to synchronize compilation and function lookup
    std::list<ThreadState> thread_states;
    Config config;
    Filesystem fs;
    Recompiler recompiler;
    bool testing = false;
    void* auxv_base = nullptr;
    size_t auxv_size = 0;
};
