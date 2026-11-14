#include "felix86/common/frame.hpp"
#include "felix86/common/log.hpp"
#include "felix86/common/state.hpp"
#include "felix86/common/utility.hpp"
#include "felix86/v2/allocations.hpp"
#include "felix86/v2/common_emit.hpp"
#include "felix86/v2/dispatcher.hpp"
#include "felix86/v2/recompiler.hpp"

static void incorrect_magic(void* sp) {
    ERROR("Incorrect magic in frame (sp: %p)", sp);
}

static void incorrect_stack(void* sp_expected, void* sp_actual) {
    ERROR("Incorrect stack in frame, expected %p, but got %p", sp_expected, sp_actual);
}

static void emitBadAddressThunks(biscuit::Assembler& as, CodeCachePointers& pointers) {
    while ((u64)as.GetCursorPointer() & 0xF) {
        as.C_NOP();
    }

    // The signal handler will find the RIP via the allocated ripreg as it tried
    // to compile a block there
    pointers.not_mapped = (u64)as.GetCursorPointer();
    as.SD(x0, 0, x0);
    as.SLTIU(x0, x0, FELIX86_HINT_NOT_MAPPED);
    as.C_UNDEF();
    as.C_UNDEF();

    pointers.not_read = (u64)as.GetCursorPointer();
    as.SD(x0, 0, x0);
    as.SLTIU(x0, x0, FELIX86_HINT_NOT_READ);
    as.C_UNDEF();
    as.C_UNDEF();

    pointers.not_exec = (u64)as.GetCursorPointer();
    as.SD(x0, 0, x0);
    as.SLTIU(x0, x0, FELIX86_HINT_NOT_EXEC);
    as.C_UNDEF();
    as.C_UNDEF();
}

static void emitInterruptibleSyscallThunk(biscuit::Assembler& as, CodeCachePointers& pointers) {
    while ((u64)as.GetCursorPointer() & 0xF) {
        as.C_NOP();
    }

    // This thunk is used to have a straight line sequence of code that first checks if any signals are deferred, then
    // loads the arguments and calls ECALL. Even if a signal happens *after* the check and before the ECALL,
    // it is able to verify it is between these two points and jump to the -EINTR return from the signal handler.
    pointers.interruptible_syscall = (InterruptibleSyscallFunc)as.GetCursorPointer();
    biscuit::Label eintr;
    as.LD(t0, 0, a0);
    as.MV(t1, a1);
    as.MV(a0, a2);
    as.MV(a1, a3);
    as.MV(a2, a4);
    as.MV(a3, a5);
    as.MV(a4, a6);
    as.MV(a5, a7);
    as.MV(a7, t1);
    as.BNEZ(t0, &eintr);
    pointers.interruptible_syscall_ecall = (u64)as.GetCursorPointer();
    as.ECALL();
    as.RET();
    as.Bind(&eintr);

    pointers.interruptible_syscall_eintr = (u64)as.GetCursorPointer();
    as.LI(a0, -FELIX86_ERESTARTNOINTR);
    as.RET();
}

static void emitDispatcherCore(biscuit::Assembler& as, CodeCachePointers& pointers, bool relocatable) {
    ScratchAllocator scratches;
    pointers.enter_dispatcher = (decltype(pointers.enter_dispatcher))as.GetCursorPointer();

    // Save the current frame in the stack
    // The size of felix86_frame is bigger than the red zone, so we need to decrement the stack instead of using a temporary
    // This is because a signal could technically thrash values outside the red zone if we don't decrement the stack pointer here
    as.MV(t0, sp);
    as.ADDI(sp, sp, -(int)sizeof(felix86_frame));
    for (size_t i = 0; i < saved_gprs.size(); i++) {
        if (saved_gprs[i] != sp) {
            as.SD(saved_gprs[i], offsetof(felix86_frame, gprs) + i * sizeof(u64), sp);
        } else {
            // Use t0 instead of sp to save our old stack
            as.SD(t0, offsetof(felix86_frame, gprs) + i * sizeof(u64), sp);
        }
    }

    // Save the ThreadState pointer
    as.SD(a0, offsetof(felix86_frame, state), sp);

    // Also save the magic number
    as.LI(t1, felix86_frame::expected_magic);
    as.SD(t1, offsetof(felix86_frame, magic), sp);

    biscuit::Label invalidate_caller_thunk;

    as.LILabel(t1, &invalidate_caller_thunk);
    as.SD(t1, offsetof(felix86_frame, invalidate_caller_thunk_ptr), sp);

    biscuit::Label compile_next_label, compile_next_32_label;
    biscuit::Label compile_next_skip_address_cache_label;

    as.LILabel(t1, &compile_next_label);
    as.SD(t1, offsetof(felix86_frame, compile_next_handler_ptr), sp);

    as.LILabel(t1, &compile_next_32_label);
    as.SD(t1, offsetof(felix86_frame, compile_next_32_handler_ptr), sp);

    as.LILabel(t1, &compile_next_skip_address_cache_label);
    as.SD(t1, offsetof(felix86_frame, compile_next_skip_address_cache_ptr), sp);

    as.MV(Allocations::threadStatePointer(), a0);

    pointers.restore_state = (u64)as.GetCursorPointer();

    CommonEmit::restoreGuestState(as, scratches, true);
    biscuit::GPR cs = scratches.scratch();
    as.LHU(cs, offsetof(ThreadState, ctx.cs), Allocations::threadStatePointer());
    as.ADDI(cs, cs, -0x23);
    as.BEQZ(cs, &compile_next_32_label);
    scratches.popScratch();

    // Align this to cache line boundary since it's hit often
    while ((u64)as.GetCursorPointer() & 0x3F) {
        as.NOP();
    }

    as.Bind(&compile_next_label);
    pointers.compile_next = (u64)as.GetCursorPointer();

    biscuit::GPR ripreg = Allocations::RIP();
    CommonEmit::addressCacheLookup(as, scratches, ripreg, [](Assembler& as, biscuit::GPR new_rip) { as.JR(new_rip); }, false);

    as.Bind(&compile_next_skip_address_cache_label);
    pointers.compile_next_skip_address_cache = (u64)as.GetCursorPointer();

    CommonEmit::writebackGuestState(as, scratches);
    as.MV(a0, Allocations::threadStatePointer());
    CommonEmit::call(as, (u64)Recompiler::compileNext, relocatable);

    biscuit::GPR retval = scratches.scratch();
    as.MV(retval, a0);
    CommonEmit::restoreGuestState(as, scratches, true);
    as.MV(t5, x0); // zero out t5, see invalidate_caller_thunk
    as.JR(retval);
    scratches.popScratch();

    while ((u64)as.GetCursorPointer() & 0x3F) {
        as.NOP();
    }

    as.Bind(&compile_next_32_label);
    pointers.compile_next_32 = (u64)as.GetCursorPointer();
    CommonEmit::addressCacheLookup(as, scratches, ripreg, [](Assembler& as, biscuit::GPR new_rip) { as.JR(new_rip); }, true);
    as.J(&compile_next_skip_address_cache_label);

    pointers.exit_dispatcher = (decltype(pointers.exit_dispatcher))as.GetCursorPointer();

    // Move stack pointer to current frame (passed as an argument)
    as.MV(sp, a0);

    // Load ThreadState* into t4
    as.LD(t4, offsetof(felix86_frame, state), a0);

    // Load the frame we had before entering the dispatcher
    // First make sure our magic is correct
    biscuit::Label magic_correct;
    as.LD(t1, offsetof(felix86_frame, magic), sp);
    as.LI(t2, felix86_frame::expected_magic);
    as.BEQ(t1, t2, &magic_correct);

    // Magic is incorrect if we get here
    as.MV(a0, sp);
    as.LI(t0, (u64)incorrect_magic);
    as.JALR(t0);
    as.GetCodeBuffer().Emit32(0);

    as.Bind(&magic_correct);

    for (size_t i = 0; i < saved_gprs.size(); i++) {
        if (saved_gprs[i] != sp) {
            as.LD(saved_gprs[i], offsetof(felix86_frame, gprs) + i * sizeof(u64), sp);
        } else {
            // Load the new stack pointer in t0 and set it later
            as.LD(t0, offsetof(felix86_frame, gprs) + i * sizeof(u64), sp);
        }
    }

    // Sanity check that the stack is just sp + sizeof(felix86_frame)
    biscuit::Label stack_correct;
    as.ADDI(t1, sp, sizeof(felix86_frame));
    as.BEQ(t1, t0, &stack_correct);

    // Stack pointer is incorrect if we get here
    as.MV(a0, sp);
    as.MV(a1, t0);
    as.LI(t0, (u64)incorrect_stack);
    as.JALR(t0);
    as.GetCodeBuffer().Emit32(0);

    as.Bind(&stack_correct);
    as.MV(sp, t0);

    // Return to wherever the dispatcher was originally entered from using enter_dispatcher
    as.JR(ra);

    as.Bind(&invalidate_caller_thunk);

    as.ADDI(sp, sp, -16);
    // All block links set the return address in t5, and we collect it here
    // The dispatcher itself sets t5 to 0 to signal that the jump comes from there
    as.SD(t5, 0, sp);
    // Contains the RISC-V PC of the block that gets invalidated
    as.SD(ra, 8, sp);

    // Call the invalidateAt function which will remove the block from the map
    // when it's called and it will go back to the dispatcher, which will trigger a new compilation
    CommonEmit::writebackGuestState(as, scratches);
    as.MV(a0, Allocations::threadStatePointer());
    as.LD(a1, 0, sp);
    as.LD(a2, 8, sp);
    as.ADDI(a2, a2, -4); // JALR links to instruction right after, go back 4 bytes
    as.ADDI(sp, sp, 16);
    CommonEmit::call(as, (u64)Recompiler::invalidateAt, relocatable);
    CommonEmit::restoreGuestState(as, scratches, true); // TODO: Jump to a skip writeback place in dispatcher
    as.J(&compile_next_skip_address_cache_label);
}

u8* Dispatcher::emitDispatcher(u8* buffer, u64 size, CodeCachePointers& pointers, bool relocatable) {
    biscuit::Assembler as(buffer, size);
    u64 start = (u64)as.GetCursorPointer();
    emitInterruptibleSyscallThunk(as, pointers);
    emitDispatcherCore(as, pointers, relocatable);
    emitBadAddressThunks(as, pointers);

    flush_icache(start, (u64)as.GetCursorPointer());
    return as.GetCursorPointer();
}
