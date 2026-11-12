#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include <readline/history.h>
#include <readline/readline.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include "felix86/common/config.hpp"
#include "felix86/common/global.hpp"
#include "felix86/common/info.hpp"
#include "felix86/common/utility.hpp"
#include "felix86/v2/handlers.hpp"
#include "felix86/v2/recompiler.hpp"

extern std::string riscv_disassemble(u32 opcode, u64 addr);
extern void riscv_set_felix86_allocations(bool enable);

static FlagMode flag_mode = FlagMode::Default;

constexpr static int color_count = 6;
const char* colors[color_count] = {
    "\x1b[31m", "\x1b[32m", "\x1b[33m", "\x1b[34m", "\x1b[35m", "\x1b[36m",
};

const char* reset = ANSI_COLOR_RESET;

static bool use_color = true;
static bool mode32 = false;

static void print_help() {
    printf("Commands:\n");
    printf("  <INSTRUCTIONS>       - compile x86 instructions, separated by semicolons, and print the result\n");
    printf("  mode64               - switch to 64-bit mode (default)\n");
    printf("  mode32               - switch to 32-bit mode\n");
    printf("  exit                 - exit this environment\n");
    printf("  flags                - instructions emit necessary flags as normal (default)\n");
    printf("  allflags             - instructions always emit all flags\n");
    printf("  noflags              - instructions never emit flags\n");
    printf("  color                - toggle color coding different instructions\n");
}

static void __attribute__((noreturn)) exit() {
    printf("Bye :(\n");
    exit(0);
}

static void toggle_color() {
    use_color ^= true;
    printf("Color: %s\n", use_color ? "enabled" : "disabled");
}

static void compile(const std::string& input) {
    char pbuffer[PATH_MAX] = "/tmp/.felix86-repl-XXXXXX";
    int fd = mkstemp(pbuffer);
    if (fd == -1) {
        printf("Couldn't make a temporary file\n");
        exit(1);
    }
    close(fd);

    {
        std::ofstream ofs(pbuffer);
        if (mode32) {
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
    const u64 ud2_offset = output.size();
    output.push_back(0x0F);
    output.push_back(0x0B);

    const size_t guest_size = (output.size() + 0xFFF) & ~0xFFFull;
    u8* guest = (u8*)mmap(nullptr, guest_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (guest == MAP_FAILED) {
        printf("mmap error\n");
        return;
    }
    memcpy(guest, output.data(), output.size());

    std::unique_ptr<Recompiler> rec = std::make_unique<Recompiler>(true);
    rec->setFlagMode(flag_mode);
    rec->compileSequence(mode32, (u64)guest, nullptr);

    TaggedAddress guest_address = TaggedAddress::make((u64)guest, mode32);
    BlockMetadata& metadata = rec->getBlockMetadata(guest_address);

    auto print_range = [](u64 start, u64 end, int color_index) {
        for (u64 address = start; address < end; address += 4) {
            u32 data = 0;
            memcpy(&data, (void*)address, 4);
            std::string out = riscv_disassemble(data, address);
            if (use_color && color_index >= 0) {
                printf("%s%s%s\n", colors[color_index % color_count], out.c_str(), reset);
            } else {
                printf("%s\n", out.c_str());
            }
        }
    };

    u64 address = metadata.host_address;
    u64 guest_offset = 0;
    for (size_t i = 0; i < metadata.translation_sizes.size(); i++) {
        const u64 riscv_size = metadata.translation_sizes[i].riscv_instructions_size;
        if (guest_offset < ud2_offset) {
            // Skip the safepoint at the start of the sequence
            const u64 print_start = i == 0 ? address + 4 : address;
            print_range(print_start, address + riscv_size, i);
        }
        address += riscv_size;
        guest_offset += metadata.translation_sizes[i].x86_instruction_size;
    }

    const u64 code_end = (u64)rec->getEndOfCodeCache();
    if (address < code_end) {
        printf("Exits:\n");
        print_range(address, code_end, -1);
    }

    munmap(guest, guest_size);
}

void __attribute__((noreturn)) enter_repl() {
    riscv_set_felix86_allocations(true);

    if (system("which nasm > /dev/null 2>&1")) {
        printf("felix86 REPL needs nasm installed, please install the nasm assembler\n");
        exit(1);
    }

    Config::initialize();
    g_config.quiet = true;
    Extensions::G = true;
    Extensions::Zba = true;
    Extensions::Zbb = true;
    Extensions::Zbs = true;
    Extensions::Zbc = true;
    Extensions::Zvbb = true;
    Extensions::Zvbc = true;
    Extensions::C = true;
    Extensions::V = true;
    Extensions::VLEN = 256;
    Extensions::Zicond = true;
    Extensions::Zvknha = true;
    Extensions::Zicclsm = true;
    Handlers::initialize();
    ThreadState::Create(nullptr);
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
            mode32 = true;
            printf("Switched to x86-32 mode\n");
        } else if (cmd == "mode64") {
            mode32 = false;
            printf("Switched to x86-64 mode\n");
        } else if (cmd == "help") {
            print_help();
        } else if (cmd == "color") {
            toggle_color();
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
