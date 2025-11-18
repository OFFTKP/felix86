#include <iostream>
#include <sstream>
#include <string>
#include "felix86/common/config.hpp"
#include "felix86/common/global.hpp"
#include "felix86/common/info.hpp"

void print_help() {
    printf("Commands:\n");
    printf("  c <INSTRUCTIONS>     - compile x86 instructions and print the result\n");
    printf("  mode64               - switch to 64-bit mode (default)\n");
    printf("  mode32               - switch to 32-bit mode\n");
    printf("  exit                 - exit this environment\n");
}

void __attribute__((noreturn)) exit() {
    printf("Bye :(\n");
    exit(0);
}

void __attribute__((noreturn)) enter_repl() {
    if (system("which nasm > /dev/null 2>&1")) {
        printf("felix86 REPL needs nasm installed, please install the nasm assembler\n");
        exit(1);
    }

    Config::initialize();
    std::string version_full = get_version_full();
    printf("%s - try the command `help`\n", version_full.c_str());

    while (true) {
        std::cout << "> ";
        std::string line;
        if (!std::getline(std::cin, line)) {
            exit();
        }

        if (line == "exit") {
            exit();
        }

        std::istringstream iss(line);
        std::string cmd;
        iss >> cmd;

        if (cmd == "mode32") {
            g_mode32 = true;
            printf("Switched to x86-32 mode\n");
        } else if (cmd == "mode64") {
            g_mode32 = false;
            printf("Switched to x86-64 mode\n");
        } else if (cmd == "c") {
            std::string assembly;
            std::getline(iss, assembly);
            if (!assembly.empty() && assembly[0] == ' ')
                assembly.erase(0, 1);
        } else {
            printf("Unknown command: %s\n", cmd.c_str());
        }
    }
}