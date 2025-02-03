#pragma once

enum ExitReason {
    EXIT_REASON_HLT = 1,
    EXIT_REASON_BAD_ALIGNMENT = 2,
    EXIT_REASON_NO_VECTOR = 3,
    EXIT_REASON_UD2 = 4,
    EXIT_REASON_TSX = 5,
    EXIT_REASON_CET = 6,
    EXIT_REASON_EXIT_SYSCALL = 7,
    EXIT_REASON_NEGATIVE_BITSTRING = 8,
};

void felix86_exit(int code);
