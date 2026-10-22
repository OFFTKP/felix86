#pragma once

#include <sys/mman.h>
#include "felix86/common/types.hpp"

struct TestConfig {
    u64 entrypoint;
    bool mode32;
    bool fill_ymm_with_trash;
    bool reduced_precision = false;
};

struct Emulator {
    [[noreturn]] static void Start();

    static void StartTest(const TestConfig& config, u64 stack);
};
