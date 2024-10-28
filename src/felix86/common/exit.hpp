#pragma once

enum ExitReason {
    EXIT_REASON_HLT = 1,
    EXIT_REASON_BAD_ALIGNMENT = 2,
    EXIT_REASON_NO_VECTOR = 3,
};

void felix86_exit(int code);
