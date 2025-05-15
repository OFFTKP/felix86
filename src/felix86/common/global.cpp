#include <algorithm>
#include <cstring>
#include <string>
#include <fcntl.h>
#include <linux/perf_event.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <sys/utsname.h>
#include "biscuit/cpuinfo.hpp"
#include "felix86/common/config.hpp"
#include "felix86/common/gdbjit.hpp"
#include "felix86/common/global.hpp"
#include "felix86/common/info.hpp"
#include "felix86/common/log.hpp"
#include "felix86/common/perf.hpp"
#include "felix86/common/state.hpp"
#include "felix86/hle/filesystem.hpp"
#include "felix86/hle/mmap.hpp"

bool g_extensions_manually_specified = false;
bool g_print_all_calls = false;
bool g_mode32 = false;
std::atomic_bool g_symbols_cached = {false};
u64 g_initial_brk = 0;
u64 g_current_brk = 0;
u64 g_current_brk_size = 0;
u64 g_max_brk_size = 0;
u64 g_dispatcher_exit_count = 0;
std::unordered_map<u64, std::vector<u64>> g_breakpoints{};
pthread_key_t g_thread_state_key = -1;
ProcessGlobals g_process_globals{};
std::unique_ptr<Mapper> g_mapper{};
std::unique_ptr<GDBJIT> g_gdbjit;
u64 g_program_end = 0;
u64 g_guest_auxv{};
size_t g_guest_auxv_size = 0;
bool g_execve_process = false;
std::unique_ptr<Filesystem> g_fs{};
std::string g_emulator_path;
StartParameters g_params{};
int g_linux_major = 0;
int g_linux_minor = 0;

// g_output_fd should be replaced upon connecting to the server, however if an error occurs before then we should at least log it
int g_output_fd = STDERR_FILENO;
int g_rootfs_fd = 0;

u64 g_interpreter_start{};
u64 g_interpreter_end{};
u64 g_executable_start{};
u64 g_executable_end{};

bool is_running_under_gdb() {
    if (g_config.gdb) {
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

    perf = std::make_unique<Perf>();

    cas128_lock = 0;

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

void set_xtheadvector(int sig, siginfo_t* info, void* ctx) {
    Extensions::Xtheadvector = true;

#ifdef __riscv
    // Skip the illegal instruction
    ((ucontext_t*)ctx)->uc_mcontext.__gregs[REG_PC] += 4;
#endif
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
    if (Extensions::Zvbb) {
        if (!extensions.empty())
            extensions += ",";
        extensions += "zvbb";
    }
    if (Extensions::Zvkned) {
        if (!extensions.empty())
            extensions += ",";
        extensions += "zvkned";
    }

    return extensions;
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
        Extensions::Zihintpause = cpuinfo.Has(RISCVExtension::Zihintpause);
        Extensions::Zfa = cpuinfo.Has(RISCVExtension::Zfa);
        Extensions::Zba = cpuinfo.Has(RISCVExtension::Zba);
        Extensions::Zvbb = cpuinfo.Has(RISCVExtension::Zvbb);
        Extensions::Zvkned = cpuinfo.Has(RISCVExtension::Zvkned);
    }

#ifdef __x86_64__
    // Just so we can run some unit tests fine
    Extensions::G = true;
    Extensions::V = true;
    Extensions::VLEN = 128;
#endif
}

void initialize_globals() {
    struct utsname buffer;

    bool detected = false;
    if (uname(&buffer) != 0) {
        WARN("Failed to detect Linux kernel version");
    } else {
        int major, minor;
        int scanned = sscanf(buffer.release, "%d.%d", &major, &minor);
        if (scanned == 2) {
            detected = true;
            g_linux_major = major;
            g_linux_minor = minor;
        }
    }

    if (!detected) {
        // Just set to earliest Linux version that supports RISC-V
        g_linux_major = 5;
        g_linux_minor = 17;
    }

    std::string environment = g_config.getEnvironment();

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

    if (g_config.rootfs_path.empty()) {
        printf("Rootfs path is empty. Please set the FELIX86_ROOTFS environment variable or the rootfs_path variable in %s\n",
               g_config.path().c_str());

        if (geteuid() == 0) {
            printf("\nI noticed you are running as root, did you forget to pass `-E` to sudo?\n");
        }
        exit(1);
    }

    if (g_config.rootfs_path.string().back() == '/') {
        // User ended the path with '/', we need to remove it to make sure some of our comparisons
        // on whether a path is inside the rootfs continue to work
        g_config.rootfs_path = g_config.rootfs_path.string().substr(0, g_config.rootfs_path.string().size() - 1);
    }
    ASSERT(std::filesystem::exists(g_config.rootfs_path));
    ASSERT(std::filesystem::is_directory(g_config.rootfs_path));
    g_rootfs_fd = open(g_config.rootfs_path.c_str(), O_DIRECTORY);

    const char* env_file = getenv("FELIX86_ENV_FILE");
    if (env_file) {
        // Handled in main
        environment += "\nFELIX86_ENV_FILE=" + std::string(env_file);
    }

    // For perf symbols
    if (!std::filesystem::exists("/tmp")) {
        std::filesystem::create_directory("/tmp");
    }

    int perfs = g_config.perf_blocks + g_config.perf_libs + g_config.perf_global;
    if (perfs > 1) {
        ERROR("Conflicting perf settings, only enable per-block OR per-library OR global, not multiple at once");
    }

    g_config.gdb = is_running_under_gdb();
    if (g_config.gdb) {
        LOG("Emitting symbols for " ANSI_BOLD "gdb" ANSI_COLOR_RESET "!");
    }

    std::string extensions = get_extensions();
    if (extensions.empty()) {
        initialize_extensions();
        extensions = get_extensions();
        ASSERT(!extensions.empty());
    }

#ifdef __riscv
    if (!Extensions::G) {
        WARN("G extension was not specified, enabling it by default");
        Extensions::G = true;
    }

    if (!Extensions::V) {
        ERROR("V extension is required for SSE instructions");
    }

    struct sigaction old_act;
    struct sigaction new_act;
    new_act.sa_sigaction = set_xtheadvector;
    new_act.sa_flags = SA_SIGINFO;
    sigemptyset(&new_act.sa_mask);

    sigaction(SIGILL, &new_act, &old_act);

    // VCSR doesn't exist in Xtheadvector, so this will hit an illegal instruction
    // So we can detect Xtheadvector in a cross platform way that works in older kernels too
    biscuit::CSRReader<CSR::VCSR> vcsr_reader;
    (vcsr_reader.GetCode<u32 (*)()>())();

    sigaction(SIGILL, &old_act, nullptr);

    if (Extensions::Xtheadvector) {
        ERROR("This board has xtheadvector, but felix86 only works with RVV 1.0 currently");
    }
#endif

    if (!g_execve_process) {
        LOG("%s", get_version_full());
        LOG("Linux %d.%d", g_linux_major, g_linux_minor);
        if (!environment.empty()) {
            LOG("Environment:%s", environment.c_str());
        }

        LOG("Extensions enabled for the recompiler: %s", extensions.c_str());
    }

    ThreadState::InitializeKey();
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
