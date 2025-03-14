// This fun pattern is found in some DRM stuff :)
// You know who you are...
#include <stdint.h>
#include <stdio.h>
#include <sys/mman.h>
#include <unistd.h>
#include "common.h"

int main() {
    /*
        xor eax, eax            ; 31 c0
        xor ecx, ecx            ; 31 c9
        inc word ptr [rip + 0]  ; 66 ff 05 00 00 00 00
        invalid opcode          ; 0e a1 <--------------- inc turns this into a beautiful cpuid -- inline smc
        dec word ptr [rip - 2]  ; 66 ff 0d fe ff ff ff
        mov eax, ecx            ; 89 c8
        ret                     ; c3
    */
    static unsigned char my_awesome_smc[] = {
        0x31, 0xc0, 0x31, 0xc9, 0x66, 0xff, 0x05, 0x00, 0x00, 0x00, 0x00, 0x0e, 0xa2, 0x66, 0xff, 0x0d, 0xfe, 0xff, 0xff, 0xff, 0x89, 0xc8, 0xc3,
    };

    int (*fun_self_modifying_code_yahoo)() = (int (*)())&my_awesome_smc[0];

    mprotect((void*)((uint64_t)my_awesome_smc & ~0xFFF), 0x1000, PROT_READ | PROT_WRITE | PROT_EXEC);

    // Now run the thing
    int eax = fun_self_modifying_code_yahoo();
    printf("Ret value: %08x\n", eax);

    if (eax == 0x6c65746e) { // 'letn' (byte swapped part of Intel)
        return FELIX86_BTEST_SUCCESS;
    } else if (eax == 0x444D4163) { // the AMD variant
        return FELIX86_BTEST_SUCCESS;
    }

    return 1;
}