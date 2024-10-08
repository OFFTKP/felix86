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
        SSAInstruction* value = ir_emit_load_guest_from_memory(entry, x86_ref_e(i));
        ir_emit_set_guest(entry, x86_ref_e(i), value);
    }

    for (u8 i = 0; i < X86_REF_COUNT; i++) {
        // Emit get_guest for every piece of state and store it to memory
        // These get_guests will be replaced with movs from a temporary or a phi
        // during the ssa pass
        SSAInstruction* value = ir_emit_get_guest(exit, x86_ref_e(i));
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

std::string IRFunction::PrintReduced(const std::function<std::string(const ReducedInstruction*)>& callback) {
    if (!IsCompiled()) {
        WARN("Print called on not compiled function");
        return "";
    }

    std::string ret;

    auto blocks = GetBlocksPostorder();
    auto it = blocks.rbegin();
    while (it != blocks.rend()) {
        ret += (*it)->PrintReduced(callback);
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
            WARN("Mismatch on uses on instruction: %s", inst->Print({}).c_str());
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

static void reverse_postorder_creation(IRFunction* function, std::vector<IRBlock*>& order) {
    IRBlock* entry = function->GetEntry();
    postorder(entry, order);

    if (order.size() != function->GetBlocks().size()) {
        ERROR("Postorder traversal did not visit all blocks: %zu vs %zu", order.size(), function->GetBlocks().size());
    }
}

std::vector<IRBlock*> IRFunction::GetBlocksPostorder() {
    std::vector<IRBlock*> order; // TODO: cache this
    reverse_postorder_creation(this, order);
    UnvisitAll();
    return order;
}