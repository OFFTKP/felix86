#pragma once

#include <sys/mman.h>
#include "felix86/common/frame.hpp"
#include "felix86/common/start_params.hpp"
#include "felix86/common/state.hpp"
#include "felix86/hle/filesystem.hpp"

struct TestConfig {
    u64 entrypoint;
    bool mode32;
    bool fill_ymm_with_trash;
};

struct Emulator {
    Filesystem& GetFilesystem() {
        return fs;
    }

    static void* CompileNext(ThreadState* state);

    [[nodiscard]] static std::pair<ExitReason, int> Start();

    static void StartTest(const TestConfig& config, u64 stack);

private:
    [[nodiscard]] static std::pair<void*, size_t> setupMainStack(ThreadState* state);

    Filesystem fs;
};
