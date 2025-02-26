#include <csetjmp>
#include <fstream>
#include <argp.h>
#include <fcntl.h>
#include <fmt/format.h>
#include <grp.h>
// #include <sys/capability.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include "felix86/common/log.hpp"
#include "felix86/emulator.hpp"

#if !defined(__riscv)
#pragma message("You are compiling for x86-64, felix86 should only be compiled for RISC-V, are you sure you want to do this?")
#endif

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

void mountme(const char* path, const std::filesystem::path& dest, const char* fs_type, unsigned flags = 0) {
    std::filesystem::create_directories(dest);

    int result = mount(path, dest.c_str(), fs_type, flags, NULL);
    if (result < 0) {
        WARN("Failed to mount %s to %s. Error: %d", path, dest.c_str(), errno);
    }
    VERBOSE("Mounting %s to %s", path, dest.c_str());

    mounts.push_back(dest);
}

// int drop_capabilities() {
//     cap_t caps = cap_init();
//     if (caps == nullptr) {
//         fprintf(stderr, "Error: cap_init() failed.\n");
//         return -1;
//     }

//     if (cap_set_proc(caps) == -1) {
//         fprintf(stderr, "Error: cap_set_proc() failed.\n");
//         return -1;
//     }

//     cap_free(caps);
//     return 0;
// }

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

    if (geteuid() != 0) {
        // Try to restart app with sudo
        LOG("I need administrator permissions to chroot and mount if necessary. Requesting administrator privileges...");
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

    LOG("%s", version_full.c_str());

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

    bool purposefully_empty = false;
    const char* env_file = getenv("FELIX86_ENV_FILE");
    if (env_file) {
        std::string env_path = env_file;
        if (std::filesystem::exists(env_path)) {
            std::ifstream env_stream(env_path);
            std::string line;
            while (std::getline(env_stream, line)) {
                config.envp.push_back(line);
            }

            if (config.envp.empty()) {
                purposefully_empty = true;
            }
        } else {
            WARN("Environment variable file %s does not exist. Using host environment variables.", env_file);
        }
    }

    if (config.envp.empty() && !purposefully_empty) {
        char** envp = environ;
        while (*envp) {
            config.envp.push_back(*envp);
            envp++;
        }
    }

    const char* allow_root_env = getenv("FELIX86_ALLOW_ROOT");
    bool allow_root = false;
    if (allow_root_env && std::string(allow_root_env) == "1") {
        WARN("Running felix86 with root privileges");
        allow_root = true;
    }

    auto it = config.envp.begin();
    while (it != config.envp.end()) {
        std::string env = *it;

        // Dont pass these to the executable itself
        if (env.find("FELIX86_") != std::string::npos) {
            it = config.envp.erase(it);
        } else {
            if (!allow_root) {
                if (env.find("SUDO_") != std::string::npos) {
                    it = config.envp.erase(it);
                } else {
                    it++;
                }
            } else {
                it++;
            }
        }
    }

    config.rootfs_path = g_rootfs_path;

    // Make it so we can work with both rootfs/path and /path
    if (config.executable_path.string().find(g_rootfs_path.string()) == std::string::npos) {
        config.executable_path = g_rootfs_path / config.executable_path.relative_path();
    }

    std::string arg0 = config.executable_path.string().substr(g_rootfs_path.string().size());
    ASSERT(!arg0.empty());
    if (arg0[0] != '/') {
        arg0 = "/" + arg0;
    }
    config.argv[0] = arg0;

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
            ERROR("Executable path does not exist: %s", config.executable_path.c_str());
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

    pthread_setname_np(pthread_self(), "MainProcess");

    // Mount the necessary filesystems
    if (config.rootfs_path.empty()) {
        ERROR("Rootfs path not specified, should not happen here");
    }

    std::filesystem::path has_mounted_var_path = "/run/felix86.mounted";

    if (!std::filesystem::exists("/run") || !std::filesystem::is_directory("/run")) {
        ERROR("/run does not exist?");
    }

    int has_mounted_var = open(has_mounted_var_path.c_str(), 0, 0666);
    if (has_mounted_var != -1) {
        // This file was already created, which means a previous instance of felix86 mounted the directories
        LOG("We are already mounted!");
        close(has_mounted_var);
    } else {
        // Mount the necessary filesystems
        LOG("Mounting filesystems...");
        mountme("proc", config.rootfs_path / "proc", "proc");
        mountme("sysfs", config.rootfs_path / "sys", "sysfs");
        mountme("udev", config.rootfs_path / "dev", "devtmpfs");
        mountme("devpts", config.rootfs_path / "dev/pts", "devpts");
        mountme("/run", config.rootfs_path / "run", "none", MS_BIND | MS_REC);

        int fd = open(has_mounted_var_path.c_str(), O_CREAT | O_EXCL, 0666);
        if (fd == -1) {
            ERROR("Failed to create the mount variable file");
        } else {
            close(fd);
        }
    }

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

    ASSERT(getuid() == 0);
    ASSERT(g_rootfs_path == config.rootfs_path); // don't change me in the future
    g_is_chrooted = true;

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

        // TODO: use this instead?
        // drop_capabilities();
    }

    result = chdir("/");
    if (result < 0) {
        ERROR("Failed to change directory to / after dropping root privileges. Error: %d", errno);
        return 1;
    }

    auto [exit_reason, exit_code] = Emulator::Start(config);

    if (!g_execve_process) {
        LOG("Main process exited with reason: %s. Exit code: %d", print_exit_reason(exit_reason), exit_code);
    } else {
        LOG("Execve process exited with reason: %s. Exit code: %d", print_exit_reason(exit_reason), exit_code);
    }

    return exit_code;
}
