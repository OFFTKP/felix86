#include <sys/mman.h>
#include "felix86/backend/backend.hpp"
#include "felix86/common/log.hpp"
#include "felix86/emulator.hpp"

using namespace biscuit;

constexpr static u64 code_cache_size = 32 * 1024 * 1024;

// If you don't flush the cache the code will randomly SIGILL
static inline void flush_icache() {
#if defined(__riscv)
    asm volatile("fence.i" ::: "memory");
#elif defined(__aarch64__)
#pragma message("Don't forget to implement me")
#elif defined(__x86_64__)
    // No need to flush the cache on x86
#endif
}

namespace {
std::string ExitReasonToString(ExitReason reason) {
    switch (reason) {
    case ExitReason::EXIT_REASON_HLT:
        return "Hit hlt instruction";
    case ExitReason::EXIT_REASON_BAD_ALIGNMENT:
        return "Bad alignment";
    case ExitReason::EXIT_REASON_NO_VECTOR:
        return "Vector extension disabled";
    case ExitReason::EXIT_REASON_UD2:
        return "Hit ud2 instruction";
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
}

Backend::~Backend() {
    deallocateCodeCache(memory);
}

void Backend::emitNecessaryStuff() {
    // We can use the thread_id to get the thread state at runtime
    // depending on which thread is running

    /* void enter_dispatcher(ThreadState* state) */
    enter_dispatcher = (decltype(enter_dispatcher))as.GetCursorPointer();

    // Give it an initial valid state
    as.VSETIVLI(x0, SUPPORTED_VLEN / 8, SEW::E8);

    biscuit::GPR address = t0;

    // Save the current register state of callee-saved registers and return address
    as.ADDI(address, a0, offsetof(ThreadState, gpr_storage));
    const auto& saved_gprs = Registers::GetSavedGPRs();
    for (size_t i = 0; i < saved_gprs.size(); i++) {
        as.SD(saved_gprs[i], i * sizeof(u64), address);
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

    as.RET();

    crash_target = as.GetCursorPointer();

    // Load the old state and print a message
    as.MV(address, Registers::ThreadStatePointer());
    as.ADDI(address, address, offsetof(ThreadState, gpr_storage));
    for (size_t i = 0; i < saved_gprs.size(); i++) {
        as.LD(saved_gprs[i], i * sizeof(u64), address);
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

    g_thread_state = state;
    enter_dispatcher(state);
}

void print_address(u64 address, int index) {
    PLAIN("Entering block 0x%016lx (%d)", address, index);
}

std::pair<void*, u64> Backend::EmitFunction(const BackendFunction& function, const AllocationMap& allocations) {
    void* start = as.GetCursorPointer();
    std::vector<const BackendBlock*> blocks_postorder = function.GetBlocksPostorder();

    struct Jump {
        ptrdiff_t offset;
        Label* label;
    };

    struct JumpConditional {
        ptrdiff_t offset;
        biscuit::GPR condition;
        Label* label_true;
        Label* label_false;
    };

    std::vector<Jump> jumps;
    std::vector<JumpConditional> jumps_conditional;

    for (auto it = blocks_postorder.rbegin(); it != blocks_postorder.rend(); it++) {
        const BackendBlock* block = *it;

        VERBOSE("Block %d (0x%016lx) corresponds to %p", block->GetIndex(), block->GetStartAddress(), as.GetCursorPointer());

        as.Bind(block->GetLabel());

        // Must not insert so many instructions to blocks that are during lr/sc
        if (g_print_block_start && !block->IsCriticalSection()) {
            Emitter::EmitPushAllCallerSaved(*this);
            as.LI(a0, block->GetStartAddress());
            as.LI(a1, block->GetIndex());
            as.LI(t0, (u64)print_address);
            as.JALR(t0);
            Emitter::EmitPopAllCallerSaved(*this);
        }

        if (block->GetIndex() == 0 && allocations.GetSpillSize() > 0) {
            // Entry block, setup the stack pointer
            as.LI(t0, allocations.GetSpillSize());
            as.SUB(Registers::StackPointer(), Registers::StackPointer(), t0);
        }

        for (const BackendInstruction& inst : block->GetInstructions()) {
            if (inst.GetOpcode() == IROpcode::Jump) {
                Jump jump;
                jump.offset = as.GetCodeBuffer().GetCursorOffset();
                jump.label = block->GetSuccessor(0)->GetLabel();
                jumps.push_back(jump);
                // TODO: make it smaller when we implement literals in jumpfar and jumpconditionalfar
                for (int i = 0; i < 30; i++) {
                    as.NOP();
                }
            } else if (inst.GetOpcode() == IROpcode::JumpConditional) {
                JumpConditional jump;
                jump.offset = as.GetCodeBuffer().GetCursorOffset();
                jump.condition = allocations.GetAllocation(inst.GetOperand(0)).AsGPR();
                jump.label_true = block->GetSuccessor(0)->GetLabel();
                jump.label_false = block->GetSuccessor(1)->GetLabel();
                jumps_conditional.push_back(jump);
                for (int i = 0; i < 30; i++) {
                    as.NOP();
                }
            } else {
                Emitter::Emit(*this, allocations, *block, inst);
            }
        }

        if (block->GetIndex() == 1 && allocations.GetSpillSize() > 0) {
            // Exit block, restore the stack pointer
            as.LI(t0, allocations.GetSpillSize());
            as.ADD(Registers::StackPointer(), Registers::StackPointer(), t0);
        }
    }

    void* end = as.GetCursorPointer();
    u64 size = (u64)end - (u64)start;

    map[function.GetStartAddress()] = {start, size};

    for (auto& [offset, label] : jumps) {
        ptrdiff_t current_offset = as.GetCodeBuffer().GetCursorOffset();
        as.RewindBuffer(offset);
        void* target = (void*)(as.GetCodeBuffer().GetOffsetAddress(*label->GetLocation()));
        void* here = as.GetCursorPointer();
        if (IsValidJTypeImm((ptrdiff_t)target - (ptrdiff_t)here)) {
            Emitter::EmitJump(*this, label);
        } else {
            Emitter::EmitJumpFar(*this, target);
        }
        as.AdvanceBuffer(current_offset);
    }

    for (auto& [offset, condition, label_true, label_false] : jumps_conditional) {
        ptrdiff_t current_offset = as.GetCodeBuffer().GetCursorOffset();
        as.RewindBuffer(offset);
        void* target_true = (void*)(as.GetCodeBuffer().GetOffsetAddress(*label_true->GetLocation()));
        void* target_false = (void*)(as.GetCodeBuffer().GetOffsetAddress(*label_false->GetLocation()));
        void* here = as.GetCursorPointer();
        if (IsValidJTypeImm((ptrdiff_t)target_true - (ptrdiff_t)here) && IsValidJTypeImm((ptrdiff_t)target_false - (ptrdiff_t)here)) {
            Emitter::EmitJumpConditional(*this, condition, label_true, label_false);
        } else {
            Emitter::EmitJumpConditionalFar(*this, condition, target_true, target_false);
        }
        as.AdvanceBuffer(current_offset);
    }

    // Make code visible to instruction fetches.
    flush_icache();

    return {start, size};
}