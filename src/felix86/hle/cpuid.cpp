#include "felix86/common/log.hpp"
#include "felix86/common/x86.hpp"
#include "felix86/frontend/instruction.hpp"
#include "felix86/hle/cpuid.hpp"

const char* manufacturer_id = "GenuineIntel";

// We emulate a Nehalem processor, which is right before AVX was introduced
// http://users.atw.hu/instlatx64/GenuineIntel/GenuineIntel00106A2_Nehalem-EP_CPUID.txt
// CPUID 00000000: 0000000B-756E6547-6C65746E-49656E69
// CPUID 00000001: 000106A2-00100800-00BCE3BD-BFEBFBFF
// CPUID 00000002: 55035A01-00F0B2E4-00000000-09CA212C
// CPUID 00000003: 00000000-00000000-00000000-00000000
// CPUID 00000004: 1C004121-01C0003F-0000003F-00000000
// CPUID 00000004: 1C004122-00C0003F-0000007F-00000000
// CPUID 00000004: 1C004143-01C0003F-000001FF-00000000
// CPUID 00000004: 1C03C163-03C0003F-00001FFF-00000002
// CPUID 00000005: 00000040-00000040-00000003-00021120
// CPUID 00000006: 00000003-00000002-00000001-00000000
// CPUID 00000007: 00000000-00000000-00000000-00000000
// CPUID 00000008: 00000000-00000000-00000000-00000000
// CPUID 00000009: 00000000-00000000-00000000-00000000
// CPUID 0000000A: 07300403-00000000-00000000-00000603
// CPUID 0000000B: 00000001-00000002-00000100-00000000
// CPUID 0000000B: 00000004-00000008-00000201-00000000
// CPUID 80000000: 80000008-00000000-00000000-00000000
// CPUID 80000001: 00000000-00000000-00000001-28100000
// CPUID 80000002: 756E6547-20656E69-65746E49-2952286C
// CPUID 80000003: 55504320-20202020-20202020-40202020
// CPUID 80000004: 30303020-20402030-37362E32-007A4847
// CPUID 80000005: 00000000-00000000-00000000-00000000
// CPUID 80000006: 00000000-00000000-01006040-00000000
// CPUID 80000007: 00000000-00000000-00000000-00000100
// CPUID 80000008: 00003028-00000000-00000000-00000000
void felix86_cpuid(ThreadState* thread_state) {
    u32 eax = thread_state->GetGpr(X86_REF_RAX);
    u32 ebx = 0;
    u32 ecx = thread_state->GetGpr(X86_REF_RCX);
    u32 edx = 0;
    VERBOSE("CPUID: %08x %08x", eax, ecx);
    switch (eax) {
    case 0: {
        eax = 0x0B;
        ebx = *(u32*)&manufacturer_id[0];
        edx = *(u32*)&manufacturer_id[4];
        ecx = *(u32*)&manufacturer_id[8];
        break;
    }
    case 1: {
        eax = 0x000106A2;
        ebx = 0x00100800;
        ecx = 0x00BCE3BD;
        edx = 0xBFEBFBFF;
        break;
    }
    case 2: {
        eax = 0x55035A01;
        ebx = 0x00F0B2E4;
        ecx = 0x00000000;
        edx = 0x09CA212C;
        break;
    }
    case 3: {
        eax = 0;
        ebx = 0;
        ecx = 0;
        edx = 0;
        break;
    }
    case 4: {
        if (ecx == 0) {
            eax = 0x1C004121;
            ebx = 0x01C0003F;
            ecx = 0x0000003F;
            edx = 0x00000000;
        } else if (ecx == 1) {
            eax = 0x1C004122;
            ebx = 0x00C0003F;
            ecx = 0x0000007F;
            edx = 0x00000000;
        } else if (ecx == 2) {
            eax = 0x1C004143;
            ebx = 0x01C0003F;
            ecx = 0x000001FF;
            edx = 0x00000000;
        } else if (ecx == 3) {
            eax = 0x1C03C163;
            ebx = 0x03C0003F;
            ecx = 0x00001FFF;
            edx = 0x00000002;
        } else {
            WARN("CPUID 0x04 %08x not implemented", ecx);
            eax = 0;
            ebx = 0;
            ecx = 0;
            edx = 0;
        }
        break;
    }
    case 5: {
        eax = 0x00000040;
        ebx = 0x00000040;
        ecx = 0x00000003;
        edx = 0x00021120;
        break;
    }
    case 6: {
        eax = 3;
        ebx = 2;
        ecx = 1;
        edx = 0;
        break;
    }
    case 7:
    case 8:
    case 9: {
        eax = 0;
        ebx = 0;
        ecx = 0;
        edx = 0;
        break;
    }
    case 0xA: {
        eax = 0x07300403;
        ebx = 0;
        ecx = 0;
        edx = 0x00000603;
        break;
    }
    case 0xB: {
        if (ecx == 0) {
            eax = 0x00000001;
            ebx = 0x00000002;
            ecx = 0x00000100;
            edx = 0;
        } else if (ecx == 1) {
            eax = 0x00000004;
            ebx = 0x00000008;
            ecx = 0x00000201;
            edx = 0;
        } else {
            WARN("CPUID 0x0B %08x not implemented", ecx);
            eax = 0;
            ebx = 0;
            ecx = 0;
            edx = 0;
        }
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
        eax = 0;
        ebx = 0;
        ecx = 0x00000001;
        edx = 0x28100000;
        break;
    }
    case 0x80000002: {
        eax = 0x756E6547;
        ebx = 0x20656E69;
        ecx = 0x65746E49;
        edx = 0x2952286C;
        break;
    }
    case 0x80000003: {
        eax = 0x55504320;
        ebx = 0x20202020;
        ecx = 0x20202020;
        edx = 0x40202020;
        break;
    }
    case 0x80000004: {
        eax = 0x30303020;
        ebx = 0x20402030;
        ecx = 0x37362E32;
        edx = 0x007A4847;
        break;
    }
    case 0x80000005: {
        eax = 0;
        ebx = 0;
        ecx = 0;
        edx = 0;
        break;
    }
    case 0x80000006: {
        eax = 0;
        ebx = 0;
        ecx = 0x01006040;
        edx = 0;
        break;
    }
    case 0x80000007: {
        eax = 0;
        ebx = 0;
        ecx = 0;
        edx = 0x00000100;
        break;
    }
    case 0x80000008: {
        eax = 0x00003028;
        ebx = 0;
        ecx = 0;
        edx = 0;
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

    thread_state->SetGpr(X86_REF_RAX, eax);
    thread_state->SetGpr(X86_REF_RBX, ebx);
    thread_state->SetGpr(X86_REF_RCX, ecx);
    thread_state->SetGpr(X86_REF_RDX, edx);
}