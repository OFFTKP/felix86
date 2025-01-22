#include <sys/mman.h>
#include "felix86/backend/backend.hpp"
#include "felix86/common/log.hpp"
#include "felix86/emulator.hpp"

using namespace biscuit;

constexpr static u64 code_cache_size = 64 * 1024 * 1024;

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
    case ExitReason::EXIT_REASON_TSX:
        return "Hit tsx instruction";
    case ExitReason::EXIT_REASON_CET:
        return "Hit cet instruction";
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

    // Save the current register state of callee-saved registers and return address
    const auto& saved_gprs = Registers::GetSavedGPRs();
    as.ADDI(sp, sp, -((int)saved_gprs.size() * 8));
    for (size_t i = 0; i < saved_gprs.size(); i++) {
        as.SD(saved_gprs[i], i * sizeof(u64), sp);
    }

    // Since we picked callee-saved registers, we don't have to save them when calling stuff,
    // but they must be set after the save of the old state that happens above this comment
    as.C_MV(Registers::ThreadStatePointer(), a0);

    // Jump
    Label exit_dispatcher_label;

    compile_next_handler = (decltype(compile_next_handler))as.GetCursorPointer();

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

    for (size_t i = 0; i < saved_gprs.size(); i++) {
        as.LD(saved_gprs[i], i * sizeof(u64), sp);
    }

    as.ADDI(sp, sp, (int)saved_gprs.size() * 8);

    as.RET();

    crash_handler = as.GetCursorPointer();

    for (size_t i = 0; i < saved_gprs.size(); i++) {
        as.LD(saved_gprs[i], i * sizeof(u64), sp);
    }

    as.ADDI(sp, sp, (int)saved_gprs.size() * 8);

    as.MV(a0, Registers::ThreadStatePointer());
    as.LI(a1, (u64)PrintExitReason);
    as.JALR(a1);

    as.EBREAK();

    VERBOSE("Enter dispatcher at: %p", enter_dispatcher);
    VERBOSE("Exit dispatcher at: %p", exit_dispatcher);
    VERBOSE("Crash target at: %p", crash_handler);
    VERBOSE("Compile next at: %p", compile_next_handler);
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

void print_address(u64 address, int index) {
    PLAIN("Entering block 0x%016lx (%d)", address, index);
}

void* Backend::AddCodeAt(u64 address, void* code, u64 size) {
    void* start = as.GetCursorPointer();
    as.GetCodeBuffer().Emit(code, size);
    flush_icache();
    map[address] = {start, size};
    return start;
}

std::pair<void*, u64> Backend::EmitFunction(const BackendFunction& function, AllocationMap& allocations) {
    void* start = as.GetCursorPointer();
    std::vector<const BackendBlock*> blocks_postorder = function.GetBlocksPostorder();

    struct Jump {
        u32 index;
        ptrdiff_t offset;
        Label* label;
    };

    struct JumpConditional {
        u32 index;
        ptrdiff_t offset;
        biscuit::GPR condition;
        Label* label_true;
        Label* label_false;
    };

    std::vector<Jump> jumps;
    std::vector<JumpConditional> jumps_conditional;

    for (auto it = blocks_postorder.rbegin(); it != blocks_postorder.rend(); it++) {
        const BackendBlock* block = *it;

        // VERBOSE("Block %d (0x%016lx) corresponds to %p", block->GetIndex(), block->GetStartAddress(), as.GetCursorPointer());

        as.Bind(block->GetLabel());

        // Must not insert so many instructions to blocks that are during lr/sc
        if (g_print_block_start && !block->IsCriticalSection()) {
            ASSERT(!g_cache_functions);
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
                jump.index = block->GetIndex();
                jump.offset = as.GetCodeBuffer().GetCursorOffset();
                jump.label = block->GetSuccessor(0)->GetLabel();
                jumps.push_back(jump);
                as.EBREAK(); // AUIPC
                as.EBREAK(); // ADDI
                as.EBREAK(); // JR
            } else if (inst.GetOpcode() == IROpcode::JumpConditional) {
                JumpConditional jump;
                jump.index = block->GetIndex();
                jump.offset = as.GetCodeBuffer().GetCursorOffset();
                jump.condition = allocations.GetAllocation(inst.GetOperand(0)).AsGPR();
                jump.label_true = block->GetSuccessor(0)->GetLabel();
                jump.label_false = block->GetSuccessor(1)->GetLabel();
                jumps_conditional.push_back(jump);

                as.EBREAK(); // BEQZ

                // True jump
                as.EBREAK(); // AUIPC
                as.EBREAK(); // ADDI
                as.EBREAK(); // JR

                // False jump
                as.EBREAK(); // AUIPC
                as.EBREAK(); // ADDI
                as.EBREAK(); // JR
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

    for (auto& [index, offset, label] : jumps) {
        ASSERT_MSG(label->GetLocation().has_value(), "Jump target has no location for block %d", index);

        ptrdiff_t current_offset = as.GetCodeBuffer().GetCursorOffset();
        as.RewindBuffer(offset);

        void* target = (void*)(as.GetCodeBuffer().GetOffsetAddress(*label->GetLocation()));
        u8* here = as.GetCursorPointer();
        if (IsValidJTypeImm((ptrdiff_t)target - (ptrdiff_t)here)) {
            Emitter::EmitJump(*this, label);
        } else {
            Emitter::EmitJumpFar(*this, target);
        }
        u8* after = as.GetCursorPointer();
        ASSERT(after - here <= 4 * 3); // there's 5 instructions worth of space for this backpatched jump

        as.AdvanceBuffer(current_offset);
    }

    for (auto& [index, offset, condition, label_true, label_false] : jumps_conditional) {
        ASSERT_MSG(label_true->GetLocation().has_value(), "True label has no location for block %d", index);
        ASSERT_MSG(label_false->GetLocation().has_value(), "False label has no location for block %d", index);

        ptrdiff_t current_offset = as.GetCodeBuffer().GetCursorOffset();
        as.RewindBuffer(offset);

        void* target_true = (void*)(as.GetCodeBuffer().GetOffsetAddress(*label_true->GetLocation()));
        void* target_false = (void*)(as.GetCodeBuffer().GetOffsetAddress(*label_false->GetLocation()));
        u8* here = as.GetCursorPointer();
        if (IsValidJTypeImm((ptrdiff_t)target_true - (ptrdiff_t)here) && IsValidJTypeImm((ptrdiff_t)target_false - (ptrdiff_t)here)) {
            Emitter::EmitJumpConditional(*this, condition, label_true, label_false);
        } else {
            Emitter::EmitJumpConditionalFar(*this, condition, target_true, target_false);
        }
        u8* after = as.GetCursorPointer();
        ASSERT(after - here <= 4 * 7); // there's 11 instructions worth of space for this backpatched jump

        as.AdvanceBuffer(current_offset);
    }

    // Make code visible to instruction fetches.
    flush_icache();

    return {start, size};
}