#pragma once

#include <atomic>
#include <map>
#include <unordered_map>
#include <vector>
#include <unistd.h>
#include "felix86/common/process_lock.hpp"
#include "felix86/common/start_params.hpp"
#include "felix86/common/types.hpp"

struct Filesystem;
struct GDBJIT;
struct Perf;
struct ThreadState;

struct MappedRegion {
    u64 base{};
    u64 end{};
    std::string file{}; // without rootfs prefix
};

struct MmapRegion {
    u64 base{};
    u64 end{};
};

struct Symbol {
    u64 start{};
    u64 size{};
    bool strong = false;
    std::string name{};
};

// Globals that are shared across processes, including threads, that have CLONE_VM set.
// This means they share the same memory space, which means access needs to be synchronized.
struct ProcessGlobals {
    void initialize(); // If a clone happens without CLONE_VM, these need to be reinitialized.

    Semaphore states_lock{};
    // States in this memory space. We don't care about states in different memory spaces, as they will have their
    // own copy of the process memory, which means we don't worry about self-modifying code there.
    std::vector<ThreadState*> states{};

    Semaphore symbols_lock{};
    std::map<u64, MappedRegion> mapped_regions{};
    std::map<u64, Symbol> symbols{};
    std::unique_ptr<Perf> perf;

    // For cmpxchg16b
    u32 cas128_lock = 0;

private:
    constexpr static size_t shared_memory_size = 0x10000;
};

struct Mapper;

extern ProcessGlobals g_process_globals;
extern std::unique_ptr<Mapper> g_mapper;

extern bool g_extensions_manually_specified;
extern bool g_print_all_calls;
extern bool g_mode32;
extern std::atomic_bool g_symbols_cached;
extern u64 g_initial_brk;
extern u64 g_current_brk;
extern u64 g_current_brk_size;
extern u64 g_dispatcher_exit_count;
extern u64 g_program_end;
extern int g_output_fd;
extern std::string g_emulator_path;
extern int g_rootfs_fd;
extern u64 g_interpreter_start, g_interpreter_end;
extern u64 g_executable_start, g_executable_end;
extern u64 g_max_brk_size;
extern const char* g_git_hash;
extern std::unordered_map<u64, std::vector<u64>> g_breakpoints;
extern pthread_key_t g_thread_state_key;
extern u64 g_guest_auxv;
extern size_t g_guest_auxv_size;
extern bool g_execve_process;
extern StartParameters g_params;
extern std::unique_ptr<Filesystem> g_fs;
extern std::unique_ptr<GDBJIT> g_gdbjit;
extern int g_linux_major;
extern int g_linux_minor;
extern bool g_no_riscv_v_state;

bool parse_extensions(const char* ext);
void initialize_globals();
void initialize_extensions();
std::string get_extensions();

struct Extensions {
// TODO: replace B with Zbb, Zba, etc
#define FELIX86_EXTENSIONS_TOTAL                                                                                                                     \
    X(G)                                                                                                                                             \
    X(C)                                                                                                                                             \
    X(B)                                                                                                                                             \
    X(V)                                                                                                                                             \
    X(Zacas)                                                                                                                                         \
    X(Zam)                                                                                                                                           \
    X(Zabha)                                                                                                                                         \
    X(Zicond)                                                                                                                                        \
    X(Zihintpause)                                                                                                                                   \
    X(Zba)                                                                                                                                           \
    X(Zfa)                                                                                                                                           \
    X(Zvfh)                                                                                                                                          \
    X(Zvbb)                                                                                                                                          \
    X(Zvkned)                                                                                                                                        \
    X(Xtheadcondmov)                                                                                                                                 \
    X(Xtheadvector)                                                                                                                                  \
    X(Xtheadba)                                                                                                                                      \
    X(TSO) /* no hardware has this so we don't care for now */

#define X(ext) static bool ext;
    FELIX86_EXTENSIONS_TOTAL
#undef X
    static int VLEN;

    static void Clear();
};