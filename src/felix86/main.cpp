#include <argp.h>
#include "biscuit/cpuinfo.hpp"
#include "felix86/common/log.hpp"
#include "felix86/common/version.hpp"
#include "felix86/emulator.hpp"
#include "felix86/hle/filesystem.hpp"

#if !defined(__riscv)
#pragma message("felix86 should only be compiled for RISC-V")
#endif

const char* argp_program_version = "felix86 " FELIX86_VERSION;
const char* argp_program_bug_address = "<https://github.com/OFFTKP/felix86/issues>";

static char doc[] = "felix86 - a userspace x86_64 emulator";
static char args_doc[] = "TARGET_BINARY [TARGET_ARGS...]";

bool extensions_manually_specified = false;

static struct argp_option options[] = {
    {"verbose", 'v', 0, 0, "Produce verbose output"},
    {"quiet", 'q', 0, 0, "Don't produce any output"},
    {"print-state", 's', 0, 0, "Print state at the end of each block"},
    {"host-envs", 'E', 0, 0, "Pass host environment variables to the guest"},
    {"print-functions", 'P', 0, 0, "Print functions as they compile"},
    {"rootfs-path", 'p', "PATH", 0, "Path to the rootfs directory"},
    {"dont-optimize", 'x', 0, 0, "Don't apply optimizations on the IR"},
    {"print-disassembly", 'd', 0, 0, "Print disassembly of emitted functions"},
    {"strace", 't', 0, 0, "Trace emulated application syscalls"},
    {"extensions", 'e', "EXTENSIONS", 0, "Manually specify available RISC-V extensions as a comma separated list. Eg: -e g,c,v"},
    {0}};

static bool parse_extensions(const char* arg) {
    while (arg) {
        const char* next = strchr(arg, ',');
        std::string extension;
        if (next) {
            extension = std::string(arg, next - arg);
            arg = next + 1;
        } else {
            extension = arg;
            arg = nullptr;
        }

        if (extension.empty()) {
            continue;
        }

        std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);

        if (extension == "g") {
            Extensions::G = true;
        } else if (extension == "v") {
            Extensions::V = true;
            Extensions::VLEN = 128;
            WARN("VLEN defaulting to 128");
        } else if (extension == "c") {
            Extensions::C = true;
        } else if (extension == "b") {
            Extensions::B = true;
        } else if (extension == "zacas") {
            Extensions::Zacas = true;
        } else if (extension == "zam") {
            Extensions::Zam = true;
        } else if (extension == "zabha") {
            Extensions::Zabha = true;
        } else if (extension == "zicond") {
            Extensions::Zicond = true;
        } else {
            ERROR("Unknown extension: %s", extension.c_str());
            return false;
        }
    }

    if (!Extensions::G) {
        WARN("G extension was not specified, enabling it by default");
        Extensions::G = true;
    }

    return true;
}

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

    LOG("Extensions enabled for the recompiler: %s", extensions.c_str());
}

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
    case 'E': {
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
    case 't': {
        g_strace = true;
        break;
    }
    case 'e': {
        if (!parse_extensions(arg)) {
            argp_usage(state);
        } else {
            extensions_manually_specified = true;
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

    argp_parse(&argp, argc, argv, 0, 0, &config);

    LOG("felix86 version %s", FELIX86_VERSION);

    // Check for FELIX86_EXTENSIONS environment variable
    const char* extensions_env = getenv("FELIX86_EXTENSIONS");
    if (extensions_env) {
        if (extensions_manually_specified) {
            WARN("FELIX86_EXTENSIONS environment variable overrides manually specified extensions");
            Extensions::Clear();
        }

        if (!parse_extensions(extensions_env)) {
            WARN("Failed to parse environment variable FELIX86_EXTENSIONS");
        } else {
            extensions_manually_specified = true;
        }
    }

    if (!extensions_manually_specified) {
        CPUInfo cpuinfo;
        Extensions::G = cpuinfo.Has(RISCVExtension::I) && cpuinfo.Has(RISCVExtension::A) && cpuinfo.Has(RISCVExtension::F) &&
                        cpuinfo.Has(RISCVExtension::D) && cpuinfo.Has(RISCVExtension::M);
        Extensions::V = cpuinfo.Has(RISCVExtension::V);
        Extensions::C = cpuinfo.Has(RISCVExtension::C);
        Extensions::VLEN = cpuinfo.GetVlenb() * 8;
    }

    print_extensions();

    if (!Extensions::G || !Extensions::V || Extensions::VLEN < 128) {
        WARN("Backend is missing some extensions or VLEN < 128 (ours: %d)", Extensions::VLEN);
        WARN("Illegal instructions may cause crashes");
    }

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

    Emulator emulator(config);

    if (argc == 1) {
        ERROR("Unimplemented");
    } else {
        emulator.Run();
    }

    felix86_exit(0);
}