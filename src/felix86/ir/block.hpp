#pragma once

#include <array>
#include <list>
#include "felix86/common/utility.hpp"
#include "felix86/ir/instruction.hpp"

#define IR_NO_ADDRESS (0)

enum class Termination {
    Null,
    Jump,
    JumpConditional,
    Exit,
};

struct IRBlock {
    IRBlock() = default;
    IRBlock(u64 address) : start_address(address) {}

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
        condition->Lock(); // this is used by the termination, don't optimize away

        successors[0]->AddPredecessor(this);
        successors[1]->AddPredecessor(this);
    }

    void TerminateExit() {
        termination = Termination::Exit;
    }

    IRInstruction* InsertAtEnd(IRInstruction&& instr) {
        instructions.push_back(std::move(instr));
        return &instructions.back();
    }

    bool IsCompiled() const {
        return compiled;
    }

    void SetCompiled() {
        compiled = true;
    }

    u64 GetStartAddress() const {
        return start_address;
    }

    bool IsVisited() const {
        return visited;
    }

    void SetVisited(bool value) const {
        visited = value;
    }

    u32 GetIndex() const {
        return list_index;
    }

    void SetIndex(u32 index) {
        list_index = index;
    }

    u32 GetPostorderIndex() const {
        return postorder_index;
    }

    void SetPostorderIndex(u32 index) {
        postorder_index = index;
    }

    IRBlock* GetSuccessor(bool index) {
        return successors[index];
    }

    const IRBlock* GetSuccessor(bool index) const {
        return successors[index];
    }

    IRBlock* GetImmediateDominator() {
        return immediate_dominator;
    }

    void SetImmediateDominator(IRBlock* block) {
        immediate_dominator = block;
    }

    std::vector<IRBlock*>& GetPredecessors() {
        return predecessors;
    }

    Termination GetTermination() const {
        return termination;
    }

    std::list<IRInstruction>& GetInstructions() {
        return instructions;
    }

    const std::list<IRInstruction>& GetInstructions() const {
        return instructions;
    }

    std::vector<IRInstruction>& GetPhiInstructions() {
        return phi_instructions;
    }

    std::vector<IRBlock*>& GetDominanceFrontiers() {
        return dominance_frontiers;
    }

    void AddDominanceFrontier(IRBlock* block) {
        dominance_frontiers.push_back(block);
    }

    void AddPhi(IRInstruction&& instr) {
        phi_instructions.push_back(std::move(instr));
    }

    std::string Print() const;

private:
    void AddPredecessor(IRBlock* pred) {
        predecessors.push_back(pred);
    }

    u64 start_address = IR_NO_ADDRESS;
    std::list<IRInstruction> instructions;
    std::vector<IRInstruction> phi_instructions;
    std::vector<IRBlock*> predecessors;
    std::array<IRBlock*, 2> successors = {nullptr, nullptr};
    std::vector<IRBlock*> dominance_frontiers;
    IRBlock* immediate_dominator = nullptr;
    Termination termination = Termination::Null;
    IRInstruction* condition = nullptr;
    bool compiled = false;
    mutable bool visited = false;
    u32 list_index = 0;
    u32 postorder_index = 0;
};
