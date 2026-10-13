#pragma once

#include <sys/mman.h>
#include "felix86/common/frame.hpp"
#include "felix86/common/state.hpp"

struct TestConfig {
    u64 entrypoint;
    bool mode32;
    bool fill_ymm_with_trash;
    bool reduced_precision = false;
};

struct Emulator {
    static void* CompileNext(ThreadState* state);

    [[noreturn]] static void Start();

    static void StartTest(const TestConfig& config, u64 stack);
};
