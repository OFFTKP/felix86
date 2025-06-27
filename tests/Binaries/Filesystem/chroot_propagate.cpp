#include <filesystem>
#include <fcntl.h>
#include <linux/limits.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include "common.h"

// WARN: this file needs to be statically linked to work, since we chroot then execve
// g++ -static -s -I../ -O3 ./chroot_propagate.cpp -o chroot_propagate.out
void touch(const std::filesystem::path& path) {
    int fd = open(path.c_str(), O_CREAT | O_WRONLY, 0644);
    if (fd > 0) {
        close(fd);
    } else {
        printf("Failed to touch?\n");
        exit(1);
    }
}

int main() {
    bool is_execve = !!getenv("__BTEST_EXECVE");
    if (is_execve) {
        // We should already be chrooted, make sure the file exists
        int fd = open("/file1_felix86", O_RDONLY, 0644);
        if (fd <= 0) {
            printf("Failed to open /file1_felix86?\n");
            return 1;
        }
        close(fd);
        return FELIX86_BTEST_SUCCESS;
    } else {
        // Unshare so we have chroot perms
        unshare(CLONE_NEWNS | CLONE_NEWUSER);

        char temp[] = "/tmp/felix86-fstest-XXXXXX";
        const char* cpath = mkdtemp(temp);
        std::filesystem::path dir = cpath;

        // Copy our binary inside the chroot
        char buffer[PATH_MAX];
        int size = readlink("/proc/self/exe", buffer, PATH_MAX);
        buffer[size] = 0;

        std::filesystem::path exec = dir / "executable_felix86";
        std::filesystem::copy(buffer, exec);

        touch(dir / "file1_felix86");

        if (::chroot(cpath) != 0) {
            printf("No perms?\n");
            return 1;
        }

        chdir("/");

        int fd = open("/file1_felix86", O_RDONLY, 0644);
        if (fd <= 0) {
            printf("Failed to open /file1_felix86?\n");
            return 1;
        }
        close(fd);

        int pid = fork();
        if (pid == 0) {
            const char* argv[] = {"/executable_felix86", nullptr};
            const char* envp[] = {"__BTEST_EXECVE=1", nullptr};
            int execed = execve("/executable_felix86", (char**)argv, (char**)envp);
            printf("Failed execve\n");
            return 1;
        } else {
            int status;
            waitpid(pid, &status, 0);
            int result = WEXITSTATUS(status);
            if (result != FELIX86_BTEST_SUCCESS) {
                return 1;
            }
        }
    }

    return FELIX86_BTEST_SUCCESS;
}