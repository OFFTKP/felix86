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

    Termination GetTermination() const {
        return termination;
    }

    const BackendInstruction* GetCondition() const {
        return condition;
    }

    const std::list<BackendInstruction>& GetInstructions() const {
        return instructions;
    }

    std::list<BackendInstruction>& GetInstructions() {
        return instructions;
    }

    u32 GetSuccessorCount() const {
        switch (termination) {
        case Termination::Jump:
            return 1;
        case Termination::JumpConditional:
            return 2;
        case Termination::BackToDispatcher:
            return 0;
        case Termination::Null: {
            UNREACHABLE();
            return 0;
        }
        default: {
            // uhhh gcc warning
            UNREACHABLE();
            return 0;
        }
        }
    }

    u32 GetPredecessorCount() const {
        return predecessors.size();
    }

    const std::array<u32, 2>& GetSuccessors() const {
        return successors;
    }

    const std::vector<u32>& GetPredecessors() const {
        return predecessors;
    }

    u32 GetSuccessor(u32 index) const {
        return successors[index];
    }

    u32 GetPredecessor(u32 index) const {
        return predecessors[index];
    }

    u32 GetIndex() const {
        return list_index;
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

    u64 GetStartAddressOffset() const {
        if (start_address >= g_executable_start && start_address < g_executable_end) {
            return start_address - g_executable_start;
        } else if (start_address >= g_interpreter_start && start_address < g_interpreter_end) {
            return start_address - g_interpreter_start;
        }
        return start_address;
    }

    [[nodiscard]] std::string Print() const;

private:
    Termination termination = Termination::Null;
    const BackendInstruction* condition = nullptr;
    std::list<BackendInstruction> instructions{};
    std::vector<u32> predecessors;
    std::array<u32, 2> successors = {UINT32_MAX, UINT32_MAX};
    u32 list_index = 0;
    u32 next_name = 0;
    u64 start_address = 0;
};