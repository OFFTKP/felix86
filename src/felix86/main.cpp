#include <csetjmp>
#include <fstream>
#include <argp.h>
#include <fcntl.h>
#include <fmt/format.h>
#include <grp.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include "felix86/common/log.hpp"
#include "felix86/emulator.hpp"
#include "felix86/hle/filesystem.hpp"

#if !defined(__riscv)
#pragma message("felix86 should only be compiled for RISC-V")
#endif

constexpr const char* lock_path = "/var/lock/felix86.lock";

std::string version_full = get_version_full();
const char* argp_program_version = version_full.c_str();
const char* argp_program_bug_address = "<https://github.com/OFFTKP/felix86/issues>";

static char doc[] = "felix86 - a userspace x86_64 emulator";
static char args_doc[] = "TARGET_BINARY [TARGET_ARGS...]";

std::vector<std::string> mounts;

static struct argp_option options[] = {
    {"verbose", 'V', 0, 0, "Produce verbose output"},
    {"quiet", 'q', 0, 0, "Don't produce any output"},
    {"rootfs-path", 'L', "PATH", 0, "Path to the rootfs directory"},
    {"strace", 't', 0, 0, "Trace emulated application syscalls"},
    {"all-extensions", 'X', "EXTS", 0,
     "Manually specify every available RISC-V extension. When using this, any extension not specified will be considered unavailable. "
     "Usage example: -e g,c,v,b,zacas"},

    {0}};

void print_extensions() {
    std::string extensions;
    if (Extensions::G) {
        extensions += "g";
    }
    if (Extensions::V) {
        if (!extensions.empty())
            extensions += ",";
        extensions += "v";
        extensions += std::to_string(Extensions::VLEN);
    }
    if (Extensions::C) {
        if (!extensions.empty())
            extensions += ",";
        extensions += "c";
    }
    if (Extensions::B) {
        if (!extensions.empty())
            extensions += ",";
        extensions += "b";
    }
    if (Extensions::Zacas) {
        if (!extensions.empty())
            extensions += ",";
        extensions += "zacas";
    }
    if (Extensions::Zam) {
        if (!extensions.empty())
            extensions += ",";
        extensions += "zam";
    }
    if (Extensions::Zabha) {
        if (!extensions.empty())
            extensions += ",";
        extensions += "zabha";
    }
    if (Extensions::Zicond) {
        if (!extensions.empty())
            extensions += ",";
        extensions += "zicond";
    }
    if (Extensions::Zfa) {
        if (!extensions.empty())
            extensions += ",";
        extensions += "zfa";
    }

    if (!extensions.empty()) {
        LOG("Extensions enabled for the recompiler: %s", extensions.c_str());
    }
}

int guest_arg_start_index = -1;

static error_t parse_opt(int key, char* arg, struct argp_state* state) {
    Config* config = (Config*)state->input;

    if (key == ARGP_KEY_ARG) {
        if (config->argv.empty()) {
            config->executable_path = arg;
        }

        config->argv.push_back(arg);
        guest_arg_start_index = state->next;
        state->next = state->argc; // tell argp to stop
        return 0;
    }

    switch (key) {
    case 'V': {
        enable_verbose();
        break;
    }
    case 'q': {
        disable_logging();
        break;
    }
    case 'L': {
        g_rootfs_path = arg;
        break;
    }
    case 't': {
        g_strace = true;
        break;
    }
    case 'X': {
        if (!parse_extensions(arg)) {
            argp_usage(state);
        } else {
            g_extensions_manually_specified = true;
        }
        break;
    }
    case ARGP_KEY_END: {
        if (config->argv.empty()) {
            argp_usage(state);
        }
        break;
    }

    default: {
        return ARGP_ERR_UNKNOWN;
    }
    }
    return 0;
}

static struct argp argp = {options, parse_opt, args_doc, doc};

int watchdog_loop(int pid);

void mountme(const char* path, const char* dest, const char* fs_type) {
    int mkdir_res = mkdir(dest, 0755);
    if (mkdir_res < 0) {
        if (errno != EEXIST) {
            ERROR("Failed to create directory %s. Error: %d", dest, errno);
            felix86_exit(1);
        }
    }

    int result = mount(path, dest, fs_type, 0, NULL);
    if (result < 0) {
        ERROR("Failed to mount %s to %s. Error: %d", path, dest, errno);
        felix86_exit(1);
    }
    VERBOSE("Mounting %s to %s", path, dest);

    mounts.push_back(dest);
}

int main(int argc, char* argv[]) {
#if 0 // for testing zydis behavior on specific instructions
    ZydisDecoder decoder;
    ZydisDecoderInit(&decoder, ZYDIS_MACHINE_MODE_LONG_64, ZYDIS_STACK_WIDTH_64);

    u8 data[] = {
        0x4c,
        0x8d,
        0x14,
        0x82,
    };

    ZydisDecodedInstruction instruction;
    ZydisDecodedOperand operands[10];
    ZyanStatus status = ZydisDecoderDecodeFull(&decoder, data, sizeof(data), &instruction, operands);
    ASSERT(ZYAN_SUCCESS(status));

    printf("operand count: %d\n", instruction.operand_count_visible);
    printf("op 0: %d\n", operands[1].mem.scale);
#endif

    Config config = {};

    argp_parse(&argp, argc, argv, ARGP_IN_ORDER, 0, &config);
    if (guest_arg_start_index != -1) {
        char** argv_next = &argv[guest_arg_start_index];
        while (*argv_next) {
            config.argv.push_back(*argv_next);
            argv_next++;
        }
    }

    // This instance of felix86 may be running as an execve'd version of an older instance
    // In this case we shouldn't print the version string and unlink the semaphore
    const char* execve_process = getenv("__FELIX86_EXECVE");

    for (int i = 0; i < guest_arg_start_index - 1; i++) {
        g_host_argv.push_back(argv[i]);
    }

    if (!execve_process) {
        LOG("%s", version_full.c_str());
    }

    std::string args = "Arguments: ";
    for (const auto& arg : config.argv) {
        args += arg;
        args += " ";
    }
    VERBOSE("%s", args.c_str());

#ifdef __x86_64__
    WARN("You're running an x86-64 executable version of felix86, get ready for a crash soon");
#endif
    g_output_fd = STDOUT_FILENO;

    initialize_globals();
    initialize_extensions();
    print_extensions();

    Signals::initialize();

    const char* env_file = getenv("FELIX86_ENV_FILE");
    if (env_file) {
        std::string env_path = env_file;
        if (std::filesystem::exists(env_path)) {
            std::ifstream env_stream(env_path);
            std::string line;
            while (std::getline(env_stream, line)) {
                config.envp.push_back(line);
            }
        } else {
            ERROR("Environment variable file %s does not exist", env_file);
        }
    } else {
        char** envp = environ;
        while (*envp) {
            config.envp.push_back(*envp);
            envp++;
        }
    }

    config.rootfs_path = g_rootfs_path;

    // Sanitize the executable path
    std::string path = config.argv[0];
    if (path.size() < g_rootfs_path.string().size()) {
        ERROR("Executable path is not part of the rootfs");
    }
    path = path.substr(g_rootfs_path.string().size());
    ASSERT(!path.empty());
    if (path[0] != '/') {
        path = "/" + path;
    }
    config.argv[0] = path;

    if (config.rootfs_path.empty()) {
        ERROR("Rootfs path not specified");
        return 1;
    } else {
        if (!std::filesystem::exists(config.rootfs_path)) {
            ERROR("Rootfs path does not exist");
            return 1;
        }

        if (!std::filesystem::is_directory(config.rootfs_path)) {
            ERROR("Rootfs path is not a directory");
            return 1;
        }
    }

    if (config.executable_path.empty()) {
        ERROR("Executable path not specified");
        return 1;
    } else {
        if (!std::filesystem::exists(config.executable_path)) {
            ERROR("Executable path does not exist");
            return 1;
        }

        if (!std::filesystem::is_regular_file(config.executable_path)) {
            ERROR("Executable path is not a regular file");
            return 1;
        }

        std::string exec_path = config.executable_path.string();
        std::string rootfs_path = config.rootfs_path.string();

        if (exec_path.size() >= rootfs_path.size()) {
            // Remove rootfs from executable path, if the user prepended it
            if (exec_path.substr(0, rootfs_path.size()) == rootfs_path) {
                config.executable_path = exec_path.substr(rootfs_path.size());
            }
        }
    }

    if (execve_process) {
        pthread_setname_np(pthread_self(), "ExecveProcess");
    } else {
        pthread_setname_np(pthread_self(), "MainProcess");
    }

    if (geteuid() != 0) {
        // Try to restart app with sudo
        std::vector<const char*> sudo_args = {"sudo"};
        sudo_args.push_back("-E");
        for (int i = 0; i < argc; i++) {
            sudo_args.push_back(argv[i]);
        }
        sudo_args.push_back(nullptr);
        execvpe("sudo", (char* const*)sudo_args.data(), environ);
        ERROR("felix86 needs administrator privileges to chroot and mount. Failed to restart felix86 with sudo. Please run felix86 with "
              "administrator privileges. Error code: %d",
              errno);
    }

    if (!execve_process) {
        unlink_semaphore(); // in case it was not closed properly last time
    }

    initialize_semaphore();

    // Child process - emulator
    if (!execve_process) {
        int lock_file = open(lock_path, O_CREAT | O_EXCL, 0666);
        if (lock_file == -1) {
            ERROR("Failed to acquire felix86.lock, is another instance of felix86 running? Otherwise remove the lock file manually: %s", lock_path);
            return 1;
        }
        close(lock_file);

        // Mount the necessary filesystems
        ASSERT(!config.rootfs_path.empty());
        ASSERT(mounts.empty());
        mountme("proc", (config.rootfs_path / "proc").c_str(), "proc");
        mountme("sysfs", (config.rootfs_path / "sys").c_str(), "sysfs");
        mountme("udev", (config.rootfs_path / "dev").c_str(), "devtmpfs");
        mountme("devpts", (config.rootfs_path / "dev/pts").c_str(), "devpts");

        int pid = fork();
        if (pid != 0) {
            // Parent process, will watch until the child process (emulator) is finished to clean up.
            return watchdog_loop(pid);
        }

        if (geteuid() == 0) { // When running as root, we can fully chroot into rootfs and save some time
            // Check that proc is mounted successfully
            char buffer1[PATH_MAX];
            char buffer2[PATH_MAX];
            int n1 = readlink("/proc/self/exe", buffer1, PATH_MAX);
            if (n1 == -1) {
                ERROR("Failed to read /proc/self/exe, is /proc mounted?");
                return 1;
            }

            int n2 = readlink((config.rootfs_path / "proc/self/exe").c_str(), buffer2, PATH_MAX);
            if (n2 == -1) {
                ERROR("Failed to read /proc/self/exe from chroot, is /proc mounted?");
                return 1;
            }

            if (n1 != n2 || memcmp(buffer1, buffer2, n1) != 0) {
                ERROR("Error while comparing /proc/self/exe results from inside and outside the chroot");
                return 1;
            }

            int result = chroot(config.rootfs_path.c_str());
            if (result < 0) {
                ERROR("Failed to chroot to %s. Error: %d", config.rootfs_path.c_str(), errno);
                return 1;
            }
            ASSERT(g_rootfs_path == config.rootfs_path); // don't change me in the future
            g_is_chrooted = true;

            // Drop root privileges
            const char* allow_root_env = getenv("FELIX86_ALLOW_ROOT");
            bool allow_root = false;
            if (allow_root_env && std::string(allow_root_env) == "1") {
                WARN("Running felix86 with root privileges");
                allow_root = true;
            }

            if (!allow_root) {
                const char* gid_env = getenv("SUDO_GID");
                const char* uid_env = getenv("SUDO_UID");

                std::string suggestion = "If you want to run felix86 with root privileges (not recommended), "
                                         "set the FELIX86_ALLOW_ROOT environment variable to 1. Otherwise run without root privileges.";

                if (!uid_env || !gid_env) {
                    ERROR("SUDO_UID or SUDO_GID not set, can't drop root privileges. %s", suggestion.c_str());
                    return 1;
                }

                std::string user = getenv("SUDO_USER");
                gid_t gid = std::stoul(gid_env);
                uid_t uid = std::stoul(uid_env);

                if (initgroups(user.c_str(), gid) != 0) {
                    ERROR("initgroups failed when trying to drop root privileges. %s", suggestion.c_str());
                    return 1;
                }

                if (setgid(gid) != 0) {
                    ERROR("setgid failed when trying to drop root privileges. %s", suggestion.c_str());
                    return 1;
                }

                if (setuid(uid) != 0) {
                    ERROR("setuid failed when trying to drop root privileges. %s", suggestion.c_str());
                    return 1;
                }

                ASSERT(geteuid() != 0);
                ASSERT(getuid() != 0);

                int result = chdir("/");
                if (result < 0) {
                    ERROR("Failed to change directory to / after dropping root privileges. Error: %d", errno);
                    return 1;
                }
            }
        } else {
            ERROR("Should not get here, felix86 has no root privileges");
        }
    }

    Emulator emulator(config);

    ThreadState* main_state = ThreadState::Get();

    emulator.Run();

    if (!execve_process) {
        LOG("Main process exited with reason: %s", print_exit_reason(main_state->exit_reason));
    } else {
        LOG("Execve process exited with reason: %s", print_exit_reason(main_state->exit_reason));
    }

    unlink_semaphore();

    felix86_exit(0);
}

int watchdog_loop(int pid) {
    int status;
    int ret = 0;
    while (true) {
        waitpid(pid, &status, 0);
        if (WIFEXITED(status)) {
            ret = WEXITSTATUS(status);
            break;
        } else if (WIFSIGNALED(status)) {
            ret = WTERMSIG(status);
            break;
        }
    }

    remove(lock_path);

    // Make sure there's no child processes, although there shouldn't ever be
    int st = waitpid(-1, NULL, WNOHANG);
    if (st == 0) {
        // A child process that isn't a zombie is still running
        // Panic
        ERROR("A child process is still running, this should never happen");
    }

    // Unmount everything
    for (auto it = mounts.rbegin(); it != mounts.rend(); it++) {
        std::string mount = *it;
        int result = umount(mount.c_str());
        VERBOSE("Unmounting %s", mount.c_str());
        if (result < 0) {
            WARN("Failed to unmount %s, please unmount it yourself. Error: %d", mount.c_str(), errno);
        } else {
            result = rmdir(mount.c_str());
            if (result < 0) {
                WARN("Failed to remove directory %s, please remove it yourself. Error: %d", mount.c_str(), errno);
            }
        }
    }

    LOG("Watchdog exiting, felix86 has finished");

    return ret;
}