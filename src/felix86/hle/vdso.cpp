#include "felix86/hle/vdso.hpp"

extern "C" {
    extern unsigned char x64_linux_vdso_so_1[];
    extern unsigned int x64_linux_vdso_so_1_len;
}

std::span<u8> VDSO::getObject64() {
    return std::span<u8>{x64_linux_vdso_so_1, x64_linux_vdso_so_1_len};
}