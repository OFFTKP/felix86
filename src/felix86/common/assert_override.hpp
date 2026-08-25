#pragma once

[[noreturn]] __attribute__((format(printf, 3, 4))) void felix86_assert(const char* file, int line, const char* format, ...);

#define BISCUIT_ASSERT(condition)                                                                                                                    \
    do {                                                                                                                                             \
        if (!(condition)) {                                                                                                                          \
            felix86_assert(__FILE__, __LINE__, "Assertion failed (%s) in function %s", #condition, __func__);                                        \
        }                                                                                                                                            \
    } while (false)
