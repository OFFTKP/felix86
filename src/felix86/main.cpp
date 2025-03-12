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
#include "felix86/common/info.hpp"
#include "felix86/common/log.hpp"
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
    {"verbose", 'V', 0, 0, "Produce verbose output"},
    {"quiet", 'q', 0, 0, "Don't produce any output"},
    {"strace", 't', 0, 0, "Trace emulated application syscalls"},
    {"all-extensions", 'X', "EXTS", 0,
     "Manually specify every available RISC-V extension. When using this, any extension not specified will be considered unavailable. "
     "Usage example: -X g,c,v,b,zacas"},

    {0}};

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
    if (!getenv("__FELIX86_LAUNCHED")) {
        ERROR("felix86_jit should be launched from the launcher, not directly -- If you want to run it regardless set the __FELIX86_LAUNCHED "
              "environment variable");
    }

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
    g_output_fd = STDOUT_FILENO;

    initialize_globals();
    initialize_extensions();
    std::string extensions = get_extensions();
    if (!extensions.empty()) {
        LOG("Extensions enabled for the recompiler: %s", extensions.c_str());
    }
    if (Extensions::VLEN != 256) {
        WARN_ONCE("felix86 is untested on chips with VLEN != 256, problems are expected to happen :(");
    }
    Signals::initialize();

    static bool initialized = false;
    if (g_thunking && !initialized) {
        initialized = true;
        Thunks::initialize();
    }

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
        } else if (env.find("LD_LIBRARY_PATH") != std::string::npos) {
            it = config.envp.erase(it);
        } else {
            it++;
        }
    }

    if (!g_rootfs_path.empty()) {
        // Remove rootfs from executable path, if the user prepended it
        if (config.executable_path.string().find(g_rootfs_path.string()) == 0) {
            std::string new_path = config.executable_path.string().substr(g_rootfs_path.string().size());
            ASSERT(new_path.size() > 0);
            ASSERT(new_path[0] == '/');
            config.executable_path = new_path;
            config.argv[0] = config.executable_path;
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
    }

    g_execve_process = !!getenv("__FELIX86_EXECVE");

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
