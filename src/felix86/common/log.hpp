#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "felix86/common/exit.hpp"
#include "felix86/common/global.hpp"

#define ANSI_COLOR_RED "\x1b[31m"
#define ANSI_COLOR_GREEN "\x1b[32m"
#define ANSI_COLOR_YELLOW "\x1b[33m"
#define ANSI_COLOR_BLUE "\x1b[34m"
#define ANSI_COLOR_MAGENTA "\x1b[35m"
#define ANSI_COLOR_CYAN "\x1b[36m"
#define ANSI_COLOR_RESET "\x1b[0m"

#define LOG(format, ...)                                                                                                                             \
    do {                                                                                                                                             \
        if (!g_quiet) {                                                                                                                              \
            dprintf(g_output_fd, ANSI_COLOR_CYAN format ANSI_COLOR_RESET "\n", ##__VA_ARGS__);                                                       \
            fsync(g_output_fd);                                                                                                                      \
        }                                                                                                                                            \
    } while (0)
#define ERROR(format, ...)                                                                                                                           \
    do {                                                                                                                                             \
        dprintf(g_output_fd, ANSI_COLOR_RED "%s:%d " format ANSI_COLOR_RESET "\n", __FILE__, __LINE__, ##__VA_ARGS__);                               \
        fsync(g_output_fd);                                                                                                                          \
        felix86_exit(1);                                                                                                                             \
    } while (0)
#define WARN(format, ...)                                                                                                                            \
    do {                                                                                                                                             \
        if (!g_quiet) {                                                                                                                              \
            dprintf(g_output_fd, ANSI_COLOR_YELLOW format ANSI_COLOR_RESET "\n", ##__VA_ARGS__);                                                     \
            fsync(g_output_fd);                                                                                                                      \
        }                                                                                                                                            \
    } while (0)
#define VERBOSE(format, ...)                                                                                                                         \
    do {                                                                                                                                             \
        if (g_verbose && !g_quiet) {                                                                                                                 \
            dprintf(g_output_fd, ANSI_COLOR_MAGENTA "%s:%d " format ANSI_COLOR_RESET "\n", __FILE__, __LINE__, ##__VA_ARGS__);                       \
            fsync(g_output_fd);                                                                                                                      \
        }                                                                                                                                            \
    } while (0)

#define STRACE(format, ...)                                                                                                                          \
    do {                                                                                                                                             \
        if (g_strace && !g_quiet) {                                                                                                                  \
            dprintf(g_output_fd, ANSI_COLOR_BLUE format ANSI_COLOR_RESET "\n", ##__VA_ARGS__);                                                       \
            fsync(g_output_fd);                                                                                                                      \
        }                                                                                                                                            \
    } while (0)

#define SUCCESS(format, ...)                                                                                                                         \
    do {                                                                                                                                             \
        if (!g_quiet) {                                                                                                                              \
            dprintf(g_output_fd, ANSI_COLOR_GREEN format ANSI_COLOR_RESET "\n", ##__VA_ARGS__);                                                      \
            fsync(g_output_fd);                                                                                                                      \
        }                                                                                                                                            \
    } while (0)

#define PLAIN(format, ...)                                                                                                                           \
    do {                                                                                                                                             \
        dprintf(g_output_fd, format "\n", ##__VA_ARGS__);                                                                                            \
        fsync(g_output_fd);                                                                                                                          \
    } while (0)

#define UNREACHABLE() ERROR("Unreachable code hit")
#define UNIMPLEMENTED() ERROR("Unimplemented code hit")

#define ASSERT(condition)                                                                                                                            \
    do {                                                                                                                                             \
        if (!(condition))                                                                                                                            \
            UNREACHABLE();                                                                                                                           \
    } while (false)

#define ASSERT_MSG(condition, format, ...)                                                                                                           \
    do {                                                                                                                                             \
        if (!(condition))                                                                                                                            \
            ERROR(format, ##__VA_ARGS__);                                                                                                            \
    } while (false)

void enable_verbose();
void disable_logging();