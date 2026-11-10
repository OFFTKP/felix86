#include <atomic>
#include <cerrno>
#include <climits>
#include <csignal>
#include <cstdarg>
#include <string>
#include <unordered_map>
#include <sys/auxv.h>
#include <sys/epoll.h>
#include <sys/file.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include "felix86/common/global.hpp"
#include "felix86/common/log.hpp"
#include "felix86/hle/fd.hpp"

static const std::atomic<u32>* g_log_server_generation = nullptr;
static std::atomic<u32> g_log_seen_generation{0};

static int g_log_file_fd = -1;
static std::atomic<int> g_server_fd{-1};
static std::atomic<bool> g_server_attached{false};

static socklen_t make_server_address(sockaddr_un* addr, uid_t uid) {
    static constexpr char prefix[] = "felix86-log-";
    memset(addr, 0, sizeof(*addr));
    addr->sun_family = AF_UNIX;
    size_t pos = 1;
    for (size_t i = 0; i < sizeof(prefix) - 1; i++) {
        addr->sun_path[pos++] = prefix[i];
    }

    char digits[16];
    int count = 0;
    do {
        digits[count++] = '0' + uid % 10;
        uid /= 10;
    } while (uid != 0);

    while (count > 0) {
        addr->sun_path[pos++] = digits[--count];
    }

    return offsetof(sockaddr_un, sun_path) + pos;
}

static bool get_peer_credentials(int fd, ucred* cred) {
    socklen_t len = sizeof(*cred);
    return getsockopt(fd, SOL_SOCKET, SO_PEERCRED, cred, &len) == 0;
}

static int connect_to_server() {
    int fd = socket(AF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC | SOCK_NONBLOCK, 0);
    if (fd < 0) {
        return -1;
    }

    sockaddr_un addr;
    socklen_t len = make_server_address(&addr, getuid());
    if (connect(fd, (sockaddr*)&addr, len) != 0) {
        ::close(fd);
        return -1;
    }

    ucred cred;
    if (!get_peer_credentials(fd, &cred)) {
        ::close(fd);
        return -1;
    }

    if (cred.uid != getuid()) {
        ::close(fd);
        return -1;
    }

    return fd;
}


static void send_to_server(const char* buffer, int size) {
    int saved_errno = errno;
    while (true) {
        ssize_t sent = send(g_server_fd.load(std::memory_order_relaxed), buffer, size, MSG_DONTWAIT | MSG_NOSIGNAL);
        if (sent >= 0) {
            break;
        }

        if (errno == EINTR) {
            continue;
        }

        if (errno != EAGAIN) {
            g_server_attached.store(false);
        }
        break;
    }
    errno = saved_errno;
}

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
    checkServer();
    bool attached = g_server_attached.load(std::memory_order_relaxed);
    if (!attached) {
        if (g_output_fd < 0 && g_log_file_fd < 0) {
            return;
        }
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

    if (attached) {
        send_to_server(buffer, size);
    } else if (g_output_fd >= 0) {
        write_fully(g_output_fd, buffer, size);
    }

    if (g_log_file_fd >= 0) {
        write_fully(g_log_file_fd, buffer, size);
    }
}

void felix86_assert(const char* file, int line, const char* format, ...) {
    char buffer[4096];
    va_list args;
    va_start(args, format);
    int size = vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    if (size < 0) {
        buffer[0] = '\0';
    }

    Logger::log(ANSI_COLOR_BLACK_ON_RED "%s:%d (Thread: %d) %s" ANSI_COLOR_RESET "\n", file, line, gettid(), buffer);
    felix86_exit(1);
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

static void* map_generation_page(int fd, bool writable) {
    constexpr size_t size = 4096;
    struct stat st;
    if (fstat(fd, &st) != 0) {
        return nullptr;
    }

    if (st.st_uid != getuid()) {
        return nullptr;
    }

    if (st.st_size < (off_t)size) {
        if (ftruncate(fd, size) != 0) {
            return nullptr;
        }
    }

    int prot = writable ? PROT_READ | PROT_WRITE : PROT_READ;
    void* mem = mmap(nullptr, size, prot, MAP_SHARED, fd, 0);
    if (mem == MAP_FAILED) {
        return nullptr;
    }

    return mem;
}

static std::atomic<u32>* map_server_generation(bool writable) {
    std::string name = "/felix86-log-" + std::to_string(getuid());
    int fd = shm_open(name.c_str(), O_RDWR | O_CREAT, 0600);
    if (fd < 0) {
        return nullptr;
    }

    void* mem = map_generation_page(fd, writable);
    ::close(fd);
    return (std::atomic<u32>*)mem;
}

void Logger::initServerConnection() {
    if (getauxval(AT_SECURE)) {
        return;
    }

    std::atomic<u32>* generation = map_server_generation(false);
    if (generation) {
        g_log_seen_generation.store(generation->load(std::memory_order_acquire));
        g_log_server_generation = generation;
    }

    tryAttach();
}

void Logger::checkServer() {
    const std::atomic<u32>* generation = g_log_server_generation;
    if (!generation) {
        return;
    }

    if (generation->load(std::memory_order_relaxed) != g_log_seen_generation.load(std::memory_order_relaxed)) {
        refreshServer();
    }
}

void Logger::refreshServer() {
    u32 current = g_log_server_generation->load(std::memory_order_acquire);
    if (g_log_seen_generation.exchange(current) != current) {
        tryAttach();
    }
}

void Logger::tryAttach() {
    int saved_errno = errno;
    int fd = connect_to_server();
    if (fd < 0) {
        errno = saved_errno;
        return;
    }

    int slot = g_server_fd.load(std::memory_order_acquire);
    if (slot < 0) {
        fd = FD::moveToHighNumber(fd);
        FD::protect(fd);
        g_server_fd.store(fd, std::memory_order_release);
    } else {
        dup3(fd, slot, O_CLOEXEC);
        ::close(fd);
    }

    g_server_attached.store(true);
    errno = saved_errno;
}

void Logger::afterForkChild() {
    if (g_server_attached.load()) {
        tryAttach();
    }
}

void Logger::startServer() {
    int listen_fd = socket(AF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC | SOCK_NONBLOCK, 0);
    if (listen_fd < 0) {
        printf("Failed to create the log server socket: %s\n", strerror(errno));
        exit(1);
    }

    sockaddr_un addr;
    socklen_t len = make_server_address(&addr, getuid());
    if (bind(listen_fd, (sockaddr*)&addr, len) != 0) {
        if (errno == EADDRINUSE) {
            printf("A felix86 log server is already running\n");
        } else {
            printf("Bind error: %s\n", strerror(errno));
        }
        exit(1);
    }

    if (listen(listen_fd, SOMAXCONN) != 0) {
        printf("Listen error: %s\n", strerror(errno));
        exit(1);
    }

    std::atomic<u32>* generation = map_server_generation(true);
    if (generation) {
        generation->fetch_add(1, std::memory_order_release);
    } else {
        printf("Failed to map the log server generation page, running felix86 processes will not attach until restarted\n");
    }

    std::string log_path = "/tmp/felix86-" + std::to_string(getpid()) + "-XXXXXX.log";
    int fd = mkstemps(log_path.data(), 4);
    if (fd < 0) {
        printf("Failed to create the log file %s: %s\n", log_path.c_str(), strerror(errno));
        exit(1);
    }

    printf("Started the log server, logs are saved to %s\n", log_path.c_str());
    fflush(stdout);

    serverLoop(listen_fd, fd);
}

void Logger::serverLoop(int listen_fd, int fd) {
#undef ASSERT_MSG
    // Use printf if we die so it's more obvious than writing to the file
#define ASSERT_MSG(condition, format, ...)                                                                                                           \
    do {                                                                                                                                             \
        if (!(condition)) {                                                                                                                          \
            printf("Log server assertion failed: " format "\n", ##__VA_ARGS__);                                                                      \
            exit(1);                                                                                                                                 \
        }                                                                                                                                            \
    } while (false)

    int epoll_fd = epoll_create1(EPOLL_CLOEXEC);
    ASSERT_MSG(epoll_fd >= 0, "Failed to create epoll instance: %d", errno);

    epoll_event event = {};
    event.events = EPOLLIN;
    event.data.fd = listen_fd;
    ASSERT_MSG(epoll_ctl(epoll_fd, EPOLL_CTL_ADD, listen_fd, &event) == 0, "Failed to add listening socket to epoll: %d", errno);

    FILE* f = fdopen(fd, "w");
    std::unordered_map<int, pid_t> clients;
    constexpr size_t buffer_size = 0x10000;
    static char buffer[buffer_size];
    epoll_event events[64];
    while (true) {
        int ready = epoll_wait(epoll_fd, events, 64, -1);
        if (ready < 0) {
            ASSERT_MSG(errno == EINTR, "Logging server got error %d during epoll_wait?", errno);
            continue;
        }

        for (int i = 0; i < ready; i++) {
            int current = events[i].data.fd;
            if (current == listen_fd) {
                while (true) {
                    int client = accept4(listen_fd, nullptr, nullptr, SOCK_CLOEXEC | SOCK_NONBLOCK);
                    if (client < 0) {
                        break;
                    }

                    ucred cred;
                    if (!get_peer_credentials(client, &cred)) {
                        close(client);
                        continue;
                    }

                    if (cred.uid != getuid()) {
                        close(client);
                        continue;
                    }

                    epoll_event client_event = {};
                    client_event.events = EPOLLIN;
                    client_event.data.fd = client;
                    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client, &client_event) != 0) {
                        close(client);
                        continue;
                    }
                    clients[client] = cred.pid;
                }
                continue;
            }

            while (true) {
                ssize_t size = recv(current, buffer, buffer_size, 0);
                if (size < 0) {
                    if (errno == EINTR) {
                        continue;
                    }

                    if (errno == EAGAIN) {
                        break;
                    }
                }

                if (size <= 0) {
                    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, current, nullptr);
                    close(current);
                    clients.erase(current);
                    break;
                }

                std::string message = "[" + std::to_string(clients[current]) + "] ";
                message.append(buffer, size);
                printf("%s", message.c_str());
                fflush(stdout);

                size_t written = fwrite(message.c_str(), 1, message.size(), f);
                ASSERT_MSG(message.size() == written, "Failed to write %zu bytes to file", written);
                fflush(f);
            }
        }
    }
}
