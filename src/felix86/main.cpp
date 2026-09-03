#include <cstddef>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <optional>
#include <argp.h>
#include <dirent.h>
#include <fcntl.h>
#include <fmt/format.h>
#include <grp.h>
#include <pwd.h>
#include <spawn.h>
#include <sys/auxv.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include "biscuit/cpuinfo.hpp"
#include "felix86/common/binfmt.hpp"
#include "felix86/common/config.hpp"
#include "felix86/common/global.hpp"
#include "felix86/common/info.hpp"
#include "felix86/common/log.hpp"
#include "felix86/common/utility.hpp"
#include "felix86/emulator.hpp"
#include "felix86/hle/signals.hpp"
#include "felix86/hle/thunks.hpp"

#if !defined(__riscv)
#pragma message("You are compiling for x86-64, felix86 should only be compiled for RISC-V, are you sure you want to do this?")
#endif

static void rootfs_not_set_error() {
    printf("Rootfs path not set in %s\n", g_config.path().c_str());
    printf("Set it using `sudo felix86 --set-config general.rootfs_path=/path/to/rootfs`.\n");
    printf("Consult the installation guide: https://felix86.com/docs/users/installation-guide/\n\n");
    printf("If you don't have an x86 rootfs, you can use the rootfs installer script to download and install one:\n");
    printf("    bash <(curl -s https://install.felix86.com/rootfs.sh)\n");
    exit(1);
}

void __attribute__((noreturn)) enter_repl();

static std::string version_full = get_version_full();
const char* argp_program_version = version_full.c_str();
const char* argp_program_bug_address = "<https://github.com/OFFTKP/felix86/issues>";

static char doc[] = "felix86 - a userspace x86 and x86_64 emulator for RISC-V";
static char args_doc[] = "TARGET_BINARY [TARGET_ARGS...]";

static struct argp_option options[] = {
    {"shell", -1, "PROGRAM", OPTION_ARG_OPTIONAL, "Enter the rootfs through a shell"},
    {"shell-debug", -2, "PROGRAM", OPTION_ARG_OPTIONAL, "Enter the rootfs through a shell, enable logging"},
    {"get-config", -3, "<GROUP.CONFIG>", 0, "Get a config value from the local felix configuration file. Format is '<group.config>'"},
    {"set-config", -4, "<GROUP.CONFIG>=<VALUE>", 0,
     "Set a config value and store it into the local felix configuration. Format is '<group.config>=<value>'"},
    {"info", 'i', 0, 0, "Print system info"},
    {"configs", 'c', 0, 0, "Print the emulator configurations"},
    {"kill-all", 'k', 0, 0, "Kill all open emulator instances"},
    {"binfmt-misc", 'b', 0, 0, "Register the emulator in binfmt_misc so that x86-64 executables can run without prepending the emulator path"},
    {"binfmt-misc-setuid", 'B', 0, 0,
     "Register the emulator in binfmt_misc so that x86-64 executables can run without prepending the emulator path. Also enables the credentials "
     "flag, which allows for running privileged executables with the emulator. This may have security implications, read: "
     "https://felix86.com/docs/users/troubleshooting#privileged-executables-dont-work"},
    {"detect-binfmt-misc", 'd', 0, 0, "Check if we are correctly registered in binfmt_misc, returns 0 if ok"},
    {"unregister-binfmt-misc", 'u', 0, 0, "Unregister the emulator from binfmt_misc"},
    {"log-server", 'l', 0, 0, "Start just the log server in the background"},
#ifdef BUILD_REPL
    {"repl", 'r', 0, 0, "Enter a REPL environment for testing instruction translations"},
#endif
    {0}};

static int guest_arg_start_index = -1;

static std::filesystem::path unmodified_executable_path;

template <>
struct fmt::formatter<std::filesystem::path> : formatter<std::string_view> {
    template <typename FormatContext>
    auto format(const std::filesystem::path& path, FormatContext& ctx) const {
        return formatter<std::string_view>::format(path.string(), ctx);
    }
};

static int print_system_info() {
    printf("%s\n", version_full.c_str());

    using namespace biscuit;
    biscuit::CPUInfo info;
    bool V = info.Has(RISCVExtension::V);
    int len = 0;
    if (V) {
        len = info.GetVlenb();
        printf("VLEN: %d\n", len * 8);
    }

    fflush(stdout);

    std::vector<const char*> args = {"fastfetch", "--structure", "cpu:gpu:host:distro:kernel:wm:memory", "--logo", "none", nullptr};
    pid_t pid;
    int status;
    int ok = posix_spawnp(&pid, "fastfetch", nullptr, nullptr, (char**)args.data(), environ);
    if (ok != 0) {
        printf("Please install fastfetch for more information\n");
        return ok;
    } else {
        int result = waitpid(pid, &status, 0);
        if (!(result == 0 && WIFEXITED(status) && WEXITSTATUS(status) == 0)) {
            return 0;
        } else {
            printf("Failed to get info from fastfetch\n");
            return 1;
        }
    }
}

static void kill_all() {
    DIR* proc_dir;
    struct dirent* entry;
    pid_t self_pid = getpid();

    proc_dir = opendir("/proc");
    if (!proc_dir) {
        perror("opendir /proc");
    }

    std::string our_name;
    char self[PATH_MAX];
    ssize_t len = readlink("/proc/self/exe", self, PATH_MAX - 1);
    if (len == -1) {
        printf("Failed to read /proc/self/exe? Using `felix86` as search name");
        our_name = "felix86";
    } else {
        self[len] = 0;
        our_name = basename(self);
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
        ssize_t len = readlink(exe_path, exe_target, PATH_MAX - 1);
        if (len == -1)
            continue;

        exe_target[len] = '\0';

        std::string path = exe_target;
        if (path.find(' ') != std::string::npos) {
            // Sometimes paths come up as "/path/to/felix86 (deleted)"
            // This happens when the executable... was deleted
            // Helpful when I eg. recompile but also wanna kill old running instances
            // ie. wine leftovers and stuff like that
            path = path.substr(0, path.find(' '));
        }

        char* base = basename(path.data());

        if (strcmp(base, our_name.c_str()) == 0) {
            if (kill(pid, SIGKILL) == 0) {
                printf("Killed process %d\n", pid);
            } else {
                printf("Failed to kill process %d", pid);
            }
        }
    }

    closedir(proc_dir);
}

static error_t parse_opt(int key, char* arg, struct argp_state* state) {
    if (key == ARGP_KEY_ARG) {
        if (g_params.argv.empty()) {
            g_params.executable_path = arg;
            unmodified_executable_path = arg;
        }

        g_params.argv.push_back(arg);
        guest_arg_start_index = state->next;
        state->next = state->argc; // tell argp to stop
        return 0;
    }

    bool shell_logging = false;

    switch (key) {
    case -2: {
        shell_logging = true;
        [[fallthrough]];
    }
    case -1: {
        std::error_code ec;
        Config::initialize();
        if (!g_config.no_rootfs) {
            if (g_config.rootfs_path.empty()) {
                rootfs_not_set_error();
            }
        } else {
            g_config.rootfs_path = "/";
        }

        bool rootfs_exists = std::filesystem::exists(g_config.rootfs_path, ec);
        if (!rootfs_exists || ec) {
            printf("Rootfs path %s does not exist\n", g_config.rootfs_path.c_str());
            exit(1);
        }

        bool rootfs_dir = std::filesystem::is_directory(g_config.rootfs_path, ec);
        if (!rootfs_dir || ec) {
            printf("Rootfs path %s is not a directory\n", g_config.rootfs_path.c_str());
            exit(1);
        }

        std::filesystem::path shell_path;
        const char* shell = secure_getenv("SHELL");
        if (shell) {
            std::filesystem::path path = g_config.rootfs_path / std::filesystem::path(shell).relative_path();
            bool exists = std::filesystem::exists(path, ec);
            bool is_file = std::filesystem::is_regular_file(path, ec);
            if (exists && is_file && !ec) {
                shell_path = path;
            }
        }

        if (shell_path.empty()) {
            shell_path = g_config.rootfs_path / "bin/bash";
            setenv("SHELL", "/bin/bash", 1);
        }

        if (!std::filesystem::exists(shell_path, ec) || !std::filesystem::is_regular_file(shell_path, ec)) {
            printf("Couldn't find a shell inside the rootfs\n");
            exit(1);
        }

        struct passwd* pw = getpwuid(geteuid());
        if (!pw || !pw->pw_dir) {
            printf("Could not determine home directory\n");
            exit(1);
        }

        const std::filesystem::path home = pw->pw_dir;
        const std::filesystem::path home_inside_rootfs = g_config.rootfs_path / home.relative_path();
        std::filesystem::create_directories(home_inside_rootfs, ec);
        if (ec) {
            // The home directory inside the rootfs doesn't exist, and we couldn't create it
            // chdir to root instead
            int result = chdir(g_config.rootfs_path.c_str());
            if (result != 0) {
                printf("Failed to chdir to %s\n", g_config.rootfs_path.c_str());
                exit(1);
            }
        } else {
            if (g_config.mount_home) {
                int result = chdir(home.c_str());
                if (result != 0) {
                    printf("Failed to chdir to %s\n", home_inside_rootfs.c_str());
                    exit(1);
                }
            } else {
                int result = chdir(home_inside_rootfs.c_str());
                if (result != 0) {
                    printf("Failed to chdir to %s\n", home_inside_rootfs.c_str());
                    exit(1);
                }
            }
        }

        std::string path_string = shell_path;
        std::string self = "/proc/self/exe";
        std::string ps1;
        std::string norc;
        if (shell_path.filename() == "zsh") {
            ps1 = "PS1=%F{215}felix86%f %F{153}%~%f > ";
            norc = "-f";
        } else if (shell_path.filename() == "bash") {
            ps1 = "PS1=\\[\\033[38;5;215m\\]felix86 \\[\\033[38;5;153m\\]\\w\\[\\033[0m\\] > ";
            norc = "--norc";
        } else {
            // We don't know the escape codes used...
            ps1 = "PS1=felix86 > ";
        }

        std::string c = "-c";
        std::vector<char*> argv;
        argv.push_back(self.data());
        argv.push_back(path_string.data());
        if (!norc.empty()) {
            argv.push_back(norc.data());
        }
        if (arg && arg[0] != 0) {
            argv.push_back(c.data());
            argv.push_back(arg);
        }
        argv.push_back(nullptr);

        std::string shell_history = "HISTFILE=";
        std::string quiet = "FELIX86_QUIET=1";
        std::vector<char*> envp;
        char** envs = environ;
        do {
            std::string str = *envs;
            if (str.find("PS1=") == 0) {
                envs++;
                continue;
            }
            envp.push_back(*envs++);
        } while (*envs);
        envp.push_back(ps1.data());
        if (!shell_logging) {
            envp.push_back(quiet.data());
        }
        if (!g_config.shell_history_path.empty()) {
            std::filesystem::path shell_history_file;
            if (!g_config.shell_history_path.is_absolute()) {
                shell_history_file += home;
                shell_history_file += "/";
            }
            shell_history_file += g_config.shell_history_path.c_str();
            shell_history += shell_history_file.string();
            envp.push_back(shell_history.data());
        }
        envp.push_back(nullptr);

        (void)execve(self.c_str(), argv.data(), envp.data());
        printf("Failed to start %s, error: %s\n", path_string.c_str(), strerror(errno));
        exit(1);
        break;
    }
    case -3: {
        // get-config
        ASSERT(Config::initialize(true));
        std::string group{};
        std::string field{};

        char c = 0;
        while (arg != NULL && (c = *arg++) && c != '.') {
            group.push_back(c);
        }
        if (c != '.' || group.empty()) {
            ERROR("expected argument to get-config to be in the format of '<GROUP.CONFIG>'");
        }

        while (arg != NULL && (c = *arg++)) {
            field.push_back(c);
        }
        if (field.empty()) {
            ERROR("expected argument to get-config to be in the format of '<GROUP.CONFIG>'");
        }

        std::optional<std::string> get = g_config.getConfigString(group.c_str(), field.c_str());
        if (get.has_value()) {
            printf("%s\n", get.value().c_str());
            exit(0);
        } else {
            ERROR("%s.%s is not a valid config tuple\n", group.c_str(), field.c_str());
        }

        break;
    }
    case -4: {
        // set-config
        if (geteuid() != 0) {
            printf("Setting config requires root permissions. Please re-run with sudo.\n");
            exit(1);
        }
        ASSERT(Config::initialize(true));
        std::string group{};
        std::string field{};
        std::string value{};

        char c = 0;
        while (arg != NULL && (c = *arg++) && c != '.') {
            group.push_back(c);
        }
        if (c != '.' || group.empty()) {
            ERROR("expected argument to set-config to be in the format of '<GROUP.CONFIG>=<VALUE>'");
        }

        while (arg != NULL && (c = *arg++) && c != '=') {
            field.push_back(c);
        }
        if (c != '=' || field.empty()) {
            ERROR("expected argument to set-config to be in the format of '<GROUP.CONFIG>=<VALUE>'");
        }

        while (arg != NULL && (c = *arg++)) {
            value.push_back(c);
        }
        // value can be an empty string

        if (g_config.setConfigString(group.c_str(), field.c_str(), value.c_str())) {
            Config::save(g_config.path(), g_config);
            exit(0);
        } else {
            ERROR("%s.%s is not a valid config tuple, or %s is not a valid value for the configuration\n", group.c_str(), field.c_str(),
                  value.c_str());
        }

        break;
    }
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
        binfmt_misc(true, false);
        exit(0);
        break;
    }
    case 'B': {
        binfmt_misc(true, true);
        printf("Credentials flag enabled. Setuid x86 binaries will run with elevated privileges.\n");
        exit(0);
        break;
    }
    case 'd': {
        Config::initialize();
        bool ok = detect_binfmt_misc();
        exit(!ok);
        break;
    }
    case 'u': {
        binfmt_misc(false, false);
        exit(0);
        break;
    }
    case 'l': {
        Logger::startServer();
        exit(0);
        break;
    }
#ifdef BUILD_REPL
    case 'r': {
        enter_repl();
        __builtin_unreachable();
        break;
    }
#endif
    case 'c': {
        // TODO: add some color here
        Config::initialize();

        std::string current_group;
        printf("These are the configurations for felix86\n");
        printf("You may edit %s or set the corresponding environment variable\n", g_config.path().c_str());

#define X(group, type, name, def, env, description)                                                                                                  \
    if (current_group != #group) {                                                                                                                   \
        current_group = #group;                                                                                                                      \
        printf("\n[%s]\n", current_group.c_str());                                                                                                   \
    }                                                                                                                                                \
    fmt::print("{} {} = {}, source: {}\n", #type, #name, g_config.name, g_config.getConfigSourceString(#group, #name).value_or(""));
#include "felix86/common/config.inc"
#undef X
        exit(0);
        break;
    }
    case ARGP_KEY_END: {
        if (g_params.argv.empty()) {
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
    if (secure_getenv("__FELIX86_TEST_BINFMT_MISC")) {
        // This shouldn't be printed as when we run /bin/env in detect_binfmt_misc we mute stdout and stderr
        WARN("__FELIX86_TEST_BINFMT_MISC was detected, if you see this then something is wrong");

        // Magic value expected by detect_binfmt_misc
        return 0x42;
    }

    std::set_terminate([]() { ERROR("std::terminate was called"); });

#ifdef __x86_64__
    WARN("You're running an x86-64 executable version of felix86, get ready for a crash soon");
#endif
    argp_parse(&argp, argc, argv, ARGP_IN_ORDER, 0, &g_params);
    if (guest_arg_start_index != -1) {
        char** argv_next = &argv[guest_arg_start_index];
        while (*argv_next) {
            g_params.argv.push_back(*argv_next);
            argv_next++;
        }
    }

    g_execve_process = !!getenv("__FELIX86_EXECVE");

    if (g_execve_process) {
        const char* guest_envs = getenv("__FELIX86_GUEST_ENVS");
        if (guest_envs) {
            std::vector<std::string> envs = split_string(guest_envs, ',');
            for (auto& env : envs) {
                g_params.envp.push_back(hex_to_string(env));
            }
        }
    } else {
        bool purposefully_empty = false;
        const char* env_file = secure_getenv("FELIX86_ENV_FILE");
        if (env_file) {
            std::string env_path = env_file;
            if (std::filesystem::exists(env_path)) {
                std::ifstream env_stream(env_path);
                std::string line;
                while (std::getline(env_stream, line)) {
                    g_params.envp.push_back(line);
                }

                if (g_params.envp.empty()) {
                    purposefully_empty = true;
                }
            } else {
                WARN("Environment variable file %s does not exist. Using host environment variables.", env_file);
            }
        }

        if (g_params.envp.empty() && !purposefully_empty) {
            char** envp = environ;
            while (*envp) {
                g_params.envp.push_back(*envp);
                envp++;
            }
        }

        if (!g_config.environment.empty()) {
            std::vector<std::string> envs = split_string(g_config.environment, ';');
            for (const auto& env : envs) {
                if (!env.empty()) {
                    auto pos = env.find("=");
                    if (pos == std::string::npos) {
                        WARN("Environment variable %s in FELIX86_ENVIRONMENT has no '=' character", env.c_str());
                    } else {
                        g_params.envp.push_back(env);
                    }
                }
            }
        }

        if (!g_config.host_environment.empty()) {
            std::vector<std::string> envs = split_string(g_config.host_environment, ';');
            for (const auto& env : envs) {
                if (!env.empty()) {
                    auto pos = env.find("=");
                    if (pos == std::string::npos) {
                        WARN("Environment variable %s in FELIX86_HOST_ENVIRONMENT has no '=' character", env.c_str());
                    } else {
                        std::string name = env.substr(0, pos);
                        std::string value = env.substr(pos + 1);
                        int result = setenv(name.c_str(), value.c_str(), true);
                        if (result != 0) {
                            WARN("Failed to set %s from FELIX86_HOST_ENVIRONMENT", name.c_str());
                        }
                    }
                }
            }
        }
    }

    Config::initialize();

    if (getenv("__FELIX86_QUIET")) {
        g_config.quiet = true;
    }

    const char* pipe = secure_getenv("__FELIX86_PIPE"); // don't inherit pipe from different uid
    if (!pipe || !*pipe) {
        Logger::openTerminal();
    } else {
        Logger::joinServer();
    }
    Logger::openLogFile();

#ifdef FELIX86_VALIDATE_BINFMT_MISC
    bool is_secure = getauxval(AT_SECURE) != 0;
    if (g_config.binfmt_misc_installed && !g_execve_process && !is_secure) {
        validate_binfmt_misc();
    }
#endif

    if (!g_config.no_rootfs && g_config.rootfs_path.empty()) {
        rootfs_not_set_error();
    }
    initialize_globals();
    Signals::initialize();

    const char* argv0_original = getenv("__FELIX86_ARGV0");
    if (argv0_original) {
        g_params.argv[0] = argv0_original;
    } else {
        ASSERT(!g_execve_process);
        if (g_params.argv[0].find(g_config.rootfs_path) == 0) {
            replace_all(g_params.argv[0], g_config.rootfs_path, "");
        }
    }

    std::string args = "Arguments: ";
    for (const auto& arg : g_params.argv) {
        args += arg;
        args += " ";
    }
    VERBOSE("%s", args.c_str());

    // TODO: These "hacky" environment variables are bandaid solutions to problems that we need to eventually fix
    // They are enabled by default
    if (g_config.hacky_envs) {
        // DOTNET tries to allocate too much heap memory, and many RISC-V boards currently come with 39-bit address space
        // To counteract this by default, we'll limit the heap memory dotnet allocates
        g_params.envp.push_back("DOTNET_GCHeapHardLimit=1C0000000");
        // Some DOTNET games will use W^X mappings which will break our current SMC detection, disable it for now
        g_params.envp.push_back("DOTNET_EnableWriteXorExecute=0");
    }

    auto it = g_params.envp.begin();
    while (it != g_params.envp.end()) {
        std::string env = *it;

        // Dont pass these to the executable itself
        if (env.find("FELIX86_") != std::string::npos) {
            it = g_params.envp.erase(it);
        } else {
            it++;
        }
    }

    if (g_params.executable_path.empty()) {
        ERROR("Executable path not specified");
        return 1;
    } else if (!g_config.no_rootfs) {
        g_params.executable_path = std::filesystem::absolute(g_params.executable_path);
        if (is_subpath(g_params.executable_path, g_config.rootfs_path)) {
            // All is good
        } else {
            // Executable path might be outside the rootfs but in a fakemount (e.g. in /tmp or in /home)
            std::error_code ec;
            bool found = false;
            std::filesystem::path canonical_path = std::filesystem::canonical(unmodified_executable_path, ec);
            if (ec) {
                ERROR("Executable not inside rootfs, couldn't canonicalize path");
            }

            for (const auto& fake_mount : g_fake_mounts) {
                if (is_subpath(canonical_path, fake_mount.src_path)) {
                    std::filesystem::path cutoff_path = canonical_path.string().substr(fake_mount.src_path.string().size());
                    std::filesystem::path executable = fake_mount.dst_path / cutoff_path.relative_path();
                    if (is_subpath(executable, g_config.rootfs_path)) {
                        executable = executable.string().substr(g_config.rootfs_path.string().size());
                    }
                    // Point /proc/self/exe & co to the fake mount guest path
                    g_executable_path_guest_override = executable;
                    g_params.argv[0] = g_executable_path_guest_override;
                    g_params.executable_path = canonical_path;
                    found = true;
                    break;
                }
            }

            if (!found) {
                ERROR("Executable not inside rootfs");
            }
        }
    }

#if 0
    // Use me if you want to strace a specific program only
    if (g_executable_path_absolute.string().find("python3") != std::string::npos) {
        g_config.strace = 1;
    }
#endif

    if (!g_config.binfmt_misc_installed && !g_execve_process && check_if_privileged_executable(g_params.executable_path)) {
        // Privileged executable but no binfmt_misc support, warn the user
        WARN("This is a privileged executable but the emulator isn't installed in binfmt_misc, might run into problems. Run `felix86 -b` to "
             "install "
             "it, make sure to remove other x86/x86-64 emulators from binfmt_misc");
    }

    SIGLOG("New felix86 instance with PID %d and executable path %s", getpid(), g_params.executable_path.c_str());

    Emulator::Start();
    UNREACHABLE();
}
