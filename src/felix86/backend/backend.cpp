#include <sys/mman.h>
#include "biscuit/cpuinfo.hpp"
#include "felix86/backend/backend.hpp"
#include "felix86/common/log.hpp"
#include "felix86/emulator.hpp"

using namespace biscuit;

constexpr static u64 code_cache_size = 32 * 1024 * 1024;

Backend::Backend(Emulator& emulator) : emulator(emulator), memory(allocateCodeCache()), as(memory, code_cache_size) {
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
        WARN("Backend is missing some extensions or doesn't have VLEN=128");
#endif
    }
}

Backend::~Backend() {
    deallocateCodeCache(memory);
}

void Backend::emitNecessaryStuff() {
    // We can use the thread_id to get the thread state at runtime
    // depending on which thread is running

    /* void enter_dispatcher(ThreadState* state) */
    enter_dispatcher = (decltype(enter_dispatcher))as.GetCursorPointer();

    biscuit::GPR address = regs.AcquireScratchGPR();

    // Save the current register state of callee-saved registers and return address
    as.ADDI(address, a0, offsetof(ThreadState, gpr_storage));
    const auto& saved_gprs = Registers::GetSavedGPRs();
    const auto& saved_fprs = Registers::GetSavedFPRs();
    for (size_t i = 0; i < saved_gprs.size(); i++) {
        as.SD(saved_gprs[i], i * sizeof(u64), address);
    }

    as.ADDI(address, address, saved_gprs.size() * sizeof(u64));
    for (size_t i = 0; i < saved_fprs.size(); i++) {
        as.FSD(saved_fprs[i], i * sizeof(u64), address);
    }

    // Since we picked callee-saved registers, we don't have to save them when calling stuff,
    // but they must be set after the save of the old state that happens above this comment
    as.C_MV(Registers::ThreadStatePointer(), a0);
    as.C_MV(Registers::SpillPointer(), a0);
    as.ADDI(Registers::SpillPointer(), Registers::SpillPointer(), offsetof(ThreadState, spill_gpr));

    // Jump
    Label exit_dispatcher_label;

    compile_next = (decltype(compile_next))as.GetCursorPointer();
    // If it's not zero it has some exit reason, exit the dispatcher
    as.LB(a0, offsetof(ThreadState, exit_dispatcher_flag), Registers::ThreadStatePointer());
    as.BNEZ(a0, &exit_dispatcher_label);
    as.LI(a0, (u64)&emulator);
    as.MV(a1, Registers::ThreadStatePointer());
    as.LI(a2, (u64)Emulator::CompileNext);
    as.JALR(a2); // returns the function pointer to the compiled function
    as.JR(a0);   // jump to the compiled function

    // When it needs to exit the dispatcher for whatever reason (such as hlt hit), jump here
    exit_dispatcher = (decltype(exit_dispatcher))as.GetCursorPointer();

    as.Bind(&exit_dispatcher_label);

    // Load the old state
    as.MV(address, Registers::ThreadStatePointer());
    as.ADDI(address, address, offsetof(ThreadState, gpr_storage));
    for (size_t i = 0; i < saved_gprs.size(); i++) {
        as.LD(saved_gprs[i], i * sizeof(u64), address);
    }

    as.ADDI(address, address, saved_gprs.size() * sizeof(u64));
    for (size_t i = 0; i < saved_fprs.size(); i++) {
        as.FLD(saved_fprs[i], i * sizeof(u64), address);
    }

    as.RET();

    crash_target = as.GetCursorPointer();
    as.EBREAK();

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

void Backend::EnterDispatcher(ThreadState* state) {
    if (!enter_dispatcher) {
        ERROR("Dispatcher not initialized??");
    }

    enter_dispatcher(state);
}

std::pair<void*, u64> Backend::EmitFunction(IRFunction* function) {
    // This is a sanity check for when stuff is refactored, not done for thread safety.
    if (compiling.load()) {
        ERROR("Already compiling");
    }
    compiling.store(true);

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
            as.NOP();
            as.NOP();
            as.NOP();
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
            for (int i = 0; i < 18; i++) {
                as.NOP();
            }
            as.EBREAK();
            break;
        }
        case Termination::BackToDispatcher: {
            Emitter::EmitJump(*this, (void*)compile_next);
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

    void* end = as.GetCursorPointer();
    u64 size = (u64)end - (u64)start;

    // This is a sanity check for when stuff is refactored, not done for thread safety.
    compiling.store(false);
    return {start, size};
}