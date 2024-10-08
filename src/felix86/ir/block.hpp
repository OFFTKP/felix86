#pragma once

#include <array>
#include <functional>
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

    using iterator = std::list<IRInstruction>::iterator;

    void TerminateExit() {
        termination = Termination::Exit;
    }

    IRInstruction* InsertAtEnd(IRInstruction&& instr);

    const IRInstruction* GetCondition() const {
        return condition;
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

    u32 GetNextName() {
        if (GetIndex() > ((1 << 12) - 1)) {
            ERROR("Too many blocks for this to work");
        }

        if (next_instruction_name > ((1 << 20) - 1)) {
            ERROR("Too many instructions for this to work");
        }

        return (GetIndex() << 20) | (next_instruction_name++);
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

    std::span<IRBlock* const> GetSuccessors() const {
        if (termination == Termination::Jump) {
            return {&successors[0], 1};
        } else if (termination == Termination::JumpConditional) {
            return {&successors[0], 2};
        } else {
            return {};
        }
    }

    void ReplaceSuccessor(IRBlock* old_block, IRBlock* new_block) {
        for (size_t i = 0; i < successors.size(); i++) {
            if (successors[i] == old_block) {
                successors[i] = new_block;
                return;
            }
        }

        ERROR("Block is not a successor of the other block");
    }

    // The IRFunction::ValidatePhis should guarantee that phis only appear at the start of the block
    bool HasPhis() const {
        return !instructions.empty() && instructions.front().IsPhi();
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

    void RemovePredecessor(IRBlock* pred) {
        for (size_t i = 0; i < predecessors.size(); i++) {
            if (predecessors[i] == pred) {
                predecessors.erase(predecessors.begin() + i);
                return;
            }
        }

        ERROR("Block is not a predecessor of the other block");
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

    std::vector<IRBlock*>& GetDominanceFrontiers() {
        return dominance_frontiers;
    }

    void AddDominanceFrontier(IRBlock* block) {
        dominance_frontiers.push_back(block);
    }

    void AddPhi(IRInstruction&& instr) {
        instructions.push_front(std::move(instr));
    }

    bool IsUsedInPhi(IRInstruction* instr) const;

    std::string Print(const std::function<std::string(const IRInstruction*)>& callback) const;

private:
    void AddPredecessor(IRBlock* pred) {
        predecessors.push_back(pred);
    }

    u64 start_address = IR_NO_ADDRESS;
    std::list<IRInstruction> instructions;
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
    u32 next_instruction_name = 0;
};
