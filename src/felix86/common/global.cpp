#include <algorithm>
#include <cstring>
#include <string>
#include "biscuit/cpuinfo.hpp"
#include "felix86/common/global.hpp"
#include "felix86/common/log.hpp"
#include "felix86/common/x86.hpp"
#include "felix86/emulator.hpp"
#include "felix86/hle/signals.hpp"
#include "fmt/format.h"
#include "version.hpp"

bool g_verbose = false;
bool g_quiet = false;
bool g_aot = false;
bool g_testing = false;
bool g_strace = false;
bool g_dont_optimize = false;
bool g_print_blocks = false;
bool g_print_block_start = false;
bool g_print_state = false;
bool g_print_disassembly = false;
bool g_cache_functions = false;
bool g_coalesce = true;
bool g_extensions_manually_specified = false;
int g_output_fd = 1;
u32 g_spilled_count = 0;
std::filesystem::path g_rootfs_path{};
thread_local ThreadState* g_thread_state;
u64 g_executable_base_hint = 0;
u64 g_interpreter_base_hint = 0;
Emulator* g_emulator = nullptr;
SignalHandler g_signal_handler;

u64 g_interpreter_start = 0;
u64 g_interpreter_end = 0;
u64 g_executable_start = 0;
u64 g_executable_end = 0;

#define X(ext) bool Extensions::ext = false;
FELIX86_EXTENSIONS_TOTAL
#undef X
int Extensions::VLEN = 0;

void Extensions::Clear() {
#define X(ext) ext = false;
    FELIX86_EXTENSIONS_TOTAL
#undef X
    VLEN = 0;
}

const char* get_version_full() {
    static std::string version = "felix86 " FELIX86_VERSION "." + std::string(g_git_hash);
    return version.c_str();
}

bool is_truthy(const char* str) {
    if (!str) {
        return false;
    }

    std::string lower = str;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    return lower == "true" || lower == "1" || lower == "yes" || lower == "on" || lower == "y" || lower == "enable";
}

void initialize_globals() {
    std::string environment;

    // Check for FELIX86_EXTENSIONS environment variable
    const char* all_extensions_env = getenv("FELIX86_ALL_EXTENSIONS");
    if (all_extensions_env) {
        if (g_extensions_manually_specified) {
            WARN("FELIX86_ALL_EXTENSIONS environment variable overrides manually specified extensions");
            Extensions::Clear();
        }

        if (!parse_extensions(all_extensions_env)) {
            WARN("Failed to parse environment variable FELIX86_EXTENSIONS");
        } else {
            g_extensions_manually_specified = true;
            environment += "\nFELIX86_ALL_EXTENSIONS=" + std::string(all_extensions_env);
        }
    }

    const char* extensions_env = getenv("FELIX86_EXTENSIONS");
    if (extensions_env) {
        if (g_extensions_manually_specified) {
            WARN("FELIX86_EXTENSIONS ignored, because extensions specified either with -X or FELIX86_ALL_EXTENSIONS");
        } else {

            if (!parse_extensions(extensions_env)) {
                WARN("Failed to parse environment variable FELIX86_EXTENSIONS");
            } else {
                environment += "\nFELIX86_EXTENSIONS=" + std::string(extensions_env);
            }
        }
    }

    const char* dont_optimize_env = getenv("FELIX86_NO_OPT");
    if (is_truthy(dont_optimize_env)) {
        g_dont_optimize = true;
        environment += "\nFELIX86_NO_OPT";
    }

    const char* strace_env = getenv("FELIX86_STRACE");
    if (is_truthy(strace_env)) {
        g_strace = true;
        environment += "\nFELIX86_STRACE";
    }

    const char* print_blocks_env = getenv("FELIX86_PRINT_BLOCKS");
    if (is_truthy(print_blocks_env)) {
        g_print_blocks = true;
        environment += "\nFELIX86_PRINT_BLOCKS";
    }

    const char* print_state_env = getenv("FELIX86_PRINT_STATE");
    if (is_truthy(print_state_env)) {
        g_print_state = true;
        environment += "\nFELIX86_PRINT_STATE";
    }

    const char* print_disassembly_env = getenv("FELIX86_PRINT_DISASSEMBLY");
    if (is_truthy(print_disassembly_env)) {
        g_print_disassembly = true;
        environment += "\nFELIX86_PRINT_DISASSEMBLY";
    }

    const char* verbose_env = getenv("FELIX86_VERBOSE");
    if (is_truthy(verbose_env)) {
        g_verbose = true;
        environment += "\nFELIX86_VERBOSE";
    }

    const char* quiet_env = getenv("FELIX86_QUIET");
    if (is_truthy(quiet_env)) {
        g_quiet = true;
        environment += "\nFELIX86_QUIET";
    }

    const char* dont_coalesce_env = getenv("FELIX86_NO_COALESCE");
    if (is_truthy(dont_coalesce_env)) {
        g_coalesce = false;
        environment += "\nFELIX86_NO_COALESCE";
    }

    const char* print_start_of_block_env = getenv("FELIX86_PRINT_BLOCK_START");
    if (is_truthy(print_start_of_block_env)) {
        g_print_block_start = true;
        environment += "\nFELIX86_PRINT_BLOCK_START";
    }

    const char* rootfs_path = getenv("FELIX86_ROOTFS");
    if (rootfs_path) {
        if (!g_rootfs_path.empty()) {
            WARN("Rootfs overwritten by environment variable FELIX86_ROOTFS");
        }
        g_rootfs_path = rootfs_path;
        environment += "\nFELIX86_ROOTFS=" + std::string(rootfs_path);
    }

    const char* executable_base = getenv("FELIX86_EXECUTABLE_BASE");
    if (executable_base) {
        g_executable_base_hint = std::stoull(executable_base, nullptr, 16);
        environment += "\nFELIX86_EXECUTABLE_BASE=" + fmt::format("{:016x}", g_executable_base_hint);
    }

    const char* interpreter_base = getenv("FELIX86_INTERPRETER_BASE");
    if (interpreter_base) {
        g_interpreter_base_hint = std::stoull(interpreter_base, nullptr, 16);
        environment += "\nFELIX86_INTERPRETER_BASE=" + fmt::format("{:016x}", g_interpreter_base_hint);
    }

    if (!g_testing) {
        const char* cache_env = getenv("FELIX86_NO_CACHE");
        if (is_truthy(cache_env)) {
            g_cache_functions = false;
        } else {
            g_cache_functions = true;
        }
    }

    if (!g_quiet && !environment.empty()) {
        LOG("Environment:%s", environment.c_str());
    }
}

void initialize_extensions() {
    if (!g_extensions_manually_specified) {
        biscuit::CPUInfo cpuinfo;
        Extensions::VLEN = cpuinfo.GetVlenb() * 8;
        Extensions::G = cpuinfo.Has(RISCVExtension::I) && cpuinfo.Has(RISCVExtension::M) && cpuinfo.Has(RISCVExtension::A) &&
                        cpuinfo.Has(RISCVExtension::F) && cpuinfo.Has(RISCVExtension::D);
        Extensions::V = cpuinfo.Has(RISCVExtension::V);
        Extensions::C = cpuinfo.Has(RISCVExtension::C);
        Extensions::B = cpuinfo.Has(RISCVExtension::Zba) && cpuinfo.Has(RISCVExtension::Zbb) && cpuinfo.Has(RISCVExtension::Zbc) &&
                        cpuinfo.Has(RISCVExtension::Zbs);
        Extensions::Zacas = cpuinfo.Has(RISCVExtension::Zacas);
        Extensions::Zicond = cpuinfo.Has(RISCVExtension::Zicond);
    }

#ifdef __riscv
    if (!Extensions::G) {
        WARN("G extension was not specified, enabling it by default");
        Extensions::G = true;
    }

    if (!Extensions::V) {
        ERROR("V extension is required for SSE instructions");
    }
#endif
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

#define X(ext)                                                                                                                                       \
    {                                                                                                                                                \
        std::string lower = #ext;                                                                                                                    \
        std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);                                                                        \
        if (extension == lower) {                                                                                                                    \
            Extensions::ext = true;                                                                                                                  \
            continue;                                                                                                                                \
        }                                                                                                                                            \
    }
        FELIX86_EXTENSIONS_TOTAL
#undef X

        {
            std::string lower = "xthead";
            std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
            if (extension == lower) {
                Extensions::Xtheadcondmov = true;
                Extensions::Xtheadba = true;
                continue;
            }
        }
    }

    if (Extensions::V) {
        biscuit::CPUInfo cpuinfo;
        Extensions::VLEN = cpuinfo.GetVlenb() * 8;
    }

    if (!Extensions::G) {
        WARN("G extension was not specified, enabling it by default");
        Extensions::G = true;
    }

    return true;
}