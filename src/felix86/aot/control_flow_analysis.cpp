#include <vector>
#include <Zydis/Utils.h>
#include "felix86/aot/aot.hpp"
#include "felix86/common/log.hpp"

/**
    Follow the control flow of the executable, starting at entry point, to find as many functions as we can.

    Aims to find a good chunk of functions to compile them ahead of time.
*/

void AOT::ControlFlowAnalysis() {
    std::vector<u64> worklist;
    worklist.push_back(elf.entry);

    std::unordered_set<u64> visited;
    visited.insert(elf.entry);

    ZydisDecodedInstruction instruction;
    ZydisDecodedOperand operands[10];
    while (!worklist.empty()) {
        u64 work = worklist.back();
        worklist.pop_back();

        while (true) {
            ZyanStatus status = decodeInstruction(instruction, operands, (u8*)work);
            if (!ZYAN_SUCCESS(status)) {
                ERROR("Failed to decode instruction at %p", (u8*)work);
            }

            bool break_outer = false;
            switch (instruction.meta.category) {
            case ZYDIS_CATEGORY_COND_BR: {
                u64 target;
                if (!ZYAN_SUCCESS(ZydisCalcAbsoluteAddress(&instruction, operands, work, &target))) {
                    // Could not get the address at compile time, skip
                    break;
                }

                if (visited.find(target) == visited.end()) {
                    visited.insert(target);
                    worklist.push_back(target);
                }
                break;
            }
            case ZYDIS_CATEGORY_UNCOND_BR: {
                // Since it's an unconditional branch we must stop this path here
                break_outer = true;

                u64 target;
                if (!ZYAN_SUCCESS(ZydisCalcAbsoluteAddress(&instruction, operands, work, &target))) {
                    // Could not get the address at compile time, skip
                    break;
                }

                // Don't add to addresses, only call targets go there
                if (visited.find(target) == visited.end()) {
                    visited.insert(target);
                    worklist.push_back(target);
                }
                break;
            }
            case ZYDIS_CATEGORY_CALL: {
                u64 target;
                if (!ZYAN_SUCCESS(ZydisCalcAbsoluteAddress(&instruction, operands, work, &target))) {
                    // Could not get the address at compile time, skip
                    break;
                }

                if (addresses.find(target) == addresses.end()) {
                    addresses.insert(target);
                    worklist.push_back(target);
                    visited.insert(target);
                }
                break;
            }
            case ZYDIS_CATEGORY_RET: {
                // Since ret is an unconditional branch we must stop this path here
                break_outer = true;
                break;
            }
            default: {
                work += instruction.length;
                continue;
            }
            }

            if (break_outer) {
                break;
            }
        }
    }
}