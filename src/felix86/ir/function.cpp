#include "felix86/common/log.hpp"
#include "felix86/ir/emitter.hpp"
#include "felix86/ir/function.hpp"

IRFunction::IRFunction(u64 address) {
    blocks.push_back(allocateBlock());
    blocks.push_back(allocateBlock());
    entry = blocks[0];
    exit = blocks[1];

    for (u8 i = 0; i < X86_REF_COUNT; i++) {
        // Load all state from memory and run the set_guest instruction
        // See ssa_pass.cpp for more information
        IRInstruction* value = ir_emit_load_guest_from_memory(entry, x86_ref_e(i));
        ir_emit_set_guest(entry, x86_ref_e(i), value);
    }

    for (u8 i = 0; i < X86_REF_COUNT; i++) {
        // Emit get_guest for every piece of state and store it to memory
        // These get_guests will be replaced with movs from a temporary or a phi
        // during the ssa pass
        IRInstruction* value = ir_emit_get_guest(exit, x86_ref_e(i));
        ir_emit_store_guest_to_memory(exit, x86_ref_e(i), value);
    }

    entry->SetIndex(0);
    exit->SetIndex(1);

    start_address_block = CreateBlockAt(address);

    entry->TerminateJump(start_address_block);
    exit->TerminateExit();
}

IRFunction::~IRFunction() {
    deallocateAll();
}

IRBlock* IRFunction::CreateBlockAt(u64 address) {
    if (address != 0 && block_map.find(address) != block_map.end()) {
        return block_map[address];
    }

    blocks.push_back(new IRBlock(address));
    IRBlock* block = blocks.back();
    block->SetIndex(blocks.size() - 1);

    if (address != 0) {
        block_map[address] = block;
    }

    return block;
}

IRBlock* IRFunction::GetBlockAt(u64 address) {
    if (block_map.find(address) != block_map.end()) {
        return block_map[address];
    }

    ERROR("Block not found: %016lx", address);
}

IRBlock* IRFunction::CreateBlock() {
    blocks.push_back(allocateBlock());
    IRBlock* block = blocks.back();
    block->SetIndex(blocks.size() - 1);
    return block;
}

IRBlock* IRFunction::allocateBlock() {
    return new IRBlock(); // TODO: use a memory pool
}

void IRFunction::deallocateAll() {
    for (auto& pair : block_map) {
        delete pair.second;
    }
}

std::string IRFunction::Print(const std::function<std::string(const IRInstruction*)>& callback) const {
    if (!IsCompiled()) {
        WARN("Print called on not compiled function");
        return "";
    }

    std::string ret;

    UnvisitAll();

    std::vector<const IRBlock*> blocks;
    blocks.push_back(GetEntry());

    while (!blocks.empty()) {
        const IRBlock* work = blocks.back();
        blocks.pop_back();
        work->SetVisited(true);

        ret += work->Print(callback);

        const IRBlock* succ1 = work->GetSuccessor(false);
        const IRBlock* succ2 = work->GetSuccessor(true);

        if (succ1 && !succ1->IsVisited()) {
            blocks.push_back(succ1);
        }

        if (succ2 && !succ2->IsVisited()) {
            blocks.push_back(succ2);
        }
    }

    return ret;
}

void IRFunction::UnvisitAll() const {
    for (auto& block : blocks) {
        block->SetVisited(false);
    }
}

bool IRFunction::Validate() const {
    struct Uses {
        u32 want = 0;
        u32 have = 0;
    };

    std::unordered_map<IRInstruction*, Uses> uses;
    for (auto& block : blocks) {
        if (block->GetTermination() == Termination::Null) {
            return false;
        }

        auto add_uses = [&uses](IRInstruction* inst) {
            uses[inst].want = inst->GetUseCount();

            switch (inst->GetExpressionType()) {
            case ExpressionType::Operands: {
                Operands& operands = inst->AsOperands();
                for (IRInstruction* operand : operands.operands) {
                    if (operand != nullptr) {
                        uses[operand].have += 1;
                    } else {
                        ERROR("Operand is null");
                    }
                }
                break;
            }
            case ExpressionType::GetGuest:
            case ExpressionType::Immediate:
            case ExpressionType::Comment: {
                break;
            }
            case ExpressionType::SetGuest: {
                SetGuest& set_guest = inst->AsSetGuest();
                if (set_guest.source != nullptr) {
                    uses[set_guest.source].have += 1;
                } else {
                    ERROR("Source is null");
                }
                break;
            }
            case ExpressionType::Phi: {
                Phi& phi = inst->AsPhi();
                for (auto value : phi.values) {
                    if (value != nullptr) {
                        uses[value].have += 1;
                    } else {
                        ERROR("Value is null");
                    }
                }
                break;
            }
            case ExpressionType::TupleAccess: {
                TupleAccess& tuple_access = inst->AsTupleAccess();
                if (tuple_access.tuple != nullptr) {
                    uses[tuple_access.tuple].have += 1;
                } else {
                    ERROR("Tuple is null");
                }
                break;
            }
            default: {
                ERROR("Unreachable");
            }
            }
        };

        for (auto& inst : block->GetInstructions()) {
            add_uses(&inst);
        }
    }

    for (const auto& [inst, use] : uses) {
        if (use.have != use.want) {
            WARN("Mismatch on uses on instruction: %s", inst->Print({}).c_str());
            return false;
        }
    }

    return true;
}