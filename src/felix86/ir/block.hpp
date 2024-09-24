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

        successors[0]->AddPredecessor(this);
    }

    void TerminateJumpConditional(IRInstruction* condition, IRBlock* target_true, IRBlock* target_false) {
        termination = Termination::JumpConditional;
        successors[0] = target_true;
        successors[1] = target_false;
        this->condition = condition;
        condition->AddUse();

        successors[0]->AddPredecessor(this);
        successors[1]->AddPredecessor(this);
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

    iterator EraseAndInvalidate(iterator pos) {
        (*pos).Invalidate();
        return instructions.erase(pos);
    }

    void AddPredecessor(IRBlock* pred) {
        predecessors.push_back(pred);
    }

    IRInstruction& GetLastInstruction() {
        return instructions.back();
    }

private:
    std::list<IRInstruction> instructions;
    std::vector<IRBlock*> predecessors;
    std::array<IRBlock*, 2> successors = {nullptr, nullptr};
    Termination termination = Termination::Null;
    IRInstruction* condition = nullptr;
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