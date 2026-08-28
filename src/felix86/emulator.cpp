#include <csignal>
#include <span>
#include <vector>
#include <elf.h>
#include <fcntl.h>
#include <fmt/base.h>
#include <fmt/format.h>
#include <linux/prctl.h>
#include <stdlib.h>
#include <sys/auxv.h>
#include <sys/mman.h>
#include <sys/personality.h>
#include <sys/prctl.h>
#include <sys/random.h>
#include <sys/ucontext.h>
#include "felix86/common/global.hpp"
#include "felix86/common/log.hpp"
#include "felix86/common/script.hpp"
#include "felix86/common/types.hpp"
#include "felix86/common/utility.hpp"
#include "felix86/emulator.hpp"
#include "felix86/hle/brk.hpp"
#include "felix86/hle/signals.hpp"
#include "felix86/hle/thread.hpp"
#include "felix86/hle/thunks.hpp"
#include "felix86/hle/vdso.hpp"
#include "felix86/v2/handlers.hpp"
#include "felix86/v2/recompiler.hpp"

extern char** environ;

static char x86_string[] = "i686";
static char x86_64_string[] = "x86_64";

static u64 stack_push64(u64 stack, u64 value) {
    stack -= 8;
    *(u64*)stack = value;
    return stack;
}

static u64 stack_push32(u64 stack, u64 value) {
    stack -= 4;
    *(u32*)stack = value;
    return stack;
}

static u64 stack_push_string(u64 stack, const char* str) {
    u64 len = strlen(str) + 1;
    stack -= len;
    strcpy((char*)stack, str);
    return stack;
}

struct auxv64_t {
    int a_type;

    union {
        u64 a_val;
        void* a_ptr;
        void (*a_fnc)();
    } a_un;
};

struct auxv32_t {
    int a_type;
    u32 a_val;
};

static bool initializeMmMap(prctl_mm_map& map) {
    char buffer[4096];
    int fd = open("/proc/self/stat", O_RDONLY);
    if (fd == -1) {
        return false;
    }

    ssize_t bytes = read(fd, buffer, sizeof(buffer));
    close(fd);

    if (bytes < 0) {
        return false;
    }

    buffer[std::min((size_t)bytes, sizeof(buffer) - 1)] = 0;

    // Thank you FEX
    int items_read = sscanf(buffer,
                            "%*d %*s %*c %*d %*d "      // 1 to 5
                            "%*d %*d %*d %*u %*u "      // 6 to 10
                            "%*u %*u %*u %*u %*u "      // 11 to 15
                            "%*d %*d %*d %*d %*d "      // 16 to 20
                            "%*d %*u %*u %*d %*u "      // 21 to 25
                            "%llu %llu %llu %*u %*u "   // 26 to 30
                            "%*u %*u %*u %*u %*u "      // 31 to 35
                            "%*u %*u %*d %*d %*u "      // 36 to 40
                            "%*u %*u %*u %*d %llu "     // 40 to 45
                            "%llu %llu %llu %llu %llu " // 46 to 50
                            "%llu",                     // 51
                            &map.start_code, &map.end_code, &map.start_stack, &map.start_data, &map.end_data, &map.start_brk, &map.arg_start,
                            &map.arg_end, &map.env_start, &map.env_end);

    if (items_read != 10) {
        return false;
    }

    map.brk = reinterpret_cast<uint64_t>(sbrk(0));
    map.auxv = NULL;
    map.auxv_size = 0;
    map.exe_fd = -1;
    return true;
}

static void setupMainStack(ThreadState* state) {
    bool mode32 = state->ctx.Mode32();
    ssize_t argc = g_params.argv.size();
    if (argc > 1) {
        VERBOSE("Passing %zu arguments to guest executable", argc - 1);
        for (ssize_t i = 1; i < argc; i++) {
            VERBOSE("Guest argument %zu: %s", i, g_params.argv[i].c_str());
        }
    }

    const char* path = g_params.argv[0].c_str();

    std::shared_ptr<Elf> elf = g_fs->GetExecutable();
    std::shared_ptr<Elf> interpreter = g_fs->GetInterpreter();

    // Initial process stack according to System V AMD64 ABI
    auto pair = Threads::AllocateStack(mode32);
    u64 rsp = (u64)pair.first;

    // Happens on x86 kernel during arch_align_stack
    int persona = personality(0xFFFFFFFF);
    if (persona != -1 && !(persona & ADDR_NO_RANDOMIZE)) {
        u32 random = 0;
        if (getrandom(&random, sizeof(random), 0) == sizeof(random)) {
            rsp -= random % 8192;
        }
        rsp &= ~0xF;
    }

    // To hold the addresses of the arguments for later pushing
    u64* argv_addresses = (u64*)alloca(argc * sizeof(u64));

    rsp = stack_push_string(rsp, path);
    const char* program_name = (const char*)rsp;

    rsp = stack_push_string(rsp, mode32 ? x86_string : x86_64_string);
    const char* platform_name = (const char*)rsp;

    // wine-preloader actually relies on us pushing these in this order
    u64 arg_end = rsp;
    for (ssize_t i = argc - 1; i >= 0; i--) {
        rsp = stack_push_string(rsp, g_params.argv[i].c_str());
        argv_addresses[i] = rsp;
    }
    u64 arg_start = rsp;

    size_t envc = g_params.envp.size();
    std::vector<u64> envp_addresses;
    envp_addresses.resize(envc);

    u64 env_end = rsp;
    for (size_t i = 0; i < envc; i++) {
        const char* env = g_params.envp[i].c_str();
        rsp = stack_push_string(rsp, env);
        envp_addresses[i] = rsp;
    }
    u64 env_start = rsp;

    // Align up, to 16 bytes
    if (rsp & 0xF) {
        rsp -= rsp & 0xF;
    }

    // Push 128-bits to stack that are gonna be used as random data
    rsp = stack_push64(rsp, 0);
    rsp = stack_push64(rsp, 0);
    u64 rand_address = rsp;

    int result = getrandom((void*)rand_address, 16, 0);
    if (result == -1 || result != 16) {
        ERROR("Failed to get random data");
    }

    std::vector<std::pair<u64, u64>> auxv_entries = {
        {AT_PAGESZ, {4096}},
        {AT_EXECFN, {(u64)program_name}},
        {AT_CLKTCK, {getauxval(AT_CLKTCK)}},
        {AT_ENTRY, {elf->GetEntrypoint()}},
        {AT_PLATFORM, {(u64)platform_name}},
        {AT_BASE, {interpreter ? (u64)interpreter->GetProgramBase() : (u64)elf->GetProgramBase()}},
        {AT_FLAGS, {0}},
        {AT_UID, {getauxval(AT_UID)}},
        {AT_EUID, {getauxval(AT_EUID)}},
        {AT_GID, {getauxval(AT_GID)}},
        {AT_EGID, {getauxval(AT_EGID)}},
        {AT_SECURE, {getauxval(AT_SECURE)}},
        {AT_PHDR, {(u64)elf->GetPhdr()}},
        {AT_PHENT, {elf->GetPhent()}},
        {AT_PHNUM, {elf->GetPhnum()}},
        {AT_RANDOM, {rand_address}},
        {AT_HWCAP, {0xBFEBFBFF}},
    };

    if (!mode32) {
        // Add pointer to VDSO object
        // Since we include it as part of the felix86 binary we can just
        // point there directly in 64-bit mode
        std::span<u8> vdso_object = VDSO::getObject64();
        void* mem = mmap(nullptr, vdso_object.size(), PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
        ASSERT(mem != MAP_FAILED);
        memcpy(mem, vdso_object.data(), vdso_object.size());
        mprotect(mem, vdso_object.size(), PROT_READ | PROT_EXEC);
        auxv_entries.push_back({AT_SYSINFO_EHDR, {(u64)mem}});
    }

    auxv_entries.push_back({AT_NULL, {0}}); // null terminator

    u16 auxv_count = std::size(auxv_entries);

    // This is the varying amount of space needed for the stack
    // past our own information block
    // It's important to calculate this because the RSP final
    // value needs to be aligned to 16 bytes
    int pointer_size = mode32 ? 4 : 8;
    u16 size_needed = (2 * pointer_size) * auxv_count + // aux vector entries
                      pointer_size +                    // null terminator
                      envc * pointer_size +             // envp
                      pointer_size +                    // null terminator
                      argc * pointer_size +             // argv
                      pointer_size;                     // argc

    // 16-byte align the RSP
    if (size_needed & 0xF) {
        rsp -= 16 - (size_needed & 0xF);
    }

    u64 final_rsp = rsp - size_needed;

    u64 (*stack_push)(u64, u64) = mode32 ? stack_push32 : stack_push64;

    for (int i = auxv_count - 1; i >= 0; i--) {
        rsp = stack_push(rsp, auxv_entries[i].second);
        rsp = stack_push(rsp, auxv_entries[i].first);
    }

    g_guest_auxv = rsp;
    g_guest_auxv_size = auxv_count * pointer_size;

    // End of environment variables
    rsp = stack_push(rsp, 0);
    for (int i = envc - 1; i >= 0; i--) {
        rsp = stack_push(rsp, envp_addresses[i]);
    }

    // End of arguments
    rsp = stack_push(rsp, 0);
    for (ssize_t i = argc - 1; i >= 0; i--) {
        rsp = stack_push(rsp, argv_addresses[i]);
    }

    // Argument count
    rsp = stack_push(rsp, argc);

    ASSERT_MSG(rsp == final_rsp, "%lx == %lx", rsp, final_rsp);
    if (rsp & 0xF) {
        ERROR("Stack not aligned to 16 bytes");
    }

    prctl_mm_map map;
    if (initializeMmMap(map)) {
        map.arg_start = arg_start;
        map.arg_end = arg_end;
        map.env_start = env_start;
        map.env_end = env_end;
        map.auxv = (__u64*)g_guest_auxv;
        map.auxv_size = g_guest_auxv_size;
        int result = prctl(PR_SET_MM, PR_SET_MM_MAP, &map, sizeof(prctl_mm_map), 0L);
        if (result != 0) {
            WARN("Failed to run PR_SET_MM_MAP");
        }
    } else {
        WARN("Failed to read /proc/self/stat");
    }

    u64 rsp_guest = rsp;
    state->SetGpr(X86_REF_RSP, rsp_guest);
}

void* Emulator::CompileNext(ThreadState* thread_state) {
    FELIX86_PROFILE_ACCUMULATION(thread_state->thread_stats, AccumulatedJITTime);
    u64 next_block = thread_state->recompiler->getCompiledBlock(thread_state, thread_state->GetRip());
    ASSERT_MSG(next_block != 0, "getCompiledBlock returned null?");
    return (void*)next_block;
}

void Emulator::Start() {
    g_process_globals.initialize();

#ifdef PR_RISCV_SET_ICACHE_FLUSH_CTX
    prctl(PR_RISCV_SET_ICACHE_FLUSH_CTX, PR_RISCV_CTX_SW_FENCEI_ON, PR_RISCV_SCOPE_PER_PROCESS);
#endif

    g_executable_path_absolute = std::filesystem::absolute(g_params.executable_path);
    ASSERT(std::filesystem::exists(g_executable_path_absolute));
    std::string path = g_executable_path_absolute;
    Elf::PeekResult peek = Elf::Peek(path);
    if (peek == Elf::PeekResult::NotElf) {
        Script::PeekResult peek = Script::Peek(path);
        if (peek == Script::PeekResult::Script) {
            ERROR("You are trying to run a script file\nPlease start emulated bash and use it to run the script instead");
        } else {
            if (std::filesystem::exists(path)) {
                FILE* f = fopen(path.c_str(), "r");
                ASSERT(f);
                fseek(f, 0L, SEEK_END);
                size_t size = ftell(f);
                fclose(f);
                if (size == 0) {
                    // Sometimes, things decide to just execute an empty file.
                    // We need to return 0 and warn
                    WARN("Tried to execute an empty file: %s, returning 0...", path.c_str());
                    _exit(0);
                }
            }
            ERROR("Unknown file format: %s", path.c_str());
        }
    }

    bool mode32 = false;
    if (peek == Elf::PeekResult::Elf32) {
        mode32 = true;
        // Allocate a 2GiB guard right after to catch bad addresses (that may need to loop around the address space?)
        constexpr u64 GB = 1024 * 1024 * 1024;
        void* guard = mmap((void*)(4 * GB), 2 * GB, PROT_NONE, MAP_FIXED_NOREPLACE | MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE, -1, 0);
        if (guard == MAP_FAILED) {
            ERROR("I failed to allocate the 32-bit guard");
        }

        prctl(PR_SET_VMA, PR_SET_VMA_ANON_NAME, 4 * GB, 2 * GB, "felix86-guard");
    }

    ThreadState* main_state = ThreadState::Create();
    main_state->ctx.cs = mode32 ? 0x23 : 0x33;

    if (!mode32 && !g_config.thunks_path.empty()) {
        Thunks::initialize();
    }

    g_fs->LoadExecutable(path);

    BRK::allocate(mode32);

    if (!g_execve_process) {
        if (!g_config.no_rootfs) {
            char buffer[PATH_MAX];
            char* cwd = getcwd(buffer, PATH_MAX);
            ASSERT(cwd == buffer);
            std::string scwd = cwd;
            bool ok = false;
            if (!is_subpath(scwd, g_config.rootfs_path)) {
                for (auto& mount : g_fake_mounts) {
                    if (is_subpath(scwd, mount.src_path)) {
                        // Current directory is inside fakemount, which is fine
                        ok = true;
                        break;
                    }
                }
            } else {
                // Current directory is inside rootfs
                ok = true;
            }

            if (!ok) {
                // If current directory is not inside rootfs or a fakemount, we need to go inside
                WARN("Chdiring inside rootfs");
                ASSERT(g_rootfs_fd > 0);
                ASSERT(fchdir(g_rootfs_fd) == 0);
            }
        }
    }

    main_state->signal_table = SignalHandlerTable::create(nullptr);
    main_state->SetRip(g_fs->GetEntrypoint());

    const char* mask = getenv("__FELIX86_SIGNAL_MASK");
    if (mask) {
        ASSERT(g_execve_process);
        u64 signal_mask;
        bool ok = to_u64(&signal_mask, mask);
        if (!ok) {
            WARN("Failed to convert __FELIX86_SIGNAL_MASK=%s to a number", mask);
        } else {
            sigset_t set;
            sigemptyset(&set);
            set.__val[0] = signal_mask;
            Signals::sigprocmask(main_state, SIG_SETMASK, &set, nullptr);
        }
    }

    setupMainStack(main_state);

    // The Emulator::Run will only return when exit_dispatcher is jumped to
    VERBOSE("Executable: %016lx - %016lx", g_executable_start, g_executable_end);
    if (g_interpreter_start != 0) {
        VERBOSE("Interpreter: %016lx - %016lx", g_interpreter_start, g_interpreter_end);
    }

    VERBOSE("Entering main thread :)");

    u64 fd = getauxval(AT_EXECFD);
    if (fd != 0 || errno != ENOENT /* it's possible fd==0 is the execfd if all other fds are closed */) {
        close(fd);
    }

    const char* options = getenv("__FELIX86_PTRACE_FLAGS");
    if (options) {
        int tracer_pid = get_tracer_pid();
        if (tracer_pid != 0) {
            const char* former_tracee = getenv("__FELIX86_PTRACE_FORMER_TRACEE");
            ASSERT(former_tracee);
            // We were being traced, we need to raise an execve-stop now that
            // felix86 had the time to initialize
            ASSERT(g_execve_process);
            u64 flags = std::atoi(options);
            ASSERT(flags != 0 || (options[0] == '0' && options[1] == '\0'));
            main_state->ptrace_data.constants.tracer_pid = tracer_pid;
            main_state->ptrace_data.constants.flags = flags;
            int former_tid = std::atoi(former_tracee);
            ASSERT(former_tid != 0);

            int event = 0;
            u64 event_msg = 0;
            if (flags & PTRACE_O_TRACEEXEC) {
                event = PTRACE_EVENT_EXEC;
                event_msg = former_tid;
            }

            // TODO: is siginfo_t reachable in event_exec?
            int sig = SIGTRAP;
            siginfo_t info;
            memset(&info, 0, sizeof(siginfo_t));
            info.si_code = SI_USER;
            main_state->ctx.orig_rax = main_state->ctx.Mode32() ? (int)felix86_x86_32_execve : (int)felix86_x86_64_execve;
            Ptrace::raise_stop(StopType::EventStop, sig, &info, event, event_msg);
            if (sig != 0) {
                ERROR("Injected signal during execve-stop, unimplemented");
            }
            if (main_state->ctx.orig_rax != -1ull) {
                WARN("Tracer didn't change our execve orig_rax to -1");
            }

            if (main_state->ptrace_data.injected.cont_type == PTRACE_SYSCALL) {
                int sig = SIGTRAP;
                siginfo_t info;
                memset(&info, 0, sizeof(siginfo_t));
                info.si_signo = SIGTRAP;
                info.si_code = SIGTRAP | 0x80; // TODO: check when do we insert 0x80 here
                main_state->ptrace_data.syscall_info.ret = 0;
                main_state->ptrace_data.syscall_info.is_error = false;
                Ptrace::raise_stop(StopType::SyscallExitStop, sig, &info);
            } else if (main_state->ptrace_data.injected.cont_type != PTRACE_CONT) {
                WARN("Not PTRACE_CONT after execve-stop");
            }
        }
    }

    Threads::StartThread(main_state);
    UNREACHABLE();
}

void Emulator::StartTest(const TestConfig& config, u64 stack) {
    ThreadState* main_state = ThreadState::Create(nullptr);
    main_state->ctx.cs = config.mode32 ? 0x23 : 0x33;
    main_state->SetGpr(X86_REF_RSP, stack);
    main_state->SetRip(config.entrypoint);

    if (config.fill_ymm_with_trash) {
        for (auto& ymm : main_state->ctx.xmm) {
            ymm.data[2] = 0xCCCC'CCCC'CCCC'CCCC;
            ymm.data[3] = 0xABCD'EF01'2345'6789;
        }
    }

    g_config.reduced_precision = config.reduced_precision;
    Handlers::initialize();

    g_testing = true;
    Threads::StartThread(main_state);
}
