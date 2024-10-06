#include <sys/mman.h>
#include "biscuit/cpuinfo.hpp"
#include "felix86/backend/backend.hpp"
#include "felix86/common/log.hpp"

using namespace biscuit;

constexpr static u64 code_cache_size = 32 * 1024 * 1024;

Backend::Backend(ThreadState& thread_state) : thread_state(thread_state), memory(allocateCodeCache()), as(memory, code_cache_size) {
    emitNecessaryStuff();
    CPUInfo cpuinfo;
    bool has_atomic = cpuinfo.Has(RISCVExtension::A);
    bool has_compressed = cpuinfo.Has(RISCVExtension::C);
    bool has_integer = cpuinfo.Has(RISCVExtension::I);
    bool has_mul = cpuinfo.Has(RISCVExtension::M);
    bool has_fpu = cpuinfo.Has(RISCVExtension::D) && cpuinfo.Has(RISCVExtension::F);
    bool has_vector = cpuinfo.Has(RISCVExtension::V);

    if (!has_atomic || !has_compressed || !has_integer || !has_mul || !has_fpu || !has_vector || cpuinfo.GetVlenb() < 128) {
#ifdef __x86_64__
        WARN("Running in x86-64 environment");
#else
        ERROR("Backend is missing some extensions");
#endif
    }
}

Backend::~Backend() {
    deallocateCodeCache(memory);
}

void Backend::emitNecessaryStuff() {
    enter_dispatcher = as.GetCursorPointer();

    biscuit::GPR scratch = regs.AcquireScratchGPR();

    // Save the current register state of callee-saved registers and return address
    u64 gpr_storage_ptr = (u64)gpr_storage.data();
    as.LI(scratch, gpr_storage_ptr);
    for (size_t i = 0; i < saved_gprs.size(); i++) {
        as.SD(saved_gprs[i], i * sizeof(u64), scratch);
    }

    u64 fpr_storage_ptr = (u64)fpr_storage.data();
    as.LI(scratch, fpr_storage_ptr);
    for (size_t i = 0; i < saved_fprs.size(); i++) {
        as.FSD(saved_fprs[i], i * sizeof(u64), scratch);
    }

    as.LI(Registers::SpillPointer(), (u64)&thread_state);

    as.LI(Registers::VMStatePointer(), (u64)&vm_storage);

    // Jump
    // ...

    exit_dispatcher = as.GetCursorPointer();

    // Load the old state
    as.LI(scratch, gpr_storage_ptr);
    for (size_t i = 0; i < saved_gprs.size(); i++) {
        as.LD(saved_gprs[i], i * sizeof(u64), scratch);
    }

    as.LI(scratch, fpr_storage_ptr);
    for (size_t i = 0; i < saved_fprs.size(); i++) {
        as.FLD(saved_fprs[i], i * sizeof(u64), scratch);
    }

    as.RET();

    regs.ReleaseScratchRegs();
}

void Backend::resetCodeCache() {
    map.clear();
    as.RewindBuffer();
    emitNecessaryStuff();
}

u8* Backend::allocateCodeCache() {
    u8 prot = PROT_READ | PROT_WRITE | PROT_EXEC;
    u8 flags = MAP_PRIVATE | MAP_ANONYMOUS;

    return (u8*)mmap(nullptr, code_cache_size, prot, flags, -1, 0);
}

void Backend::deallocateCodeCache(u8* memory) {
    munmap(memory, code_cache_size);
}