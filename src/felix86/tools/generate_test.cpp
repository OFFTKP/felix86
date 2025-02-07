#include <cstdio>
#include <fstream>
#include <string>
#include <fmt/format.h>
#include <sys/stat.h>

int main() {
    std::string inst = "maxps";
    std::string inst1 = "maxpd";

    std::string assembly;
    assembly += "bits 64\n";
    assembly += "global my_xmm_func\n";
    assembly += "my_xmm_func:";
    for (int i = 0; i < 256; i++) {
        uint8_t reg = rand() & 0xF;
        uint8_t reg2 = rand() & 0xF;
        uint8_t reg3 = rand() & 0xF;
        uint8_t reg4 = rand() & 0xF;
        assembly += fmt::format("movdqa xmm{}, [rdi + {}]\n", reg, i * 16);
        assembly += fmt::format("movdqa xmm{}, [rdi + {}]\n", reg2, i * 16 + 16);
        assembly += fmt::format("movdqa xmm{}, [rdi + {}]\n", reg3, i * 16 + 32);
        assembly += fmt::format("movdqa xmm{}, [rdi + {}]\n", reg4, i * 16 + 48);
        // assembly += fmt::format("{} xmm{}, [rel .random_data + {}], {}\n", inst, reg, i * 16, rand() & 0xFF);
        // assembly += fmt::format("{} xmm{}, {}\n", inst, reg, rand() & 0xFF);
        assembly += fmt::format("{} xmm{}, xmm{}\n", inst, reg, reg2);
        assembly += fmt::format("{} xmm{}, xmm{}\n", inst1, reg2, reg3);
        assembly += fmt::format("{} xmm{}, xmm{}\n", inst, reg3, reg4);
        assembly += fmt::format("{} xmm{}, xmm{}\n", inst1, reg, reg4);
        // assembly += fmt::format("{} xmm{}, xmm{}, {}\n", inst, reg, reg2, rand() & 15);
        assembly += fmt::format("movdqa [rdi + {}], xmm{}\n", i * 16, reg);
        assembly += fmt::format("movdqa [rdi + {}], xmm{}\n", i * 16 + 16, reg2);
        assembly += fmt::format("movdqa [rdi + {}], xmm{}\n", i * 16 + 32, reg3);
        assembly += fmt::format("movdqa [rdi + {}], xmm{}\n", i * 16 + 48, reg4);
    }

    assembly += "lea rax, [rel .random_data]\n";

    assembly += "ret\n";

    // align to 16 bytes
    assembly += "align 16\n";
    assembly += ".random_data:\n";
    for (int i = 0; i < 256; i++) {
        assembly += fmt::format("db ");
        for (int j = 0; j < 16; j++) {
            assembly += fmt::format("0x{:02x}", rand() & 0xFF);
            if (j != 15) {
                assembly += ", ";
            }
        }
        assembly += "\n";
    }

    std::string c_program = R"(
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

uint64_t my_xmm_func(uint8_t* data);

void fill_with_junk(uint8_t* data, size_t size) {
    for (size_t i = 0; i < size; i++) {
        data[i] = (i & 0xFF) + rand();
    }
}

#define POLY 0x82f63b78
uint32_t crc32c(uint32_t crc, const unsigned char *buf, size_t len)
{
    int k;

    crc = ~crc;
    while (len--) {
        crc ^= *buf++;
        for (k = 0; k < 8; k++)
            crc = crc & 1 ? (crc >> 1) ^ POLY : crc >> 1;
    }
    return ~crc;
}

#define XSIZE 800

int main() {
    uint8_t data[XSIZE * 16];
    fill_with_junk(data, sizeof(data));
    uint8_t data_old_copy[XSIZE * 16];
    memcpy(data_old_copy, data, sizeof(data));
    uint8_t* rodata = (uint8_t*)my_xmm_func(data);
    for (int i = 0; i < 256; i++) {
        printf("data[%d]: ", i);

        for (int j = 0; j < 16; j++) {
            printf("%02x ", data_old_copy[i * 16 + j]);
        }

        printf(" WITH ");

        for (int j = 0; j < 16; j++) {
            printf("%02x ", rodata[i * 16 + j]);
        }

        printf(" EQUALS ");

        for (int j = 0; j < 16; j++) {
            printf("%02x ", data[i * 16 + j]);
        }
        printf("\n");
    }

    int crc = crc32c(0, data, sizeof(data));
    printf("crc32c: 0x%08x\n", crc);

    return 0;
}
)";

    mkdir("build", 0755);

    std::ofstream file("build/xmm_func.asm");
    file << assembly;
    file.close();

    file.open("build/main.c");
    file << c_program;
    file.close();

    std::string nasm_command = "nasm -felf64 build/xmm_func.asm -o build/xmm_func.o";
    std::string gcc_command = "gcc -o build/main build/main.c build/xmm_func.o";

    system(nasm_command.c_str());
    system(gcc_command.c_str());

    return 0;
}