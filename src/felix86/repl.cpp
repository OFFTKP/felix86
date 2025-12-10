#include <fstream>
#include <sstream>
#include <string>
#include <readline/history.h>
#include <readline/readline.h>
#include <sys/wait.h>
#include "felix86/common/config.hpp"
#include "felix86/common/global.hpp"
#include "felix86/common/info.hpp"
#include "felix86/common/utility.hpp"
#include "felix86/v2/handlers.hpp"
#include "felix86/v2/recompiler.hpp"

extern "C" const char* rv64_print(uint32_t opcode, uintptr_t addr);

FlagMode flag_mode = FlagMode::Default;

void print_help() {
    printf("Commands:\n");
    printf("  <INSTRUCTIONS>       - compile x86 instructions, separated by semicolons, and print the result\n");
    printf("  mode64               - switch to 64-bit mode (default)\n");
    printf("  mode32               - switch to 32-bit mode\n");
    printf("  exit                 - exit this environment\n");
    printf("  flags                - instructions emit necessary flags as normal (default)\n");
    printf("  allflags             - instructions always emit all flags\n");
    printf("  noflags              - instructions never emit flags\n");
}

void __attribute__((noreturn)) exit() {
    printf("Bye :(\n");
    exit(0);
}

void compile(const std::string& input) {
    char pbuffer[PATH_MAX] = "/tmp/.felix86-repl-XXXXXX";
    int fd = mkstemp(pbuffer);
    if (fd == -1) {
        printf("Couldn't make a temporary file\n");
        exit(1);
    }
    close(fd);

    {
        std::ofstream ofs(pbuffer);
        if (g_mode32) {
            ofs << "bits 32\n";
        } else {
            ofs << "bits 64\n";
        }

        std::string edited = input;
        replace_all(edited, ";", "\n");
        ofs << edited;
    }

    const char* argv[] = {
        "nasm", pbuffer, "-fbin", "-o", "/dev/stdout", nullptr,
    };

    // Run the nasm as a separate process, read the output from stdout
    int pipefd[2];
    if (pipe(pipefd) == -1) {
        printf("pipe error\n");
        exit(1);
    }

    std::vector<u8> output;
    pid_t fork_result = fork();
    if (fork_result == 0) {
        close(pipefd[0]);
        dup2(pipefd[1], 1);
        close(pipefd[1]);
        execvp(argv[0], (char* const*)argv);
        printf("execvp error\n");
        exit(1);
    } else {
        close(pipefd[1]);
        int status;
        waitpid(fork_result, &status, 0);
        if (WIFEXITED(status) && WEXITSTATUS(status) != 0) {
            printf("nasm error\n");
            ::remove(pbuffer);
            return;
        }
        u8 temp_buf[4096];
        while (true) {
            ssize_t n = read(pipefd[0], temp_buf, sizeof(temp_buf));
            if (n > 0) {
                output.insert(output.end(), temp_buf, temp_buf + n);
            } else if (n == 0) {
                break;
            } else {
                if (errno == EINTR)
                    continue;
                printf("read error\n");
                exit(1);
            }
        }
        close(pipefd[0]);
    }

    ::remove(pbuffer);

    // Push back an UD2 to trigger recompilation end
    output.push_back(0x0F);
    output.push_back(0x0B);

    std::unique_ptr<Recompiler> rec = std::make_unique<Recompiler>(true);
    rec->setFlagMode(flag_mode);
    auto start = rec->getAssembler().GetCursorPointer();
    rec->compileSequence((u64)output.data());
    auto end = rec->getAssembler().GetCursorPointer();

    // Remove compiled UNDEF instructions off the end, if any
    u16* fin = (u16*)(end - 2);
    while (*fin == 0) {
        end -= 2;
        fin = (u16*)(end - 2);
    }

    for (int i = 0; i < end - start;) {
        void* address = start + i;
        i += 4;
        u32 data = 0;
        memcpy(&data, address, 4);
        const char* out = rv64_print(data, (u64)address);
        printf("%s\n", out);
    }
}

void __attribute__((noreturn)) enter_repl() {
    if (system("which nasm > /dev/null 2>&1")) {
        printf("felix86 REPL needs nasm installed, please install the nasm assembler\n");
        exit(1);
    }

    Config::initialize();
    g_config.quiet = true;
    g_config.inline_syscalls = false;
    g_config.scan_ahead_multi = false;
    Extensions::G = true;
    Extensions::B = true;
    Extensions::C = true;
    Extensions::V = true;
    Extensions::VLEN = 256;
    Extensions::Zicond = true;
    Handlers::initialize();
    using_history();
    std::string version_full = get_version_full();
    printf("%s - try the command `help`\n", version_full.c_str());

    while (true) {
        char* str = readline("> ");
        if (!str) {
            exit();
        }

        std::string line = str;
        if (line == "exit") {
            exit();
        }

        add_history(str);
        std::istringstream iss(line);
        std::string cmd;
        iss >> cmd;

        if (cmd == "mode32") {
            g_mode32 = true;
            printf("Switched to x86-32 mode\n");
        } else if (cmd == "mode64") {
            g_mode32 = false;
            printf("Switched to x86-64 mode\n");
        } else if (cmd == "help") {
            print_help();
        } else if (cmd == "noflags") {
            flag_mode = FlagMode::NeverEmit;
            printf("Switched to never emitting RISC-V instructions for x86 flags\n");
        } else if (cmd == "allflags") {
            flag_mode = FlagMode::AlwaysEmit;
            printf("Switched to always emitting RISC-V instructions for x86 flags\n");
        } else if (cmd == "flags") {
            flag_mode = FlagMode::Default;
            printf("Switched to default flag emitting mode\n");
        } else {
            compile(line.c_str());
        }
    }
}