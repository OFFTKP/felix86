#include "felix86/common/log.hpp"
#include "felix86/common/state.hpp"
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
    // {0x00000000, NO_SUBLEAF, 0x0000000A, 0x756E6547, 0x6C65746E, 0x49656E69},
    // {0x00000001, NO_SUBLEAF, 0x00010676, 0x00040800, 0x000CE3BD, 0xBFEBFBFF},
    // {0x00000002, NO_SUBLEAF, 0x05B0B101, 0x005657F0, 0x00000000, 0x2CB4304E},
    // {0x00000003, NO_SUBLEAF, 0x00000000, 0x00000000, 0x00000000, 0x00000000},
    // {0x00000004, 0x00000000, 0x0C000121, 0x01C0003F, 0x0000003F, 0x00000001},
    // {0x00000004, 0x00000001, 0x0C000122, 0x01C0003F, 0x0000003F, 0x00000001},
    // {0x00000004, 0x00000002, 0x0C004143, 0x05C0003F, 0x00000FFF, 0x00000001},
    // {0x00000005, NO_SUBLEAF, 0x00000040, 0x00000040, 0x00000003, 0x00002220},
    // {0x00000006, NO_SUBLEAF, 0x00000001, 0x00000002, 0x00000001, 0x00000000},
    // {0x00000007, NO_SUBLEAF, 0x00000000, 0x00000000, 0x00000000, 0x00000000},
    // {0x00000008, NO_SUBLEAF, 0x00000400, 0x00000000, 0x00000000, 0x00000000},
    // {0x00000009, NO_SUBLEAF, 0x00000000, 0x00000000, 0x00000000, 0x00000000},
    // {0x0000000a, NO_SUBLEAF, 0x07280202, 0x00000000, 0x00000000, 0x00000503},
    // {0x80000000, NO_SUBLEAF, 0x80000008, 0x00000000, 0x00000000, 0x00000000},
    // {0x80000001, NO_SUBLEAF, 0x00000000, 0x00000000, 0x00000001, 0x20100000},
    // {0x80000002, NO_SUBLEAF, 0x65746E49, 0x2952286C, 0x6F655820, 0x2952286E},
    // {0x80000003, NO_SUBLEAF, 0x55504320, 0x20202020, 0x20202020, 0x45202020},
    // {0x80000004, NO_SUBLEAF, 0x32363435, 0x20402020, 0x30382E32, 0x007A4847},
    // {0x80000005, NO_SUBLEAF, 0x00000000, 0x00000000, 0x00000000, 0x00000000},
    // {0x80000006, NO_SUBLEAF, 0x00000000, 0x00000000, 0x18008040, 0x00000000},
    // {0x80000007, NO_SUBLEAF, 0x00000000, 0x00000000, 0x00000000, 0x00000000},
    // {0x80000008, NO_SUBLEAF, 0x00003026, 0x00000000, 0x00000000, 0x00000000},

    // Pentium 4 CPU
    {0x00000000, NO_SUBLEAF, 0x00000002, 0x756E6547, 0x6C65746E, 0x49656E69},
    {0x00000001, NO_SUBLEAF, 0x00000F29, 0x00020809, 0x00004400, 0xBFEBFBFF},
    {0x00000002, NO_SUBLEAF, 0x665B5001, 0x00000000, 0x00000000, 0x007B7040},
    {0x80000000, NO_SUBLEAF, 0x80000004, 0x00000000, 0x00000000, 0x00000000},
    {0x80000001, NO_SUBLEAF, 0x00000000, 0x00000000, 0x00000000, 0x00000000},
    {0x80000002, NO_SUBLEAF, 0x20202020, 0x20202020, 0x20202020, 0x6E492020},
    {0x80000003, NO_SUBLEAF, 0x286C6574, 0x50202952, 0x69746E65, 0x52286D75},
    {0x80000004, NO_SUBLEAF, 0x20342029, 0x20555043, 0x30382E32, 0x007A4847},
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

    // We can't unset mmx bits for interpreter cpuid calls as those are needed to even get the program started
    if (thread_state->rip >= g_interpreter_end || thread_state->rip < g_interpreter_start) {
        u64 mmxbits = 0b11 << 22;
        if (leaf == 1) {
            // Unset the MMX bits for now, SDL chooses MMX paths when it's present
            edx &= ~mmxbits;
        }
    }

    STRACE("CPUID(%08x, %08x) -> %08x %08x %08x %08x", leaf, subleaf, eax, ebx, ecx, edx);
    thread_state->SetGpr(X86_REF_RAX, eax);
    thread_state->SetGpr(X86_REF_RBX, ebx);
    thread_state->SetGpr(X86_REF_RCX, ecx);
    thread_state->SetGpr(X86_REF_RDX, edx);
}