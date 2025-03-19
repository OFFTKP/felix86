#include <csetjmp>
#include <fstream>
#include <argp.h>
#include <fcntl.h>
#include <fmt/format.h>
#include <grp.h>
// #include <sys/capability.h>
#include <spawn.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include "biscuit/cpuinfo.hpp"
#include "felix86/common/info.hpp"
#include "felix86/common/log.hpp"
#include "felix86/common/script.hpp"
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
    {"verbose", 'V', 0, 0, "Produce verbose output"},
    {"quiet", 'q', 0, 0, "Don't produce any output"},
    {"strace", 't', 0, 0, "Trace emulated application syscalls"},
    {"all-extensions", 'X', "EXTS", 0,
     "Manually specify every available RISC-V extension. When using this, any extension not specified will be considered unavailable. "
     "Usage example: -X g,c,v,b,zacas"},

    {0}};

int guest_arg_start_index = -1;

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
        g_verbose = true;
        break;
    }
    case 'q': {
        g_quiet = true;
        break;
    }
    case 'i': {
        exit(print_system_info());
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
#ifdef __x86_64__
    WARN("You're running an x86-64 executable version of felix86, get ready for a crash soon");
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

    g_execve_process = !!getenv("__FELIX86_EXECVE");
    if (!g_execve_process) {
        Logger::startServer();
    } else {
        // Open the existing write pipe of the emulator instance, passed to this execve process
        // via the __FELIX86_PIPE environment variable
        const char* file = getenv("__FELIX86_PIPE");
        ASSERT(file);
        g_output_fd = open(file, O_WRONLY, 0644);
        if (g_output_fd == -1) {
            printf("Bad g_output_fd -- errno: %d\n", errno);
            exit(42);
        }
    }

    initialize_globals();
    initialize_extensions();

    if (!g_execve_process) {
        ASSERT(!g_rootfs_path.empty());

        // First time running the emulator (ie. the emulator is not running itself with execve) we need to link some stuff
        // and copy some stuff inside the rootfs
        auto copy = [](const char* src, const std::filesystem::path& dst) {
            if (!std::filesystem::exists(src)) {
                printf("I couldn't find %s to copy to the rootfs, may cause problems with some games", src);
                return;
            }

            using co = std::filesystem::copy_options;

            std::error_code ec;
            std::filesystem::copy(src, dst, co::overwrite_existing | co::recursive, ec);
            if (ec) {
                WARN("Error while copying %s: %s", src, ec.message().c_str());
            }
        };

        std::filesystem::create_directories(g_rootfs_path / "var" / "lib");
        std::filesystem::create_directories(g_rootfs_path / "etc");

        // Copy some stuff to the g_rootfs_path
        copy("/var/lib/dbus", g_rootfs_path / "var" / "lib" / "dbus");
        copy("/etc/mtab", g_rootfs_path / "etc" / "mtab");
        copy("/etc/passwd", g_rootfs_path / "etc" / "passwd");
        copy("/etc/passwd-", g_rootfs_path / "etc" / "passwd");
        copy("/etc/hosts", g_rootfs_path / "etc" / "hosts");
        copy("/etc/hostname", g_rootfs_path / "etc" / "hostname");
        copy("/etc/resolv.conf", g_rootfs_path / "etc" / "resolv.conf");

        // Symlink some directories to make our lives easier and not have to overlay them
        ASSERT_MSG(Symlinker::link("/run", g_rootfs_path / "run"), "Failed to symlink /run: %s", strerror(errno));
        ASSERT_MSG(Symlinker::link("/proc", g_rootfs_path / "proc"), "Failed to symlink /proc: %s", strerror(errno));
        ASSERT_MSG(Symlinker::link("/sys", g_rootfs_path / "sys"), "Failed to symlink /sys: %s", strerror(errno));
        ASSERT_MSG(Symlinker::link("/dev", g_rootfs_path / "dev"), "Failed to symlink /dev: %s", strerror(errno));
        mkdirat(g_rootfs_fd, "tmp", 0777);
    }

    if (Extensions::VLEN != 256) {
        WARN_ONCE("felix86 is untested on chips with VLEN != 256, problems are expected to happen :(");
    }
    Signals::initialize();

    if (g_thunking) {
        Thunks::initialize();
    }

    if (is_subpath(config.argv[0], g_rootfs_path)) {
        config.argv[0] = config.argv[0].substr(g_rootfs_path.string().size());
        ASSERT(!config.argv[0].empty());
        if (config.argv[0].at(0) != '/') {
            config.argv[0] = '/' + config.argv[0];
        }
    }

    std::string args = "Arguments: ";
    for (const auto& arg : config.argv) {
        args += arg;
        args += " ";
    }
    VERBOSE("%s", args.c_str());

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

    auto it = config.envp.begin();
    while (it != config.envp.end()) {
        std::string env = *it;

        // Dont pass these to the executable itself
        if (env.find("FELIX86_") != std::string::npos) {
            it = config.envp.erase(it);
        } else {
            it++;
        }
    }

    // Resolve symlinks, get absolute path. If the symlink is resolved, it may not start with
    // the rootfs prefix, and we need to add it back
    const std::string rootfs_string = g_rootfs_path.string();
    std::filesystem::path resolved = Symlinker::resolve(config.executable_path);
    if (resolved.string().find(rootfs_string) != 0) {
        resolved = g_rootfs_path / resolved.relative_path();
    }
    config.executable_path = resolved;

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
    }

    if (g_execve_process) {
        pthread_setname_np(pthread_self(), "ExecveProcess");
    } else {
        pthread_setname_np(pthread_self(), "MainProcess");
    }

    auto [exit_reason, exit_code] = Emulator::Start(config);

    if (!g_execve_process) {
        LOG("Main process exited with reason: %s. Exit code: %d", print_exit_reason(exit_reason), exit_code);
    } else {
        LOG("Execve process exited with reason: %s. Exit code: %d", print_exit_reason(exit_reason), exit_code);
    }

    return exit_code;
}
