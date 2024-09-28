#pragma once

#include <array>
#include <list>
#include "felix86/common/utility.hpp"
#include "felix86/ir/instruction.hpp"
#include "tsl/robin_map.h"

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

    IRInstruction& GetLastInstruction() {
        return instructions.back();
    }

    bool IsCompiled() {
        return compiled;
    }

    void SetCompiled() {
        compiled = true;
    }

    u64 GetStartAddress() {
        return start_address;
    }

    bool IsVisited() {
        return visited;
    }

    void SetVisited(bool value) {
        visited = value;
    }

    u32 GetIndex() {
        return list_index;
    }

    void SetIndex(u32 index) {
        list_index = index;
    }

    u32 GetPostorderIndex() {
        return postorder_index;
    }

    void SetPostorderIndex(u32 index) {
        postorder_index = index;
    }

    IRBlock* GetSuccessor(bool index) {
        return successors[index];
    }

    IRBlock* GetImmediateDominator() {
        return idom;
    }

    void SetImmediateDominator(IRBlock* block) {
        idom = block;
    }

    std::vector<IRBlock*>& GetPredecessors() {
        return predecessors;
    }
    
    std::list<IRInstruction>& GetInstructions() {
        return instructions;
    }

    std::vector<IRBlock*>& GetDominanceFrontiers() {
        return dominance_frontiers;
    }

    void AddDominanceFrontier(IRBlock* block) {
        dominance_frontiers.push_back(block);
    }

private:
    void AddPredecessor(IRBlock* pred) {
        predecessors.push_back(pred);
    }

    u64 start_address = IR_NO_ADDRESS;
    std::list<IRInstruction> instructions;
    std::vector<IRBlock*> predecessors;
    std::array<IRBlock*, 2> successors = {nullptr, nullptr};
    std::vector<IRBlock*> dominance_frontiers;
    IRBlock* idom = nullptr; // immediate dominator
    Termination termination = Termination::Null;
    IRInstruction* condition = nullptr;
    bool compiled = false;
    bool visited = false;
    u32 list_index = 0; // TODO: remove if unnecessary
    u32 postorder_index = 0;
};

struct IRFunction {
    IRFunction(u64 address);

    IRBlock* GetEntry() {
        return entry;
    }

    IRBlock* GetExit() {
        return exit;
    }

    IRBlock* CreateBlockAt(u64 address);

    IRBlock* GetBlockAt(u64 address);

    IRBlock* CreateBlock();

    std::vector<IRBlock>& GetBlocks() {
        return blocks;
    }

private:
    IRBlock* entry = nullptr;
    IRBlock* exit = nullptr;
    std::vector<IRBlock> blocks;
    tsl::robin_map<u64, IRBlock*> block_map;
};