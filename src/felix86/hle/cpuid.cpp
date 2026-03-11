#include <array>
#include <span>
#include <fcntl.h>
#include "felix86/common/config.hpp"
#include "felix86/common/feature.hpp"
#include "felix86/common/global.hpp"
#include "felix86/common/log.hpp"
#include "felix86/common/state.hpp"
#include "felix86/common/xsave.hpp"
#include "felix86/hle/cpuid.hpp"

constexpr u32 NO_SUBLEAF = 0xFFFFFFFF;

static inline void bit_set(u32& data, int position, bool value) {
    data &= ~(1u << position);
    data |= value << position;
}

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
bool cpu_name_tried = false;
bool cpu_name_set = false;
char cpu_name[48];

const char* get_version_full();

Cpuid felix86_cpuid_impl(u32 leaf, u32 subleaf) {
    // Try getting the CPU name
    if (!cpu_name_tried) {
        cpu_name_tried = true;
        if (!g_config.cpu_name.empty() && g_config.cpu_name.size() < 48) {
            snprintf(cpu_name, sizeof(cpu_name), "%s", g_config.cpu_name.c_str());
            cpu_name_set = true;
        } else {
            int fd = open("/proc/device-tree/cpus/cpu@0/model", O_RDONLY);
            if (fd != -1) {
                char buffer[48];
                int bytes_read = read(fd, buffer, sizeof(buffer) - 1);
                if (bytes_read != -1) {
                    buffer[bytes_read] = 0;
                    std::string version = get_version_full();
                    snprintf(cpu_name, sizeof(cpu_name), "%s on %s", version.c_str(), buffer);
                    cpu_name_set = true;
                }
                ASSERT(close(fd) == 0);
            }
        }
    }

    if (!cpu_name_set) {
        std::string version = get_version_full();
        snprintf(cpu_name, sizeof(cpu_name), "%s on Unknown CPU", version.c_str());
        cpu_name_set = true;
    }

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

    if (found && leaf == 0x00000000) {
        if (!g_config.manufacturer_id.empty()) {
            g_config.manufacturer_id.resize(12);
            memcpy(&result.ebx, g_config.manufacturer_id.data(), 4);
            memcpy(&result.edx, g_config.manufacturer_id.data() + 4, 4);
            memcpy(&result.ecx, g_config.manufacturer_id.data() + 8, 4);
        }
    }

    if (found && leaf == 0x00000001) {
        bit_set(result.edx, 26, is_feature_enabled(x86_feature::SSE2));
        bit_set(result.ecx, 0, is_feature_enabled(x86_feature::SSE3));
        bit_set(result.ecx, 1, is_feature_enabled(x86_feature::PCLMULQDQ));
        bit_set(result.ecx, 9, is_feature_enabled(x86_feature::SSSE3));
        bit_set(result.ecx, 19, is_feature_enabled(x86_feature::SSE4_1));
        bit_set(result.ecx, 20, is_feature_enabled(x86_feature::SSE4_2));
        bit_set(result.ecx, 23, is_feature_enabled(x86_feature::LZCNT_POPCNT));
        bit_set(result.ecx, 25, is_feature_enabled(x86_feature::AES));
        bit_set(result.ecx, 27, is_feature_enabled(x86_feature::OSXSAVE));
        bit_set(result.ecx, 28, is_feature_enabled(x86_feature::AVX));
        bit_set(result.ecx, 29, is_feature_enabled(x86_feature::F16C));
    }

    if (found && leaf == 0x0000'0007) {
        bit_set(result.ecx, 9, is_feature_enabled(x86_feature::VAES));
        bit_set(result.ecx, 10, is_feature_enabled(x86_feature::VPCLMULQDQ));
        bit_set(result.ebx, 5, is_feature_enabled(x86_feature::AVX2));
    }

    if (found && leaf == 0x0000'000D) {
        result.eax = 0;
        result.ebx = 0;
        result.ecx = 0;
        result.edx = 0;
        if (subleaf == 2 && felix86_xsave_contains_ymms()) {
            // AVX YMM_HI size and offset in XSAVE
            result.eax = sizeof(ymm_hi);
            result.ebx = sizeof(fxsave_frame) + sizeof(xsave_header);
        }
    }

    if (found && leaf == 0x8000'0001) {
        bit_set(result.ecx, 0, true); // LAHF/SAHF
        bit_set(result.ecx, 5, is_feature_enabled(x86_feature::LZCNT_POPCNT));
    }

    if (found && leaf == 0x8000'0002 && cpu_name_set) {
        result.eax = *(uint32_t*)(cpu_name + 0);
        result.ebx = *(uint32_t*)(cpu_name + 4);
        result.ecx = *(uint32_t*)(cpu_name + 8);
        result.edx = *(uint32_t*)(cpu_name + 12);
    }

    if (found && leaf == 0x8000'0003 && cpu_name_set) {
        result.eax = *(uint32_t*)(cpu_name + 16 + 0);
        result.ebx = *(uint32_t*)(cpu_name + 16 + 4);
        result.ecx = *(uint32_t*)(cpu_name + 16 + 8);
        result.edx = *(uint32_t*)(cpu_name + 16 + 12);
    }

    if (found && leaf == 0x8000'0004 && cpu_name_set) {
        result.eax = *(uint32_t*)(cpu_name + 32 + 0);
        result.ebx = *(uint32_t*)(cpu_name + 32 + 4);
        result.ecx = *(uint32_t*)(cpu_name + 32 + 8);
        result.edx = *(uint32_t*)(cpu_name + 32 + 12);
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
