#pragma once

#include <filesystem>
#include <unordered_map>
#include <vector>
#include "felix86/common/utility.hpp"

#define SUPPORTED_VLEN 128
extern bool g_verbose;
extern bool g_quiet;
extern bool g_testing;
extern bool g_strace;
extern bool g_extensions_manually_specified;
extern bool g_profile_compilation;
extern u64 g_dispatcher_exit_count;
extern std::chrono::nanoseconds g_compilation_total_time;
extern int g_output_fd;
extern u32 g_spilled_count;
extern std::filesystem::path g_rootfs_path;
extern u64 g_interpreter_start, g_interpreter_end;
extern u64 g_executable_start, g_executable_end;
extern thread_local ThreadState* g_thread_state;
extern u64 g_interpreter_base_hint;
extern u64 g_executable_base_hint;
extern const char* g_git_hash;
extern struct Emulator* g_emulator;
extern std::unordered_map<u64, std::vector<u64>> g_breakpoints;

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
    X(Xtheadcondmov)                                                                                                                                 \
    X(Xtheadba)

#define X(ext) static bool ext;
    FELIX86_EXTENSIONS_TOTAL
#undef X
    static int VLEN;

    static void Clear();
};