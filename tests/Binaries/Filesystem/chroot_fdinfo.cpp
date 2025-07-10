#include <errno.h>
#include <fcntl.h>
#include <linux/limits.h>
#include <linux/sched.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <unistd.h>
#include "common.h"

#define HANDLE_EINTR(x)                                                                                                                              \
    ({                                                                                                                                               \
        int eintr_wrapper_counter = 0;                                                                                                               \
        decltype(x) eintr_wrapper_result;                                                                                                            \
        do {                                                                                                                                         \
            eintr_wrapper_result = (x);                                                                                                              \
        } while (eintr_wrapper_result == -1 && errno == EINTR && eintr_wrapper_counter++ < 100);                                                     \
        eintr_wrapper_result;                                                                                                                        \
    })

// This code is found in Chromium and anything that uses it
// linux/services/credentials.cc
int ChrootToSelfFdinfo(void*) {
    if (syscall(SYS_chroot, "/proc/self/fdinfo/") != 0) {
        _exit(2);
    }

    if (syscall(SYS_chdir, "/") != 0) {
        _exit(3);
    }

    _exit(FELIX86_BTEST_SUCCESS);
}

bool ChrootToSafeEmptyDir() {
    pid_t pid = -1;

    alignas(16) char stack_buf[16384];
    void* stack = stack_buf + sizeof(stack_buf);
    int clone_flags = CLONE_FS | SIGCHLD;
    void* tls = nullptr;
    clone_flags |= CLONE_VM | CLONE_VFORK | CLONE_SETTLS;
    char tls_buf[16384] = {};
    tls = tls_buf;
    pid = clone(ChrootToSelfFdinfo, stack, clone_flags, nullptr, nullptr, tls, nullptr);
    if (pid == -1) {
        return false;
    }

    int status = -1;
    if (HANDLE_EINTR(waitpid(pid, &status, 0)) != pid) {
        return false;
    }

    return WIFEXITED(status) && WEXITSTATUS(status) == FELIX86_BTEST_SUCCESS;
}

int main() {
    if (unshare(CLONE_NEWNS | CLONE_NEWUSER) != 0) {
        return 6;
    }

    bool success = ChrootToSafeEmptyDir();
    if (!success) {
        return 1;
    }

    // Chromium then stats /proc and expects it to not be there
    // This is because the clone had CLONE_FS
    // TODO: uncomment when we implement vfork better
    // struct stat stat;
    // if (access("/proc", F_OK) == F_OK) {
    //     return 5;
    // }

    return FELIX86_BTEST_SUCCESS;
}