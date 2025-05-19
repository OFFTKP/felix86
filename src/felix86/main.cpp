#include <csetjmp>
#include <fstream>
#include <argp.h>
#include <dirent.h>
#include <fcntl.h>
#include <fmt/format.h>
#include <grp.h>
#include <spawn.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include "Zydis/DecoderTypes.h"
#include "Zydis/Disassembler.h"
#include "biscuit/cpuinfo.hpp"
#include "felix86/common/config.hpp"
#include "felix86/common/info.hpp"
#include "felix86/common/log.hpp"
#include "felix86/common/sudo.hpp"
#include "felix86/common/symlink.hpp"
#include "felix86/emulator.hpp"
#include "felix86/hle/thunks.hpp"

#if !defined(__riscv)
#pragma message("You are compiling for x86-64, felix86 should only be compiled for RISC-V, are you sure you want to do this?")
#endif

std::string version_full = get_version_full();
const char* argp_program_version = version_full.c_str();
const char* argp_program_bug_address = "<https://github.com/OFFTKP/felix86/issues>";

static char doc[] = "felix86 - a userspace x86_64 emulator";
static char args_doc[] = "TARGET_BINARY [TARGET_ARGS...]";

static struct argp_option options[] = {
    {"info", 'i', 0, 0, "Print system info"},
    {"configs", 'c', 0, 0, "Print the emulator configurations"},
    {"kill-all", 'k', 0, 0, "Kill all open emulator instances"},
    {"set-rootfs", 's', "DIR", 0, "Set the rootfs path in config.toml"},
    {"binfmt-misc", 'b', 0, 0, "Register the emulator in binfmt_misc so that x86-64 executables can run without prepending the emulator path"},
    {0}};

int guest_arg_start_index = -1;

template <>
struct fmt::formatter<std::filesystem::path> : formatter<std::string_view> {
    template <typename FormatContext>
    auto format(const std::filesystem::path& path, FormatContext& ctx) const {
        return formatter<std::string_view>::format(path.string(), ctx);
    }
};

int print_system_info() {
    printf("%s\n", version_full.c_str());

    using namespace biscuit;
    biscuit::CPUInfo info;
    bool V = info.Has(Extension::V);
    int len = 0;
    if (V) {
        len = info.GetVlenb();
        printf("VLEN: %d\n", len * 8);
    }

    fflush(stdout);

    std::vector<const char*> args = {"neofetch", "cpu", nullptr};

    pid_t pid;
    int status;
    int ok = posix_spawnp(&pid, "neofetch", nullptr, nullptr, (char**)args.data(), environ);
    if (ok != 0)
        goto error;
    waitpid(pid, &status, 0);

    args[1] = "gpu";
    ok = posix_spawnp(&pid, "neofetch", nullptr, nullptr, (char**)args.data(), environ);
    if (ok != 0)
        goto error;
    waitpid(pid, &status, 0);

    args[1] = "model";
    ok = posix_spawnp(&pid, "neofetch", nullptr, nullptr, (char**)args.data(), environ);
    if (ok != 0)
        goto error;
    waitpid(pid, &status, 0);

    args[1] = "distro";
    ok = posix_spawnp(&pid, "neofetch", nullptr, nullptr, (char**)args.data(), environ);
    if (ok != 0)
        goto error;
    waitpid(pid, &status, 0);

    args[1] = "de";
    ok = posix_spawnp(&pid, "neofetch", nullptr, nullptr, (char**)args.data(), environ);
    if (ok != 0)
        goto error;
    waitpid(pid, &status, 0);

    args[1] = "wm";
    ok = posix_spawnp(&pid, "neofetch", nullptr, nullptr, (char**)args.data(), environ);
    if (ok != 0)
        goto error;
    waitpid(pid, &status, 0);

    args[1] = "kernel";
    ok = posix_spawnp(&pid, "neofetch", nullptr, nullptr, (char**)args.data(), environ);
    if (ok != 0)
        goto error;
    waitpid(pid, &status, 0);

    args[1] = "memory";
    ok = posix_spawnp(&pid, "neofetch", nullptr, nullptr, (char**)args.data(), environ);
    if (ok != 0)
        goto error;
    waitpid(pid, &status, 0);

    return 0;

error:
    printf("Please install neofetch for more information\n");
    return ok;
}

void binfmt_misc() {
    if (!Sudo::hasPermissions()) {
        PLAIN("I need root permissions to register/unregister felix86 in binfmt_misc, please re-run with root permissions");
        exit(1);
    }

    char exe_path[4096];
    ssize_t len = readlink("/proc/self/exe", exe_path, sizeof(exe_path) - 1);
    if (len == -1) {
        perror("readlink");
    }
    exe_path[len] = '\0';

    std::string registration_string_x64 = fmt::format(
        R"!(:felix86-x86_64:M:0:\x7fELF\x02\x01\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00\x02\x00\x3e\x00:\xff\xff\xff\xff\xff\xfe\xfe\x00\x00\x00\x00\xff\xff\xff\xff\xff\xfe\xff\xff\xff:{}:OCF)!",
        exe_path);
    std::string registration_string_i386 = fmt::format(
        R"!(:felix86-i386:M:0:\x7fELF\x01\x01\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00\x02\x00\x03\x00:\xff\xff\xff\xff\xff\xfe\xfe\x00\x00\x00\x00\xff\xff\xff\xff\xff\xfe\xff\xff\xff:{}:OCF)!",
        exe_path);

    // Running felix86 -b either registers or unregisters if they already exist
    if (std::filesystem::exists("/proc/sys/fs/binfmt_misc/felix86-x86_64") || std::filesystem::exists("/proc/sys/fs/binfmt_misc/felix86-i386")) {
        auto unregister = [](const char* path) {
            if (!std::filesystem::exists(path)) {
                return;
            }

            FILE* fp = fopen(path, "w");
            if (!fp) {
                ERROR("Failed to fopen %s", path);
            }

            if (fwrite("-1", 1, 2, fp) != 2) {
                fclose(fp);
                ERROR("Failed to write -1 to %s", path);
            }

            fclose(fp);
        };

        unregister("/proc/sys/fs/binfmt_misc/felix86-x86_64");
        unregister("/proc/sys/fs/binfmt_misc/felix86-i386");

        printf("felix86 successfully unregistered from binfmt_misc\n");
    } else {
        FILE* fp = fopen("/proc/sys/fs/binfmt_misc/register", "w");

        if (!fp) {
            ERROR("Failed to open /proc/sys/fs/binfmt_misc/register");
        }

        if (fwrite(registration_string_x64.c_str(), 1, registration_string_x64.size(), fp) != registration_string_x64.size()) {
            fclose(fp);
            ERROR("Failed to register for x86-64");
        }

        fp = fopen("/proc/sys/fs/binfmt_misc/register", "w");

        if (fwrite(registration_string_i386.c_str(), 1, registration_string_i386.size(), fp) != registration_string_i386.size()) {
            fclose(fp);
            ERROR("Failed to register for i386");
        }

        printf("felix86 successfully registered to binfmt_misc\nTo unregister run `felix86 -b`\n");
    }
}

void kill_all() {
    DIR* proc_dir;
    struct dirent* entry;
    pid_t self_pid = getpid();

    proc_dir = opendir("/proc");
    if (!proc_dir) {
        perror("opendir /proc");
    }

    while ((entry = readdir(proc_dir)) != NULL) {
        pid_t pid = atoi(entry->d_name);
        if (pid == self_pid)
            continue;

        if (pid == 0)
            continue;

        char exe_path[PATH_MAX];
        snprintf(exe_path, sizeof(exe_path), "/proc/%d/exe", pid);

        char exe_target[PATH_MAX];
        ssize_t len = readlink(exe_path, exe_target, sizeof(exe_target) - 1);
        if (len == -1)
            continue;

        exe_target[len] = '\0';

        std::string path = exe_target;
        if (path.find(' ') != std::string::npos) {
            // Sometimes paths come up as "/path/to/felix86 (deleted)"
            path = path.substr(0, path.find(' '));
        }

        char* base = basename(path.data());

        if (strcmp(base, "felix86") == 0) {
            if (kill(pid, SIGKILL) == 0) {
                printf("Killed process %d\n", pid);
            } else {
                printf("Failed to kill process %d", pid);
            }
        }
    }
}

static error_t parse_opt(int key, char* arg, struct argp_state* state) {
    StartParameters* params = (StartParameters*)state->input;

    if (key == ARGP_KEY_ARG) {
        if (params->argv.empty()) {
            params->executable_path = arg;
        }

        params->argv.push_back(arg);
        guest_arg_start_index = state->next;
        state->next = state->argc; // tell argp to stop
        return 0;
    }

    switch (key) {
    case 'i': {
        exit(print_system_info());
        break;
    }
    case 'k': {
        kill_all();
        exit(0);
        break;
    }
    case 'b': {
        binfmt_misc();
        exit(0);
        break;
    }
    case 's': {
        Config::initialize();
        char* real_path = realpath(arg, nullptr);
        if (!real_path) {
            printf("Could not resolve %s to an absolute path", arg);
            exit(1);
        }
        printf("Setting rootfs to %s\n", real_path);
        g_config.rootfs_path = real_path;
        Config::save(g_config.path(), g_config);
        exit(0);
        break;
    }
    case 'c': {
        // TODO: add some color here
        Config::initialize();

        std::string current_group;
        printf("These are the configurations for felix86\n");
        printf("You may edit %s or set the corresponding environment variable\n", g_config.path().c_str());

#define X(group, type, name, def, env, description, required)                                                                                        \
    if (current_group != #group) {                                                                                                                   \
        current_group = #group;                                                                                                                      \
        printf("\n[%s]\n", current_group.c_str());                                                                                                   \
    }                                                                                                                                                \
    fmt::print("{} {} = {} (default: {}) -- Environment variable: {}\n", #type, #name, g_config.name, #def, #env);
#include "felix86/common/config.inc"
#undef X
        exit(0);
        break;
    }
    case ARGP_KEY_END: {
        if (params->argv.empty()) {
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

int main(int argc, char* argv[]) {
#ifdef __x86_64__
    WARN("You're running an x86-64 executable version of felix86, get ready for a crash soon");
#endif
    StartParameters params = {};

    argp_parse(&argp, argc, argv, ARGP_IN_ORDER, 0, &params);
    if (guest_arg_start_index != -1) {
        char** argv_next = &argv[guest_arg_start_index];
        while (*argv_next) {
            params.argv.push_back(*argv_next);
            argv_next++;
        }
    }

    g_execve_process = !!getenv("__FELIX86_EXECVE");
    if (!g_execve_process) {
        Logger::startServer();
    } else {
        Logger::joinServer();
    }

    Config::initialize();
    initialize_globals();

    if (!g_execve_process) {
        ASSERT(!g_config.rootfs_path.empty());

        // First time running the emulator (ie. the emulator is not running itself with execve) we need to link some stuff
        // and copy some stuff inside the rootfs
        auto copy = [](const char* src, const std::filesystem::path& dst) {
            if (!std::filesystem::exists(src)) {
                printf("I couldn't find %s to copy to the rootfs, may cause problems with some games\n", src);
                return;
            }

            using co = std::filesystem::copy_options;

            std::error_code ec;
            std::filesystem::copy(src, dst, co::overwrite_existing | co::recursive, ec);
            if (ec) {
                VERBOSE("Error while copying %s: %s", src, ec.message().c_str());
            }
        };

        std::filesystem::create_directories(g_config.rootfs_path / "var" / "lib");
        std::filesystem::create_directories(g_config.rootfs_path / "etc");

        // Copy some stuff to the g_config.rootfs_path
        copy("/var/lib/dbus", g_config.rootfs_path / "var" / "lib" / "dbus");
        copy("/etc/mtab", g_config.rootfs_path / "etc" / "mtab");
        copy("/etc/passwd", g_config.rootfs_path / "etc" / "passwd");
        copy("/etc/passwd-", g_config.rootfs_path / "etc" / "passwd-");
        copy("/etc/group", g_config.rootfs_path / "etc" / "group");
        copy("/etc/group-", g_config.rootfs_path / "etc" / "group-");
        copy("/etc/shadow", g_config.rootfs_path / "etc" / "shadow");
        copy("/etc/shadow-", g_config.rootfs_path / "etc" / "shadow-");
        copy("/etc/gshadow", g_config.rootfs_path / "etc" / "gshadow");
        copy("/etc/gshadow-", g_config.rootfs_path / "etc" / "gshadow-");
        copy("/etc/hosts", g_config.rootfs_path / "etc" / "hosts");
        copy("/etc/hostname", g_config.rootfs_path / "etc" / "hostname");
        copy("/etc/timezone", g_config.rootfs_path / "etc" / "timezone");
        copy("/etc/localtime", g_config.rootfs_path / "etc" / "localtime");
        copy("/etc/fstab", g_config.rootfs_path / "etc" / "fstab");
        copy("/etc/subuid", g_config.rootfs_path / "etc" / "subuid");
        copy("/etc/subgid", g_config.rootfs_path / "etc" / "subgid");
        copy("/etc/machine-id", g_config.rootfs_path / "etc" / "machine-id");
        copy("/etc/resolv.conf", g_config.rootfs_path / "etc" / "resolv.conf");

        // Symlink some directories to make our lives easier and not have to overlay them
        ASSERT_MSG(Symlinker::link("/run", g_config.rootfs_path / "run"), "Failed to symlink /run: %s", strerror(errno));
        ASSERT_MSG(Symlinker::link("/proc", g_config.rootfs_path / "proc"), "Failed to symlink /proc: %s", strerror(errno));
        ASSERT_MSG(Symlinker::link("/sys", g_config.rootfs_path / "sys"), "Failed to symlink /sys: %s", strerror(errno));
        ASSERT_MSG(Symlinker::link("/dev", g_config.rootfs_path / "dev"), "Failed to symlink /dev: %s", strerror(errno));
        mkdirat(g_rootfs_fd, "tmp", 0777);
    }

    Signals::initialize();

    if (!g_config.thunks_path.empty()) {
        Thunks::initialize();
    }

    if (is_subpath(params.argv[0], g_config.rootfs_path)) {
        params.argv[0] = params.argv[0].substr(g_config.rootfs_path.string().size());
        ASSERT(!params.argv[0].empty());
        if (params.argv[0].at(0) != '/') {
            params.argv[0] = '/' + params.argv[0];
        }
    }

    std::string args = "Arguments: ";
    for (const auto& arg : params.argv) {
        args += arg;
        args += " ";
    }
    VERBOSE("%s", args.c_str());

    if (g_execve_process) {
        const char* guest_envs = getenv("__FELIX86_GUEST_ENVS");
        if (guest_envs) {
            std::vector<std::string> envs = split_string(guest_envs, ',');
            for (auto& env : envs) {
                params.envp.push_back(env);
            }
        }
    } else {
        bool purposefully_empty = false;
        const char* env_file = getenv("FELIX86_ENV_FILE");
        if (env_file) {
            std::string env_path = env_file;
            if (std::filesystem::exists(env_path)) {
                std::ifstream env_stream(env_path);
                std::string line;
                while (std::getline(env_stream, line)) {
                    params.envp.push_back(line);
                }

                if (params.envp.empty()) {
                    purposefully_empty = true;
                }
            } else {
                WARN("Environment variable file %s does not exist. Using host environment variables.", env_file);
            }
        }

        if (params.envp.empty() && !purposefully_empty) {
            char** envp = environ;
            while (*envp) {
                params.envp.push_back(*envp);
                envp++;
            }
        }
    }

    // TODO: These "hacky" environment variables are bandaid solutions to problems that we need to eventually fix
    // They are enabled by default
    if (g_config.hacky_envs) {
        // Go uses a bunch of signals for preemption and this breaks our current signal handling
        // Apps like `snap` use go, and those are used sometimes by `apt`, and this async preemption is useless in a lot of programs
        params.envp.push_back("GODEBUG=asyncpreemptoff=1");
    }

    auto it = params.envp.begin();
    while (it != params.envp.end()) {
        std::string env = *it;

        // Dont pass these to the executable itself
        if (env.find("FELIX86_") != std::string::npos) {
            it = params.envp.erase(it);
        } else {
            it++;
        }
    }

    // Resolve symlinks, get absolute path. If the symlink is resolved, it may not start with
    // the rootfs prefix, and we need to add it back
    const std::string rootfs_string = g_config.rootfs_path.string();
    std::filesystem::path resolved = Symlinker::resolve(params.executable_path);
    if (resolved.string().find(rootfs_string) != 0) {
        resolved = g_config.rootfs_path / resolved.relative_path();
    }
    params.executable_path = resolved;

    if (params.executable_path.empty()) {
        ERROR("Executable path not specified");
        return 1;
    } else {
        if (!std::filesystem::exists(params.executable_path)) {
            ERROR("Executable path does not exist: %s", params.executable_path.c_str());
            return 1;
        }

        if (!std::filesystem::is_regular_file(params.executable_path)) {
            ERROR("Executable path is not a regular file");
            return 1;
        }
    }

    if (g_execve_process) {
        pthread_setname_np(pthread_self(), "ExecveProcess");
    } else {
        pthread_setname_np(pthread_self(), "MainProcess");
    }

    auto [exit_reason, exit_code] = Emulator::Start(params);

    if (!g_execve_process) {
        LOG("Main process %d exited with reason: %s. Exit code: %d", getpid(), print_exit_reason(exit_reason), exit_code);
    } else {
        LOG("Execve process %d exited with reason: %s. Exit code: %d", getpid(), print_exit_reason(exit_reason), exit_code);
    }

    return exit_code;
}
