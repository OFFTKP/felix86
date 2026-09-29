#include <array>
#include <span>
#include <fcntl.h>
#include <sys/syscall.h>
#include "felix86/common/config.hpp"
#include "felix86/common/feature.hpp"
#include "felix86/common/global.hpp"
#include "felix86/common/log.hpp"
#include "felix86/common/state.hpp"
#include "felix86/common/xsave.hpp"
#include "felix86/hle/cpuid.hpp"

#ifdef __riscv
#include <asm/hwprobe.h>
#endif

constexpr static u32 NO_SUBLEAF = 0xFFFFFFFF;
constexpr static u32 CPUID_ICACHE = 2;
constexpr static u32 CPUID_DCACHE = 1;
constexpr static u32 CPUID_UNIFIED_CACHE = 3;

static inline void bit_set(u32& data, int position, bool value) {
    data &= ~(1u << position);
    data |= value << position;
}

static i64 sysconf_or_previous(int name, i64 previous) {
    auto val = sysconf(name);
    if (val > 0) {
        return val;
    } else {
        return previous;
    }
}

[[maybe_unused]] constexpr static std::array nehalem_mappings = {
    // http://users.atw.hu/instlatx64/GenuineIntel/GenuineIntel00106A2_Nehalem-EP_CPUID.txt
    (Cpuid){0x00000000, NO_SUBLEAF, 0x00000015, 0x756E6547, 0x6C65746E, 0x49656E69},
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

static std::span<const Cpuid> selected_mappings = nehalem_mappings;
static bool cpu_name_tried = false;
static bool cpu_name_set = false;
static char cpu_name[48];

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
                char fmt[] = "%s on %s";
                char buffer[sizeof(cpu_name) - sizeof(fmt)];
                int bytes_read = read(fd, buffer, sizeof(buffer) - 1);
                if (bytes_read != -1) {
                    buffer[bytes_read] = 0;
                    std::string version = get_version_full();
                    snprintf(cpu_name, sizeof(cpu_name), fmt, version.c_str(), buffer);
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

    bool mode32 = ThreadState::Get()->ctx.Mode32();
    auto& mappings = selected_mappings;
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

        if (is_feature_enabled(x86_feature::OSXSAVE) && result.eax < 0x0000000D) {
            result.eax = 0x0000000D;
        }
    }

    if (found && leaf == 0x00000001) {
        bit_set(result.edx, 26, is_feature_enabled(x86_feature::SSE2));
        bit_set(result.ecx, 0, is_feature_enabled(x86_feature::SSE3));
        bit_set(result.ecx, 1, is_feature_enabled(x86_feature::PCLMULQDQ));
        bit_set(result.ecx, 9, is_feature_enabled(x86_feature::SSSE3));
        bit_set(result.ecx, 12, is_feature_enabled(x86_feature::FMA3));
        bit_set(result.ecx, 19, is_feature_enabled(x86_feature::SSE4_1));
        bit_set(result.ecx, 20, is_feature_enabled(x86_feature::SSE4_2));
        bit_set(result.ecx, 22, is_feature_enabled(x86_feature::MOVBE));
        bit_set(result.ecx, 23, is_feature_enabled(x86_feature::LZCNT_POPCNT));
        bit_set(result.ecx, 25, is_feature_enabled(x86_feature::AES));
        bit_set(result.ecx, 26, is_feature_enabled(x86_feature::OSXSAVE));
        bit_set(result.ecx, 27, is_feature_enabled(x86_feature::OSXSAVE));
        bit_set(result.ecx, 28, is_feature_enabled(x86_feature::AVX));
        bit_set(result.ecx, 29, is_feature_enabled(x86_feature::F16C));
    }

    if (leaf == 0x0000'0002) {
        result.ecx = 0;
        result.edx = 0;
        // 64-byte prefetch, TLB stuff
        result.ebx = 0xCAF0B255;
        result.eax = 0x01;       // Bit required to be set
        result.eax |= 0xFF << 8; // Look at leaf 4 for cache info
        // more TLB stuff
        result.eax |= 0x035A << 16;
    }

    if (leaf == 0x0000'0004) {
        struct CacheInfo {
            u64 size = 0;
            u16 line_size = 0;
            u16 assoc = 0;
            u8 level = 0;
            u8 type = 0;
        };
        static std::array<CacheInfo, 5> caches = []() {
            std::array<CacheInfo, 5> ret;
            // Fill with some data in case we fail
            ret[0].level = 1;
            ret[0].type = CPUID_ICACHE;
            ret[0].line_size = 64;
            ret[0].size = 64 * 1024;
            ret[0].assoc = 4;
            ret[1].level = 1;
            ret[1].type = CPUID_DCACHE;
            ret[1].line_size = 64;
            ret[1].size = 64 * 1024;
            ret[1].assoc = 4;
            ret[2].level = 2;
            ret[2].type = CPUID_UNIFIED_CACHE;
            ret[2].line_size = 64;
            ret[2].size = 4096 * 1024;
            ret[2].assoc = 16;
            ret[3].level = 3;
            ret[3].type = CPUID_UNIFIED_CACHE;
            ret[3].line_size = 64;
            ret[3].assoc = 16;
            ret[4].level = 4; // Doesn't exist but whatever
            ret[4].type = CPUID_UNIFIED_CACHE;
            ret[4].line_size = 64;
            ret[4].assoc = 16;
            for (int i = 0; i < 5; i++) {
                ret[i].size = sysconf_or_previous(_SC_LEVEL1_ICACHE_SIZE + i * 3, ret[i].size);
                ret[i].line_size = sysconf_or_previous(_SC_LEVEL1_ICACHE_LINESIZE + i * 3, ret[i].line_size);
                ret[i].assoc = sysconf_or_previous(_SC_LEVEL1_ICACHE_ASSOC + i * 3, ret[i].assoc);
            }
            return ret;
        }();

        if (subleaf > 4 || caches[subleaf].size == 0) {
            result.eax = 0;
            result.ebx = 0;
            result.ecx = 0;
            result.edx = 0;
        } else {
            u32 eax = caches[subleaf].type;
            eax |= caches[subleaf].level << 5;
            eax |= 1 << 8; // self-initializing
            eax |= 0 << 9; // fully associative
            int cpu_count = get_cpu_count();
            if (subleaf >= 2 && cpu_count != -1) {
                // Assume cache is shared with all processors
                eax |= (cpu_count - 1) << 14;
            } else {
                // Assume L1 is not shared (i.e. hyperthreading)
            }
            result.eax = eax;

            u32 ebx = caches[subleaf].line_size - 1;
            ebx |= 0 << 12; // 1 partition, no bits give this info on RISC-V
            ebx |= (caches[subleaf].assoc - 1) << 22;
            result.ebx = ebx;

            const u64 size = caches[subleaf].size;
            const u64 assoc = caches[subleaf].assoc;
            const u64 line_size = caches[subleaf].line_size;
            result.ecx = size / (assoc * line_size) - 1; // set count
            result.edx = 0;
        }
    }

    if (found && leaf == 0x0000'0007) {
        bit_set(result.ecx, 9, is_feature_enabled(x86_feature::VAES));
        bit_set(result.ecx, 10, is_feature_enabled(x86_feature::VPCLMULQDQ));
        bit_set(result.ebx, 5, is_feature_enabled(x86_feature::AVX2));
        bit_set(result.ebx, 3, is_feature_enabled(x86_feature::BMI1));
        bit_set(result.ebx, 8, is_feature_enabled(x86_feature::BMI2));
        bit_set(result.ebx, 19, is_feature_enabled(x86_feature::ADX));
        bit_set(result.ebx, 29, is_feature_enabled(x86_feature::SHA));
    }

    if (leaf == 0x0000'000D) {
        result.eax = 0;
        result.ebx = 0;
        result.ecx = 0;
        result.edx = 0;
        u64 xsave_size = sizeof(fxsave_frame) + sizeof(xsave_header) + (felix86_xsave_contains_ymms() ? sizeof(ymm_hi) : 0);
        if (subleaf == 2 && felix86_xsave_contains_ymms()) {
            // AVX YMM_HI size and offset in XSAVE
            result.eax = sizeof(ymm_hi);
            result.ebx = sizeof(fxsave_frame) + sizeof(xsave_header);
            found = true;
        } else if (subleaf == 0) {
            result.ebx = xsave_size;
            result.ecx = xsave_size;
            result.eax = get_xfeature_enabled_mask();
            result.edx = 0;
            found = true;
        } else if (subleaf == 1) {
            result.ebx = xsave_size;
            found = true;
        }
    }

    if (leaf == 0x0000'0015) {
        result.ecx = rdtime_frequency();
        result.eax = 1;
        result.ebx = 1 << rdtime_frequency_shift();
        result.edx = 0;
        found = true;
    }

    if (found && leaf == 0x8000'0001) {
        bit_set(result.ecx, 0, true); // LAHF/SAHF
        bit_set(result.ecx, 5, is_feature_enabled(x86_feature::LZCNT_POPCNT));
        bit_set(result.edx, 27, is_feature_enabled(x86_feature::RDTSCP));
        bit_set(result.edx, 11, !mode32); // Clear SYSCALL bit in 32-bit mode, similar to HW
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

    CPUIDLOG("CPUID %08x %08x -> %08x %08x %08x %08x", leaf, subleaf, result.eax, result.ebx, result.ecx, result.edx);

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
