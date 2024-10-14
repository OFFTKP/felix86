#include <fstream>
#include <sys/wait.h>
#include <unistd.h>
#include "felix86/common/print.hpp"
#include "fex_test_loader.hpp"
#include "nlohmann/json.hpp"

FEXTestLoader::FEXTestLoader(const std::filesystem::path& path) {
    std::filesystem::path cpath = std::filesystem::absolute(path);
    if (!std::filesystem::exists(cpath)) {
        ERROR("File does not exist: %s", cpath.string().c_str());
    }

    std::string spath = path.string();
    ssize_t bytes_read;
    buffer.resize(1024 * 1024);

    std::ifstream file(path);
    std::string line;
    bool add_to_json = false;
    while (std::getline(file, line)) {
        if (!add_to_json) {
            if (line == "%ifdef CONFIG") {
                add_to_json = true;
            }
        } else {
            if (line == "%endif") {
                add_to_json = false;
                break;
            }
            json += line + "\n";
        }
    }

    if (json.empty()) {
        ERROR("Failed to find JSON in file: %s", spath.c_str());
    }

    if (add_to_json) {
        ERROR("Failed to find end of JSON in file: %s", spath.c_str());
    }

    const char* argv[] = {
        "nasm", spath.c_str(), "-fbin", "-o", "/dev/stdout", nullptr,
    };

    // Run the nasm as a separate process, read the output from stdout
    int pipefd[2];
    if (pipe(pipefd) == -1) {
        perror("pipe");
        exit(1);
    }

    pid_t fork_result = fork();
    if (fork_result == 0) {
        close(pipefd[0]);
        dup2(pipefd[1], 1);
        close(pipefd[1]);
        execvp(argv[0], (char* const*)argv);
        perror("execvp");
        exit(1);
    } else {
        close(pipefd[1]);
        bytes_read = read(pipefd[0], buffer.data(), buffer.size());
        if (bytes_read == -1) {
            ERROR("Failed to read from pipe");
        }
        close(pipefd[0]);
        int status;
        waitpid(fork_result, &status, 0);
        if (WIFEXITED(status) && WEXITSTATUS(status) != 0) {
            ERROR("nasm failed with exit code: %d", WEXITSTATUS(status));
        }
    }

    // At this point buffer should contain the compiled binary as raw bytes and
    // the json string should contain the json data
    nlohmann::json j = nlohmann::json::parse(json, nullptr, false);
    if (j.empty()) {
        ERROR("Failed to parse JSON");
    }

    if (j.find("RegData") == j.end()) {
        ERROR("JSON missing RegData for file: %s", spath.c_str());
    }

    std::unordered_map<std::string, nlohmann::json> regs;
    regs = j["RegData"].get<std::unordered_map<std::string, nlohmann::json>>();

#define fill(x)                                                                                                                                      \
    if (regs.find(#x) != regs.end()) {                                                                                                               \
        expected_gpr[X86_REF_##x - X86_REF_RAX] = std::stoull(regs[#x].get<std::string>(), nullptr, 16);                                             \
    }
    fill(RAX);
    fill(RCX);
    fill(RDX);
    fill(RBX);
    fill(RSP);
    fill(RBP);
    fill(RSI);
    fill(RDI);
    fill(R8);
    fill(R9);
    fill(R10);
    fill(R11);
    fill(R12);
    fill(R13);
    fill(R14);
    fill(R15);
#undef fill

#define fill(x)                                                                                                                                      \
    if (regs.find(#x) != regs.end()) {                                                                                                               \
        std::vector<std::string> data = regs[#x].get<std::vector<std::string>>();                                                                    \
        XmmReg reg = {};                                                                                                                             \
        reg.data[0] = std::stoull(data[0], nullptr, 16);                                                                                             \
        reg.data[1] = std::stoull(data[1], nullptr, 16);                                                                                             \
        expected_xmm[X86_REF_##x - X86_REF_XMM0] = reg;                                                                                              \
    }
    fill(XMM0);
    fill(XMM1);
    fill(XMM2);
    fill(XMM3);
    fill(XMM4);
    fill(XMM5);
    fill(XMM6);
    fill(XMM7);
    fill(XMM8);
    fill(XMM9);
    fill(XMM10);
    fill(XMM11);
    fill(XMM12);
    fill(XMM13);
    fill(XMM14);
    fill(XMM15);
#undef fill

    // 16 pages at 0xe000'0000
    memory_mappings.push_back({0xE000'0000, 16 * 4096});

    // 2 pages at 0xe800'f000
    memory_mappings.push_back({0xE800'F000, 2 * 4096});

    if (j.find("MemoryRegions") != j.end()) {
        std::unordered_map<std::string, std::string> memory_regions;
        memory_regions = j["MemoryRegions"].get<std::unordered_map<std::string, std::string>>();

        for (auto& [key, value] : memory_regions) {
            u64 address = std::stoull(key, nullptr, 16);
            u64 size = std::stoull(value, nullptr, 16);
            memory_mappings.push_back({address, size});
        }
    }

    TestConfig config = {};
    config.entrypoint = buffer.data();

    emulator = std::make_unique<Emulator>(config);
    state = emulator->GetTestState();
}

void FEXTestLoader::Run() {
    emulator->Run();
    Validate();
}

void FEXTestLoader::Validate() {
    for (size_t i = 0; i < expected_gpr.size(); i++) {
        auto& expected = expected_gpr[i];
        if (expected.has_value()) {
            u64 value = *expected;
            x86_ref_e ref = (x86_ref_e)(X86_REF_RAX + i);
            u64 actual = state->GetGpr(ref);
            if (actual != value) {
                ERROR("%s mismatch: Expected: 0x%016lx, got: 0x%016lx", print_guest_register(ref).c_str(), value, actual);
            }
        }
    }

    for (size_t i = 0; i < expected_xmm.size(); i++) {
        auto& expected = expected_xmm[i];
        if (expected.has_value()) {
            XmmReg value = *expected;
            x86_ref_e ref = (x86_ref_e)(X86_REF_XMM0 + i);
            XmmReg actual = state->GetXmmReg(ref);
            for (int j = 0; j < 2; j++) {
                if (actual.data[j] != value.data[j]) {
                    ERROR("%s mismatch for qword %d: Expected: 0x%016lx, got: 0x%016lx", print_guest_register(ref).c_str(), j, value.data[j],
                          actual.data[j]);
                }
            }
        }
    }

    // In case we go higher in the future
    static_assert(sizeof(XmmReg) == 16, "XmmReg size mismatch");
}

void FEXTestLoader::RunTest(const std::filesystem::path& path) {
    std::string exe_path;
    exe_path.resize(PATH_MAX);
    int res = readlink("/proc/self/exe", exe_path.data(), exe_path.size());
    if (res == -1) {
        perror("readlink");
        exit(1);
    }
    FEXTestLoader loader(std::filesystem::path(exe_path).parent_path() / path);
    loader.Run();
}