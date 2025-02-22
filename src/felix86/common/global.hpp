#pragma once

#include <atomic>
#include <filesystem>
#include <map>
#include <unordered_map>
#include <vector>
#include <unistd.h>
#include "felix86/common/process_lock.hpp"
#include "felix86/common/shared_memory.hpp"
#include "felix86/common/utility.hpp"

struct MappedRegion {
    u64 base;
    u64 end;
    std::string file; // without rootfs prefix
};

// Globals that are shared across processes, including threads, that have CLONE_VM set.
// This means they share the same memory space, which means access needs to be synchronized.
struct ProcessGlobals {
    void initialize(); // If a clone happens without CLONE_VM, these need to be reinitialized.

    SharedMemory memory{};
    ProcessLock states_lock{};
    // States in this memory space. We don't care about states in different memory spaces, as they will have their
    // own copy of the process memory, which means we don't worry about self-modifying code there.
    std::vector<ThreadState*> states{};

    ProcessLock mapped_regions_lock{};
    std::map<u64, MappedRegion> mapped_regions{};
    std::unordered_map<u64, std::string> symbols{};
    std::atomic_bool cached_symbols = {false};

private:
    constexpr static size_t shared_memory_size = 0x1000;
};

extern ProcessGlobals g_process_globals;

extern bool g_verbose;
extern bool g_quiet;
extern bool g_testing;
extern bool g_strace;
extern bool g_calltrace;
extern bool g_extensions_manually_specified;
extern bool g_paranoid;
extern bool g_is_chrooted;
extern bool g_dont_link;
extern bool g_dont_inline_syscalls;
extern bool g_use_block_cache;
extern bool g_single_step;
extern bool g_log_instructions;
extern bool g_dont_protect_pages;
extern bool g_print_all_calls;
extern bool g_no_sse2;
extern bool g_no_sse3;
extern bool g_no_ssse3;
extern bool g_no_sse4_1;
extern bool g_no_sse4_2;
extern bool g_print_all_insts;
extern u64 g_initial_brk;
extern u64 g_current_brk;
extern u64 g_current_brk_size;
extern u64 g_dispatcher_exit_count;
extern std::chrono::nanoseconds g_compilation_total_time;
extern int g_output_fd;
extern u32 g_spilled_count;
extern std::filesystem::path g_rootfs_path;
extern u64 g_interpreter_start, g_interpreter_end;
extern u64 g_executable_start, g_executable_end;
extern u64 g_interpreter_base_hint;
extern u64 g_executable_base_hint;
extern const char* g_git_hash;
extern struct Emulator* g_emulator;
extern std::unordered_map<u64, std::vector<u64>> g_breakpoints;
extern pthread_key_t g_thread_state_key;
extern std::vector<const char*> g_host_argv;

bool parse_extensions(const char* ext);
void initialize_globals();
void initialize_extensions();
const char* get_version_full();

struct Extensions {
#define FELIX86_EXTENSIONS_TOTAL                                                                                                                     \
    X(G)                                                                                                                                             \
    X(C)                                                                                                                                             \
    X(B)                                                                                                                                             \
    X(V)                                                                                                                                             \
    X(Zacas)                                                                                                                                         \
    X(Zam)                                                                                                                                           \
    X(Zabha)                                                                                                                                         \
    X(Zicond)                                                                                                                                        \
    X(Zfa)                                                                                                                                           \
    X(Zvbb)                                                                                                                                          \
    X(Xtheadcondmov)                                                                                                                                 \
    X(Xtheadba)

#define X(ext) static bool ext;
    FELIX86_EXTENSIONS_TOTAL
#undef X
    static int VLEN;

    static void Clear();
};