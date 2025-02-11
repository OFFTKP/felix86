#pragma once

enum ExitReason {
    EXIT_REASON_UNKNOWN = 0,
    EXIT_REASON_HLT = 1,
    EXIT_REASON_EXIT_SYSCALL = 2,
    EXIT_REASON_EXIT_GROUP_SYSCALL = 3,
};

void felix86_exit(int code);
