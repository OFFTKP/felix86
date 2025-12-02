#include <cstring>
#include <set>
#include <fcntl.h>
#include "felix86/common/log.hpp"
#include "felix86/hle/fd.hpp"

std::set<int> g_protected_fds{};

void FD::protect(int fd) {
    ASSERT(fd > 2);

    g_protected_fds.insert(fd);

    // Important: If a process sharing a file descriptor table calls execve(2), its file descriptor table is duplicated (unshared)
    // This means that FD_CLOEXEC won't close the fds for any processes sharing the FD table through CLONE_FILES, which is good
    int result = fcntl(fd, F_SETFD, FD_CLOEXEC);
    if (result != 0) {
        WARN("Failed to set FD_CLOEXEC for fd %d", fd);
    }
}

void FD::unprotectAndClose(int fd) {
    ASSERT(g_protected_fds.contains(fd));
    g_protected_fds.erase(fd);
    ASSERT_MSG(::close(fd) == 0, "Failed to close our protected fd: %d", fd);
}

int FD::close(int fd) {
    if (fd < 2) {
        return ::close(fd);
    }

    auto guard = g_process_globals.states_lock.lock();
    if (g_protected_fds.contains(fd)) {
        WARN("Program tried to close one of our fds: %d", fd);
        return 0; // pretend we closed the fd but keep it open
    } else {
        return ::close(fd);
    }
}

int FD::close_range(u32 start, u32 end, int flags) {
    u32 current_start = start;
    auto guard = g_process_globals.states_lock.lock();
    for (u32 protected_fd : g_protected_fds) {
        if (protected_fd == current_start) {
            // Skip this fd
            WARN("Program tried to close one of our fds: %d", protected_fd);
            current_start++;
            continue;
        } else if (protected_fd < current_start) {
            continue;
        } else if (protected_fd > current_start) {
            // Close until fd - 1 and set next start to fd + 1
            WARN("Program tried to close one of our fds: %d", protected_fd);
            int result = ::close_range(current_start, protected_fd - 1, flags);
            if (result != 0) {
                // Kernel gave us an error, return the code now
                return result;
            }
            current_start = protected_fd + 1;
        }
    }

    if (current_start <= end) {
        // Close the remaining fds
        return ::close_range(current_start, end, flags);
    } else {
        return 0;
    }
}

int FD::dup2(int old_fd, int new_fd) {
    {
        auto guard = g_process_globals.states_lock.lock();
        for (u32 protected_fd : g_protected_fds) {
            if (old_fd == (int)protected_fd) {
                WARN("dup2 with old_fd == protected FD: %d", protected_fd);
            }
            if (new_fd == (int)protected_fd) {
                WARN("Program tried to trample our protected FD with dup2, returning EBADF");
                return -EBADF;
            }
        }
    }
    return ::dup2(old_fd, new_fd);
}

int FD::dup3(int old_fd, int new_fd, int flags) {
    {
        auto guard = g_process_globals.states_lock.lock();
        for (u32 protected_fd : g_protected_fds) {
            if (old_fd == (int)protected_fd) {
                WARN("dup2 with old_fd == protected FD: %d", protected_fd);
            }
            if (new_fd == (int)protected_fd) {
                WARN("Program tried to trample our protected FD with dup3, returning EBADF");
                return -EBADF;
            }
        }
    }
    return ::dup3(old_fd, new_fd, flags);
}

int FD::moveToHighNumber(int fd) {
    // rand() so that it has a higher likelyhood of succeeding first try
    int high_fd = 512 + rand() % 64;
    int tries = 50;
    while (tries-- > 0) {
        int result = fcntl(high_fd, F_GETFD);
        if (result < 0) {
            // We can use this FD
            int new_fd = ::dup2(fd, high_fd);
            ASSERT_MSG(new_fd > 0, "Failed to duplicate fd %d to %d with error %s", fd, high_fd, strerror(errno));
            return new_fd;
        }
        high_fd++;
    }

    ERROR("Failed to find available FD to duplicate %d", fd);
    return -1;
}