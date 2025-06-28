#include <csignal>
#include <cstdarg>
#include <sys/file.h>
#include <sys/inotify.h>
#include <sys/prctl.h>
#include <sys/stat.h>
#include "felix86/common/global.hpp"
#include "felix86/common/log.hpp"
#include "felix86/hle/signals.hpp"

std::string pipe_name;

void Logger::log(const char* format, ...) {
    SignalGuard guard;
    va_list args;
    va_start(args, format);
    vdprintf(g_output_fd, format, args);
    va_end(args);
}

const char* Logger::getPipeName() {
    return pipe_name.c_str();
}

void Logger::startServer(bool detach) {
    std::string log_path = "/tmp/felix86-" + std::to_string(getpid());
    pipe_name = log_path + ".pipe";
    log_path += "-XXXXXX.log";
    int fd = mkstemps(log_path.data(), 4);
    ASSERT(fd != -1);

    int ok = mkfifo(pipe_name.c_str(), 0666);
    ASSERT(ok == 0);

    if (detach) {
        std::string message = "Started the log server in the background. Use `export __FELIX86_PIPE=";
        message += Logger::getPipeName();
        message += "` to join to this log server from future felix86 instances.\n";
        printf("%s", message.c_str());
        setsid();
    }

    if (!detach) {
        int pid = fork();
        if (pid == 0) {
            // When the parent dies (main emulator thread), make sure the logging "server" also dies
            prctl(PR_SET_PDEATHSIG, SIGTERM);
            serverLoop(fd);
        } else {
            // Open write end of pipe -- we need to do it here otherwise the thing will hang (both ends need to be opened simultaneously)
            g_output_fd = open(pipe_name.c_str(), O_WRONLY, 0644);
            ASSERT(g_output_fd > 0);
        }
    } else {
        int pid = fork();
        if (pid == 0) {
            serverLoop(fd);
        } else {
            exit(0);
        }
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
    g_output_fd = open(file, O_WRONLY, 0644);
    if (g_output_fd == -1) {
        ERROR("Bad g_output_fd -- errno: %d -- pipe: %s", errno, file);
    }

    // Also set this for when this process runs execve...
    pipe_name = file;
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

    // This is going to be the logging "server". Basically we don't want to print anything to stdout
    // as applications may read it. So we start a separate process with its own stdout to handle
    // the displaying of messages.
    sigset_t mask;
    sigfillset(&mask);
    sigdelset(&mask, SIGTERM);
    sigprocmask(SIG_SETMASK, &mask, nullptr);

    int read_pipe = open(pipe_name.c_str(), O_RDONLY, 0666);
    ASSERT(read_pipe > 0);
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

        // Also write it to the file
        size_t written = fwrite(message.c_str(), 1, message.size(), f);
        ASSERT_MSG(message.size() == written, "Failed to write %zu bytes to file", written);
        fflush(f);
    }
}