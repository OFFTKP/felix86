
#include <filesystem>
#include <catch2/catch_test_macros.hpp>
#include <sys/wait.h>
#include "felix86/common/log.hpp"
#include "fmt/format.h"

void run_test(const std::filesystem::path& felix_path, const std::filesystem::path& path) {
    int pipefd[2];
    if (pipe(pipefd) == -1) {
        perror("pipe");
        exit(1);
    }

    CATCH_INFO(fmt::format("Running test: {}", path.filename().string()));

    std::string buffer(1024 * 1024, 0);
    std::string srootfs = "FELIX86_ROOTFS=" + g_rootfs_path.string();
    std::string spath = path.string();

    const char* argv[] = {
        felix_path.c_str(),
        spath.c_str(),
        nullptr,
    };

    const char* envp[] = {
        srootfs.c_str(),
        "FELIX86_DONT_VALIDATE_EXE_PATH=1",
        nullptr,
    };

    pid_t fork_result = fork();
    if (fork_result == 0) {
        close(pipefd[0]);
        dup2(pipefd[1], 1);
        close(pipefd[1]);
        execvpe(argv[0], (char* const*)argv, (char* const*)envp);
        perror("execvpe");
        exit(1);
    } else {
        close(pipefd[1]);
        int status;
        waitpid(fork_result, &status, 0);
        size_t bytes_read = read(pipefd[0], buffer.data(), buffer.size());
        close(pipefd[0]);

        CATCH_INFO(fmt::format("Output: {}", buffer.substr(0, bytes_read)));
        CATCH_REQUIRE(WEXITSTATUS(status) == 0);
    }

    SUCCESS("Test passed: %s", path.string().c_str());
}

void common_loader(const std::filesystem::path& path) {
    std::filesystem::path exe_path = std::filesystem::canonical("/proc/self/exe");
    std::filesystem::path dir = exe_path.parent_path();
    if (!std::filesystem::exists(dir / "felix86")) {
        ERROR("felix86 executable not found");
    }

    if (g_rootfs_path.empty() || !std::filesystem::exists(g_rootfs_path)) {
        ERROR("This test requires a rootfs directory, set via FELIX86_ROOTFS");
    }

    CATCH_REQUIRE(std::filesystem::is_directory(dir / "Binaries" / path));
    std::filesystem::directory_iterator it(dir / "Binaries" / path);
    for (const auto& entry : it) {
        std::string extension = entry.path().extension().string();
        if (extension == ".out") {
            run_test(dir / "felix86", entry.path().string());
        }
    }
}

CATCH_TEST_CASE("Signals", "[Binaries]") {
    // common_loader("Signals"); // TODO: Fix the sigsegv_simple.c test to not get stuck
}