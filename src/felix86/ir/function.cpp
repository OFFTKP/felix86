#include "felix86/common/log.hpp"
#include "felix86/common/print.hpp"
#include "felix86/ir/emitter.hpp"
#include "felix86/ir/function.hpp"

IRFunction::IRFunction(u64 address) {
    blocks.push_back(new IRBlock());
    blocks.push_back(new IRBlock());
    entry = blocks[0];
    exit = blocks[1];
    entry->SetIndex(0);
    exit->SetIndex(1);

    IREmitter entry_emitter(*entry, IR_NO_ADDRESS);
    thread_state_pointer = entry_emitter.GetThreadStatePointer();

    for (u8 i = 0; i < X86_REF_COUNT; i++) {
        // Load all state from memory and run the set_guest instruction
        // See ssa_pass.cpp for more information
        SSAInstruction* value = entry_emitter.LoadGuestFromMemory(x86_ref_e(i));
        entry_emitter.SetGuest(x86_ref_e(i), value);
    }

    IREmitter exit_emitter(*exit, IR_NO_ADDRESS);
    for (u8 i = 0; i < X86_REF_COUNT; i++) {
        // Emit get_guest for every piece of state and store it to memory
        // These get_guests will be replaced with movs from a temporary or a phi
        // during the ssa pass
        SSAInstruction* value = exit_emitter.GetGuest(x86_ref_e(i));
        exit_emitter.StoreGuestToMemory(value, x86_ref_e(i));
        if (g_print_state) {
            exit_emitter.CallHostFunction((u64)print_gprs);
        }
    }

    start_address_block = CreateBlockAt(address);

    entry->TerminateJump(start_address_block);
    exit->TerminateBackToDispatcher();
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
    return nullptr;
}

IRBlock* IRFunction::CreateBlock() {
    blocks.push_back(new IRBlock());
    IRBlock* block = blocks.back();
    block->SetIndex(blocks.size() - 1);
    return block;
}

void IRFunction::deallocateAll() {
    for (auto& block : blocks) {
        delete block;
    }
}

std::string IRFunction::Print(const std::function<std::string(const SSAInstruction*)>& callback) {
    if (!IsCompiled()) {
        WARN("Print called on not compiled function");
        return "";
    }

    std::string ret;

    auto blocks = GetBlocksPostorder();
    auto it = blocks.rbegin();
    while (it != blocks.rend()) {
        ret += (*it)->Print(callback);
        ++it;
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

    std::unordered_map<SSAInstruction*, Uses> uses;
    for (auto& block : blocks) {
        if (block->GetTermination() == Termination::Null) {
            return false;
        }

        auto add_uses = [&uses](SSAInstruction* inst) {
            uses[inst].want = inst->GetUseCount();

            for (auto& inst : inst->GetUsedInstructions()) {
                uses[inst].have++;
            }
        };

        for (auto& inst : block->GetInstructions()) {
            add_uses(&inst);
        }
    }

    for (const auto& [inst, use] : uses) {
        if (use.have != use.want) {
            WARN("Mismatch on uses on instruction: %d, want: %d, have: %d", inst->GetName(), use.want, use.have);
            return false;
        }
    }

    return true;
}

// Some algorithms rely on all the phis being at the start of the instruction list to be
// easily collected into a span, so we need to validate that if phis are present, they are
// all collected at the start of the instruction list
bool IRFunction::ValidatePhis() const {
    for (auto& block : blocks) {
        bool found_non_phi = false;
        bool found_phi = false;
        std::vector<IRBlock*> block_order = {};
        size_t size = 0;
        for (auto& inst : block->GetInstructions()) {
            if (inst.IsPhi()) {
                if (!found_phi) {
                    size = inst.AsPhi().blocks.size();
                    block_order = inst.AsPhi().blocks;
                }

                if (found_non_phi) {
                    WARN("Phi instruction found after non-phi instruction in block %d", block->GetIndex());
                    return false;
                }

                if (size != inst.AsPhi().blocks.size()) {
                    WARN("Phi instruction with different number of predecessors in block %d", block->GetIndex());
                    return false;
                }

                if (size != inst.AsPhi().values.size()) {
                    WARN("Phi instruction with different number of values in block %d", block->GetIndex());
                    return false;
                }

                for (size_t i = 0; i < size; i++) {
                    if (block_order[i] != inst.AsPhi().blocks[i]) {
                        WARN("Phi instruction with different predecessor order in block %d", block->GetIndex());
                        return false;
                    }
                }
            } else {
                found_non_phi = true;
            }
        }
    }

    return true;
}

static void postorder(IRBlock* block, std::vector<IRBlock*>& output) {
    if (block->IsVisited()) {
        return;
    }

    block->SetVisited(true);

    for (IRBlock* successor : block->GetSuccessors()) {
        postorder(successor, output);
    }

    output.push_back(block); // TODO: don't use vector in the future
}

static void postorder_creation(IRFunction* function, std::vector<IRBlock*>& order) {
    IRBlock* entry = function->GetEntry();
    postorder(entry, order);

    if (order.size() != function->GetBlocks().size()) {
        ERROR("Postorder traversal did not visit all blocks: %zu vs %zu", order.size(), function->GetBlocks().size());
    }
}

std::vector<IRBlock*> IRFunction::GetBlocksPostorder() {
    std::vector<IRBlock*> order; // TODO: cache this
    postorder_creation(this, order);
    UnvisitAll();
    return order;
}