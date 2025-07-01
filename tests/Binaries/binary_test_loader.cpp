
#include <filesystem>
#include <optional>
#include <thread>
#include <vector>
#include <catch2/catch_test_macros.hpp>
#include <fcntl.h>
#include <spawn.h>
#include <sys/wait.h>
#include "common.h"
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
    std::filesystem::path exe_path = std::filesystem::canonical("/proc/self/exe");
    std::filesystem::path dir = exe_path.parent_path();
    if (!std::filesystem::exists(dir / "felix86")) {
        CATCH_FAIL("felix86 executable not found in the current directory");
    }

    if (g_config.rootfs_path.empty() || !std::filesystem::exists(g_config.rootfs_path)) {
        CATCH_FAIL("This test requires a rootfs directory, set via FELIX86_ROOTFS");
    }

    std::filesystem::path dir_i386 = dir / "Binaries" / "binary_tests" / "fex-gcc-target-tests-bins" / "32";
    if (!std::filesystem::is_directory(dir_i386)) {
        CATCH_FAIL("These tests need you to clone the submodules: `git submodule update --init`");
    }

    int thread_count = std::thread::hardware_concurrency();
    if (thread_count <= 0) {
        thread_count = 2;
    }

    std::vector<JobThreadData> threads;
    threads.resize(thread_count);

    size_t index = 0;
    std::vector<std::filesystem::path> tests;
    std::filesystem::directory_iterator it_i386(dir_i386);
    for (const auto& entry : it_i386) {
        tests.push_back(entry.path());
    }

    std::filesystem::path dir_x64 = dir / "Binaries" / "binary_tests" / "fex-gcc-target-tests-bins" / "64";
    if (!std::filesystem::is_directory(dir_x64)) {
        CATCH_FAIL("64-bit dir not found?");
    }

    std::filesystem::directory_iterator it_x64(dir_x64);
    for (const auto& entry : it_x64) {
        tests.push_back(entry.path());
    }

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