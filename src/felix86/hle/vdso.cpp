#include "felix86/hle/guest_libs/x64-linux-vdso.h"
#include "felix86/hle/vdso.hpp"

std::span<u8> VDSO::getObject64() {
    return std::span<u8>{(u8*)x64_linux_vdso_so_1, x64_linux_vdso_so_1_size};
}