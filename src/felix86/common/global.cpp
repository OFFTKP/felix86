#include <algorithm>
#include <cstring>
#include <string>
#include <fcntl.h>
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
bool g_preload = false;
bool g_coalesce = true;
bool g_dont_link = false;
bool g_extensions_manually_specified = false;
bool g_include_comments = false;
bool g_graph_coloring = false;
bool g_fast_recompiler = false;
bool g_profile_compilation = false;
u64 g_dispatcher_exit_count = 0;
std::unordered_map<u64, std::vector<u64>> g_breakpoints{};
std::chrono::nanoseconds g_compilation_total_time = std::chrono::nanoseconds(0);

// Having too many basic blocks in a function can cause the register allocator to take insanely long times
// So a block limit can sacrifice some potential runtime performance for way better compilation times
constexpr int default_block_limit = 1;
int g_block_limit = default_block_limit;
int g_output_fd = 1;
u32 g_spilled_count = 0;
std::filesystem::path g_rootfs_path{};
thread_local ThreadState* g_thread_state;
u64 g_executable_base_hint = 0;
u64 g_interpreter_base_hint = 0;
Emulator* g_emulator = nullptr;

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

    const char* block_limit_env = getenv("FELIX86_BLOCK_LIMIT");
    if (block_limit_env) {
        g_block_limit = std::atoi(block_limit_env);
        environment += "\nFELIX86_BLOCK_LIMIT=" + std::string(block_limit_env);
        if (g_block_limit < 0) {
            WARN("Block limit is less than 0, setting to default value of %d", default_block_limit);
            g_block_limit = default_block_limit;
        }
    }

    const char* dont_coalesce_env = getenv("FELIX86_NO_COALESCE");
    if (is_truthy(dont_coalesce_env)) {
        g_coalesce = false;
        environment += "\nFELIX86_NO_COALESCE";
    }

    const char* dont_link_env = getenv("FELIX86_NO_LINK");
    if (is_truthy(dont_link_env)) {
        g_dont_link = true;
        environment += "\nFELIX86_NO_LINK";
    }

    const char* graph_coloring_env = getenv("FELIX86_GRAPH_COLORING");
    if (is_truthy(graph_coloring_env)) {
        g_graph_coloring = true;
        environment += "\nFELIX86_GRAPH_COLORING";
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

    const char* fast_recompiler_env = getenv("FELIX86_FAST_RECOMPILER");
    if (is_truthy(fast_recompiler_env)) {
        g_fast_recompiler = true;
        environment += "\nFELIX86_FAST_RECOMPILER";
    }

    const char* profile_compilation_env = getenv("FELIX86_PROFILE_COMPILATION");
    if (is_truthy(profile_compilation_env)) {
        g_profile_compilation = true;
        environment += "\nFELIX86_PROFILE_COMPILATION";

        std::atexit([]() {
            printf("Total compilation time: %ldms\n", g_compilation_total_time.count() / 1000000);
            printf("Total dispatcher exits: %ld\n", g_dispatcher_exit_count);
            printf("Total code cache size: %ldKB\n", g_emulator->GetCodeCacheSize() / 1024);
        });
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

    const char* log_file = getenv("FELIX86_LOG_FILE");
    if (log_file) {
        int fd = open(log_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (fd == -1) {
            ERROR("Failed to open log file %s: %s", log_file, strerror(errno));
        } else {
            g_output_fd = fd;
            environment += "\nFELIX86_LOG_FILE=" + std::string(log_file);
        }
    }

    if (!g_testing) {
        const char* cache_env = getenv("FELIX86_NO_CACHE");
        if (is_truthy(cache_env)) {
            g_cache_functions = false;
            environment += "\nFELIX86_NO_CACHE";
        } else {
            g_cache_functions = true;

            const char* preload_env = getenv("FELIX86_NO_PRELOAD");
            if (is_truthy(preload_env)) {
                g_preload = false;
            } else {
                g_preload = false;
                // g_preload = true; TODO: fix preloading
            }
        }
    }

    const char* env_file = getenv("FELIX86_ENV_FILE");
    if (env_file) {
        // Handled in main
        environment += "\nFELIX86_ENV_FILE=" + std::string(env_file);
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