#include "felix86/ir/passes/passes.hpp"

void replace_load_guest(SSAInstruction& inst, SSAInstruction* thread_state_pointer) {
    // Break this to a regular load
    const GetGuest& get_guest = inst.AsGetGuest();
    switch (get_guest.ref) {
    case X86_REF_RAX ... X86_REF_R15: {
        Operands op;
        op.operands[0] = thread_state_pointer;
        op.operand_count = 1;
        op.immediate_data = offsetof(ThreadState, gprs[get_guest.ref - X86_REF_RAX]);
        inst.ReplaceDontInvalidate(op, IROpcode::ReadQWordRelative);
        break;
    }
    case X86_REF_XMM0 ... X86_REF_XMM15: {
        Operands op;
        op.operands[0] = thread_state_pointer;
        op.operand_count = 1;
        op.immediate_data = offsetof(ThreadState, xmm[get_guest.ref - X86_REF_XMM0]);
        inst.ReplaceDontInvalidate(op, IROpcode::ReadXmmWordRelative);
        break;
    }
    case X86_REF_ST0 ... X86_REF_ST7: {
        if (inst.GetUseCount() > 1) {
            UNIMPLEMENTED(); // might be used
        }
        WARN("ST0-ST7 are not supported, ignoring"); // will be removed by dce
        break;
    }
    case X86_REF_RIP: {
        Operands op;
        op.operands[0] = thread_state_pointer;
        op.operand_count = 1;
        op.immediate_data = offsetof(ThreadState, rip);
        inst.ReplaceDontInvalidate(op, IROpcode::ReadQWordRelative);
        break;
    }
    case X86_REF_GS: {
        Operands op;
        op.operands[0] = thread_state_pointer;
        op.operand_count = 1;
        op.immediate_data = offsetof(ThreadState, gsbase);
        inst.ReplaceDontInvalidate(op, IROpcode::ReadQWordRelative);
        break;
    }
    case X86_REF_FS: {
        Operands op;
        op.operands[0] = thread_state_pointer;
        op.operand_count = 1;
        op.immediate_data = offsetof(ThreadState, fsbase);
        inst.ReplaceDontInvalidate(op, IROpcode::ReadQWordRelative);
        break;
    }
    case X86_REF_CF: {
        Operands op;
        op.operands[0] = thread_state_pointer;
        op.operand_count = 1;
        op.immediate_data = offsetof(ThreadState, cf);
        inst.ReplaceDontInvalidate(op, IROpcode::ReadByteRelative);
        break;
    }
    case X86_REF_ZF: {
        Operands op;
        op.operands[0] = thread_state_pointer;
        op.operand_count = 1;
        op.immediate_data = offsetof(ThreadState, zf);
        inst.ReplaceDontInvalidate(op, IROpcode::ReadByteRelative);
        break;
    }
    case X86_REF_AF: {
        Operands op;
        op.operands[0] = thread_state_pointer;
        op.operand_count = 1;
        op.immediate_data = offsetof(ThreadState, af);
        inst.ReplaceDontInvalidate(op, IROpcode::ReadByteRelative);
        break;
    }
    case X86_REF_PF: {
        Operands op;
        op.operands[0] = thread_state_pointer;
        op.operand_count = 1;
        op.immediate_data = offsetof(ThreadState, pf);
        inst.ReplaceDontInvalidate(op, IROpcode::ReadByteRelative);
        break;
    }
    case X86_REF_SF: {
        Operands op;
        op.operands[0] = thread_state_pointer;
        op.operand_count = 1;
        op.immediate_data = offsetof(ThreadState, sf);
        inst.ReplaceDontInvalidate(op, IROpcode::ReadByteRelative);
        break;
    }
    case X86_REF_OF: {
        Operands op;
        op.operands[0] = thread_state_pointer;
        op.operand_count = 1;
        op.immediate_data = offsetof(ThreadState, of);
        inst.ReplaceDontInvalidate(op, IROpcode::ReadByteRelative);
        break;
    }
    case X86_REF_COUNT: {
        UNIMPLEMENTED();
        break;
    }
    }
}

void replace_store_guest(SSAInstruction& inst, SSAInstruction* thread_state_pointer) {
    const SetGuest& set_guest = inst.AsSetGuest();
    switch (set_guest.ref) {
    case X86_REF_RAX ... X86_REF_R15: {
        Operands op;
        op.operands[0] = thread_state_pointer;
        op.operands[1] = set_guest.source;
        op.operand_count = 2;
        op.immediate_data = offsetof(ThreadState, gprs[set_guest.ref - X86_REF_RAX]);
        inst.ReplaceDontInvalidate(op, IROpcode::WriteQWordRelative);
        break;
    }
    case X86_REF_XMM0 ... X86_REF_XMM15: {
        Operands op;
        op.operands[0] = thread_state_pointer;
        op.operands[1] = set_guest.source;
        op.operand_count = 2;
        op.immediate_data = offsetof(ThreadState, xmm[set_guest.ref - X86_REF_XMM0]);
        inst.ReplaceDontInvalidate(op, IROpcode::WriteXmmWordRelative);
        break;
    }
    case X86_REF_ST0 ... X86_REF_ST7: {
        UNREACHABLE();
        break;
    }
    case X86_REF_RIP: {
        Operands op;
        op.operands[0] = thread_state_pointer;
        op.operands[1] = set_guest.source;
        op.operand_count = 2;
        op.immediate_data = offsetof(ThreadState, rip);
        inst.ReplaceDontInvalidate(op, IROpcode::WriteQWordRelative);
        break;
    }
    case X86_REF_GS: {
        Operands op;
        op.operands[0] = thread_state_pointer;
        op.operands[1] = set_guest.source;
        op.operand_count = 2;
        op.immediate_data = offsetof(ThreadState, gsbase);
        inst.ReplaceDontInvalidate(op, IROpcode::WriteQWordRelative);
        break;
    }
    case X86_REF_FS: {
        Operands op;
        op.operands[0] = thread_state_pointer;
        op.operands[1] = set_guest.source;
        op.operand_count = 2;
        op.immediate_data = offsetof(ThreadState, fsbase);
        inst.ReplaceDontInvalidate(op, IROpcode::WriteQWordRelative);
        break;
    }
    case X86_REF_CF: {
        Operands op;
        op.operands[0] = thread_state_pointer;
        op.operands[1] = set_guest.source;
        op.operand_count = 2;
        op.immediate_data = offsetof(ThreadState, cf);
        inst.ReplaceDontInvalidate(op, IROpcode::WriteByteRelative);
        break;
    }
    case X86_REF_ZF: {
        Operands op;
        op.operands[0] = thread_state_pointer;
        op.operands[1] = set_guest.source;
        op.operand_count = 2;
        op.immediate_data = offsetof(ThreadState, zf);
        inst.ReplaceDontInvalidate(op, IROpcode::WriteByteRelative);
        break;
    }
    case X86_REF_AF: {
        Operands op;
        op.operands[0] = thread_state_pointer;
        op.operands[1] = set_guest.source;
        op.operand_count = 2;
        op.immediate_data = offsetof(ThreadState, af);
        inst.ReplaceDontInvalidate(op, IROpcode::WriteByteRelative);
        break;
    }
    case X86_REF_PF: {
        Operands op;
        op.operands[0] = thread_state_pointer;
        op.operands[1] = set_guest.source;
        op.operand_count = 2;
        op.immediate_data = offsetof(ThreadState, pf);
        inst.ReplaceDontInvalidate(op, IROpcode::WriteByteRelative);
        break;
    }
    case X86_REF_SF: {
        Operands op;
        op.operands[0] = thread_state_pointer;
        op.operands[1] = set_guest.source;
        op.operand_count = 2;
        op.immediate_data = offsetof(ThreadState, sf);
        inst.ReplaceDontInvalidate(op, IROpcode::WriteByteRelative);
        break;
    }
    case X86_REF_OF: {
        Operands op;
        op.operands[0] = thread_state_pointer;
        op.operands[1] = set_guest.source;
        op.operand_count = 2;
        op.immediate_data = offsetof(ThreadState, of);
        inst.ReplaceDontInvalidate(op, IROpcode::WriteByteRelative);
        break;
    }
    case X86_REF_COUNT: {
        UNREACHABLE();
        break;
    }
    }
}

// On entry blocks we load *all* state from VM (so that each use is dominated by a definition) and
// on exit blocks we store back all state. But if the exit block stores the exact same variable loaded on entry,
// that can be removed.
// We can find out only after moving to SSA and copy propagating the IR mov/set_guest instructions.
void ir_extraneous_writeback_pass(IRFunction* function) {
    std::array<SSAInstruction*, X86_REF_COUNT> entry_defs{};

    IRBlock* entry = function->GetEntry();
    for (auto& inst : entry->GetInstructions()) {
        if (inst.GetOpcode() == IROpcode::LoadGuestFromMemory) {
            entry_defs[inst.AsGetGuest().ref] = &inst;
            replace_load_guest(inst, function->ThreadStatePointer());
        }
    }

    for (IRBlock* block : function->GetBlocks()) {
        std::list<SSAInstruction>& insts = block->GetInstructions();
        auto it = insts.begin();
        auto end = insts.end();
        while (it != end) {
            SSAInstruction& inst = *it;
            if (inst.GetOpcode() == IROpcode::StoreGuestToMemory) {
                const SetGuest& set_guest = inst.AsSetGuest();
                if (entry_defs[set_guest.ref] == set_guest.source) {
                    // It's the same one that was loaded in entry block, store can be removed
                    inst.Unlock();
                    inst.Invalidate();
                    it = insts.erase(it);
                    continue;
                } else {
                    // Replace with write to memory while we are at it
                    replace_store_guest(inst, function->ThreadStatePointer());
                }
            }
            it++;
        }
    }
}