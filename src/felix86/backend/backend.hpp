#pragma once

#include "biscuit/assembler.hpp"
#include "felix86/backend/allocation_map.hpp"
#include "felix86/backend/emitter.hpp"
#include "felix86/backend/function.hpp"
#include "felix86/backend/registers.hpp"
#include "felix86/common/utility.hpp"
#include "felix86/common/x86.hpp"

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

    void* AddCodeAt(u64 address, void* code, u64 size);

    u8 AvailableGPRs() const;
    u8 AvailableVec() const;

    void EnterDispatcher(ThreadState* state);

    std::pair<void*, u64> EmitFunction(const BackendFunction& function, AllocationMap& allocations);

    Assembler& GetAssembler() {
        return as;
    }

    Emulator& GetEmulator() {
        return emulator;
    }

    void* GetCrashHandler() {
        return crash_handler;
    }

    void* GetCompileNext() {
        return compile_next_handler;
    }

    u64 GetCodeCacheSize() const {
        return as.GetCursorPointer() - memory;
    }

private:
    static u8* allocateCodeCache();
    static void deallocateCodeCache(u8* memory);

    void emitNecessaryStuff();
    void resetCodeCache();

    Emulator& emulator;

    u8* memory = nullptr;
    biscuit::Assembler as{};
    std::unordered_map<u64, std::pair<void*, u64>> map{}; // map functions to host code

    void (*enter_dispatcher)(ThreadState*) = nullptr;
    void* exit_dispatcher = nullptr;
    void* compile_next_handler = nullptr;
    void* crash_handler = nullptr;
};
