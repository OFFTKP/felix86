#include <filesystem>
#include <fcntl.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <unistd.h>

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
    int result = unshare(CLONE_NEWNS | CLONE_NEWUSER);
    if (result != 0) {
        perror("unshare");
    }

    char tmp[] = "/tmp/felix86_pivot_test.XXXXXX";
    mkdtemp(tmp);

    std::filesystem::path rootdir = tmp;
    rootdir /= "root";

    result = mkdir(rootdir.c_str(), 0777);
    if (result != 0) {
        perror("mkdir");
    }

    std::filesystem::path mountdir = tmp;
    mountdir /= "mount";

    result = mkdir(mountdir.c_str(), 0777);
    if (result != 0) {
        perror("mkdir");
    }

    touch(rootdir / "file1");

    result = mount(rootdir.c_str(), mountdir.c_str(), nullptr, MS_BIND, nullptr);
    if (result != 0) {
        perror("mount");
    }

    // First, chroot so our current root is some known place that we know the directories inside
    result = chroot(tmp);
    if (result != 0) {
        perror("chroot");
    }

    result = chdir("/");

    // Now we are ready to chroot
    result = chdir("mount");
    if (result != 0) {
        perror("chdir");
    }

    result = syscall(SYS_pivot_root, ".", ".");
    if (result != 0) {
        perror("pivot_root");
    }

    // Now unmount the old root like bubblewrap
    result = umount2(".", MNT_DETACH);
}