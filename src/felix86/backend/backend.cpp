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

    if (!has_atomic || !has_compressed || !has_integer || !has_mul || !has_fpu || !has_vector || cpuinfo.GetVlenb() != 128) {
#ifdef __x86_64__
        WARN("Running in x86-64 environment");
#else
        ERROR("Backend is missing some extensions or doesn't have VLEN=128");
#endif
    }

    spill_storage.resize(32768);
}

Backend::~Backend() {
    deallocateCodeCache(memory);
}

void Backend::emitNecessaryStuff() {
    enter_dispatcher = as.GetCursorPointer();

    biscuit::GPR scratch = regs.AcquireScratchGPR();

    // Save the current register state of callee-saved registers and return address
    u64 gpr_storage_ptr = (u64)gpr_storage.data();
    const auto& saved_gprs = Registers::GetSavedGPRs();
    const auto& saved_fprs = Registers::GetSavedFPRs();
    as.LI(scratch, gpr_storage_ptr);
    for (size_t i = 0; i < saved_gprs.size(); i++) {
        as.SD(saved_gprs[i], i * sizeof(u64), scratch);
    }

    u64 fpr_storage_ptr = (u64)fpr_storage.data();
    as.LI(scratch, fpr_storage_ptr);
    for (size_t i = 0; i < saved_fprs.size(); i++) {
        as.FSD(saved_fprs[i], i * sizeof(u64), scratch);
    }

    as.LI(Registers::SpillPointer(), (u64)spill_storage.data());

    as.LI(Registers::ThreadStatePointer(), (u64)&thread_state);

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

void* Backend::EmitFunction(IRFunction* function) {
    void* start = as.GetCursorPointer();
    tsl::robin_map<IRBlock*, void*> block_map;

    struct ConditionalJump {
        ptrdiff_t location;
        Allocation allocation;
        IRBlock* target_true;
        IRBlock* target_false;
    };

    struct DirectJump {
        ptrdiff_t location;
        IRBlock* target;
    };

    std::vector<ConditionalJump> conditional_jumps;
    std::vector<DirectJump> direct_jumps;

    std::vector<IRBlock*> blocks_postorder = function->GetBlocksPostorder();

    for (auto it = blocks_postorder.rbegin(); it != blocks_postorder.rend(); it++) {
        IRBlock* block = *it;
        block_map[block] = as.GetCursorPointer();
        for (const BackendInstruction& inst : block->GetBackendInstructions()) {
            Emitter::Emit(*this, inst);
        }

        switch (block->GetTermination()) {
        case Termination::Jump: {
            direct_jumps.push_back({as.GetCodeBuffer().GetCursorOffset(), block->GetSuccessor(0)});
            // Some space for the backpatched jump
            as.NOP();
            as.NOP();
            as.NOP();
            as.EBREAK();
            break;
        }
        case Termination::JumpConditional: {
            conditional_jumps.push_back(
                {as.GetCodeBuffer().GetCursorOffset(), block->GetConditionAllocation(), block->GetSuccessor(0), block->GetSuccessor(1)});
            // Some space for the backpatched jump
            as.NOP();
            as.NOP();
            as.NOP();
            as.NOP();
            as.NOP();
            as.EBREAK();
            break;
        }
        case Termination::Exit: {
            Emitter::EmitJump(*this, exit_dispatcher);
            break;
        }
        default: {
            UNREACHABLE();
        }
        }
    }

    for (const DirectJump& jump : direct_jumps) {
        if (block_map.find(jump.target) == block_map.end()) {
            ERROR("Block not found");
        }

        u8* cursor = as.GetCursorPointer();
        as.RewindBuffer(jump.location);
        Emitter::EmitJump(*this, block_map[jump.target]);
        as.GetCodeBuffer().SetCursor(cursor);
    }

    for (const ConditionalJump& jump : conditional_jumps) {
        if (block_map.find(jump.target_true) == block_map.end() || block_map.find(jump.target_false) == block_map.end()) {
            ERROR("Block not found");
        }

        u8* cursor = as.GetCursorPointer();
        as.RewindBuffer(jump.location);
        Emitter::EmitJumpConditional(*this, jump.allocation, block_map[jump.target_true], block_map[jump.target_false]);
        as.GetCodeBuffer().SetCursor(cursor);
    }

    return start;
}