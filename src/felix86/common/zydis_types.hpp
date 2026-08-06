#include <algorithm>
#include <utility>
#include <vector>
#include <Zydis/Zydis.h>
#include "felix86/common/log.hpp"
#include "felix86/common/types.hpp"

using OperandArray = ZydisDecodedOperand[ZYDIS_MAX_OPERAND_COUNT];
using InstructionData = std::pair<ZydisDecodedInstruction, OperandArray>;

struct FlagAccessData {
    struct ScanAccess {
        u64 rip;
        ZydisAccessedFlagsMask flags_used;
        ZydisAccessedFlagsMask flags_changed;
    };

    struct FlagAccess {
        u64 rip;
        ZydisAccessedFlagsMask flags_needed;
    };

    FlagAccessData() = default;
    explicit FlagAccessData(const std::vector<ScanAccess>& scan_entries) : initialized(true) {
        constexpr ZydisAccessedFlagsMask all_flags =
            ZYDIS_CPUFLAG_OF | ZYDIS_CPUFLAG_CF | ZYDIS_CPUFLAG_ZF | ZYDIS_CPUFLAG_SF | ZYDIS_CPUFLAG_AF | ZYDIS_CPUFLAG_PF;
        ZydisAccessedFlagsMask live = all_flags;
        for (int i = (int)scan_entries.size() - 1; i >= 0; i--) {
            live &= ~scan_entries[i].flags_changed;
            live |= scan_entries[i].flags_used;
            flag_access.push_back({.rip = scan_entries[i].rip, .flags_needed = live});
        }
        std::reverse(flag_access.begin(), flag_access.end());
    }

    ~FlagAccessData() = default;
    FlagAccessData(const FlagAccessData&) = delete;
    FlagAccessData& operator=(const FlagAccessData&) = delete;
    FlagAccessData(FlagAccessData&&) = default;
    FlagAccessData& operator=(FlagAccessData&&) = default;

    ZydisAccessedFlagsMask getFlagsNeeded(u64 rip, ZydisAccessedFlagsMask mask) {
        ASSERT(initialized);
        auto it = std::lower_bound(flag_access.begin(), flag_access.end(), rip, [](const FlagAccess& fa, u64 r) { return fa.rip <= r; });

        if (it != flag_access.end()) {
            return it->flags_needed & mask;
        }

        return true;
    }

private:
    bool initialized = false;
    std::vector<FlagAccess> flag_access{};
};
