#include <csetjmp>
#include <fstream>
#include <thread>
#include <argp.h>
#include <fmt/format.h>
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
    {"verbose", 'V', 0, 0, "Produce verbose output"},
    {"quiet", 'q', 0, 0, "Don't produce any output"},
    {"rootfs-path", 'p', "PATH", 0, "Path to the rootfs directory"},
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
    case 'p': {
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
    g_output_fd = dup(STDOUT_FILENO);

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
    }

    pthread_setname_np(pthread_self(), "MainThread");

    unlink_semaphore(); // in case it was not closed properly last time
    initialize_semaphore();

    Emulator emulator(config);

    if (argc == 1) {
        ERROR("Unimplemented");
    } else {
        emulator.Run();
    }

    unlink_semaphore();

    felix86_exit(0);
}