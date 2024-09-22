#include <argp.h>
#include <stdio.h>
#include "felix86/common/log.hpp"
#include "felix86/common/prompt.hpp"
#include "felix86/common/version.hpp"
#include "felix86/gui.hpp"
#include "felix86/hle/filesystem.hpp"
#include "felix86/loader/loader.hpp"

const char* argp_program_version = "felix86 " FELIX86_VERSION;
const char* argp_program_bug_address = "<https://github.com/OFFTKP/felix86/issues>";

static char doc[] = "felix86 - a userspace x86_64 emulator";
static char args_doc[] = "BINARY [ARGS...]";

static struct argp_option options[] = {
    {"verbose", 'v', 0, 0, "Produce verbose output"},
    {"quiet", 'q', 0, 0, "Don't produce any output"},
    {"interpreter", 'i', 0, 0, "Run in interpreter mode"},
    {"host-envs", 'e', 0, 0, "Pass host environment variables to the guest"},
    {"print-functions", 'P', 0, 0, "Print functions as they compile"},
    {"dont-optimize", 'O', 0, 0, "Don't run IR optimizations"},
    {"squashfs-path", 'p', "PATH", 0, "Path to the rootfs squashfs image"},
    {0}};

static error_t parse_opt(int key, char* arg, struct argp_state* state) {
    loader_config_t* config = state->input;

    if (key == ARGP_KEY_ARG) {
        // This is one of the guest executable arguments
        if (config->argc == 255) {
            printf("Too many guest arguments\n");
            argp_usage(state);
        }

        config->argv[config->argc++] = arg;
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
        config->squashfs_path = arg;
        break;
    }
    case 'e': {
        config->use_host_envs = true;
        break;
    }
    case 'P': {
        config->print_blocks = true;
        break;
    }
    case 'i': {
        config->use_interpreter = true;
        break;
    }
    case 'O': {
        config->dont_optimize = true;
        break;
    }
    case ARGP_KEY_END: {
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
    loader_config_t config = {0};
    config.use_interpreter = false;

    argp_parse(&argp, argc, argv, 0, 0, &config);

    if (config.squashfs_path == NULL) {
        ERROR("No squashfs image path provided");
        return 1;
    }

    felix86_fs_init(config.squashfs_path, config.argv[0]);

    if (argc == 1) {
        felix86_gui();
    } else {
        loader_run_elf(&config);
    }

    felix86_exit(0);
}