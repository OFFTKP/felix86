#include <fstream>
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>
#include "catch2/catch_message.hpp"
#include "catch2/catch_test_macros.hpp"
#include "felix86/common/print.hpp"
#include "fex_test_loader.hpp"
#include "fmt/format.h"
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
        int status;
        waitpid(fork_result, &status, 0);
        if (WIFEXITED(status) && WEXITSTATUS(status) != 0) {
            ERROR("nasm failed with exit code: %d -- Do you have nasm installed? It is required for these tests", WEXITSTATUS(status));
        }
        bytes_read = read(pipefd[0], buffer.data(), buffer.size());
        if (bytes_read == -1) {
            ERROR("Failed to read from pipe");
        }
        close(pipefd[0]);
    }

    // At this point buffer should contain the compiled binary as raw bytes and
    // the json string should contain the json data
    nlohmann::json j = nlohmann::json::parse(json, nullptr, false);
    if (j.is_discarded()) {
        ERROR("Failed to parse JSON");
    }

    if (j.find("RegData") != j.end()) {
        std::unordered_map<std::string, nlohmann::json> regs;
        regs = j["RegData"].get<std::unordered_map<std::string, nlohmann::json>>();

#define fill(x)                                                                                                                                      \
    if (regs.find(#x) != regs.end()) {                                                                                                               \
        int base = 10;                                                                                                                               \
        std::string str = regs[#x].get<std::string>();                                                                                               \
        if (str.size() > 2 && str[0] == '0' && str[1] == 'x')                                                                                        \
            base = 16;                                                                                                                               \
        expected_gpr[X86_REF_##x - X86_REF_RAX] = std::stoull(regs[#x].get<std::string>(), nullptr, base);                                           \
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

#define fill(x)                                                                                                                                      \
    if (regs.find(#x) != regs.end()) {                                                                                                               \
        int base = 10;                                                                                                                               \
        std::string str = regs[#x].get<std::string>();                                                                                               \
        if (str.size() > 2 && str[0] == '0' && str[1] == 'x')                                                                                        \
            base = 16;                                                                                                                               \
        expected_mm[X86_REF_##x - X86_REF_MM0] = std::stoull(regs[#x].get<std::string>(), nullptr, base);                                            \
    }
        fill(MM0);
        fill(MM1);
        fill(MM2);
        fill(MM3);
        fill(MM4);
        fill(MM5);
        fill(MM6);
        fill(MM7);
#undef fill
    }

    bool is_mode32 = false;
    if (j.find("Mode") != j.end()) {
        ASSERT(j["Mode"] == "32BIT");
        is_mode32 = true;
    }

    // 16 pages at 0xe000'0000
    memory_mappings.push_back({0xE000'0000, 16 * 4096});

    // 2 pages at 0xe800'f000
    memory_mappings.push_back({0xE800'F000, 2 * 4096});

    // 1 page at 0xC000'0000 for stack
    // According to the example assembly this is configurable but haven't found a test that configures it
    memory_mappings.push_back({0xC000'0000, 4096});

    if (j.find("MemoryRegions") != j.end()) {
        std::unordered_map<std::string, std::string> memory_regions;
        memory_regions = j["MemoryRegions"].get<std::unordered_map<std::string, std::string>>();

        for (auto& [key, value] : memory_regions) {
            u64 address = std::stoull(key, nullptr, 16);
            u64 size = std::stoull(value, nullptr, 16);
            memory_mappings.push_back({address, size});
        }
    }

    void* address = mmap((void*)0x10'000, 0x10'000, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);
    if (address != (void*)0x10'000) {
        perror("mmap");
        exit(1);
    }

    memcpy((void*)0x10'000, buffer.data(), bytes_read);

    config.entrypoint = 0x10'000;
    config.mode32 = is_mode32;
}

FEXTestLoader::~FEXTestLoader() {
    for (auto& ptr : munmap_me) {
        munmap(ptr.first, ptr.second);
    }

    ThreadState* state = ThreadState::Get();
    ASSERT(state);
    ThreadState::Destroy(state);
    pthread_setspecific(g_thread_state_key, nullptr);
}

void FEXTestLoader::Run() {
    for (auto& [address, size] : memory_mappings) {
        auto stuff = mmap((void*)address, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);
        if (stuff == MAP_FAILED) {
            WARN("mmap failed for memory mapping %lx with size %lx during test -- errno: %s", address, size, strerror(errno));
        }
        munmap_me.push_back({stuff, size});
    }
    u64 stack = 0xC000'0000 + 4096;
    Emulator::StartTest(config, stack);
    Validate();
}

void FEXTestLoader::Validate() {
    ThreadState* state = ThreadState::Get();
    for (size_t i = 0; i < expected_gpr.size(); i++) {
        auto& pexpected = expected_gpr[i];
        if (pexpected.has_value()) {
            u64 expected = *pexpected;
            x86_ref_e ref = (x86_ref_e)(X86_REF_RAX + i);
            u64 actual = state->GetGpr(ref);
            CATCH_INFO(fmt::format("Checking {}", print_guest_register(ref)));
            CATCH_REQUIRE(expected == actual);
        }
    }

    for (size_t i = 0; i < expected_xmm.size(); i++) {
        auto& pexpected = expected_xmm[i];
        if (pexpected.has_value()) {
            XmmReg expected = *pexpected;
            x86_ref_e ref = (x86_ref_e)(X86_REF_XMM0 + i);
            XmmReg actual = state->GetXmm(ref);
            for (int j = 0; j < 2; j++) {
                CATCH_INFO(fmt::format("Checking XMM{}[{}]", i, j));
                CATCH_REQUIRE(expected.data[j] == actual.data[j]);
            }
        }
    }

    for (size_t i = 0; i < expected_mm.size(); i++) {
        auto& pexpected = expected_mm[i];
        if (pexpected.has_value()) {
            u64 expected = *pexpected;
            x86_ref_e ref = (x86_ref_e)(X86_REF_MM0 + i);
            u64 actual = state->GetMm(ref);
            CATCH_INFO(fmt::format("Checking {}", print_guest_register(ref)));
            CATCH_REQUIRE(expected == actual);
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

    SUCCESS("Test passed: %s", path.string().c_str());
}