#pragma once

#include "felix86/backend/backend.hpp"

// Converts an Allocation (which may be a spill location) to a register
struct AllocationWrapper {
    AllocationWrapper(Backend& backend, const Allocation& allocation, bool load) : backend(backend), load(load) {
        if (allocation.IsSpilled()) {
            Spill spill = allocation.AsSpill();
            ASSERT(spill.size != SpillSize::Null);

            spilled = true;
            spill_location = spill.location * ((u8)spill.size);

            Assembler& as = backend.GetAssembler();
            switch (spill.reg_type) {
            case AllocationType::GPR: {
                reg = backend.GetRegisters().AcquireScratchGPR();

                if (load) {
                    biscuit::GPR gpr = AsGPR();
                    as.LD(gpr, spill_location, Registers::SpillPointer());
                }
                break;
            }
            case AllocationType::FPR: {
                reg = backend.GetRegisters().AcquireScratchFPR();

                if (load) {
                    biscuit::FPR fpr = AsFPR();
                    as.FLD(fpr, spill_location, Registers::SpillPointer());
                }
                break;
            }
            case AllocationType::Vec: {
                reg = backend.GetRegisters().AcquireScratchVec();
                UNREACHABLE();
                break;
            }
            default: {
                UNREACHABLE();
                break;
            }
            }
        } else if (allocation.IsGPR()) {
            reg = allocation.AsGPR();
        } else if (allocation.IsFPR()) {
            reg = allocation.AsFPR();
        } else if (allocation.IsVec()) {
            reg = allocation.AsVec();
        } else {
            UNREACHABLE();
        }
    }

    ~AllocationWrapper() {
        if (spilled && !load) {
            Assembler& as = backend.GetAssembler();
            // Store to spilled location
            switch (reg.index()) {
            case 0: {
                biscuit::GPR gpr = AsGPR();
                as.SD(gpr, spill_location, Registers::SpillPointer());
                break;
            }
            case 1: {
                biscuit::FPR fpr = AsFPR();
                as.FSD(fpr, spill_location, Registers::SpillPointer());
                break;
            }
            case 2: {
                // IMPLME
                UNREACHABLE();
                break;
            }
            default: {
                UNREACHABLE();
            }
            }
        }
    }

    AllocationWrapper(const AllocationWrapper&) = delete;
    AllocationWrapper& operator=(const AllocationWrapper&) = delete;

    AllocationWrapper(AllocationWrapper&& other) = delete;
    AllocationWrapper& operator=(AllocationWrapper&& other) = delete;

    operator biscuit::GPR() const {
        return std::get<biscuit::GPR>(reg);
    }

    operator biscuit::FPR() const {
        return std::get<biscuit::FPR>(reg);
    }

    operator biscuit::Vec() const {
        return std::get<biscuit::Vec>(reg);
    }

    bool IsGPR() const {
        return reg.index() == 0;
    }

    bool IsFPR() const {
        return reg.index() == 1;
    }

    bool IsVec() const {
        return reg.index() == 2;
    }

    biscuit::GPR AsGPR() const {
        return std::get<biscuit::GPR>(reg);
    }

    biscuit::FPR AsFPR() const {
        return std::get<biscuit::FPR>(reg);
    }

    biscuit::Vec AsVec() const {
        return std::get<biscuit::Vec>(reg);
    }

    Backend& backend;
    std::variant<biscuit::GPR, biscuit::FPR, biscuit::Vec> reg;
    u64 spill_location = 0;
    bool spilled = false;
    bool load = false;
};
