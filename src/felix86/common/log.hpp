#pragma once

#include <stdbool.h>
#include <stdlib.h>
#include <unistd.h>
#include "felix86/common/config.hpp"
#include "felix86/common/exit.hpp"
#include "felix86/common/global.hpp"

#define ANSI_COLOR_RED "\x1b[31m"
#define ANSI_COLOR_GREEN "\x1b[32m"
#define ANSI_COLOR_YELLOW "\x1b[33m"
#define ANSI_COLOR_BLUE "\x1b[34m"
#define ANSI_COLOR_MAGENTA "\x1b[35m"
#define ANSI_COLOR_CYAN "\x1b[36m"
#define ANSI_BOLD "\x1b[1m"
#define ANSI_COLOR_RESET "\x1b[0m"
#define ANSI_COLOR_BLACK_ON_CYAN "\x1b[30;106m"
#define ANSI_COLOR_BLACK_ON_GREEN "\x1b[30;42m"

#ifndef SOURCE_PATH_SIZE
#pragma message("SOURCE_PATH_SIZE was not defined correctly")
#define __FILENAME__ __FILE__
#else
#define __FILENAME__ (&__FILE__[SOURCE_PATH_SIZE])
#endif

struct Logger {
    static void log(const char* format, ...);

    static const char* getPipeName();

    static void startServer(bool detach = false);

    static void joinServer();

private:
    static void serverLoop(int fd);
};

#define LOG(format, ...)                                                                                                                             \
    do {                                                                                                                                             \
        if (!g_config.quiet) {                                                                                                                       \
            Logger::log(ANSI_COLOR_CYAN format ANSI_COLOR_RESET "\n", ##__VA_ARGS__);                                                                \
        }                                                                                                                                            \
    } while (0)

#define SIGLOG(format, ...)                                                                                                                          \
    do {                                                                                                                                             \
        if (!g_config.quiet && g_config.print_signals) {                                                                                             \
            Logger::log(ANSI_COLOR_BLACK_ON_CYAN format ANSI_COLOR_RESET "\n", ##__VA_ARGS__);                                                       \
        }                                                                                                                                            \
    } while (0)

#define SMCLOG(format, ...)                                                                                                                          \
    do {                                                                                                                                             \
        if (!g_config.quiet && g_config.print_smc) {                                                                                                 \
            Logger::log(ANSI_COLOR_BLACK_ON_GREEN format ANSI_COLOR_RESET "\n", ##__VA_ARGS__);                                                      \
        }                                                                                                                                            \
    } while (0)

#define ERROR(format, ...)                                                                                                                           \
    do {                                                                                                                                             \
        Logger::log(ANSI_COLOR_RED "%s:%d (Thread: %d) " format ANSI_COLOR_RESET "\n", __FILENAME__, __LINE__, gettid(), ##__VA_ARGS__);             \
        felix86_exit(1);                                                                                                                             \
    } while (0)

#define WARN(format, ...)                                                                                                                            \
    do {                                                                                                                                             \
        if (!g_config.quiet) {                                                                                                                       \
            Logger::log(ANSI_COLOR_YELLOW format ANSI_COLOR_RESET "\n", ##__VA_ARGS__);                                                              \
        }                                                                                                                                            \
    } while (0)

#define VERBOSE(format, ...)                                                                                                                         \
    do {                                                                                                                                             \
        if (g_config.verbose && !g_config.quiet) {                                                                                                   \
            Logger::log(ANSI_COLOR_MAGENTA "%s:%d " format ANSI_COLOR_RESET "\n", __FILENAME__, __LINE__, ##__VA_ARGS__);                            \
        }                                                                                                                                            \
    } while (0)

#define STRACE(format, ...)                                                                                                                          \
    do {                                                                                                                                             \
        if ((g_config.strace || g_config.strace_errors) && !g_config.quiet) {                                                                        \
            Logger::log(ANSI_COLOR_BLUE format ANSI_COLOR_RESET "\n", ##__VA_ARGS__);                                                                \
        }                                                                                                                                            \
    } while (0)

#define SUCCESS(format, ...)                                                                                                                         \
    do {                                                                                                                                             \
        if (!g_config.quiet) {                                                                                                                       \
            Logger::log(ANSI_COLOR_GREEN format ANSI_COLOR_RESET "\n", ##__VA_ARGS__);                                                               \
        }                                                                                                                                            \
    } while (0)

#define PLAIN(format, ...)                                                                                                                           \
    do {                                                                                                                                             \
        if (!g_config.quiet) {                                                                                                                       \
            Logger::log(format "\n", ##__VA_ARGS__);                                                                                                 \
        }                                                                                                                                            \
    } while (0)

#define WARN_ONCE(format, ...)                                                                                                                       \
    do {                                                                                                                                             \
        static bool warned = false;                                                                                                                  \
        if (!warned) {                                                                                                                               \
            WARN(format, ##__VA_ARGS__);                                                                                                             \
            warned = true;                                                                                                                           \
        }                                                                                                                                            \
    } while (0)

#define UNREACHABLE()                                                                                                                                \
    do {                                                                                                                                             \
        ERROR("Unreachable code hit");                                                                                                               \
        __builtin_unreachable();                                                                                                                     \
    } while (false)

#define UNIMPLEMENTED() ERROR("Unimplemented code hit")

#define ASSERT(condition)                                                                                                                            \
    do {                                                                                                                                             \
        if (!(condition)) {                                                                                                                          \
            ERROR("Assertion failed: %s", #condition);                                                                                               \
        }                                                                                                                                            \
    } while (false)

#define ASSERT_MSG(condition, format, ...)                                                                                                           \
    do {                                                                                                                                             \
        if (!(condition))                                                                                                                            \
            ERROR("Assertion failed: %s -- " format, #condition, ##__VA_ARGS__);                                                                     \
    } while (false)
