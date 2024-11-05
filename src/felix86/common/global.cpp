#include <algorithm>
#include <cstring>
#include <string>
#include <vector>
#include "biscuit/cpuinfo.hpp"
#include "felix86/common/global.hpp"
#include "felix86/common/log.hpp"

#ifdef __riscv
#include <vector>
#include <asm/hwprobe.h>
#include <sys/syscall.h>
#include <unistd.h>
#endif

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
u32 g_spilled_count = 0;

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
    if (dont_optimize_env) {
        g_dont_optimize = true;
        environment += "\nFELIX86_NO_OPT";
    }

    const char* strace_env = getenv("FELIX86_STRACE");
    if (strace_env) {
        g_strace = true;
        environment += "\nFELIX86_STRACE";
    }

    const char* print_blocks_env = getenv("FELIX86_PRINT_BLOCKS");
    if (print_blocks_env) {
        g_print_blocks = true;
        environment += "\nFELIX86_PRINT_BLOCKS";
    }

    const char* print_state_env = getenv("FELIX86_PRINT_STATE");
    if (print_state_env) {
        g_print_state = true;
        environment += "\nFELIX86_PRINT_STATE";
    }

    const char* print_disassembly_env = getenv("FELIX86_PRINT_DISASSEMBLY");
    if (print_disassembly_env) {
        g_print_disassembly = true;
        environment += "\nFELIX86_PRINT_DISASSEMBLY";
    }

    const char* verbose_env = getenv("FELIX86_VERBOSE");
    if (verbose_env) {
        g_verbose = true;
        environment += "\nFELIX86_VERBOSE";
    }

    const char* quiet_env = getenv("FELIX86_QUIET");
    if (quiet_env) {
        g_quiet = true;
        environment += "\nFELIX86_QUIET";
    }

    const char* dont_coalesce_env = getenv("FELIX86_NO_COALESCE");
    if (dont_coalesce_env) {
        g_coalesce = false;
        environment += "\nFELIX86_NO_COALESCE";
    }

    const char* print_start_of_block_env = getenv("FELIX86_PRINT_BLOCK_START");
    if (print_start_of_block_env) {
        g_print_block_start = true;
        environment += "\nFELIX86_PRINT_BLOCK_START";
    }

    if (!g_testing) {
        const char* cache_env = getenv("FELIX86_CACHE");
        if (cache_env) {
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

#ifdef __riscv
        // clang-format off
        std::vector<riscv_hwprobe> pairs = {
            {RISCV_HWPROBE_KEY_BASE_BEHAVIOR, 0},
            {RISCV_HWPROBE_KEY_IMA_EXT_0, 0},
        };
        // clang-format on

        long result = syscall(SYS_riscv_hwprobe, pairs.data(), pairs.size(), 0, nullptr, 0);
        if (result < 0) {
            ERROR("Failed to probe hardware capabilities: %ld", result);
            return;
        }

        for (const auto& pair : pairs) {
            switch (pair.key) {
            case RISCV_HWPROBE_KEY_BASE_BEHAVIOR:
                ASSERT(pair.value & RISCV_HWPROBE_BASE_BEHAVIOR_IMA);
                break;
            case RISCV_HWPROBE_KEY_IMA_EXT_0:
                Extensions::G = pair.value & RISCV_HWPROBE_IMA_FD;
                Extensions::V = pair.value & RISCV_HWPROBE_IMA_V;
                Extensions::C = pair.value & RISCV_HWPROBE_IMA_C;
                Extensions::B = (pair.value & (RISCV_HWPROBE_EXT_ZBA | RISCV_HWPROBE_EXT_ZBB | RISCV_HWPROBE_EXT_ZBC | RISCV_HWPROBE_EXT_ZBS)) ==
                                (RISCV_HWPROBE_EXT_ZBA | RISCV_HWPROBE_EXT_ZBB | RISCV_HWPROBE_EXT_ZBC | RISCV_HWPROBE_EXT_ZBS);
                Extensions::Zacas = pair.value & RISCV_HWPROBE_EXT_ZACAS;
                Extensions::Zicond = pair.value & RISCV_HWPROBE_EXT_ZICOND;
#ifdef RISCV_HWPROBE_EXT_ZAM // remove me when defined
                Extensions::Zam = pair.value & RISCV_HWPROBE_EXT_ZAM;
#endif
#ifdef RISCV_HWPROBE_EXT_ZABHA // remove me when defined
                Extensions::Zabha = pair.value & RISCV_HWPROBE_EXT_ZABHA;
#endif
                break;
            default:
                break;
            }
        }
#endif
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
    }

    if (Extensions::V) {
        Extensions::VLEN = 128;
        WARN("Setting VLEN to 128");
    }

    if (!Extensions::G) {
        WARN("G extension was not specified, enabling it by default");
        Extensions::G = true;
    }

    return true;
}