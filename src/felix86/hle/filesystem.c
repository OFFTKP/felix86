#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include "felix86/common/log.h"
#include "felix86/hle/filesystem.h"

char squashfs_path[PATH_MAX] = {0};
char emulated_executable_path[PATH_MAX] = {0};
bool mounted = false;

const char* proc_self_exe = "/proc/self/exe";

void felix86_fs_init(const char* squashfs_file_path, const char* executable_path) {
    strncpy(emulated_executable_path, executable_path, sizeof(emulated_executable_path));

    snprintf(squashfs_path, sizeof(squashfs_path), "/tmp/felix86-%d-XXXXXX", getpid());
    mkdtemp(squashfs_path);

    pid_t pid = fork();

    if (pid == 0) {
        const char* args[4] = {
            "squashfuse",
            squashfs_file_path,
            squashfs_path,
            NULL,
        };

        int result = execvp("squashfuse", (char* const*)args);
        if (result != 0) {
            exit(1);
        }
    } else {
        int status;
        waitpid(pid, &status, 0);

        if (WIFEXITED(status) && WEXITSTATUS(status) != 0) {
            ERROR("Failed to mount squashfs image");
        }

        VERBOSE("Mounted squashfs image at %s", squashfs_path);
        mounted = true;
    }
}

void felix86_fs_cleanup() {
    if (!mounted) {
        return;
    }

    pid_t pid = fork();

    if (pid == 0) {
        char* args[5] = {
            "fusermount", "-u", "-q", squashfs_path, NULL,
        };

        execvp("fusermount", args);
    } else {
        int status;
        while (waitpid(pid, &status, 0) == -1 && errno == EINTR)
            ;

        if (WIFEXITED(status) && WEXITSTATUS(status) != 0) {
            WARN("Failed to unmount squashfs image, please unmount manually: %s", squashfs_path);
        }

        VERBOSE("Unmounted squashfs image at %s", squashfs_path);
        rmdir(squashfs_path);
    }
}

u32 felix86_fs_readlinkat(u32 dirfd, const char* pathname, char* buf, u32 bufsiz) {
    if (strncmp(pathname, proc_self_exe, strlen(proc_self_exe)) == 0) {
        snprintf(buf, bufsiz, "%s", emulated_executable_path);
        return strlen(emulated_executable_path);
    }

    ERROR("Unsupported readlinkat call: (%d) %s", dirfd, pathname);
}