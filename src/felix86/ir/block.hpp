#pragma once

#include <array>
#include <list>
#include "felix86/common/utility.hpp"
#include "felix86/ir/instruction.hpp"

#define IR_NO_ADDRESS (-1ull)

enum class Termination {
    Null,
    Jump,
    JumpConditional,
    Exit,
};

struct IRBlock {
    using iterator = std::list<IRInstruction>::iterator;

    void TerminateJump(IRBlock* target) {
        termination = Termination::Jump;
        successors[0] = target;
    }

    void TerminateJumpConditional(IRBlock* target_true, IRBlock* target_false) {
        termination = Termination::JumpConditional;
        successors[0] = target_true;
        successors[1] = target_false;
    }

    void TerminateExit() {
        termination = Termination::Exit;
    }

    iterator InsertAfter(iterator pos, IRInstruction&& instr) {
        return instructions.insert(std::next(pos), std::move(instr));
    }

    iterator InsertBefore(iterator pos, IRInstruction&& instr) {
        return instructions.insert(pos, std::move(instr));
    }

    void InsertAtEnd(IRInstruction&& instr) {
        instructions.push_back(std::move(instr));
    }

    iterator EraseAndUndoUses(iterator pos) {
        (*pos).UndoUses();
        return instructions.erase(pos);
    }

    void AddPredecessor(IRBlock* pred) {
        predecessors.push_back(pred);
    }

private:
    std::list<IRInstruction> instructions;
    std::vector<IRBlock*> predecessors;
    std::array<IRBlock*, 2> successors = {nullptr, nullptr};
    Termination termination = Termination::Null;
};

struct IRFunction {
    IRFunction();

    IRBlock* GetEntry() {
        return entry;
    }

    IRBlock* GetExit() {
        return exit;
    }

    IRBlock* GetBlockAt(IRBlock* predecessor, u64 address);

    IRBlock* GetBlock(IRBlock* predecessor);

private:
    IRBlock* entry = nullptr;
    IRBlock* exit = nullptr;
    std::vector<IRBlock> blocks;
};