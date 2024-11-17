// Generates CPUID results given input from http://instlatx64.atw.hu/

#include <cstdint>
#include <regex>
#include <sstream>
#include <string>
#include <unordered_map>

std::string g_input = R"(
CPUID 00000000: 0000000A-756E6547-6C65746E-49656E69
CPUID 00000001: 00010676-00040800-000CE3BD-BFEBFBFF
CPUID 00000002: 05B0B101-005657F0-00000000-2CB4304E
CPUID 00000003: 00000000-00000000-00000000-00000000
CPUID 00000004: 0C000121-01C0003F-0000003F-00000001
CPUID 00000004: 0C000122-01C0003F-0000003F-00000001
CPUID 00000004: 0C004143-05C0003F-00000FFF-00000001
CPUID 00000005: 00000040-00000040-00000003-00002220
CPUID 00000006: 00000001-00000002-00000001-00000000
CPUID 00000007: 00000000-00000000-00000000-00000000
CPUID 00000008: 00000400-00000000-00000000-00000000
CPUID 00000009: 00000000-00000000-00000000-00000000
CPUID 0000000A: 07280202-00000000-00000000-00000503
CPUID 80000000: 80000008-00000000-00000000-00000000
CPUID 80000001: 00000000-00000000-00000001-20100000
CPUID 80000002: 65746E49-2952286C-6F655820-2952286E
CPUID 80000003: 55504320-20202020-20202020-45202020
CPUID 80000004: 32363435-20402020-30382E32-007A4847
CPUID 80000005: 00000000-00000000-00000000-00000000
CPUID 80000006: 00000000-00000000-18008040-00000000
CPUID 80000007: 00000000-00000000-00000000-00000000
CPUID 80000008: 00003026-00000000-00000000-00000000
)";

int main(int argc, char* argv[]) {
    std::stringstream input(g_input);

    std::unordered_map<int, int> leaf_count;
    std::string line;
    struct cpuid {
        int leaf;
        int subleaf;
        uint32_t eax;
        uint32_t ebx;
        uint32_t ecx;
        uint32_t edx;
    };
    std::vector<cpuid> cpuids;
    while (std::getline(input, line)) {
        if (line.empty()) {
            continue;
        }

        if (line.find("CPUID") == std::string::npos) {
            continue;
        }

        std::regex re("CPUID ([0-9A-F]+): ([0-9A-F]+)-([0-9A-F]+)-([0-9A-F]+)-([0-9A-F]+)");
        std::smatch match;
        if (std::regex_search(line, match, re)) {
            int leaf = std::stol(match[1].str(), nullptr, 16);
            int subleaf = leaf_count[leaf]++;
            uint32_t eax = std::stol(match[2].str(), nullptr, 16);
            uint32_t ebx = std::stol(match[3].str(), nullptr, 16);
            uint32_t ecx = std::stol(match[4].str(), nullptr, 16);
            uint32_t edx = std::stol(match[5].str(), nullptr, 16);
            cpuids.push_back({leaf, subleaf, eax, ebx, ecx, edx});
        }
    }

    for (auto& cpuid : cpuids) {
        if (leaf_count[cpuid.leaf] == 1) {
            printf("{0x%08x, NO_SUBLEAF, 0x%08X, 0x%08X, 0x%08X, 0x%08X},\n", cpuid.leaf, cpuid.eax, cpuid.ebx, cpuid.ecx, cpuid.edx);
        } else {
            printf("{0x%08x, 0x%08x, 0x%08X, 0x%08X, 0x%08X, 0x%08X},\n", cpuid.leaf, cpuid.subleaf, cpuid.eax, cpuid.ebx, cpuid.ecx, cpuid.edx);
        }
    }
}