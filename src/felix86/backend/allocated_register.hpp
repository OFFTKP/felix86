#pragma once

#include "felix86/backend/backend.hpp"
#include "felix86/backend/emitter.hpp"

// RAII generic allocated reg, can be gpr/fpr/vec and if it's spilled it will load/store
struct AllocatedReg {
    AllocatedReg(Backend& backend, const IRInstruction* inst, bool load) : backend(backend) {
        this->load = load;
        if (inst->IsSpilled()) {
            spilled = true;
            spill_location = inst->GetSpillLocation() * (inst->IsVec() ? 16 : 8);
            if (inst->IsGPR()) {
                reg = backend.AcquireScratchGPRFromSpill(spill_location);
            } else {
                ERROR("Implme");
            }
        } else {
            reg = inst->GetGPR();
        }
    }

    ~AllocatedReg() {
        if (spilled && !load) {
            switch (reg.index()) {
            case 0: {
                biscuit::GPR gpr = std::get<biscuit::GPR>(reg);
                // Store to spilled location
                backend.GetAssembler().SD(gpr, spill_location, Registers::SpillPointer());
                break;
            }
            case 1: {
                biscuit::FPR fpr = std::get<biscuit::FPR>(reg);
                backend.GetAssembler().FSD(fpr, spill_location, Registers::SpillPointer());
                break;
            }
            case 2: {
                ERROR("Implme, needs vector spill location instead because they are 128-bit");
                break;
            }
            default: {
                UNREACHABLE();
            }
            }
        }
    }

    AllocatedReg(const AllocatedReg&) = delete;
    AllocatedReg& operator=(const AllocatedReg&) = delete;

    AllocatedReg(AllocatedReg&& other) = delete;
    AllocatedReg& operator=(AllocatedReg&& other) = delete;

    operator biscuit::GPR() const {
        return std::get<biscuit::GPR>(reg);
    }

    operator biscuit::FPR() const {
        return std::get<biscuit::FPR>(reg);
    }

    operator biscuit::Vec() const {
        return std::get<biscuit::Vec>(reg);
    }

    bool spilled = false;
    bool load = false;
    u64 spill_location = 0;
    std::variant<biscuit::GPR, biscuit::FPR, biscuit::Vec> reg;
    Backend& backend;
};

#define _RegRO_(instruction) AllocatedReg(backend, instruction, true)
#define _RegWO_(instruction) AllocatedReg(backend, instruction, false)