#include <cstdio>
#include <string>
#include <fmt/format.h>

int main() {
    std::string assembly;
    assembly += "bits 64\n";
    assembly += "global my_xmm_func\n";
    assembly += "my_xmm_func:";
    for (int i = 0; i < 256; i++) {
        uint8_t reg = rand() & 0xF;
        assembly += fmt::format("pshufd xmm{}, [rel .random_data + {}], {}\n", reg, i * 16, rand() & 0xFF);
        assembly += fmt::format("movdqa [rdi + {}], xmm{}\n", i * 16, reg);
    }

    assembly += "ret\n";

    // align to 16 bytes
    assembly += "align 16\n";
    assembly += ".random_data:";
    for (int i = 0; i < 16; i++) {
        assembly += "db ";
        for (int j = 0; j < 16; j++) {
            assembly += fmt::format("0x{:02x}", rand() & 0xFF);
            if (j != 15) {
                assembly += ", ";
            }
        }
        assembly += "\n";
    }

    printf("%s\n", assembly.c_str());
}