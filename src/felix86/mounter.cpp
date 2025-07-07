#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <linux/limits.h>
#include <spawn.h>
#include <sys/file.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include "mounter.h"

std::string rootfs_path;
int lock_fd = -1;
uid_t uid;
gid_t gid;

#define DIE(message, ...)                                                                                                                            \
    do {                                                                                                                                             \
        printf("felix86-mounter: " message "\n", ##__VA_ARGS__);                                                                                     \
        if (lock_fd != -1) {                                                                                                                         \
            int result = flock(lock_fd, LOCK_UN);                                                                                                    \
            if (result != 0) {                                                                                                                       \
                printf("Failed to unlock /run/felix86/mounter.lock, please remove manually\n");                                                      \
            }                                                                                                                                        \
            close(lock_fd);                                                                                                                          \
        }                                                                                                                                            \
        exit(1);                                                                                                                                     \
    } while (0)

[[noreturn]] void success(const std::string& mounted_path) {
    if (flock(lock_fd, LOCK_UN) != 0) {
        printf("Failed to unlock /run/felix86/mounter.lock, please remove manually\n");
    }
    close(lock_fd);
    if (mounted_path.empty()) {
        DIE("Mounted path was empty at the end?");
    }
    // Print it in stdout so felix86 can pick it up
    printf("%s", mounted_path.c_str());
    exit(0);
}

void prepare_mount(const char* path, const std::filesystem::path& target) {
    std::error_code ec;
    bool exists = std::filesystem::exists(target, ec);
    if (ec) {
        DIE("Failed while checking for %s", target.c_str());
    }

    if (!exists) {
        std::filesystem::create_directory(target, ec);
        if (ec) {
            DIE("Failed to create directory %s", target.c_str());
        }
    } else {
        bool is_symlink = std::filesystem::is_symlink(target, ec);
        if (ec) {
            DIE("is_symlink failed?");
        }

        if (is_symlink) {
            if (rootfs_path.empty()) {
                DIE("Rootfs empty here?");
            }

            printf("Old versions of felix86 would symlink /dev, /proc, /run, /sys and /tmp\n");
            printf("Newer versions of felix86 want to mount these directories\n");
            printf("Please remove the old symlinks:\n\n");
            printf("    unlink %s/dev\n", rootfs_path.c_str());
            printf("    unlink %s/sys\n", rootfs_path.c_str());
            printf("    unlink %s/proc\n", rootfs_path.c_str());
            printf("    unlink %s/run\n", rootfs_path.c_str());
            printf("    unlink %s/tmp\n", rootfs_path.c_str());
            DIE("%s is a symlink", target.c_str());
        }
    }
}

void bind_mount(const char* path, const std::filesystem::path& target) {
    prepare_mount(path, target);

    int result = ::mount(path, target.c_str(), nullptr, MS_BIND, nullptr);
    if (result != 0) {
        DIE("Failed to mount %s to %s", path, target.c_str());
    }
}

void fs_mount(const std::string& type, const std::string& source, const std::string& target) {
    // The mount command does non-trivial work that's not equivalent to calling the mount syscall
    // So we just pass this labor to it
    const char* argv[] = {"mount", "-t", type.c_str(), source.c_str(), target.c_str(), nullptr};

    int status;
    pid_t pid = fork();

    if (pid == 0) {
        if (setuid(geteuid()) != 0) {
            DIE("Couldn't setuid, do I not have permissions?");
        }
        execve("/usr/bin/mount", const_cast<char* const*>(argv), environ);
        DIE("execve(mount) failed");
    } else {
        if (waitpid(pid, &status, 0) == -1) {
            DIE("waitpid failed");
        }

        if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
            DIE("Failed while mounting %s", target.c_str());
        }
    }
}

void own(const std::filesystem::path& path) {
    if (chown(path.c_str(), uid, gid) != 0) {
        DIE("Failed to chown %s", path.c_str());
    }

    if (chmod(path.c_str(), 0777)) {
        DIE("Failed to chmod %s", path.c_str());
    }
}

/**
    We have a special felix86 directory:
    /run/felix86

    Which is going to have felix86 related stuff until we shutdown

    The filesystem structure will go like this:

    /run/felix86/mounts:
        Contains mounts of rootfs dirs. Each rootfs will be mount only once in this dir
        Example follows

    /run/felix86/mounts/mount-XXXXXX
        A dir that will contain data about a rootfs. Which rootfs? Whatever was passed to argv[1] to this executable
        It only gets mounted once. If a directory exists for whatever was passed to argv[1] then we use the pre-existing one
        Because users may want to use multiple rootfs mountings, multiple mount-XXXXXX dirs may appear

    /run/felix86/mounts/mount-XXXXXX/rootfs
        The actual rootfs mount, mounted to argv[1]. Inside here will be the x86 /lib, /usr etc. but also the host /dev, /proc, etc.
        This itself is a bind mount of the actual rootfs. Programs may rely on the fact that "/" is a mount, so this is normal

    /run/felix86/mounts/mount-XXXXXX/path.txt
        The host path of the rootfs. This exists so we can check if a specific path is already mounted somewhere
        so we don't have to mount it again. It contains the actual path to the rootfs.
*/
int main(int argc, char* argv[]) {
    if (!getenv("__FELIX86_I_KNOW_WHAT_IM_DOING")) {
        DIE("Do not use felix86-mounter by yourself. It is used by felix86 to mount the rootfs and necessary folders");
    }

    if (argc != 3) {
        DIE("Bad argument count. Usage: felix86-mounter <rootfs path> <mounter version>");
    }

    if (!argv[1]) {
        DIE("argv[1] is null?");
    }

    if (!argv[2]) {
        DIE("argv[2] is null?");
    }

    std::string version = argv[2];
    if (version != FELIX86_MOUNTER_VERSION) {
        DIE("felix86-mounter version mismatch, %s vs %s. Please update your felix86-mounter binary", version.c_str(), FELIX86_MOUNTER_VERSION);
    }

    if (strlen(argv[1]) >= PATH_MAX) {
        DIE("Argument too big");
    }

    if (strlen(argv[1]) <= 0) {
        DIE("Path is bad?");
    }

    if (geteuid() != 0) {
        char buffer[PATH_MAX];
        int size = readlink("/proc/self/exe", buffer, PATH_MAX);
        buffer[size] = 0;
        printf("felix86-mounter was not given proper permissions to run\n");
        printf("Please install using the installation script\n\nOR\n\n");
        printf("Run the following commands:\n");
        printf("    sudo chown root:root %s\n", buffer);
        printf("    sudo chmod u+s %s\n", buffer);
        DIE("Failed permission check");
    }

    bool exists;
    std::error_code ec;
    rootfs_path = std::filesystem::canonical(argv[1], ec);
    if (ec) {
        DIE("Failed while turning rootfs path to canonical, does rootfs exist?");
    }

    if (rootfs_path == "/") {
        DIE("Rootfs path is /, seems suspicious");
    }

    exists = std::filesystem::exists("/run", ec);
    if (ec) {
        DIE("Failed when checking if /run exists");
    }

    if (!exists) {
        DIE("No /run dir");
    }

    exists = std::filesystem::exists("/run/felix86", ec);
    if (ec) {
        DIE("Failed when checking for /run/felix86 existence");
    }

    struct stat info;
    if (stat(rootfs_path.c_str(), &info) == 0) {
        uid = info.st_uid;
        gid = info.st_gid;
    } else {
        DIE("Failed to stat rootfs");
    }

    if (!exists) {
        bool ok = std::filesystem::create_directory("/run/felix86", ec);
        if (!ok || ec) {
            DIE("Failed to create /run/felix86");
        }

        own("/run/felix86");
    }

    int fd = open("/run/felix86/mounter.lock", O_CREAT | O_RDWR, 0666);
    if (fd < 0) {
        DIE("Failed to open /run/felix86/mounter.lock");
    }

    auto start = std::chrono::high_resolution_clock::now();
    while (true) {
        int result = flock(fd, LOCK_EX | LOCK_NB);
        if (result == 0) {
            lock_fd = fd;
            break;
        } else {
            auto now = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::seconds>(now - start);
            if (duration > std::chrono::seconds(3)) {
                printf("Locking /run/felix86/mounter.lock took more than 3 seconds, something is stuck with the lock\n");
                printf("Please find what's holding the lock, kill it, remove the lock file, and try again\n");
                printf("Example:\n");
                printf("   lsof /run/felix86/mounter.lock\n");
                printf("   kill -9 <PID>\n");
                printf("   rm -f /run/felix86/mounter.lock\n");
                printf("   (These operations may need sudo)\n");
                close(fd);
                exit(1);
            }
        }
    }

    // We are now properly locked, time to search for the mount or create a new one
    exists = std::filesystem::exists("/run/felix86/mounts", ec);
    if (ec) {
        DIE("Failed when checking for /run/felix86/mounts existence");
    }

    if (!exists) {
        bool ok = std::filesystem::create_directory("/run/felix86/mounts", ec);
        if (!ok || ec) {
            DIE("Failed to create /run/felix86/mounts");
        }

        own("/run/felix86/mounts");
    }

    auto it = std::filesystem::directory_iterator("/run/felix86/mounts", ec);
    if (ec) {
        DIE("Failed when creating iterator for /run/felix86/mounts");
    }

    int i = 0;
    for (const auto& entry : it) {
        if (std::filesystem::is_directory(entry, ec)) {
            if (std::filesystem::exists(entry.path() / "path.txt", ec)) {
                std::ifstream ifs(entry.path() / "path.txt");
                std::string path;
                ifs >> path;

                if (path == rootfs_path) {
                    // Turns out we already mounted this rootfs in the past, so use this!
                    bool exists = std::filesystem::exists(entry.path() / "rootfs", ec);
                    if (!exists || ec) {
                        DIE("I found a mount, but /rootfs doesn't exist?");
                    }

                    success(entry.path() / "rootfs");
                    DIE("Unreachable");
                }
            }
        }

        i++;
        if (i > 40) {
            // Just die after an arbitrary amount of mounted rootfses
            // It would be either a bug or suspicious that we'd have that many
            DIE("More than 40 different mounted rootfs paths?");
        }
    }

    // If we got here, we didn't find a mount. This means we need to create a new one
    char buffer[] = "/run/felix86/mounts/mount-XXXXXX";
    char* tmp = mkdtemp(buffer);
    if (!tmp) {
        DIE("Failed to create template");
    }

    if (strlen(buffer) != strlen(tmp)) {
        DIE("What?");
    }

    own(tmp);

    std::filesystem::path mount_base = tmp;
    std::filesystem::path mount_target = mount_base / "rootfs";

    // Create the place where we will mount the actual rootfs
    bool ok = std::filesystem::create_directory(mount_target, ec);
    if (!ok || ec) {
        DIE("Error while creating %s", mount_target.c_str());
    }

    own(mount_target);

    // Create the place where we can store auxiliary mountings for pivot_root
    ok = std::filesystem::create_directory(mount_base / "mounts", ec);
    if (!ok || ec) {
        DIE("Error while creating /mounts");
    }

    own(mount_base / "mounts");

    bind_mount(rootfs_path.c_str(), mount_target);

    // Rootfs was mounted, mount everything else we need
    prepare_mount("/dev", mount_target / "dev");
    prepare_mount("/dev/pts", mount_target / "dev" / "pts");
    prepare_mount("/proc", mount_target / "proc");
    prepare_mount("/sys", mount_target / "sys");
    prepare_mount("/run", mount_target / "run");
    fs_mount("proc", "proc", mount_target / "proc");
    fs_mount("sysfs", "sysfs", mount_target / "sys");
    fs_mount("devtmpfs", "udev", mount_target / "dev");
    fs_mount("devpts", "devpts", mount_target / "dev" / "pts");
    bind_mount("/run", mount_target / "run");
    bind_mount("/tmp", mount_target / "tmp");

    // Fix permissions in /run/user to match the id they belong to
    std::filesystem::path run_user = mount_target / "run" / "user";
    std::filesystem::directory_iterator dir_it(run_user);
    for (auto& entry : dir_it) {
        std::string number = entry.path().filename();
        long id = std::atol(number.c_str());
        if (id) {
            chown(entry.path().c_str(), id, id);
        }
    }

    auto copy = [](const char* src, const std::filesystem::path& dst) {
        if (!std::filesystem::exists(src)) {
            return;
        }

        using co = std::filesystem::copy_options;

        std::error_code ec;
        std::filesystem::copy(src, dst, co::overwrite_existing | co::recursive, ec);
    };

    std::filesystem::create_directories(mount_target / "etc", ec);
    std::filesystem::create_directories(mount_target / "var" / "lib", ec);

    // Copy some stuff to the rootfs_path
    copy("/var/lib/dbus", mount_target / "var" / "lib" / "dbus");
    copy("/etc/mtab", mount_target / "etc" / "mtab");
    copy("/etc/passwd", mount_target / "etc" / "passwd");
    copy("/etc/passwd-", mount_target / "etc" / "passwd-");
    copy("/etc/group", mount_target / "etc" / "group");
    copy("/etc/group-", mount_target / "etc" / "group-");
    copy("/etc/shadow", mount_target / "etc" / "shadow");
    copy("/etc/shadow-", mount_target / "etc" / "shadow-");
    copy("/etc/gshadow", mount_target / "etc" / "gshadow");
    copy("/etc/gshadow-", mount_target / "etc" / "gshadow-");
    copy("/etc/hosts", mount_target / "etc" / "hosts");
    copy("/etc/hostname", mount_target / "etc" / "hostname");
    copy("/etc/timezone", mount_target / "etc" / "timezone");
    copy("/etc/localtime", mount_target / "etc" / "localtime");
    copy("/etc/fstab", mount_target / "etc" / "fstab");
    copy("/etc/subuid", mount_target / "etc" / "subuid");
    copy("/etc/subgid", mount_target / "etc" / "subgid");
    copy("/etc/machine-id", mount_target / "etc" / "machine-id");
    copy("/etc/resolv.conf", mount_target / "etc" / "resolv.conf");

    // Only now that everything was mounted, write to the path.txt for future invocations
    {
        std::ofstream ofs(mount_base / "path.txt");
        ofs << rootfs_path;
    }

    success(mount_target);

    DIE("Unreachable");
    return 1;
}