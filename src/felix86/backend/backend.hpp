#pragma once

#include "biscuit/assembler.hpp"
#include "felix86/backend/allocation_map.hpp"
#include "felix86/backend/emitter.hpp"
#include "felix86/backend/function.hpp"
#include "felix86/backend/registers.hpp"
#include "felix86/common/utility.hpp"
#include "felix86/common/x86.hpp"

#include <tsl/robin_map.h>

struct Emulator;

// There is a single backend that services all threads. A mutex is locked to synchronize.
struct Backend {
    Backend(Emulator& emulator);
    ~Backend();

    std::pair<void*, u64> GetCodeAt(u64 address) {
        if (map.find(address) != map.end()) {
            return map[address];
        }

        return {nullptr, 0};
    }

    u8 AvailableGPRs() const;
    u8 AvailableVec() const;

    void EnterDispatcher(ThreadState* state);

    std::pair<void*, u64> EmitFunction(const BackendFunction& function, const AllocationMap& allocations);

    Assembler& GetAssembler() {
        return as;
    }

    Emulator& GetEmulator() {
        return emulator;
    }

    void* GetCrashTarget() {
        return crash_target;
    }

    void* GetCompileNext() {
        return compile_next;
    }

private:
    static u8* allocateCodeCache();
    static void deallocateCodeCache(u8* memory);

    void emitNecessaryStuff();
    void resetCodeCache();

    Emulator& emulator;

    u8* memory = nullptr;
    biscuit::Assembler as{};
    tsl::robin_map<u64, std::pair<void*, u64>> map{}; // map functions to host code

    void (*enter_dispatcher)(ThreadState*) = nullptr;
    void* exit_dispatcher = nullptr;
    void* compile_next = nullptr;
    void* crash_target = nullptr;
};
