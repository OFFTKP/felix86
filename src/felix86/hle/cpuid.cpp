#include <array>
#include <span>
#include "felix86/common/config.hpp"
#include "felix86/common/log.hpp"
#include "felix86/common/state.hpp"
#include "felix86/hle/cpuid.hpp"

const char* manufacturer_id = "GenuineIntel";

constexpr u32 NO_SUBLEAF = 0xFFFFFFFF;

// Generated using generate_cpuid.cpp
[[maybe_unused]] constexpr std::array p4_mappings = {
    // Pentium 4 CPU
    (Cpuid){0x00000000, NO_SUBLEAF, 0x00000002, 0x756E6547, 0x6C65746E, 0x49656E69},
    (Cpuid){0x00000001, NO_SUBLEAF, 0x00000F29, 0x00020809, 0x00004400, 0xBFEBFBFF},
    (Cpuid){0x00000002, NO_SUBLEAF, 0x665B5001, 0x00000000, 0x00000000, 0x007B7040},
    (Cpuid){0x80000000, NO_SUBLEAF, 0x80000004, 0x00000000, 0x00000000, 0x00000000},
    (Cpuid){0x80000001, NO_SUBLEAF, 0x00000000, 0x00000000, 0x00000000, 0x00000000},
    (Cpuid){0x80000002, NO_SUBLEAF, 0x20202020, 0x20202020, 0x20202020, 0x6E492020},
    (Cpuid){0x80000003, NO_SUBLEAF, 0x286C6574, 0x50202952, 0x69746E65, 0x52286D75},
    (Cpuid){0x80000004, NO_SUBLEAF, 0x20342029, 0x20555043, 0x30382E32, 0x007A4847},
};

[[maybe_unused]] constexpr std::array p4_mappings_sse3 = {
    (Cpuid){0x00000000, NO_SUBLEAF, 0x00000005, 0x756E6547, 0x6C65746E, 0x49656E69},
    (Cpuid){0x00000001, NO_SUBLEAF, 0x00000F41, 0x00010800, 0x0000651D, 0xBFEBFBFF},
    (Cpuid){0x00000002, NO_SUBLEAF, 0x605B5101, 0x00000000, 0x00000000, 0x003C7040},
    (Cpuid){0x00000003, NO_SUBLEAF, 0x00000000, 0x00000000, 0x00000000, 0x00000000},
    (Cpuid){0x00000004, 0x00000000, 0x00000121, 0x01C0003F, 0x0000001F, 0x00000000},
    (Cpuid){0x00000004, 0x00000001, 0x00000143, 0x00C0103F, 0x000001FF, 0x00000000},
    (Cpuid){0x00000005, NO_SUBLEAF, 0x00000040, 0x00000040, 0x00000000, 0x00000000},
    (Cpuid){0x80000000, NO_SUBLEAF, 0x80000008, 0x00000000, 0x00000000, 0x00000000},
    (Cpuid){0x80000001, NO_SUBLEAF, 0x00000000, 0x00000000, 0x00000000, 0x20100000},
    (Cpuid){0x80000002, NO_SUBLEAF, 0x20202020, 0x20202020, 0x20202020, 0x20202020},
    (Cpuid){0x80000003, NO_SUBLEAF, 0x65746E49, 0x2952286C, 0x6C654320, 0x6E6F7265},
    (Cpuid){0x80000004, NO_SUBLEAF, 0x20295228, 0x20555043, 0x33352E32, 0x007A4847},
    (Cpuid){0x80000005, NO_SUBLEAF, 0x00000000, 0x00000000, 0x00000000, 0x00000000},
    (Cpuid){0x80000006, NO_SUBLEAF, 0x00000000, 0x00000000, 0x01004040, 0x00000000},
    (Cpuid){0x80000007, NO_SUBLEAF, 0x00000000, 0x00000000, 0x00000000, 0x00000000},
    (Cpuid){0x80000008, NO_SUBLEAF, 0x00003024, 0x00000000, 0x00000000, 0x00000000},
};

[[maybe_unused]] constexpr std::array p4_mappings_32 = {
    // Pentium 4 CPU, no x86-64
    (Cpuid){0x00000000, NO_SUBLEAF, 0x00000002, 0x756E6547, 0x6C65746E, 0x49656E69},
    (Cpuid){0x00000001, NO_SUBLEAF, 0x00000F29, 0x00020809, 0x00004400, 0xBFEBFBFF},
    (Cpuid){0x00000002, NO_SUBLEAF, 0x665B5001, 0x00000000, 0x00000000, 0x007B7040},
    (Cpuid){0x80000000, NO_SUBLEAF, 0x80000004, 0x00000000, 0x00000000, 0x00000000},
    (Cpuid){0x80000001, NO_SUBLEAF, 0x00000000, 0x00000000, 0x00000000, 0x00000000},
    (Cpuid){0x80000002, NO_SUBLEAF, 0x20202020, 0x20202020, 0x20202020, 0x6E492020},
    (Cpuid){0x80000003, NO_SUBLEAF, 0x286C6574, 0x50202952, 0x69746E65, 0x52286D75},
    (Cpuid){0x80000004, NO_SUBLEAF, 0x20342029, 0x20555043, 0x30382E32, 0x007A4847},
};

[[maybe_unused]] constexpr std::array nehalem_mappings = {
    // http://users.atw.hu/instlatx64/GenuineIntel/GenuineIntel00106A2_Nehalem-EP_CPUID.txt
    (Cpuid){0x00000000, NO_SUBLEAF, 0x0000000A, 0x756E6547, 0x6C65746E, 0x49656E69},
    (Cpuid){0x00000001, NO_SUBLEAF, 0x00010676, 0x00040800, 0x000CE3BD, 0xBFEBFBFF},
    (Cpuid){0x00000002, NO_SUBLEAF, 0x05B0B101, 0x005657F0, 0x00000000, 0x2CB4304E},
    (Cpuid){0x00000003, NO_SUBLEAF, 0x00000000, 0x00000000, 0x00000000, 0x00000000},
    (Cpuid){0x00000004, 0x00000000, 0x0C000121, 0x01C0003F, 0x0000003F, 0x00000001},
    (Cpuid){0x00000004, 0x00000001, 0x0C000122, 0x01C0003F, 0x0000003F, 0x00000001},
    (Cpuid){0x00000004, 0x00000002, 0x0C004143, 0x05C0003F, 0x00000FFF, 0x00000001},
    (Cpuid){0x00000005, NO_SUBLEAF, 0x00000040, 0x00000040, 0x00000003, 0x00002220},
    (Cpuid){0x00000006, NO_SUBLEAF, 0x00000001, 0x00000002, 0x00000001, 0x00000000},
    (Cpuid){0x00000007, NO_SUBLEAF, 0x00000000, 0x00000000, 0x00000000, 0x00000000},
    (Cpuid){0x00000008, NO_SUBLEAF, 0x00000400, 0x00000000, 0x00000000, 0x00000000},
    (Cpuid){0x00000009, NO_SUBLEAF, 0x00000000, 0x00000000, 0x00000000, 0x00000000},
    (Cpuid){0x0000000a, NO_SUBLEAF, 0x07280202, 0x00000000, 0x00000000, 0x00000503},
    (Cpuid){0x80000000, NO_SUBLEAF, 0x80000008, 0x00000000, 0x00000000, 0x00000000},
    (Cpuid){0x80000001, NO_SUBLEAF, 0x00000000, 0x00000000, 0x00000001, 0x20100000},
    (Cpuid){0x80000002, NO_SUBLEAF, 0x65746E49, 0x2952286C, 0x6F655820, 0x2952286E},
    (Cpuid){0x80000003, NO_SUBLEAF, 0x55504320, 0x20202020, 0x20202020, 0x45202020},
    (Cpuid){0x80000004, NO_SUBLEAF, 0x32363435, 0x20402020, 0x30382E32, 0x007A4847},
    (Cpuid){0x80000005, NO_SUBLEAF, 0x00000000, 0x00000000, 0x00000000, 0x00000000},
    (Cpuid){0x80000006, NO_SUBLEAF, 0x00000000, 0x00000000, 0x18008040, 0x00000000},
    (Cpuid){0x80000007, NO_SUBLEAF, 0x00000000, 0x00000000, 0x00000000, 0x00000000},
    (Cpuid){0x80000008, NO_SUBLEAF, 0x00003026, 0x00000000, 0x00000000, 0x00000000},
};

std::span<const Cpuid> selected_mappings = nehalem_mappings;
std::span<const Cpuid> selected_mappings_32 = p4_mappings_sse3;

Cpuid felix86_cpuid_impl(u32 leaf, u32 subleaf) {
    Cpuid result{};
    bool found = false;

    auto& mappings = g_mode32 ? selected_mappings_32 : selected_mappings;
    for (const Cpuid& cpuid : mappings) {
        if (cpuid.leaf == leaf && (cpuid.subleaf == subleaf || cpuid.subleaf == NO_SUBLEAF)) {
            result = cpuid;
            found = true;
            break;
        }
    }

    if (found && leaf == 0x00000001) {
        if (g_config.no_sse2) {
            result.edx &= ~(1 << 26);
        }
        if (g_config.no_sse3) {
            result.ecx &= ~(1 << 0);
        }
        if (g_config.no_ssse3) {
            result.ecx &= ~(1 << 9);
        }
        if (g_config.no_sse4_1) {
            result.ecx &= ~(1 << 19);
        }
        if (g_config.no_sse4_2 || !Extensions::B /* CRC32 needs Zbc */) {
            result.ecx &= ~(1 << 20);
        }
        if (!Extensions::B) {
            result.ecx &= ~(1 << 1); // disable PCLMULQDQ
        } else if (g_config.pclmulqdq) {
            result.ecx |= 1 << 1;
        }
    }

    if (found && leaf == 0x8000'0001) {
        result.ecx |= (1 << 5); // lzcnt/popcnt support
    }

    if (!found) {
        WARN("Unknown CPUID(%08x, %08x)", leaf, subleaf);
    }

    return result;
}

void felix86_cpuid(ThreadState* thread_state) {
    u32 leaf = thread_state->GetGpr(X86_REF_RAX);
    u32 subleaf = thread_state->GetGpr(X86_REF_RCX);

    Cpuid cpuid = felix86_cpuid_impl(leaf, subleaf);

    STRACE("CPUID(%08x, %08x) -> %08x %08x %08x %08x", leaf, subleaf, cpuid.eax, cpuid.ebx, cpuid.ecx, cpuid.edx);
    thread_state->SetGpr(X86_REF_RAX, cpuid.eax);
    thread_state->SetGpr(X86_REF_RBX, cpuid.ebx);
    thread_state->SetGpr(X86_REF_RCX, cpuid.ecx);
    thread_state->SetGpr(X86_REF_RDX, cpuid.edx);
}