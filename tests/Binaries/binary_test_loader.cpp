
#include <filesystem>
#include <optional>
#include <thread>
#include <vector>
#include <catch2/catch_test_macros.hpp>
#include <fcntl.h>
#include <spawn.h>
#include <sys/wait.h>
#include "common.h"
#include "felix86/common/elf.hpp"
#include "felix86/common/log.hpp"
#include "fmt/format.h"

struct JobThreadData {
    std::optional<std::string> failures;
    std::vector<std::filesystem::path> tests;
    std::thread thread;
};

bool run_test(const std::filesystem::path& felix_path, const std::filesystem::path& path, int expected_exit_status) {
    int pipefd[2];
    if (pipe(pipefd) == -1) {
        perror("pipe");
        exit(1);
    }

    const std::filesystem::path tmp_path = "/tmp/felix86_binary_tests";
    const std::filesystem::path exec_path = tmp_path / path.filename();
    const std::string extension = path.extension();

    std::string buffer(1024 * 1024, 0);
    std::string srootfs = "FELIX86_ROOTFS=" + g_config.rootfs_path.string();
    std::string spath = exec_path;

    std::vector<const char*> argv;
    std::vector<const char*> envp;

    argv.push_back(felix_path.c_str());
    if (extension == ".exe") {
        argv.push_back("/usr/lib/wine/wine64");
        envp.push_back("WINEDEBUG=-all");
    }
    argv.push_back(spath.c_str());
    argv.push_back(nullptr);

    char** env = environ;
    while (*env) {
        envp.push_back(*env);
        env++;
    }
    envp.push_back(srootfs.c_str());
    envp.push_back(nullptr);

    std::filesystem::create_directories(g_config.rootfs_path / tmp_path.relative_path());

    // Copy our test binary to the temp path
    std::filesystem::copy(path, g_config.rootfs_path / exec_path.relative_path(), std::filesystem::copy_options::overwrite_existing);

    pid_t fork_result = fork();
    if (fork_result == 0) {
        close(pipefd[0]);
        dup2(pipefd[1], 1);
        close(pipefd[1]);
        execvpe(argv[0], (char* const*)argv.data(), (char* const*)envp.data());
        perror("execvpe");
        exit(1);
    } else {
        close(pipefd[1]);
        int status;
        waitpid(fork_result, &status, 0);
        size_t bytes_read = read(pipefd[0], buffer.data(), buffer.size());
        close(pipefd[0]);
        return WIFEXITED(status) && WEXITSTATUS(status) == expected_exit_status;
    }
}

void common_loader(const std::filesystem::path& path) {
    std::filesystem::path exe_path = std::filesystem::canonical("/proc/self/exe");
    std::filesystem::path dir = exe_path.parent_path();
    if (!std::filesystem::exists(dir / "felix86")) {
        CATCH_FAIL("felix86 executable not found in the current directory");
    }

    if (g_config.rootfs_path.empty() || !std::filesystem::exists(g_config.rootfs_path)) {
        CATCH_FAIL("This test requires a rootfs directory, set via FELIX86_ROOTFS");
    }

    CATCH_REQUIRE(std::filesystem::is_directory(dir / "Binaries" / path));
    std::filesystem::directory_iterator it(dir / "Binaries" / path);
    bool all_passed = true;
    std::string failures;
    for (const auto& entry : it) {
        std::string extension = entry.path().extension().string();
        if (extension == ".out" || extension == ".exe") {
            CATCH_INFO(fmt::format("Running test: {}", entry.path().filename().string()));
            bool passed = run_test(dir / "felix86", entry.path().string(), FELIX86_BTEST_SUCCESS);
            if (!passed) {
                all_passed = false;
                failures += entry.path().string() + "\n";
            } else {
                SUCCESS("Test passed: %s", entry.path().filename().c_str());
            }
        }
    }

    if (!all_passed) {
        CATCH_FAIL((std::string("Failed some tests:\n") + failures).c_str());
    }
}

void common_loader_concurrent(const std::filesystem::path& path) {
    std::filesystem::path exe_path = std::filesystem::canonical("/proc/self/exe");
    std::filesystem::path dir = exe_path.parent_path();
    if (!std::filesystem::exists(dir / "felix86")) {
        CATCH_FAIL("felix86 executable not found in the current directory");
    }

    if (g_config.rootfs_path.empty() || !std::filesystem::exists(g_config.rootfs_path)) {
        CATCH_FAIL("This test requires a rootfs directory, set via FELIX86_ROOTFS");
    }

    ASSERT(path.is_relative());
    std::filesystem::path dir_tests = dir / path;
    if (!std::filesystem::is_directory(dir_tests)) {
        CATCH_FAIL(
            fmt::format("Missing directory: {}\nThese tests need you to clone the submodules: `git submodule update --init`", dir_tests.c_str()));
    }

    int thread_count = std::thread::hardware_concurrency();
    if (thread_count <= 0) {
        thread_count = 2;
    }

    std::vector<JobThreadData> threads;
    threads.resize(thread_count);

    size_t index = 0;
    std::vector<std::filesystem::path> tests;
    std::filesystem::recursive_directory_iterator it(dir_tests);
    for (const auto& entry : it) {
        if (Elf::Peek(entry) != Elf::PeekResult::NotElf) {
            tests.push_back(entry.path());
        }
    }

    CATCH_REQUIRE(!tests.empty());

    size_t tests_per_thread = tests.size() / thread_count;
    for (const auto& test : tests) {
        auto& data = threads[index];
        data.tests.push_back(test);

        if (index != threads.size() - 1 && threads[index].tests.size() > tests_per_thread) {
            index++;
        }
    }

    for (auto& thread_data : threads) {
        thread_data.thread = std::thread([dir, &thread_data]() {
            for (const auto& entry : thread_data.tests) {
                bool passed = run_test(dir / "felix86", entry, 0);
                if (!passed) {
                    if (!thread_data.failures) {
                        thread_data.failures = "";
                    }
                    *thread_data.failures += entry.filename().string() + "\n";
                } else {
                    SUCCESS("Test passed: %s", entry.filename().c_str());
                }
            }
        });
    }

    for (auto& thread_data : threads) {
        thread_data.thread.join();
    }

    std::string failures;
    for (const auto& thread_data : threads) {
        if (thread_data.failures) {
            failures += *thread_data.failures;
        }
    }

    if (!failures.empty()) {
        CATCH_FAIL((std::string("Failed some tests:\n") + failures).c_str());
    }
}

CATCH_TEST_CASE("Signals", "[Binaries]") {
    common_loader("Signals");
}

CATCH_TEST_CASE("Simple", "[Binaries]") {
    common_loader("Simple");
}

CATCH_TEST_CASE("Clone", "[Binaries]") {
    common_loader("Clone");
}

CATCH_TEST_CASE("SMC", "[Binaries]") {
    // common_loader("SMC"); -- we don't handle smc rn
}

CATCH_TEST_CASE("Filesystem", "[Binaries]") {
    common_loader("Filesystem");
}

CATCH_TEST_CASE("GCC tests", "[Binaries]") {
    std::filesystem::path dir_i386 = std::filesystem::path("Binaries") / "binary_tests" / "fex-gcc-target-tests-bins" / "32";
    std::filesystem::path dir_x64 = std::filesystem::path("Binaries") / "binary_tests" / "fex-gcc-target-tests-bins" / "64";
    common_loader_concurrent(dir_i386);
    common_loader_concurrent(dir_x64);
}

CATCH_TEST_CASE("Posix tests", "[Binaries]") {
    std::filesystem::path dir = std::filesystem::path("Binaries") / "binary_tests" / "fex-posixtest-bins";
    // TODO: combine these, needs to make the test names not collide
    common_loader_concurrent(dir / "conformance/behavior/WIFEXITED");
    common_loader_concurrent(dir / "conformance/behavior/timers");
    common_loader_concurrent(dir / "conformance/definitions/errno_h");
    common_loader_concurrent(dir / "conformance/definitions/mqueue_h");
    common_loader_concurrent(dir / "conformance/definitions/signal_h");
    common_loader_concurrent(dir / "conformance/interfaces/aio_cancel");
    common_loader_concurrent(dir / "conformance/interfaces/aio_error");
    common_loader_concurrent(dir / "conformance/interfaces/aio_fsync");
    common_loader_concurrent(dir / "conformance/interfaces/aio_read");
    common_loader_concurrent(dir / "conformance/interfaces/aio_return");
    common_loader_concurrent(dir / "conformance/interfaces/aio_suspend");
    common_loader_concurrent(dir / "conformance/interfaces/aio_write");
    common_loader_concurrent(dir / "conformance/interfaces/clock");
    common_loader_concurrent(dir / "conformance/interfaces/clock_getcpuclockid");
    common_loader_concurrent(dir / "conformance/interfaces/clock_getres");
    common_loader_concurrent(dir / "conformance/interfaces/clock_gettime");
    common_loader_concurrent(dir / "conformance/interfaces/clock_nanosleep");
    common_loader_concurrent(dir / "conformance/interfaces/clock_settime");
    common_loader_concurrent(dir / "conformance/interfaces/ctime");
    common_loader_concurrent(dir / "conformance/interfaces/difftime");
    common_loader_concurrent(dir / "conformance/interfaces/fsync");
    common_loader_concurrent(dir / "conformance/interfaces/clock_nanosleep");
    common_loader_concurrent(dir / "conformance/interfaces/gmtime");
    common_loader_concurrent(dir / "conformance/interfaces/kill");
    common_loader_concurrent(dir / "conformance/interfaces/killpg");
    common_loader_concurrent(dir / "conformance/interfaces/lio_listio");
    common_loader_concurrent(dir / "conformance/interfaces/localtime");
    common_loader_concurrent(dir / "conformance/interfaces/mktime");
    common_loader_concurrent(dir / "conformance/interfaces/mlock");
    common_loader_concurrent(dir / "conformance/interfaces/mlock/speculative");
    common_loader_concurrent(dir / "conformance/interfaces/mlockall");
    common_loader_concurrent(dir / "conformance/interfaces/mlockall/speculative");
    common_loader_concurrent(dir / "conformance/interfaces/mmap");
    common_loader_concurrent(dir / "conformance/interfaces/mq_close");
    common_loader_concurrent(dir / "conformance/interfaces/mq_open");
    common_loader_concurrent(dir / "conformance/interfaces/mq_send");
    common_loader_concurrent(dir / "conformance/interfaces/mq_timedsend");
    common_loader_concurrent(dir / "conformance/interfaces/mq_unlink");
    common_loader_concurrent(dir / "conformance/interfaces/munlock");
    common_loader_concurrent(dir / "conformance/interfaces/munlockall");
    common_loader_concurrent(dir / "conformance/interfaces/munmap");
    common_loader_concurrent(dir / "conformance/interfaces/nanosleep");
    common_loader_concurrent(dir / "conformance/interfaces/pthread_atfork");
    common_loader_concurrent(dir / "conformance/interfaces/pthread_attr_destroy");
    common_loader_concurrent(dir / "conformance/interfaces/pthread_attr_getdetachstate");
    common_loader_concurrent(dir / "conformance/interfaces/pthread_attr_getinheritsched");
    common_loader_concurrent(dir / "conformance/interfaces/pthread_attr_getschedparam");
    common_loader_concurrent(dir / "conformance/interfaces/pthread_attr_getschedpolicy");
    common_loader_concurrent(dir / "conformance/interfaces/pthread_attr_getscope");
    common_loader_concurrent(dir / "conformance/interfaces/pthread_attr_init");
    common_loader_concurrent(dir / "conformance/interfaces/pthread_attr_setdetachstate");
    common_loader_concurrent(dir / "conformance/interfaces/pthread_attr_setinheritsched");
    common_loader_concurrent(dir / "conformance/interfaces/pthread_attr_setschedparam");
    common_loader_concurrent(dir / "conformance/interfaces/pthread_attr_setschedparam/speculative");
    common_loader_concurrent(dir / "conformance/interfaces/pthread_attr_setschedpolicy");
    common_loader_concurrent(dir / "conformance/interfaces/pthread_attr_setschedpolicy/speculative");
    common_loader_concurrent(dir / "conformance/interfaces/pthread_attr_setscope");
    common_loader_concurrent(dir / "conformance/interfaces/pthread_cond_destroy");
    common_loader_concurrent(dir / "conformance/interfaces/pthread_cond_init");
    common_loader_concurrent(dir / "conformance/interfaces/pthread_condattr_destroy");
    common_loader_concurrent(dir / "conformance/interfaces/pthread_condattr_init");
    common_loader_concurrent(dir / "conformance/interfaces/pthread_mutex_destroy");
    common_loader_concurrent(dir / "conformance/interfaces/pthread_mutex_destroy/speculative");
    common_loader_concurrent(dir / "conformance/interfaces/pthread_mutex_init");
    common_loader_concurrent(dir / "conformance/interfaces/pthread_mutex_init/speculative");
    common_loader_concurrent(dir / "conformance/interfaces/pthread_mutex_lock");
    common_loader_concurrent(dir / "conformance/interfaces/pthread_mutex_unlock");
    common_loader_concurrent(dir / "conformance/interfaces/raise");
    common_loader_concurrent(dir / "conformance/interfaces/sched_get_priority_max");
    common_loader_concurrent(dir / "conformance/interfaces/sched_get_priority_min");
    common_loader_concurrent(dir / "conformance/interfaces/sched_getparam");
    common_loader_concurrent(dir / "conformance/interfaces/sched_getparam/speculative");
    common_loader_concurrent(dir / "conformance/interfaces/sched_getscheduler");
    common_loader_concurrent(dir / "conformance/interfaces/sched_rr_get_interval");
    common_loader_concurrent(dir / "conformance/interfaces/sched_rr_get_interval/speculative");
    common_loader_concurrent(dir / "conformance/interfaces/sched_setparam");
    common_loader_concurrent(dir / "conformance/interfaces/sched_setscheduler");
    common_loader_concurrent(dir / "conformance/interfaces/sched_yield");
    common_loader_concurrent(dir / "conformance/interfaces/sem_init");
    common_loader_concurrent(dir / "conformance/interfaces/sem_open");
    common_loader_concurrent(dir / "conformance/interfaces/shm_open");
    common_loader_concurrent(dir / "conformance/interfaces/sigaction");
    common_loader_concurrent(dir / "conformance/interfaces/sigaddset");
    common_loader_concurrent(dir / "conformance/interfaces/sigaltstack");
    common_loader_concurrent(dir / "conformance/interfaces/sigdelset");
    common_loader_concurrent(dir / "conformance/interfaces/sigemptyset");
    common_loader_concurrent(dir / "conformance/interfaces/sigfillset");
    common_loader_concurrent(dir / "conformance/interfaces/sighold");
    common_loader_concurrent(dir / "conformance/interfaces/sigignore");
    common_loader_concurrent(dir / "conformance/interfaces/sigismember");
    common_loader_concurrent(dir / "conformance/interfaces/signal");
    common_loader_concurrent(dir / "conformance/interfaces/sigpause");
    common_loader_concurrent(dir / "conformance/interfaces/sigpending");
    common_loader_concurrent(dir / "conformance/interfaces/sigprocmask");
    common_loader_concurrent(dir / "conformance/interfaces/sigqueue");
    common_loader_concurrent(dir / "conformance/interfaces/sigrelse");
    common_loader_concurrent(dir / "conformance/interfaces/sigset");
    common_loader_concurrent(dir / "conformance/interfaces/sigsuspend");
    common_loader_concurrent(dir / "conformance/interfaces/sigtimedwait");
    common_loader_concurrent(dir / "conformance/interfaces/sigwait");
    common_loader_concurrent(dir / "conformance/interfaces/sigwaitinfo");
    common_loader_concurrent(dir / "conformance/interfaces/strftime");
    common_loader_concurrent(dir / "conformance/interfaces/time");
    common_loader_concurrent(dir / "conformance/interfaces/timer_create");
    common_loader_concurrent(dir / "conformance/interfaces/timer_create/speculative");
    common_loader_concurrent(dir / "conformance/interfaces/timer_getoverrun");
}

CATCH_TEST_CASE("Gvisor tests", "[Binaries]") {
    // These don't load at all currently
    // std::filesystem::path dir = std::filesystem::path("Binaries") / "binary_tests" / "fex-gvisor-tests-bins";
    // common_loader_concurrent(dir);
}

CATCH_TEST_CASE("Valgrind tests", "[Binaries]") {
    std::filesystem::path dir = std::filesystem::path("Binaries") / "binary_tests" / "valgrind-tests-bins";
    common_loader_concurrent(dir);
}