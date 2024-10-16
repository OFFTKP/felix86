
#include <cstdarg>
#include "catch2/catch_test_macros.hpp"

void felix86_test_failure(const char* format, ...) {
    char buffer[4096];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    CATCH_FAIL(buffer);
}