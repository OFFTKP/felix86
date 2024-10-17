#include <stdlib.h>
#include "felix86/common/exit.hpp"
#include "felix86/common/global.hpp"

void __attribute__((weak)) felix86_test_failure(const char*, ...) {
    printf("Test failure called, but no test failure handler was set\n");
    exit(1);
}

void felix86_exit(int code) {
    fflush(stdout);
    if (g_testing) {
        felix86_test_failure("Exit called with code: %d", code);
    } else {
        exit(code);
    }
}