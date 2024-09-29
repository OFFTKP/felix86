#include "felix86/common/log.hpp"
#include "felix86/common/x86.hpp"
#include "felix86/frontend/instruction.hpp"
#include "felix86/hle/cpuid.hpp"

const char* manufacturer_id = "GenuineIntel";

void felix86_cpuid(u64* peax, u64* pebx, u64* pecx, u64* pedx) {
    u32 eax = *peax;
    u32 ebx = 0;
    u32 ecx = *pecx;
    u32 edx = 0;
    VERBOSE("CPUID: %08x", eax);
    switch (eax) {
    case 0: {
        eax = 0x0B; // Nehalem, which doesn't have AVX
        ebx = *(u32*)&manufacturer_id[0];
        edx = *(u32*)&manufacturer_id[4];
        ecx = *(u32*)&manufacturer_id[8];
        break;
    }
    case 1: {
        ERROR("Needs additional info in ebx and eax");
        eax = 0;
        feature_info_ecx_t ecx_info = {0};
        feature_info_edx_t edx_info = {0};
        edx_info.fpu = 1;
        edx_info.tsc = 1;
        edx_info.msr = 1;
        edx_info.pae = 1;
        edx_info.cx8 = 1;
        edx_info.sep = 1;
        edx_info.cmov = 1;
        edx_info.mmx = 1;
        edx_info.fxsr = 1;
        edx_info.sse = 1;
        edx_info.sse2 = 1;

        ecx_info.sse3 = 1;
        ecx_info.pclmulqdq = 1;
        ecx_info.ssse3 = 1;
        ecx_info.fma = 1;
        ecx_info.cmpxchg16b = 1;
        ecx_info.sse4_1 = 1;
        ecx_info.sse4_2 = 1;
        ecx_info.movbe = 1;
        ecx_info.popcnt = 1;

        ecx = *(u32*)&ecx_info;
        edx = *(u32*)&edx_info;
        break;
    }
    case 7: {
        eax = 0;
        ebx = 0;
        ecx = 0;
        edx = 0;
        break;
    }
    case 0x80000000: {
        eax = 0x80000008; // Nehalem highest function
        ebx = 0;          // on AMD they are set to manufacturer ID, but on Intel they are not
        ecx = 0;
        edx = 0;
        break;
    }
    case 0x80000001: {
        feature_info_80000001_ecx_t ecx_info = {0};
        feature_info_80000001_edx_t edx_info = {0};
        edx_info.fpu = 1;
        edx_info.tsc = 1;
        edx_info.msr = 1;
        edx_info.pae = 1;
        edx_info.cx8 = 1;
        edx_info.syscall = 1;
        edx_info.cmov = 1;
        edx_info.mmx = 1;
        edx_info.fxsr = 1;
        edx_info.lm = 1;

        edx = *(u32*)&edx_info;
        ecx = *(u32*)&ecx_info;
        break;
    }
    default: {
        WARN("CPUID %08x not implemented", eax);
        eax = 0;
        ebx = 0;
        ecx = 0;
        edx = 0;
        break;
    }
    }

    *peax = eax;
    *pebx = ebx;
    *pecx = ecx;
    *pedx = edx;
}