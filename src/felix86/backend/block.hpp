#pragma once

#include <list>
#include "felix86/backend/instruction.hpp"
#include "felix86/common/termination.hpp"
#include "felix86/common/utility.hpp"
#include "felix86/ir/block.hpp"

struct NamedPhi {
    const Phi* phi = nullptr;
    u32 name = 0;
};

struct BackendBlock {
    static BackendBlock FromIRBlock(const IRBlock* block, std::vector<NamedPhi>& phis);

    const std::list<BackendInstruction>& GetInstructions() const {
        return instructions;
    }

    std::list<BackendInstruction>& GetInstructions() {
        return instructions;
    }

    u32 GetSuccessorCount() const {
        u8 count = 0;
        for (int i = 0; i < 2; i++) {
            if (successors[i] != nullptr) {
                count++;
            } else {
                break;
            }
        }
        return count;
    }

    u32 GetPredecessorCount() const {
        return predecessors.size();
    }

    const std::array<BackendBlock*, 2>& GetSuccessors() const {
        return successors;
    }

    const std::vector<BackendBlock*>& GetPredecessors() const {
        return predecessors;
    }

    BackendBlock* GetSuccessor(u32 index) const {
        return successors[index];
    }

    void SetSuccessor(u32 index, BackendBlock* value) {
        successors[index] = value;
    }

    Label* GetLabel() const {
        return &label;
    }

    BackendBlock* GetPredecessor(u32 index) const {
        return predecessors[index];
    }

    void AddPredecessor(BackendBlock* value) {
        predecessors.push_back(value);
    }

    void SetPredecessor(u32 index, BackendBlock* value) {
        predecessors[index] = value;
    }

    u32 GetIndex() const {
        return list_index;
    }

    void SetIndex(u32 index) {
        list_index = index;
    }

    void InsertAtEnd(BackendInstruction&& instruction) {
        instructions.push_back(std::move(instruction));
    }

    u32 GetNextName() {
        ASSERT(next_name != 0);
        return (list_index << 20) | (next_name++);
    }

    u64 GetStartAddress() const {
        return start_address;
    }

    void SetVisited(bool value) const {
        visited = value;
    }

    bool IsVisited() const {
        return visited;
    }

    u64 GetStartAddressOffset() const {
        if (start_address >= g_executable_start && start_address < g_executable_end) {
            return start_address - g_executable_start;
        } else if (start_address >= g_interpreter_start && start_address < g_interpreter_end) {
            return start_address - g_interpreter_start;
        }
        return start_address;
    }

    void RemovePredecessor(BackendBlock* block) {
        for (auto it = predecessors.begin(); it != predecessors.end(); ++it) {
            if (*it == block) {
                predecessors.erase(it);
                return;
            }
        }
        UNREACHABLE();
    }

    [[nodiscard]] std::string Print() const;

private:
    friend struct BackendFunction;

    mutable Label label;
    std::list<BackendInstruction> instructions{};
    std::vector<BackendBlock*> predecessors;
    std::array<BackendBlock*, 2> successors = {nullptr, nullptr};
    u64 start_address = 0;
    u32 list_index = 0;
    u32 next_name = 0;
    mutable bool visited = false;
};