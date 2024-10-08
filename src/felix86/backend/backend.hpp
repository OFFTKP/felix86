#pragma once

#include "biscuit/assembler.hpp"
#include "felix86/backend/emitter.hpp"
#include "felix86/backend/registers.hpp"
#include "felix86/common/utility.hpp"
#include "felix86/common/x86.hpp"
#include "felix86/ir/function.hpp"

#include <tsl/robin_map.h>

struct Backend {
    Backend(ThreadState& thread_state);
    ~Backend();

    void MapCompiledFunction(u64 address, void* function) {
        map[address] = function;
    }

    void* GetCompiledFunction(u64 address) {
        if (map.find(address) != map.end()) {
            return map[address];
        }

        return nullptr;
    }

    u8 AvailableGPRs() const;
    u8 AvailableFPRs() const;
    u8 AvailableVec() const;

    Registers& GetRegisters() {
        return regs;
    }

    ThreadState& GetThreadState() {
        return thread_state;
    }

    biscuit::GPR AcquireScratchGPR() {
        return regs.AcquireScratchGPR();
    }

    biscuit::GPR AcquireScratchGPRFromSpill(u64 spill_location) {
        return regs.AcquireScratchGPRFromSpill(as, spill_location);
    }

    void ReleaseScratchRegs() {
        regs.ReleaseScratchRegs();
    }

    void* EmitFunction(IRFunction* function);

    Assembler& GetAssembler() {
        return as;
    }

    bool HasB() const {
        return true; // TODO: proper way to check for bitmanip extension?
    }

private:
    static u8* allocateCodeCache();
    static void deallocateCodeCache(u8* memory);

    void emitNecessaryStuff();
    void resetCodeCache();

    std::array<u64, Registers::GetSavedGPRs().size()> gpr_storage{};
    std::array<u64, Registers::GetSavedFPRs().size()> fpr_storage{};

    std::vector<u64> spill_storage{};

    ThreadState& thread_state;
    u8* memory = nullptr;
    biscuit::Assembler as{};
    tsl::robin_map<u64, void*> map{}; // map functions to host code

    // Special addresses within the code cache
    u8* enter_dispatcher = nullptr;
    u8* exit_dispatcher = nullptr;

    Registers regs;
};
