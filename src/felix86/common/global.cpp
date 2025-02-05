#include <algorithm>
#include <cstring>
#include <string>
#include <fcntl.h>
#include "biscuit/cpuinfo.hpp"
#include "felix86/common/global.hpp"
#include "felix86/common/log.hpp"
#include "felix86/common/state.hpp"
#include "felix86/emulator.hpp"
#include "fmt/format.h"
#include "version.hpp"

bool g_paranoid = false;
bool g_verbose = false;
bool g_quiet = false;
bool g_testing = false;
bool g_strace = false;
bool g_dont_link = false;
bool g_extensions_manually_specified = false;
bool g_dont_validate_exe_path = false;
bool g_calltrace = false;
u64 g_current_brk = 0;
sem_t* g_semaphore = nullptr;
u64 g_dispatcher_exit_count = 0;
std::list<ThreadState*> g_thread_states{};
std::unordered_map<u64, std::vector<u64>> g_breakpoints{};
std::chrono::nanoseconds g_compilation_total_time = std::chrono::nanoseconds(0);
pthread_key_t g_thread_state_key = -1;

int g_output_fd = 1;
std::filesystem::path g_rootfs_path{};
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

    const char* dont_link = getenv("FELIX86_DONT_LINK");
    if (is_truthy(dont_link)) {
        g_dont_link = true;
        environment += "\nFELIX86_DONT_LINK";
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

    const char* dont_validate_exe_path = getenv("FELIX86_DONT_VALIDATE_EXE_PATH");
    if (is_truthy(dont_validate_exe_path)) {
        g_dont_validate_exe_path = true;
        environment += "\nFELIX86_DONT_VALIDATE_EXE_PATH";
    }

    const char* env_file = getenv("FELIX86_ENV_FILE");
    if (env_file) {
        // Handled in main
        environment += "\nFELIX86_ENV_FILE=" + std::string(env_file);
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

// Needs to be reopened on new processes, the very first time it will be null though
void initialize_semaphore() {
    if (!g_semaphore) {
        g_semaphore = sem_open("/felix86_semaphore", O_CREAT | O_EXCL, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP, 1);
    } else {
        g_semaphore = sem_open("/felix86_semaphore", 0);
    }

    if (g_semaphore == SEM_FAILED) {
        ERROR("Failed to create semaphore: %s", strerror(errno));
        exit(1);
    }
}

void unlink_semaphore() {
    sem_unlink("/felix86_semaphore");
}