#include <algorithm>
#include <cstring>
#include <fstream>
#include <list>
#include <string>
#include <fcntl.h>
#include <linux/perf_event.h>
#include <sys/mman.h>
#include "biscuit/cpuinfo.hpp"
#include "felix86/common/gdbjit.hpp"
#include "felix86/common/global.hpp"
#include "felix86/common/info.hpp"
#include "felix86/common/log.hpp"
#include "felix86/common/overlay.hpp"
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
bool g_safe_flags = true;
bool g_dont_protect_pages = true; // disabled by default until SMC is fixed and tested
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
bool g_rsb = false; // off by default until we fix stack overflow problems ie in Celeste (or probably similar apps with jit)
bool g_perf = false;
bool g_gdb = false;
bool g_thunking = false;
bool g_always_tso = false;
bool g_dont_cache = false;
bool g_dont_link_indirect = true; // doesn't seem to impact performance from limited testing, so off by default
int g_vlen = 0;
std::atomic_bool g_symbols_cached = {false};
u64 g_initial_brk = 0;
u64 g_current_brk = 0;
u64 g_current_brk_size = 0;
u64 g_max_brk_size = 0;
u64 g_dispatcher_exit_count = 0;
std::list<ThreadState*> g_thread_states{};
std::unordered_map<u64, std::vector<u64>> g_breakpoints{}; // TODO: HostAddress
pthread_key_t g_thread_state_key = -1;
ProcessGlobals g_process_globals{};
std::unique_ptr<Mapper> g_mapper{};
std::unique_ptr<GDBJIT> g_gdbjit;
u64 g_program_end;
HostAddress g_guest_auxv{};
size_t g_guest_auxv_size = 0;
bool g_execve_process = false;
std::unique_ptr<Filesystem> g_fs{};
std::string g_emulator_path;
Config g_config{};

int g_output_fd = -1;
std::filesystem::path g_rootfs_path{};
int g_rootfs_fd = 0;
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

bool is_falsey(const char* str) {
    if (!str) {
        return false; // doesn't exist, neither false or true
    }

    return !is_truthy(str);
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

bool is_running_under_gdb() {
    const char* gdb_env = getenv("FELIX86_GDB");
    if (is_truthy(gdb_env)) {
        return true;
    }

    // Don't detect, only enable this when the environment variable is set
    // int ppid = getppid();

    // std::string line;
    // std::ifstream ifs("/proc/" + std::to_string(ppid) + "/comm");
    // if (!ifs) {
    //     WARN("Failed to check if gdb is a parent process");
    //     return false;
    // }

    // std::getline(ifs, line);

    // if (line == "gdb") {
    //     return true;
    // }

    return false;
}

void ProcessGlobals::initialize() {
    // New address space (clone used without CLONE_VM)
    // Re-initialize these
    states_lock = Semaphore();
    symbols_lock = Semaphore();

    // Reset the states stored here
    states = {};

    // Also reset our allocator
    g_mapper = std::make_unique<Mapper>();

    // And the GDB mappings
    g_gdbjit = std::make_unique<GDBJIT>();

    // Don't reset the /proc/self/maps mapped regions, we can reuse the ones from parent process
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

    g_emulator_path.resize(PATH_MAX);
    int read = readlink("/proc/self/exe", g_emulator_path.data(), PATH_MAX);
    ASSERT(read != -1);

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

    const char* dont_inline_syscalls_env = getenv("FELIX86_DONT_INLINE_SYSCALLS");
    if (is_truthy(dont_inline_syscalls_env)) {
        g_dont_inline_syscalls = true;
        environment += "\nFELIX86_DONT_INLINE_SYSCALLS";
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

    const char* unsafe_flags = getenv("FELIX86_UNSAFE_FLAGS");
    if (is_truthy(unsafe_flags)) {
        g_safe_flags = false;
        environment += "\nFELIX86_UNSAFE_FLAGS";
    }

    const char* dump_regs_env = getenv("FELIX86_DUMP_REGS");
    if (dump_regs_env) {
        g_dump_regs = true;
        environment += "\nFELIX86_DUMP_REGS";
    }

    const char* rootfs_path = getenv("FELIX86_ROOTFS");
    if (rootfs_path) {
        g_rootfs_path = rootfs_path;
        environment += "\nFELIX86_ROOTFS=" + std::string(rootfs_path);

        ASSERT(std::filesystem::exists(g_rootfs_path));
        ASSERT(std::filesystem::is_directory(g_rootfs_path));
        g_rootfs_fd = open(g_rootfs_path.c_str(), O_DIRECTORY);
    } else {
        ERROR("Rootfs path is empty, set it with the environment variable FELIX86_ROOTFS\n"
              "Example: `export FELIX86_ROOTFS=/home/me/somefolder/myx86rootfs`");
    }

    const char* thunk_env = getenv("FELIX86_THUNKS");
    if (thunk_env && !g_testing) {
        std::filesystem::path thunks = thunk_env;
        ASSERT_MSG(std::filesystem::exists(thunks), "The thunks path set with FELIX86_THUNKS %s does not exist", thunk_env);
        std::string srootfs = g_rootfs_path.string();
        ASSERT_MSG(thunks.string().find(srootfs.c_str()) == 0, "The thunks path set with FELIX86_THUNKS %s is not part of the rootfs (%s)", thunk_env,
                   srootfs.c_str());

        g_thunking = true;
        environment += "\nFELIX86_THUNKS=";
        environment += thunk_env;

        // TODO: should probably not be done here?
        std::filesystem::path glx_thunk;
        bool found_glx = false;

        auto check_glx = [&](const char* path) {
            if (!found_glx && std::filesystem::exists(thunks / path)) {
                glx_thunk = thunks / path;
                found_glx = true;
            }
        };

        check_glx("libGLX.so.0");
        check_glx("libGLX.so");
        check_glx("libGLX-thunked.so");

        if (!glx_thunk.empty()) {
            Overlays::addOverlay("libGLX.so.0", glx_thunk);
        } else {
            WARN("I couldn't find libGLX-thunked.so in %s", thunks.c_str());
        }

        std::filesystem::path egl_thunk;
        bool found_egl = false;

        auto check_egl = [&](const char* path) {
            if (!found_egl && std::filesystem::exists(thunks / path)) {
                egl_thunk = thunks / path;
                found_egl = true;
            }
        };

        check_egl("libEGL.so.1");
        check_egl("libEGL.so");
        check_egl("libEGL-thunked.so");

        if (!egl_thunk.empty()) {
            Overlays::addOverlay("libEGL.so.1", egl_thunk);
        } else {
            WARN("I couldn't find libEGL-thunked.so in %s", thunks.c_str());
        }
    }

    const char* tso_env = getenv("FELIX86_TSO");
    if (is_truthy(tso_env)) {
        g_always_tso = true;
        environment += "\nFELIX86_TSO";
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

    const char* rsb_env = getenv("FELIX86_RSB");
    if (is_truthy(rsb_env)) {
        g_rsb = true;
        environment += "\nFELIX86_RSB";
    }

    const char* min_max_accurate_env = getenv("FELIX86_MIN_MAX_ACCURATE");
    if (is_truthy(min_max_accurate_env)) {
        g_min_max_accurate = true;
        environment += "\nFELIX86_MIN_MAX_ACCURATE";
    }

    const char* brk_size = getenv("FELIX86_BRK_SIZE");
    if (brk_size) {
        g_max_brk_size = std::atoll(brk_size); // if can't be parsed returns 0, that's fine
        environment += "\nFELIX86_BRK_SIZE=";
        environment += brk_size;
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

    const char* dont_cache = getenv("FELIX86_DONT_CACHE");
    if (is_truthy(dont_cache)) {
        g_dont_cache = true;
        g_dont_protect_pages = true;
        environment += "\nFELIX86_DONT_CACHE";
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

        LOG("Emitting symbols for " ANSI_BOLD "perf" ANSI_COLOR_RESET "!");
    }

    g_gdb = is_running_under_gdb();
    if (g_gdb) {
        if (!std::filesystem::exists("/tmp")) {
            std::filesystem::create_directory("/tmp");
        }

        LOG("Emitting symbols for " ANSI_BOLD "gdb" ANSI_COLOR_RESET "!");
    }

    const char* single_step = getenv("FELIX86_SINGLE_STEP");
    const char* single_stepping = getenv("FELIX86_SINGLE_STEPPING");
    if (is_truthy(single_step) || is_truthy(single_stepping)) {
        g_single_step = true;
        environment += "\nFELIX86_SINGLE_STEP";
    }

    if (!g_execve_process) {
        LOG("%s", get_version_full());
        if (!environment.empty()) {
            LOG("Environment:%s", environment.c_str());
        }

        std::string extensions = get_extensions();
        if (!extensions.empty()) {
            LOG("Extensions enabled for the recompiler: %s", extensions.c_str());
        }
    }

    g_vlen = biscuit::CPUInfo().GetVlenb() * 8;

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
