#include <algorithm>
#include <cstring>
#include <fstream>
#include <list>
#include <string>
#include <fcntl.h>
#include <linux/perf_event.h>
#include <sys/mman.h>
#include "biscuit/cpuinfo.hpp"
#include "felix86/common/global.hpp"
#include "felix86/common/log.hpp"
#include "felix86/common/state.hpp"
#include "felix86/hle/filesystem.hpp"
#include "fmt/format.h"

bool g_paranoid = false;
bool g_verbose = false;
bool g_quiet = false;
bool g_testing = false;
bool g_strace = false;
bool g_dump_regs = false;
bool g_dont_link = false;
bool g_extensions_manually_specified = false;
bool g_calltrace = false;
bool g_use_block_cache = true;
bool g_single_step = false;
bool g_dont_protect_pages = false;
bool g_print_all_calls = false;
bool g_no_sse2 = false;
bool g_no_sse3 = false;
bool g_no_ssse3 = false;
bool g_no_sse4_1 = false;
bool g_no_sse4_2 = false;
bool g_print_all_insts = false;
bool g_dont_inline_syscalls = false;
bool g_min_max_accurate = false;
int g_block_trace = 0;
bool g_mode32 = false;
bool g_rsb = true;
bool g_perf = false;
bool g_always_tso = false;
bool g_dont_link_indirect = true; // doesn't seem to impact performance from limited testing, so off by default
std::atomic_bool g_symbols_cached = {false};
u64 g_initial_brk = 0;
u64 g_current_brk = 0;
u64 g_current_brk_size = 0;
u64 g_dispatcher_exit_count = 0;
u64 g_address_space_base = 0;
std::list<ThreadState*> g_thread_states{};
std::unordered_map<u64, std::vector<u64>> g_breakpoints{}; // TODO: HostAddress
pthread_key_t g_thread_state_key = -1;
ProcessGlobals g_process_globals{};
HostAddress g_guest_auxv{};
size_t g_guest_auxv_size = 0;
bool g_execve_process = false;
std::unique_ptr<Filesystem> g_fs{};
Config g_config{};

int g_output_fd = 1;
std::filesystem::path g_rootfs_path{};
u64 g_executable_base_hint = 0;
u64 g_interpreter_base_hint = 0;
u64 g_brk_base_hint = 0;

HostAddress g_interpreter_start{};
HostAddress g_interpreter_end{};
HostAddress g_executable_start{};
HostAddress g_executable_end{};

bool is_truthy(const char* str) {
    if (!str) {
        return false;
    }

    std::string lower = str;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    return lower == "true" || lower == "1" || lower == "yes" || lower == "on" || lower == "y" || lower == "enable";
}

bool is_running_under_perf() {
    // Always enable symbol emission when this is enabled, in case our detection fails
    const char* perf_env = getenv("FELIX86_PERF");
    if (is_truthy(perf_env)) {
        return true;
    }

    int ppid = getppid();

    std::string line;
    std::ifstream ifs("/proc/" + std::to_string(ppid) + "/comm");
    if (!ifs) {
        WARN("Failed to check if perf is a parent process");
        return false;
    }

    std::getline(ifs, line);

    if (line == "perf") {
        return true;
    }

    return false;
}

void ProcessGlobals::initialize() {
    // Open a new shared memory region
    memory = std::make_unique<SharedMemory>(shared_memory_size);
    states_lock = ProcessLock(*memory);
    symbols_lock = ProcessLock(*memory);

    // Reset the states stored here
    states = {};

    // Don't reset the mapped regions, we can reuse the ones from parent process
}

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

std::string get_extensions() {
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

    return extensions;
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

    const char* strace_env = getenv("FELIX86_STRACE");
    if (is_truthy(strace_env)) {
        g_strace = true;
        environment += "\nFELIX86_STRACE";
    }

    const char* verbose_env = getenv("FELIX86_VERBOSE");
    if (is_truthy(verbose_env)) {
        g_verbose = true;
        environment += "\nFELIX86_VERBOSE";
    }

    const char* quiet_env = getenv("FELIX86_QUIET");
    if (is_truthy(quiet_env)) {
        if (!g_testing)
            g_quiet = true;
        environment += "\nFELIX86_QUIET";
    }

    const char* dump_regs_env = getenv("FELIX86_DUMP_REGS");
    if (dump_regs_env) {
        g_dump_regs = true;
        environment += "\nFELIX86_DUMP_REGS";
    }

    const char* rootfs_path = getenv("FELIX86_ROOTFS");
    if (rootfs_path) {
        if (!g_rootfs_path.empty()) {
            WARN("Rootfs overwritten by environment variable FELIX86_ROOTFS");
        }
        g_rootfs_path = rootfs_path;
        environment += "\nFELIX86_ROOTFS=" + std::string(rootfs_path);
    } else {
        const char* rootfs_path = getenv("FELIX86_ROOTFS_PATH");
        if (rootfs_path) {
            if (!g_rootfs_path.empty()) {
                WARN("Rootfs overwritten by environment variable FELIX86_ROOTFS_PATH");
            }
            g_rootfs_path = rootfs_path;
            environment += "\nFELIX86_ROOTFS_PATH=" + std::string(rootfs_path);
        }
    }

    const char* calltrace_env = getenv("FELIX86_CALLTRACE");
    if (is_truthy(calltrace_env)) {
        g_calltrace = true;
        environment += "\nFELIX86_CALLTRACE";
    }

    const char* paranoid_env = getenv("FELIX86_PARANOID");
    if (is_truthy(paranoid_env)) {
        g_paranoid = true;
        environment += "\nFELIX86_PARANOID";
    }

    const char* dont_rsb_env = getenv("FELIX86_DONT_RSB");
    if (is_truthy(dont_rsb_env)) {
        g_rsb = false;
        environment += "\nFELIX86_DONT_RSB";
    }

    const char* min_max_accurate_env = getenv("FELIX86_MIN_MAX_ACCURATE");
    if (is_truthy(min_max_accurate_env)) {
        g_min_max_accurate = true;
        environment += "\nFELIX86_MIN_MAX_ACCURATE";
    }

    const char* block_trace = getenv("FELIX86_BLOCK_TRACE");
    if (block_trace) {
        g_block_trace = std::stoi(block_trace);
        g_dont_link = true; // needed to trace blocks
        g_dont_link_indirect = true;
        environment += "\nFELIX86_BLOCK_TRACE=";
        environment += block_trace;
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

    const char* brk_base = getenv("FELIX86_BRK_BASE");
    if (brk_base) {
        g_brk_base_hint = std::stoull(brk_base, nullptr, 16);
        environment += "\nFELIX86_BRK_BASE=" + fmt::format("{:016x}", g_brk_base_hint);
    }

    const char* dont_link = getenv("FELIX86_DONT_LINK");
    if (is_truthy(dont_link)) {
        g_dont_link = true;
        g_dont_link_indirect = true;
        environment += "\nFELIX86_DONT_LINK";
    }

    const char* link_indirect = getenv("FELIX86_LINK_INDIRECT");
    if (is_truthy(link_indirect)) {
        g_dont_link_indirect = false;
        environment += "\nFELIX86_LINK_INDIRECT";
    }

    const char* dont_protect_pages = getenv("FELIX86_DONT_PROTECT_PAGES");
    if (is_truthy(dont_protect_pages)) {
        g_dont_protect_pages = true;
        environment += "\nFELIX86_DONT_PROTECT_PAGES";
    }

    const char* dont_use_block_cache = getenv("FELIX86_DONT_USE_BLOCK_CACHE");
    if (is_truthy(dont_use_block_cache)) {
        g_use_block_cache = false;
        environment += "\nFELIX86_DONT_USE_BLOCK_CACHE";
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

    const char* no_sse2_env = getenv("FELIX86_NO_SSE2");
    if (is_truthy(no_sse2_env)) {
        g_no_sse2 = true;
        environment += "\nFELIX86_NO_SSE2";
    }

    const char* no_sse3_env = getenv("FELIX86_NO_SSE3");
    if (is_truthy(no_sse3_env)) {
        g_no_sse3 = true;
        environment += "\nFELIX86_NO_SSE3";
    }

    const char* no_ssse3_env = getenv("FELIX86_NO_SSSE3");
    if (is_truthy(no_ssse3_env)) {
        g_no_ssse3 = true;
        environment += "\nFELIX86_NO_SSSE3";
    }

    const char* no_sse4_1_env = getenv("FELIX86_NO_SSE4_1");
    if (is_truthy(no_sse4_1_env)) {
        g_no_sse4_1 = true;
        environment += "\nFELIX86_NO_SSE4_1";
    }

    const char* no_sse4_2_env = getenv("FELIX86_NO_SSE4_2");
    if (is_truthy(no_sse4_2_env)) {
        g_no_sse4_2 = true;
        environment += "\nFELIX86_NO_SSE4_2";
    }

    const char* env_file = getenv("FELIX86_ENV_FILE");
    if (env_file) {
        // Handled in main
        environment += "\nFELIX86_ENV_FILE=" + std::string(env_file);
    }

    g_perf = is_running_under_perf();
    if (g_perf) {
        if (!std::filesystem::exists("/tmp")) {
            std::filesystem::create_directory("/tmp");
        }

        LOG("Running under " ANSI_BOLD "perf" ANSI_COLOR_RESET "!");
    }

    const char* single_step = getenv("FELIX86_SINGLE_STEP");
    const char* single_stepping = getenv("FELIX86_SINGLE_STEPPING");
    if (is_truthy(single_step) || is_truthy(single_stepping)) {
        g_single_step = true;
        environment += "\nFELIX86_SINGLE_STEP";
    }

    if (!g_quiet && !environment.empty()) {
        LOG("Environment:%s", environment.c_str());
    }

    ThreadState::InitializeKey();
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
        Extensions::Zfa = cpuinfo.Has(RISCVExtension::Zfa);
        Extensions::Zvbb = cpuinfo.Has(RISCVExtension::Zvbb);
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
