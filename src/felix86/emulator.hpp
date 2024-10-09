#pragma once

#include "felix86/backend/backend.hpp"
#include "felix86/common/log.hpp"
#include "felix86/common/x86.hpp"
#include "felix86/hle/filesystem.hpp"

struct Config {
    std::filesystem::path rootfs_path;
    std::filesystem::path executable_path;
    bool testing = false;
    bool print_blocks = false;
    bool use_interpreter = false;
    std::vector<std::string> argv;
    std::vector<std::string> envp;
};

struct Emulator {
    Emulator(const Config& config) : backend(*this), config(config) {
        fs.LoadRootFS(config.rootfs_path);
        fs.LoadExecutable(config.executable_path);
        ThreadState* main_state = createThreadState();
        setupMainStack(main_state);
        main_state->SetRip((u64)fs.GetEntrypoint());
    }

    ~Emulator() = default;

    Filesystem& GetFilesystem() {
        return fs;
    }

    Config& GetConfig() {
        return config;
    }

    void Run();

    static void* CompileNext(Emulator* emulator, ThreadState* state);

private:
    void setupMainStack(ThreadState* state);

    void* compileFunction(u64 rip);

    ThreadState* createThreadState() {
        thread_states.push_back(ThreadState{});
        ThreadState* thread_state = &thread_states.back();
        return thread_state;
    }

    std::mutex compilation_mutex; // to synchronize compilation and function lookup
    std::list<ThreadState> thread_states;
    Backend backend;
    Filesystem fs;
    Config config;
};
