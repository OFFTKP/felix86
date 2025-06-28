
#include <filesystem>
#include <vector>
#include <catch2/catch_test_macros.hpp>
#include <fcntl.h>
#include <spawn.h>
#include <sys/wait.h>
#include "common.h"
#include "felix86/common/log.hpp"
#include "fmt/format.h"

void run_test(const std::filesystem::path& felix_path, const std::filesystem::path& path, bool is_exe) {
    int pipefd[2];
    if (pipe(pipefd) == -1) {
        perror("pipe");
        exit(1);
    }

    const std::filesystem::path tmp_path = "/tmp/felix86_binary_tests";
    const std::filesystem::path exec_path = tmp_path / path.filename();
    const std::string extension = path.extension();

    CATCH_INFO(fmt::format("Running test: {}", path.filename().string()));

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

        CATCH_INFO(fmt::format("Output: {}", buffer.substr(0, bytes_read)));
        CATCH_REQUIRE(WEXITSTATUS(status) == FELIX86_BTEST_SUCCESS);
    }

    SUCCESS("Test passed: %s", path.filename().c_str());
}

void common_loader(const std::filesystem::path& path) {
    std::filesystem::path exe_path = std::filesystem::canonical("/proc/self/exe");
    std::filesystem::path dir = exe_path.parent_path();
    if (!std::filesystem::exists(dir / "felix86")) {
        ERROR("felix86 executable not found");
    }

    if (g_config.rootfs_path.empty() || !std::filesystem::exists(g_config.rootfs_path)) {
        ERROR("This test requires a rootfs directory, set via FELIX86_ROOTFS");
    }

    CATCH_REQUIRE(std::filesystem::is_directory(dir / "Binaries" / path));
    std::filesystem::directory_iterator it(dir / "Binaries" / path);
    for (const auto& entry : it) {
        std::string extension = entry.path().extension().string();
        if (extension == ".out" || extension == ".exe") {
            run_test(dir / "felix86", entry.path().string(), extension == ".exe");
        }
    }
}

CATCH_TEST_CASE("Signals", "[Signals]") {
    common_loader("Signals");
}

CATCH_TEST_CASE("Simple", "[Simple]") {
    common_loader("Simple");
}

CATCH_TEST_CASE("Clone", "[Clone]") {
    common_loader("Clone");
}

CATCH_TEST_CASE("SMC", "[SMC]") {
    // common_loader("SMC"); -- we don't handle smc rn
}

CATCH_TEST_CASE("Filesystem", "[Filesystem]") {
    common_loader("Filesystem");
}
