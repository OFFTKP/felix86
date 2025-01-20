#pragma once

#include "felix86/aot/aot.hpp"
#include "felix86/backend/backend.hpp"
#include "felix86/common/log.hpp"
#include "felix86/common/x86.hpp"
#include "felix86/hle/filesystem.hpp"
#include "felix86/hle/signals.hpp"
#include "felix86/v2/fast_recompiler.hpp"

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
    Emulator(const Config& config) : config(config), backend(*this), fast_recompiler(*this) {
        g_emulator = this;
        fs.LoadRootFS(config.rootfs_path);
        fs.LoadExecutable(config.executable_path);
        ThreadState* main_state = createThreadState();
        setupMainStack(main_state);
        main_state->brk_current_address = fs.GetBRK();
        main_state->SetRip((u64)fs.GetEntrypoint());

        AOT aot(*this, fs.GetExecutable());
        if (g_preload) {
            aot.PreloadAll();
        }

        if (g_aot) {
            aot.CompileAll();
        }
    }

    Emulator(const TestConfig& config) : backend(*this), fast_recompiler(*this) {
        g_emulator = this;
        ThreadState* main_state = createThreadState();
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

    std::pair<void*, u64> GetCodeAt(u64 rip) {
        return backend.GetCodeAt(rip);
    }

    Backend& GetBackend() {
        return backend;
    }

    Assembler& GetAssembler() {
        return fast_recompiler.getAssembler();
    }

    void Run();

    static void* CompileNext(Emulator* emulator, ThreadState* state);

    static void CompileFunction(Emulator* emulator, u64 rip) {
        emulator->compileFunction(rip);
    }

    std::pair<void*, size_t> GetAuxv() {
        return {auxv_base, auxv_size};
    }

    void* LoadFromCache(u64 rip, const std::string& hash);

    u64 GetCodeCacheSize() {
        return backend.GetCodeCacheSize();
    }

    FastRecompiler& GetRecompiler() {
        return fast_recompiler;
    }

private:
    void setupMainStack(ThreadState* state);

    void* compileFunction(u64 rip);

    void* compileFunctionFast(u64 rip);

    ThreadState* createThreadState();

    std::mutex compilation_mutex; // to synchronize compilation and function lookup
    std::list<ThreadState> thread_states;
    Config config;
    Backend backend;
    Filesystem fs;
    FastRecompiler fast_recompiler;
    bool testing = false;
    void* auxv_base = nullptr;
    size_t auxv_size = 0;
};
