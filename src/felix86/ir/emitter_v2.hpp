#pragma once

#include "felix86/common/utility.hpp"
#include "felix86/ir/block.hpp"

struct IREmitter {
    IREmitter(IRBlock& block, u64 address) : block(&block), current_address(address) {}

    void Exit() {
        exit = true;
    }

    bool IsExit() const {
        return exit;
    }

    void IncrementAddress(u64 increment) {
        current_address += increment;
    }

    SSAInstruction* GetReg(x86_ref_e reg, x86_size_e size, bool high = false);
    SSAInstruction* GetReg(const x86_operand_t& operand) {
        ASSERT(operand.type == X86_OP_TYPE_REGISTER);
        return GetReg(operand.reg.ref, operand.size);
    }
    SSAInstruction* GetReg(x86_ref_e reg) {
        return getGuest(reg);
    }

    void SetReg(SSAInstruction* value, x86_ref_e reg, x86_size_e size, bool high = false);
    void SetReg(const x86_operand_t& operand, SSAInstruction* value) {
        ASSERT(operand.type == X86_OP_TYPE_REGISTER);
        SetReg(value, operand.reg.ref, operand.size);
    }
    void SetReg(SSAInstruction* value, x86_ref_e reg) {
        setGuest(reg, value);
    }

    SSAInstruction* GetFlag(x86_ref_e flag);
    SSAInstruction* GetFlagNot(x86_ref_e flag);
    void SetFlag(SSAInstruction* value, x86_ref_e flag);

    SSAInstruction* GetRm(const x86_operand_t& operand);
    void SetRm(const x86_operand_t& operand, SSAInstruction* value);

    SSAInstruction* ReadMemory(SSAInstruction* address, x86_size_e size);
    void WriteMemory(SSAInstruction* address, SSAInstruction* value, x86_size_e size);

    SSAInstruction* LoadGuestFromMemory(x86_ref_e ref);
    void StoreGuestToMemory(SSAInstruction* value, x86_ref_e ref);

    SSAInstruction* Imm(u64 value);
    SSAInstruction* Add(SSAInstruction* lhs, SSAInstruction* rhs);
    SSAInstruction* Addi(SSAInstruction* lhs, i64 rhs);
    SSAInstruction* Sub(SSAInstruction* lhs, SSAInstruction* rhs);
    SSAInstruction* Shl(SSAInstruction* lhs, SSAInstruction* rhs);
    SSAInstruction* Shli(SSAInstruction* lhs, i64 rhs);
    SSAInstruction* Shr(SSAInstruction* lhs, SSAInstruction* rhs);
    SSAInstruction* Shri(SSAInstruction* lhs, i64 rhs);
    SSAInstruction* Sar(SSAInstruction* lhs, SSAInstruction* rhs);
    SSAInstruction* Sari(SSAInstruction* lhs, i64 rhs);
    SSAInstruction* Rol(SSAInstruction* lhs, SSAInstruction* rhs, x86_size_e size);
    SSAInstruction* Ror(SSAInstruction* lhs, SSAInstruction* rhs, x86_size_e size);
    SSAInstruction* Select(SSAInstruction* cond, SSAInstruction* true_value, SSAInstruction* false_value);
    SSAInstruction* Clz(SSAInstruction* value);
    SSAInstruction* Ctz(SSAInstruction* value);
    SSAInstruction* Ctzh(SSAInstruction* value);
    SSAInstruction* Ctzw(SSAInstruction* value);
    SSAInstruction* Parity(SSAInstruction* value);
    SSAInstruction* And(SSAInstruction* lhs, SSAInstruction* rhs);
    SSAInstruction* Andi(SSAInstruction* lhs, u64 rhs);
    SSAInstruction* Or(SSAInstruction* lhs, SSAInstruction* rhs);
    SSAInstruction* Ori(SSAInstruction* lhs, u64 rhs);
    SSAInstruction* Xor(SSAInstruction* lhs, SSAInstruction* rhs);
    SSAInstruction* Xori(SSAInstruction* lhs, u64 rhs);
    SSAInstruction* Not(SSAInstruction* value);
    SSAInstruction* Neg(SSAInstruction* value);
    SSAInstruction* Mul(SSAInstruction* lhs, SSAInstruction* rhs);
    SSAInstruction* Mulh(SSAInstruction* lhs, SSAInstruction* rhs);
    SSAInstruction* Mulhu(SSAInstruction* lhs, SSAInstruction* rhs);
    SSAInstruction* Div(SSAInstruction* lhs, SSAInstruction* rhs);
    SSAInstruction* Divu(SSAInstruction* lhs, SSAInstruction* rhs);
    SSAInstruction* Rem(SSAInstruction* lhs, SSAInstruction* rhs);
    SSAInstruction* Remu(SSAInstruction* lhs, SSAInstruction* rhs);
    SSAInstruction* Divw(SSAInstruction* lhs, SSAInstruction* rhs);
    SSAInstruction* Divuw(SSAInstruction* lhs, SSAInstruction* rhs);
    SSAInstruction* Remw(SSAInstruction* lhs, SSAInstruction* rhs);
    SSAInstruction* Remuw(SSAInstruction* lhs, SSAInstruction* rhs);
    SSAInstruction* Seqz(SSAInstruction* value);
    SSAInstruction* Snez(SSAInstruction* value);
    SSAInstruction* Equal(SSAInstruction* lhs, SSAInstruction* rhs);
    SSAInstruction* NotEqual(SSAInstruction* lhs, SSAInstruction* rhs);
    SSAInstruction* LessThanSigned(SSAInstruction* lhs, SSAInstruction* rhs);
    SSAInstruction* LessThanUnsigned(SSAInstruction* lhs, SSAInstruction* rhs);
    SSAInstruction* GreaterThanSigned(SSAInstruction* lhs, SSAInstruction* rhs);
    SSAInstruction* GreaterThanUnsigned(SSAInstruction* lhs, SSAInstruction* rhs);
    SSAInstruction* Sext(SSAInstruction* value, x86_size_e size);
    SSAInstruction* Zext(SSAInstruction* value, x86_size_e size);
    SSAInstruction* AmoAdd(SSAInstruction* address, SSAInstruction* source, MemoryOrdering ordering, x86_size_e size);
    SSAInstruction* AmoAnd(SSAInstruction* address, SSAInstruction* source, MemoryOrdering ordering, x86_size_e size);
    SSAInstruction* AmoOr(SSAInstruction* address, SSAInstruction* source, MemoryOrdering ordering, x86_size_e size);
    SSAInstruction* AmoXor(SSAInstruction* address, SSAInstruction* source, MemoryOrdering ordering, x86_size_e size);
    SSAInstruction* AmoSwap(SSAInstruction* address, SSAInstruction* source, MemoryOrdering ordering, x86_size_e size);
    SSAInstruction* AmoCAS(SSAInstruction* address, SSAInstruction* expected, SSAInstruction* source, MemoryOrdering ordering, x86_size_e size);
    SSAInstruction* VUnpackByteLow(SSAInstruction* lhs, SSAInstruction* rhs);
    SSAInstruction* VUnpackWordLow(SSAInstruction* lhs, SSAInstruction* rhs);
    SSAInstruction* VUnpackDWordLow(SSAInstruction* lhs, SSAInstruction* rhs);
    SSAInstruction* VUnpackQWordLow(SSAInstruction* lhs, SSAInstruction* rhs);
    SSAInstruction* VPackedEqualByte(SSAInstruction* lhs, SSAInstruction* rhs);
    SSAInstruction* VPackedEqualWord(SSAInstruction* lhs, SSAInstruction* rhs);
    SSAInstruction* VPackedEqualDWord(SSAInstruction* lhs, SSAInstruction* rhs);
    SSAInstruction* VPackedEqualQWord(SSAInstruction* lhs, SSAInstruction* rhs);
    SSAInstruction* VPackedAddByte(SSAInstruction* lhs, SSAInstruction* rhs);
    SSAInstruction* VPackedAddWord(SSAInstruction* lhs, SSAInstruction* rhs);
    SSAInstruction* VPackedAddDWord(SSAInstruction* lhs, SSAInstruction* rhs);
    SSAInstruction* VPackedAddQWord(SSAInstruction* lhs, SSAInstruction* rhs);
    SSAInstruction* VPackedSubByte(SSAInstruction* lhs, SSAInstruction* rhs);
    SSAInstruction* VPackedMinByte(SSAInstruction* lhs, SSAInstruction* rhs);
    SSAInstruction* VMoveByteMask(SSAInstruction* value);
    SSAInstruction* VPackedShuffleDWord(SSAInstruction* value, u8 shuffle);
    SSAInstruction* VPackedShr(SSAInstruction* lhs, SSAInstruction* rhs);
    SSAInstruction* VAnd(SSAInstruction* lhs, SSAInstruction* rhs);
    SSAInstruction* VOr(SSAInstruction* lhs, SSAInstruction* rhs);
    SSAInstruction* VXor(SSAInstruction* lhs, SSAInstruction* rhs);
    SSAInstruction* IToV(SSAInstruction* value);
    SSAInstruction* VToI(SSAInstruction* value);
    SSAInstruction* Lea(const x86_operand_t& operand);
    SSAInstruction* GetFlags();
    SSAInstruction* IsZero(SSAInstruction* value, x86_size_e size);
    SSAInstruction* IsNegative(SSAInstruction* value, x86_size_e size);
    SSAInstruction* IsCarryAdd(SSAInstruction* source, SSAInstruction* result, x86_size_e size);
    SSAInstruction* IsAuxAdd(SSAInstruction* source1, SSAInstruction* source2);
    SSAInstruction* IsOverflowAdd(SSAInstruction* source1, SSAInstruction* source2, SSAInstruction* result, x86_size_e size_e);
    SSAInstruction* IsCarrySub(SSAInstruction* source1, SSAInstruction* source2);
    SSAInstruction* IsAuxSub(SSAInstruction* source1, SSAInstruction* source2);
    SSAInstruction* IsOverflowSub(SSAInstruction* source1, SSAInstruction* source2, SSAInstruction* result, x86_size_e size_e);
    void Group1(x86_instruction_t* inst);
    void Group2(x86_instruction_t* inst, SSAInstruction* shift_amount);
    void Syscall();
    void Cpuid();
    void Rdtsc();
    void SetCPAZSO(SSAInstruction* c, SSAInstruction* p, SSAInstruction* a, SSAInstruction* z, SSAInstruction* s, SSAInstruction* o);
    void SetExitReason(ExitReason reason);

    void RepStart(IRBlock* loop_block, IRBlock* exit_block);
    void RepEnd(x86_rep_e rep_type, IRBlock* loop_block, IRBlock* exit_block);

    void TerminateJump(IRBlock* target);
    void TerminateJumpConditional(SSAInstruction* cond, IRBlock* target_true, IRBlock* target_false);

    u64 GetCurrentAddress() {
        return current_address;
    }

    void SetBlock(IRBlock* block) {
        ASSERT(block);
        this->block = block;
    }

private:
    SSAInstruction* getGuest(x86_ref_e ref);
    SSAInstruction* setGuest(x86_ref_e ref, SSAInstruction* value);
    SSAInstruction* getGpr8Low(x86_ref_e reg);
    SSAInstruction* getGpr8High(x86_ref_e reg);
    SSAInstruction* getGpr16(x86_ref_e reg);
    SSAInstruction* getGpr32(x86_ref_e reg);
    SSAInstruction* getGpr64(x86_ref_e reg);
    SSAInstruction* getVector(x86_ref_e reg);
    SSAInstruction* readByte(SSAInstruction* address);
    SSAInstruction* readWord(SSAInstruction* address);
    SSAInstruction* readDWord(SSAInstruction* address);
    SSAInstruction* readQWord(SSAInstruction* address);
    SSAInstruction* readXmmWord(SSAInstruction* address);
    void setGpr8Low(x86_ref_e reg, SSAInstruction* value);
    void setGpr8High(x86_ref_e reg, SSAInstruction* value);
    void setGpr16(x86_ref_e reg, SSAInstruction* value);
    void setGpr32(x86_ref_e reg, SSAInstruction* value);
    void setGpr64(x86_ref_e reg, SSAInstruction* value);
    void setVector(x86_ref_e reg, SSAInstruction* value);
    void writeByte(SSAInstruction* address, SSAInstruction* value);
    void writeWord(SSAInstruction* address, SSAInstruction* value);
    void writeDWord(SSAInstruction* address, SSAInstruction* value);
    void writeQWord(SSAInstruction* address, SSAInstruction* value);
    void writeXmmWord(SSAInstruction* address, SSAInstruction* value);
    SSAInstruction* getSignMask(x86_size_e size);
    SSAInstruction* insertInstruction(IROpcode opcode, std::initializer_list<SSAInstruction*> operands);
    SSAInstruction* insertInstruction(IROpcode opcode, std::initializer_list<SSAInstruction*> operands, u64 immediate);

    void loadPartialState(std::span<const x86_ref_e> refs);
    void storePartialState(std::span<const x86_ref_e> refs);
    u16 getBitSize(x86_size_e size);

    IRBlock* block = nullptr;

    u64 current_address = 0;
    bool exit = false;
};