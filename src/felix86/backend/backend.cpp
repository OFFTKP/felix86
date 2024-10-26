#include <sys/mman.h>
#include "biscuit/cpuinfo.hpp"
#include "felix86/backend/backend.hpp"
#include "felix86/common/log.hpp"
#include "felix86/common/print.hpp"
#include "felix86/emulator.hpp"

using namespace biscuit;

constexpr static u64 code_cache_size = 32 * 1024 * 1024;

namespace {
std::string ExitReasonToString(ExitReason reason) {
    switch (reason) {
    case ExitReason::EXIT_REASON_HLT:
        return "Hit hlt instruction";
    case ExitReason::EXIT_REASON_BAD_ALIGNMENT:
        return "Bad alignment";
    }

    UNREACHABLE();
    return "";
}

void PrintExitReason(ThreadState* state) {
    fmt::print("Exit reason: {}\n", ExitReasonToString((ExitReason)state->exit_reason));
}
} // namespace

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
        if (!g_testing) // too much spam if testing
            WARN("Backend is missing some extensions or doesn't have VLEN=128");
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

    biscuit::GPR address = t0;

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

    // Jump
    Label exit_dispatcher_label;

    compile_next = (decltype(compile_next))as.GetCursorPointer();

    // If it's not zero it has some exit reason, exit the dispatcher
    as.LB(a0, offsetof(ThreadState, exit_reason), Registers::ThreadStatePointer());
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

    // Load the old state and print a message
    as.MV(address, Registers::ThreadStatePointer());
    as.ADDI(address, address, offsetof(ThreadState, gpr_storage));
    for (size_t i = 0; i < saved_gprs.size(); i++) {
        as.LD(saved_gprs[i], i * sizeof(u64), address);
    }

    as.ADDI(address, address, saved_gprs.size() * sizeof(u64));
    for (size_t i = 0; i < saved_fprs.size(); i++) {
        as.FLD(saved_fprs[i], i * sizeof(u64), address);
    }

    as.MV(a0, Registers::ThreadStatePointer());
    as.LI(a1, (u64)PrintExitReason);
    as.JALR(a1);

    as.EBREAK();

    VERBOSE("Enter dispatcher at: %p", enter_dispatcher);
    VERBOSE("Exit dispatcher at: %p", exit_dispatcher);
    VERBOSE("Crash target at: %p", crash_target);
    VERBOSE("Compile next at: %p", compile_next);
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

std::pair<void*, u64> Backend::EmitFunction(const BackendFunction& function, const AllocationMap& allocations) {
    void* start = as.GetCursorPointer();
    tsl::robin_map<const BackendBlock*, void*> block_map;

    struct ConditionalJump {
        ptrdiff_t location;
        Allocation allocation;
        const BackendBlock* target_true;
        const BackendBlock* target_false;
    };

    struct DirectJump {
        ptrdiff_t location;
        const BackendBlock* target;
    };

    std::vector<ConditionalJump> conditional_jumps;
    std::vector<DirectJump> direct_jumps;

    std::vector<const BackendBlock*> blocks_postorder = function.GetBlocksPostorder();

    for (auto it = blocks_postorder.rbegin(); it != blocks_postorder.rend(); it++) {
        const BackendBlock* block = *it;

        VERBOSE("Block %d (0x%016lx) corresponds to %p", block->GetIndex(), block->GetStartAddress(), as.GetCursorPointer());
        if (block->GetIndex() == 0 && allocations.GetSpillSize() > 0) {
            // Entry block, setup the stack pointer
            as.LI(t0, allocations.GetSpillSize());
            as.SUB(Registers::StackPointer(), Registers::StackPointer(), t0);
        }

        block_map[block] = as.GetCursorPointer();

        for (const BackendInstruction& inst : block->GetInstructions()) {
            Emitter::Emit(*this, allocations, inst);
        }

        if (block->GetIndex() == 1 && allocations.GetSpillSize() > 0) {
            // Exit block, restore the stack pointer
            as.LI(t0, allocations.GetSpillSize());
            as.ADD(Registers::StackPointer(), Registers::StackPointer(), t0);
        }

        switch (block->GetTermination()) {
        case Termination::Jump: {
            ptrdiff_t offset = as.GetCodeBuffer().GetCursorOffset();
            const BackendBlock* target = &function.GetBlock(block->GetSuccessor(0));
            direct_jumps.push_back({offset, target});
            // Some space for the backpatched jump
            for (int i = 0; i < 18; i++) {
                as.NOP();
            }
            as.EBREAK();
            break;
        }
        case Termination::JumpConditional: {
            ptrdiff_t offset = as.GetCodeBuffer().GetCursorOffset();
            Allocation condition = allocations.GetAllocation(block->GetCondition()->GetName());
            const BackendBlock* target_true = &function.GetBlock(block->GetSuccessor(0));
            const BackendBlock* target_false = &function.GetBlock(block->GetSuccessor(1));
            conditional_jumps.push_back({offset, condition, target_true, target_false});
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
        ASSERT(block_map.find(jump.target_true) != block_map.end());
        ASSERT(block_map.find(jump.target_false) != block_map.end());

        u8* cursor = as.GetCursorPointer();
        as.RewindBuffer(jump.location);

        Emitter::EmitJumpConditional(*this, jump.allocation.AsGPR(), block_map[jump.target_true], block_map[jump.target_false]);
        as.GetCodeBuffer().SetCursor(cursor);
    }

    void* end = as.GetCursorPointer();
    u64 size = (u64)end - (u64)start;

    map[function.GetStartAddress()] = {start, size};

    return {start, size};
}