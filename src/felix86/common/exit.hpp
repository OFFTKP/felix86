#pragma once

enum ExitReason {
    EXIT_REASON_HLT = 1,
    EXIT_REASON_BAD_ALIGNMENT = 2,
};

void felix86_exit(int code);
