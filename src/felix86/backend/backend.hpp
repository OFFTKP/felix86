#pragma once

#include <atomic>
#include "biscuit/assembler.hpp"
#include "felix86/backend/emitter.hpp"
#include "felix86/backend/registers.hpp"
#include "felix86/common/utility.hpp"
#include "felix86/common/x86.hpp"
#include "felix86/ir/function.hpp"

#include <tsl/robin_map.h>

struct Emulator;

// There is a single backend that services all threads. A mutex is locked to synchronize.
struct Backend {
    Backend(Emulator& emulator);
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

    biscuit::GPR AcquireScratchGPR() {
        return regs.AcquireScratchGPR();
    }

    biscuit::GPR AcquireScratchGPRFromSpill(u64 spill_location) {
        return regs.AcquireScratchGPRFromSpill(as, spill_location);
    }

    void ReleaseScratchRegs() {
        regs.ReleaseScratchRegs();
    }

    void EnterDispatcher(ThreadState* state);

    std::pair<void*, u64> EmitFunction(IRFunction* function);

    Assembler& GetAssembler() {
        return as;
    }

    Emulator& GetEmulator() {
        return emulator;
    }

    bool HasB() const {
        return true; // TODO: proper way to check for bitmanip extension?
    }

private:
    static u8* allocateCodeCache();
    static void deallocateCodeCache(u8* memory);

    void emitNecessaryStuff();
    void resetCodeCache();

    Emulator& emulator;

    u8* memory = nullptr;
    biscuit::Assembler as{};
    tsl::robin_map<u64, void*> map{}; // map functions to host code

    void (*enter_dispatcher)(ThreadState*) = nullptr;
    void (*exit_dispatcher)() = nullptr;
    void (*compile_next)() = nullptr;

    Registers regs;

    // This isn't important to ensure thread safety or anything, it's just a simple sanity check at the start of
    // compilation to ensure only one thread owns this. This should always be the case because we lock a mutex,
    // but it's there for if the code changes and we forget.
    std::atomic_bool compiling{false};
};
