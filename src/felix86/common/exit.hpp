#pragma once

enum ExitReasons {
    EXIT_REASON_HLT = 1,
};

void felix86_exit(int code) __attribute__((noreturn));
