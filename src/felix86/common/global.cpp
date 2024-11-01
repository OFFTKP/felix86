#include <algorithm>
#include <cstring>
#include "felix86/common/global.hpp"
#include "felix86/common/log.hpp"

bool g_verbose = false;
bool g_quiet = false;
bool g_testing = false;
bool g_strace = false;
bool g_dont_optimize = false;
bool g_print_blocks = false;
bool g_print_state = false;
bool g_print_disassembly = false;
bool g_extensions_manually_specified = false;
u32 g_spilled_count = 0;

bool Extensions::G = false;
bool Extensions::C = false;
bool Extensions::B = false;
bool Extensions::V = false;
bool Extensions::Zacas = false;
bool Extensions::Zam = false;
bool Extensions::Zabha = false;
bool Extensions::Zicond = false;
int Extensions::VLEN = 0;

void Extensions::Clear() {
    G = false;
    C = false;
    B = false;
    V = false;
    Zacas = false;
    Zam = false;
    Zabha = false;
    Zicond = false;
    VLEN = 0;
}

void initialize_globals() {
    // Check for FELIX86_EXTENSIONS environment variable
    const char* extensions_env = getenv("FELIX86_EXTENSIONS");
    if (extensions_env) {
        if (g_extensions_manually_specified) {
            WARN("FELIX86_EXTENSIONS environment variable overrides manually specified extensions");
            Extensions::Clear();
        }

        if (!parse_extensions(extensions_env)) {
            WARN("Failed to parse environment variable FELIX86_EXTENSIONS");
        } else {
            g_extensions_manually_specified = true;
        }
    }

    const char* dont_optimize_env = getenv("FELIX86_NO_OPT");
    if (dont_optimize_env) {
        g_dont_optimize = true;
    }

    const char* strace_env = getenv("FELIX86_STRACE");
    if (strace_env) {
        g_strace = true;
    }

    const char* print_blocks_env = getenv("FELIX86_PRINT_BLOCKS");
    if (print_blocks_env) {
        g_print_blocks = true;
    }

    const char* print_state_env = getenv("FELIX86_PRINT_STATE");
    if (print_state_env) {
        g_print_state = true;
    }

    const char* print_disassembly_env = getenv("FELIX86_PRINT_DISASSEMBLY");
    if (print_disassembly_env) {
        g_print_disassembly = true;
    }
}

bool parse_extensions(const char* arg) {
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