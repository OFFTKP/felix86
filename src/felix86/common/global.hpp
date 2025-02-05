#pragma once

#include <filesystem>
#include <list>
#include <unordered_map>
#include <vector>
#include <semaphore.h>
#include <unistd.h>
#include "felix86/common/utility.hpp"

#define SUPPORTED_VLEN 128
extern bool g_verbose;
extern bool g_quiet;
extern bool g_testing;
extern bool g_strace;
extern bool g_calltrace;
extern bool g_extensions_manually_specified;
extern bool g_dont_validate_exe_path;
extern bool g_paranoid;
extern bool g_dont_link;
extern u64 g_current_brk;
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
extern sem_t* g_semaphore;
extern pthread_key_t g_thread_state_key;
extern std::list<struct ThreadState*> g_thread_states;

bool parse_extensions(const char* ext);
void initialize_globals();
void initialize_extensions();
void initialize_semaphore();
void unlink_semaphore();
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