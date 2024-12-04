#include <csetjmp>
#include <thread>
#include <argp.h>
#include <fmt/format.h>
#include "felix86/common/disk_cache.hpp"
#include "felix86/common/log.hpp"
#include "felix86/common/version.hpp"
#include "felix86/emulator.hpp"
#include "felix86/hle/filesystem.hpp"

#if !defined(__riscv)
#pragma message("felix86 should only be compiled for RISC-V")
#endif

std::string version_full = get_version_full();
const char* argp_program_version = version_full.c_str();
const char* argp_program_bug_address = "<https://github.com/OFFTKP/felix86/issues>";

static char doc[] = "felix86 - a userspace x86_64 emulator";
static char args_doc[] = "TARGET_BINARY [TARGET_ARGS...]";

static struct argp_option options[] = {
    {"aot", 'a', 0, 0, "Ahead-of-time compile the target binary"},
    {"verbose", 'V', 0, 0, "Produce verbose output"},
    {"quiet", 'q', 0, 0, "Don't produce any output"},
    {"print-state", 's', 0, 0, "Print state at the end of each block"},
    {"host-envs", 'E', 0, 0, "Pass host environment variables to the guest"},
    {"print-functions", 'P', 0, 0, "Print functions as they compile"},
    {"rootfs-path", 'p', "PATH", 0, "Path to the rootfs directory"},
    {"dont-optimize", 'o', 0, 0, "Don't apply optimizations on the IR"},
    {"print-disassembly", 'd', 0, 0, "Print disassembly of emitted functions"},
    {"strace", 't', 0, 0, "Trace emulated application syscalls"},
    {"clear-cache", 'c', 0, 0, "Clear the compiled function cache"},
    {"extensions", 'x', "EXTS", 0,
     "Manually specify additional available RISC-V extensions, in addition to the ones detected. Useful because some extensions might not be "
     "detectable. Usage example: -e zacas,xtheadcondmov"},
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
    case 'a': {
        g_aot = true;
        break;
    }
    case 'V': {
        enable_verbose();
        break;
    }
    case 'q': {
        disable_logging();
        break;
    }
    case 'p': {
        g_rootfs_path = arg;
        break;
    }
    case 'o': {
        g_dont_optimize = true;
        break;
    }
    case 'E': {
        char** envp = environ;
        while (*envp) {
            config->envp.push_back(*envp);
            envp++;
        }
        break;
    }
    case 'c': {
        DiskCache::Clear();
        LOG("Function cache cleared!");
        break;
    }
    case 'P': {
        g_print_blocks = true;
        break;
    }
    case 's': {
        g_print_state = true;
        break;
    }
    case 'd': {
        g_print_disassembly = true;
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
    case 'x': {
        if (!parse_extensions(arg)) {
            argp_usage(state);
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

int main(int argc, char* argv[]) {
    Config config = {};

    argp_parse(&argp, argc, argv, ARGP_IN_ORDER, 0, &config);
    if (guest_arg_start_index != -1) {
        char** argv_next = &argv[guest_arg_start_index];
        while (*argv_next) {
            config.argv.push_back(*argv_next);
            argv_next++;
        }
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

    initialize_globals();
    initialize_extensions();
    print_extensions();

    g_output_fd = dup(STDOUT_FILENO);
    config.rootfs_path = g_rootfs_path;

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
    }

    std::thread main_thread([argc, &config]() {
        pthread_setname_np(pthread_self(), "MainThread");

        Emulator emulator(config);

        if (argc == 1) {
            ERROR("Unimplemented");
        } else {
            emulator.Run();
        }
    });
    main_thread.join();

    felix86_exit(0);
}