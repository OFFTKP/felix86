#include <argp.h>
#include <stdio.h>
#include "felix86/common/log.hpp"
#include "felix86/common/version.hpp"
#include "felix86/emulator.hpp"
#include "felix86/hle/filesystem.hpp"

const char* argp_program_version = "felix86 " FELIX86_VERSION;
const char* argp_program_bug_address = "<https://github.com/OFFTKP/felix86/issues>";

static char doc[] = "felix86 - a userspace x86_64 emulator";
static char args_doc[] = "BINARY [ARGS...]";

static struct argp_option options[] = {{"verbose", 'v', 0, 0, "Produce verbose output"},
                                       {"quiet", 'q', 0, 0, "Don't produce any output"},
                                       {"print-state", 's', 0, 0, "Print state at the end of each block"},
                                       {"host-envs", 'e', 0, 0, "Pass host environment variables to the guest"},
                                       {"print-functions", 'P', 0, 0, "Print functions as they compile"},
                                       {"rootfs-path", 'p', "PATH", 0, "Path to the rootfs directory"},
                                       {"dont-optimize", 'x', 0, 0, "Don't apply optimizations on the IR"},
                                       {"print-disassembly", 'd', 0, 0, "Print disassembly of emitted functions"},
                                       {0}};

static error_t parse_opt(int key, char* arg, struct argp_state* state) {
    Config* config = (Config*)state->input;

    if (key == ARGP_KEY_ARG) {
        if (config->argv.empty()) {
            config->executable_path = arg;
        }

        config->argv.push_back(arg);
        return 0;
    }

    switch (key) {
    case 'v': {
        enable_verbose();
        break;
    }
    case 'q': {
        disable_logging();
        break;
    }
    case 'p': {
        config->rootfs_path = arg;
        break;
    }
    case 'x': {
        config->optimize = false;
        break;
    }
    case 'e': {
        char** envp = environ;
        while (*envp) {
            config->envp.push_back(*envp);
            envp++;
        }
        break;
    }
    case 'P': {
        config->print_blocks = true;
        break;
    }
    case 's': {
        config->print_state = true;
        break;
    }
    case 'd': {
        config->print_disassembly = true;
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

    argp_parse(&argp, argc, argv, 0, 0, &config);

    if (config.rootfs_path.empty()) {
        ERROR("Rootfs path not specified");
        return 1;
    }

    if (config.executable_path.empty()) {
        ERROR("Executable path not specified");
        return 1;
    }

    LOG("felix86 version %s", FELIX86_VERSION);

    Emulator emulator(config);

    if (argc == 1) {
        ERROR("Unimplemented");
    } else {
        emulator.Run();
    }

    felix86_exit(0);
}