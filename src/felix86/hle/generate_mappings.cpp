#include <algorithm>
#include <set>
#include <string>
#include <vector>

// Generates syscalls_common.inc

std::vector<std::string> x86 = {
#define X(name, ...) #name,
#include "syscalls_x86_64.inc"
#undef X
};

std::vector<std::string> riscv64 = {
#define X(name, ...) #name,
#include "syscalls_riscv64.inc"
#undef X
};

int main() {
    // Generate X Macro for the similarities
    std::set<std::string> common;
    for (const auto& x : x86) {
        if (std::find(riscv64.begin(), riscv64.end(), x) != riscv64.end()) {
            common.insert(x);
        }
    }

    for (const auto& x : common) {
        printf("X(%s)\n", x.c_str());
    }
}