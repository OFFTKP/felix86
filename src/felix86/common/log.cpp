#include <cerrno>
#include <climits>
#include <csignal>
#include <cstdarg>
#include <sys/file.h>
#include <sys/inotify.h>
#include <sys/prctl.h>
#include <sys/stat.h>
#include "felix86/common/global.hpp"
#include "felix86/common/log.hpp"
#include "felix86/hle/fd.hpp"

static std::string g_pipe_name;
static int g_log_file_fd = -1;

static void write_fully(int fd, const char* buffer, int size) {
    int offset = 0;
    while (offset < size) {
        ssize_t written = write(fd, buffer + offset, size - offset);
        if (written < 0) {
            if (errno == EINTR) {
                continue;
            }
            return;
        }

        if (written == 0) {
            return;
        }

        offset += written;
    }
}

void Logger::log(const char* format, ...) {
    if (g_output_fd < 0 && g_log_file_fd < 0) {
        return;
    }

    char buffer[PIPE_BUF];
    va_list args;
    va_start(args, format);
    int size = vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    if (size < 0) {
        return;
    }

    if (size >= (int)sizeof(buffer)) {
        size = sizeof(buffer) - 1;
    }

    if (g_output_fd >= 0) {
        write_fully(g_output_fd, buffer, size);
    }

    if (g_log_file_fd >= 0) {
        write_fully(g_log_file_fd, buffer, size);
    }
}

void Logger::openTerminal() {
    int fd = open("/dev/tty", O_WRONLY);
    if (fd < 0) {
        g_output_fd = -1;
        return;
    }

    fd = FD::moveToHighNumber(fd);
    FD::protect(fd);
    g_output_fd = fd;
}

void Logger::openLogFile() {
    if (!g_config.log_file) {
        return;
    }

    std::string log_path = "/tmp/felix86-" + std::to_string(getpid()) + "-XXXXXX.log";
    int fd = mkstemps(log_path.data(), 4);
    if (fd < 0) {
        return;
    }

    fd = FD::moveToHighNumber(fd);
    FD::protect(fd);
    g_log_file_fd = fd;
}

const char* Logger::getPipeName() {
    return g_pipe_name.c_str();
}

void Logger::startServer() {
    std::string log_path = "/tmp/felix86-" + std::to_string(getpid());
    g_pipe_name = log_path + ".pipe";
    log_path += "-XXXXXX.log";
    int fd = mkstemps(log_path.data(), 4);
    ASSERT(fd != -1);

    int ok = mkfifo(g_pipe_name.c_str(), 0600);
    ASSERT(ok == 0);

    // mkfifo uses umask to set the permissions, override them
    ok = chmod(g_pipe_name.c_str(), 0600);
    ASSERT(ok == 0);

    std::string message = "Started the log server in the background. Use `export __FELIX86_PIPE=";
    message += Logger::getPipeName();
    message += "` to join to this log server from future felix86 instances.\n";
    printf("%s", message.c_str());
    fflush(stdout);
    setsid();

    int pid = fork();
    if (pid == 0) {
        serverLoop(fd);
    } else {
        exit(0);
    }
}

void Logger::joinServer() {
    // Open the existing write pipe of the emulator instance, passed to this execve process
    // via the __FELIX86_PIPE environment variable
    const char* file = getenv("__FELIX86_PIPE");
    if (!file) {
        // Use printf as we haven't connected yet
        printf("__FELIX86_PIPE not set?\n");
        exit(1);
    }
    g_output_fd = open(file, O_RDWR | O_NONBLOCK, 0644);
    if (g_output_fd == -1) {
        printf("Bad g_output_fd -- errno: %d -- pipe: %s", errno, file);
        exit(1);
    }
    g_output_fd = FD::moveToHighNumber(g_output_fd);
    FD::protect(g_output_fd);

    // Also set this for when this process runs execve...
    g_pipe_name = file;
    VERBOSE("felix86 PID %d joined log server at file %s", getpid(), file);
}

void Logger::serverLoop(int fd) {
#undef ASSERT_MSG
    // Use printf if we die so it's more obvious than writing to the file
#define ASSERT_MSG(condition, format, ...)                                                                                                           \
    do {                                                                                                                                             \
        if (!(condition)) {                                                                                                                          \
            printf("Log server assertion failed: " format "\n", ##__VA_ARGS__);                                                                      \
            exit(1);                                                                                                                                 \
        }                                                                                                                                            \
    } while (false)

    sigset_t mask;
    sigfillset(&mask);
    sigdelset(&mask, SIGTERM);
    sigprocmask(SIG_SETMASK, &mask, nullptr);

    int read_pipe = open(g_pipe_name.c_str(), O_RDONLY, 0666);
    ASSERT(read_pipe > 0);
    // Ensure there's always a writer, so that the reader will sleep even when there's no processes attached
    int keepalive_pipe = open(g_pipe_name.c_str(), O_WRONLY, 0666);
    ASSERT(keepalive_pipe > 0);
    FILE* f = fdopen(fd, "w"); // create the log file to store the log if we need it later
    constexpr size_t buffer_size = 0x10000;
    char buffer[buffer_size];
    while (true) {
        // Writes to pipes less than PIPE_BUF in size (which all our logs should be) are atomic
        int size = read(read_pipe, buffer, buffer_size);
        if (size == -1) {
            if (errno == EAGAIN) {
                continue;
            } else {
                ASSERT_MSG(false, "Logging server got error %d during read?", errno);
            }
        }

        // There's new logs to output!
        // Print the message to our stdout
        std::string message(buffer, size);
        printf("%s", message.c_str());
        fflush(stdout);

        // Also write it to the file
        size_t written = fwrite(message.c_str(), 1, message.size(), f);
        ASSERT_MSG(message.size() == written, "Failed to write %zu bytes to file", written);
        fflush(f);
    }
}
