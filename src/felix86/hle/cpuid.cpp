#include "felix86/common/log.hpp"
#include "felix86/common/x86.hpp"
#include "felix86/frontend/instruction.hpp"
#include "felix86/hle/cpuid.hpp"

const char* manufacturer_id = "GenuineIntel";

// We emulate a Nehalem processor, which is right before AVX was introduced
// http://users.atw.hu/instlatx64/GenuineIntel/GenuineIntel00106A2_Nehalem-EP_CPUID.txt

struct Cpuid {
    u32 leaf;
    u32 subleaf;
    u32 eax;
    u32 ebx;
    u32 ecx;
    u32 edx;
};

constexpr u32 NO_SUBLEAF = 0xFFFFFFFF;

// Generated using generate_cpuid.cpp
constexpr Cpuid mappings[] = {
    {0x00000000, NO_SUBLEAF, 0x0000000B, 0x756E6547, 0x6C65746E, 0x49656E69},
    {0x00000001, NO_SUBLEAF, 0x000106A2, 0x00100800, 0x00BCE3BD, 0xBFEBFBFF},
    {0x00000002, NO_SUBLEAF, 0x55035A01, 0x00F0B2E4, 0x00000000, 0x09CA212C},
    {0x00000003, NO_SUBLEAF, 0x00000000, 0x00000000, 0x00000000, 0x00000000},
    {0x00000004, 0x00000000, 0x1C004121, 0x01C0003F, 0x0000003F, 0x00000000},
    {0x00000004, 0x00000001, 0x1C004122, 0x00C0003F, 0x0000007F, 0x00000000},
    {0x00000004, 0x00000002, 0x1C004143, 0x01C0003F, 0x000001FF, 0x00000000},
    {0x00000004, 0x00000003, 0x1C03C163, 0x03C0003F, 0x00001FFF, 0x00000002},
    {0x00000005, NO_SUBLEAF, 0x00000040, 0x00000040, 0x00000003, 0x00021120},
    {0x00000006, NO_SUBLEAF, 0x00000003, 0x00000002, 0x00000001, 0x00000000},
    {0x00000007, NO_SUBLEAF, 0x00000000, 0x00000000, 0x00000000, 0x00000000},
    {0x00000008, NO_SUBLEAF, 0x00000000, 0x00000000, 0x00000000, 0x00000000},
    {0x00000009, NO_SUBLEAF, 0x00000000, 0x00000000, 0x00000000, 0x00000000},
    {0x0000000a, NO_SUBLEAF, 0x07300403, 0x00000000, 0x00000000, 0x00000603},
    {0x0000000b, 0x00000000, 0x00000001, 0x00000002, 0x00000100, 0x00000000},
    {0x0000000b, 0x00000001, 0x00000004, 0x00000008, 0x00000201, 0x00000000},
    {0x80000000, NO_SUBLEAF, 0x80000008, 0x00000000, 0x00000000, 0x00000000},
    {0x80000001, NO_SUBLEAF, 0x00000000, 0x00000000, 0x00000001, 0x28100000},
    {0x80000002, NO_SUBLEAF, 0x756E6547, 0x20656E69, 0x65746E49, 0x2952286C},
    {0x80000003, NO_SUBLEAF, 0x55504320, 0x20202020, 0x20202020, 0x40202020},
    {0x80000004, NO_SUBLEAF, 0x30303020, 0x20402030, 0x37362E32, 0x007A4847},
    {0x80000005, NO_SUBLEAF, 0x00000000, 0x00000000, 0x00000000, 0x00000000},
    {0x80000006, NO_SUBLEAF, 0x00000000, 0x00000000, 0x01006040, 0x00000000},
    {0x80000007, NO_SUBLEAF, 0x00000000, 0x00000000, 0x00000000, 0x00000100},
    {0x80000008, NO_SUBLEAF, 0x00003028, 0x00000000, 0x00000000, 0x00000000},
};

void felix86_cpuid(ThreadState* thread_state) {
    u32 eax = 0;
    u32 ebx = 0;
    u32 ecx = 0;
    u32 edx = 0;
    u32 leaf = thread_state->GetGpr(X86_REF_RAX);
    u32 subleaf = thread_state->GetGpr(X86_REF_RCX);
    bool found = false;

    for (const Cpuid& cpuid : mappings) {
        if (cpuid.leaf == leaf && (cpuid.subleaf == subleaf || cpuid.subleaf == NO_SUBLEAF)) {
            eax = cpuid.eax;
            ebx = cpuid.ebx;
            ecx = cpuid.ecx;
            edx = cpuid.edx;
            found = true;
            break;
        }
    }

    if (!found) {
        WARN("Unknown CPUID(%08x, %08x)", leaf, subleaf);
    }

    STRACE("CPUID(%08x, %08x) -> %08x %08x %08x %08x", leaf, subleaf, eax, ebx, ecx, edx);
    thread_state->SetGpr(X86_REF_RAX, eax);
    thread_state->SetGpr(X86_REF_RBX, ebx);
    thread_state->SetGpr(X86_REF_RCX, ecx);
    thread_state->SetGpr(X86_REF_RDX, edx);
}