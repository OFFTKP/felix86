#pragma once

#include "biscuit/assembler.hpp"
#include "felix86/backend/registers.hpp"
#include "felix86/common/utility.hpp"
#include "felix86/common/x86.hpp"

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

    u64 GetVMStatePointer() const {
        return (u64)(vm_storage.data());
    }

    Registers& GetRegisters() {
        return regs;
    }

private:
    static u8* allocateCodeCache();
    static void deallocateCodeCache(u8* memory);

    void emitNecessaryStuff();
    void resetCodeCache();

    constexpr static u64 vm_storage_size = 32 * 8 + 32 * 8 + 32 * 16; // 32 GPRs, 32 FPRs, 32 Vecs
    std::array<u8, vm_storage_size> vm_storage{};

    constexpr static std::array saved_gprs = {biscuit::ra, biscuit::sp, biscuit::gp,  biscuit::tp, biscuit::s0, biscuit::s1,
                                              biscuit::s2, biscuit::s3, biscuit::s4,  biscuit::s5, biscuit::s6, biscuit::s7,
                                              biscuit::s8, biscuit::s9, biscuit::s10, biscuit::s11};

    constexpr static std::array saved_fprs = {biscuit::fs0, biscuit::fs1, biscuit::fs2, biscuit::fs3, biscuit::fs4,  biscuit::fs5,
                                              biscuit::fs6, biscuit::fs7, biscuit::fs8, biscuit::fs9, biscuit::fs10, biscuit::fs11};

    std::array<u64, saved_gprs.size()> gpr_storage{};
    std::array<u64, saved_fprs.size()> fpr_storage{};

    ThreadState& thread_state;
    u8* memory = nullptr;
    biscuit::Assembler as{};
    tsl::robin_map<u64, void*> map{}; // map functions to host code

    // Special addresses within the code cache
    u8* enter_dispatcher = nullptr;
    u8* exit_dispatcher = nullptr;

    Registers regs;
};
