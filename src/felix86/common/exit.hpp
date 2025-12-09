#pragma once

#include <cstdint>

enum ExitReason : uint8_t {
    EXIT_REASON_UNKNOWN = 0,
    EXIT_REASON_HLT = 1,
    EXIT_REASON_EXIT_SYSCALL = 2,
    EXIT_REASON_EXIT_GROUP_SYSCALL = 3,
    EXIT_REASON_GUEST_CODE_FINISHED = 4,
    EXIT_REASON_SIGRETURN = 5,
};

void felix86_exit(int code);
