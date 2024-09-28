#include <algorithm>
#include <array>
#include <cstdio>
#include <deque>
#include <list>
#include <stack>
#include <unordered_map>
#include <vector>
#include "felix86/common/log.hpp"
#include "felix86/ir/passes.hpp"

/*
    This is written with my current understanding of SSA at this time, parts of
   it could be wrong.

    We want to convert registers (rax, rcx, ...) to SSA form.
    This is because when there's code like this:

    mov rax, 1
    mov rax, 2

    If we emit IR like so:

    rax = 1
    rax = 2

    This is essentially code that is not in SSA, if you view these registers as
   variables. What we'd like is for them to be transformed into IR like so:

    rax_0 = 1
    rax_1 = 2
    ...

    A thing that enables this is the fact that we have an entry block that loads
    the entire VM state and an exit block that writebacks the entire VM state.
    This might seem wasteful, however we can optimize this later.

    ---

    We also want to use definitions of registers from previous blocks. This will
   be done by inserting a phi instruction A phi instruction selects a value
   based on the block it was reached from. Phi instructions are something that
   only exists while in SSA form, and are later removed when moving out of SSA
   form.

    Phi instructions are not difficult to add and there's multiple algorithms
   such as Cytron et al. and Braun et al.

    Cytron describes translation to minimal SSA form to go in three steps:
        1. The dominance frontier mapping is constructed from the control flow
   graph
        2. Using the dominance frontiers, the locations of the phi functions for
   each variable in the original program are determined
        3. The variables are renamed by replacing each mention of an original
   variable V with an appropriate mention of a new variable Vi

    For 1. we are going to be using a different algorithm by Cooper et al. that
   is faster (and imo simpler) than Cytron et al.

    It's important to note that in our IR, the only variables are the registers
   (rax, rcx, ..., xmm0, ..., cf, zf, ...) and those are the ones that need to
   be renamed. Temporary variables produced by instructions do not need to be
    renamed as they already are in SSA form.

    So for example, when we have the following instruction:

    or eax, 1
    (more instructions that set or use rax)

    The following IR is generated:
    (note that we handle the size in the IR explicitly, there's no different
   instructions to deal with different GPR sizes as of this moment)

    t0 = rax
    t1 = t0 | 1
    t2 = 0xFFFFFFFF
    t3 = t1 & t2
    rax = t3
    (more IR that sets flags, uses rax or sets rax)

    t0-t3 and the rest of the temporaries down the road are in SSA form already,
   it's just the registers that need to be renamed
*/

struct IRDominatorTreeNode {
    IRBlock* block = nullptr;
    std::vector<IRDominatorTreeNode*> children = {};
};

static void postorder(IRBlock* block, std::vector<IRBlock*>& output) {
    if (block->IsVisited()) {
        return;
    }

    block->SetVisited(true);

    IRBlock* first_successor = block->GetSuccessor(0);
    if (first_successor) {
        postorder(first_successor, output);
    }

    IRBlock* second_successor = block->GetSuccessor(1);
    if (second_successor) {
        postorder(second_successor, output);
    }

    output.push_back(block); // TODO: don't use vector in the future
}

static void reverse_postorder_creation(IRFunction* function, std::vector<IRBlock*>& order) {
    IRBlock* entry = function->GetEntry();
    postorder(entry, order);

    if (order.size() != function->GetBlocks().size()) {
        ERROR("Postorder traversal did not visit all blocks");
    }

    for (size_t i = 0; i < order.size(); i++) {
        order[i]->SetPostorderIndex(i);
    }

    std::reverse(order.begin(), order.end());
}

static IRBlock* intersect(IRBlock* a, IRBlock* b) {
    IRBlock* finger1 = a;
    IRBlock* finger2 = b;

    while (finger1->GetPostorderIndex() != finger2->GetPostorderIndex()) {
        while (finger1->GetPostorderIndex() < finger2->GetPostorderIndex()) {
            finger1 = finger1->GetImmediateDominator();
        }

        while (finger2->GetPostorderIndex() < finger1->GetPostorderIndex()) {
            finger2 = finger2->GetImmediateDominator();
        }
    }

    return finger1;
}

// See Cytron et al. paper figure 11
static void place_phi_functions(std::vector<ir_ssa_block_t>& list) {
    std::vector<IRBlock*> worklist = {};
    worklist.reserve(list.size());
    std::vector<int> work;        // indicates whether X has ever been added to worklist
                                  // during the current iteration of the outer loop.
    std::vector<int> has_already; // indicates whether a phi function has already
                                  // been placed in X
    work.resize(list.size());
    has_already.resize(list.size());

    std::array<std::vector<ir_ssa_block_t*>, X86_REF_COUNT> assignments = {};

    for (size_t i = 0; i < list.size(); i++) {
        ir_ssa_block_t* block = &list[i];
        std::list<ir_instruction_t>& instructions = block->instructions;
        for (const ir_instruction_t& inst : instructions) {
            // Make sure it wasn't already added in this list of instructions
            if (inst.opcode == IR_SET_GUEST) {
                if (assignments[inst.set_guest.ref].empty() ||
                    assignments[inst.set_guest.ref].back() != block) {
                    assignments[inst.set_guest.ref].push_back(block);
                }
            } else if (inst.opcode == IR_HINT_OUTPUTS) {
                for (u8 j = 0; j < inst.side_effect.count; j++) {
                    x86_ref_e ref = inst.side_effect.registers_affected[j];
                    if (assignments[ref].empty() || assignments[ref].back() != block) {
                        assignments[ref].push_back(block);
                    }
                }
            }
        }
    }

    // Placement of phi functions
    int iter_count = 0;
    for (size_t i = 0; i < X86_REF_COUNT; i++) {
        iter_count += 1;

        for (const auto& block : assignments[i]) {
            work[block->list_index] = iter_count;
            worklist.push_back(block);
        }

        while (!worklist.empty()) {
            ir_ssa_block_t* X = worklist.back();
            worklist.pop_back();

            for (auto& df : X->dominance_frontiers) {
                if (has_already[df->list_index] < iter_count) {
                    ir_instruction_t phi = {0};
                    phi.type = IR_TYPE_PHI;
                    phi.opcode = IR_PHI;
                    phi.phi.ref = static_cast<x86_ref_e>(i);
                    std::vector<ir_phi_node_t>* list = new std::vector<ir_phi_node_t>();
                    list->resize(df->predecessors.size());
                    phi.phi.list = list;

                    df->instructions.push_front(phi);
                    df->phi_instructions.push_back(&df->instructions.front());

                    has_already[df->list_index] = iter_count;
                    if (work[df->list_index] < iter_count) {
                        work[df->list_index] = iter_count;
                        worklist.push_back(df);
                    }
                }
            }
        }
    }
}

int which_pred(ir_ssa_block_t* pred, ir_ssa_block_t* block) {
    for (size_t i = 0; i < block->predecessors.size(); i++) {
        if (block->predecessors[i] == pred) {
            return i;
        }
    }

    return -1;
}

void name(ir_instruction_t* instruction, x86_ref_e* pref, u32 count) {
    u32 name = 0;
    if (pref) {
        u32 ref = *pref;
        name |= 0x8000'0000; // we use bit 31 to indicate this is a register
        name |= ref << 20;   // store here which register this is

        if (count >= ((1 << 20) - 1)) {
            ERROR("Too many names");
        }

        name |= count;
    } else {
        name = count;
    }

    instruction->name = name;
}

static void search(ir_dominator_tree_node_t* node,
                   std::array<std::stack<ir_instruction_t*>, X86_REF_COUNT>& stacks,
                   std::array<int, X86_REF_COUNT>& counters) {
    ir_ssa_block_t* block = node->block;
    auto previous_it = block->instructions.begin();
    std::array<int, X86_REF_COUNT> pop_count = {};
    for (auto it = block->instructions.begin(); it != block->instructions.end();) {
        ir_instruction_t& inst = *it;
        // These are the only instructions we care about moving to SSA, the rest should already be
        // in SSA
        if (inst.opcode == IR_SET_GUEST || inst.opcode == IR_GET_GUEST) {
            static_assert(offsetof(ir_instruction_t, set_guest.ref) ==
                              offsetof(ir_instruction_t, get_guest.ref),
                          "Offsets are not the same");
            x86_ref_e ref = inst.get_guest.ref;

            if (inst.opcode == IR_GET_GUEST) {
                // At this point we eliminate the get_guest instruction, effectively forwarding
                // set_guest
                ir_instruction_t* def = stacks[ref].top();
                inst.type = IR_TYPE_ONE_OPERAND;
                inst.opcode = IR_MOV;
                inst.operands.args[0] = def;
                def->uses += 1;
            } else {
                // This is a definition (set_guest) and we wanna push it to the appropriate stack.
                int i = counters[ref];
                name(&inst, &ref, i);
                stacks[ref].push(&inst);
                pop_count[ref]++;
                counters[ref] = i + 1;
            }
        }

        previous_it = it;
        it++;
    }

    int successor_count = 0;
    std::array<ir_ssa_block_t*, 2> successors = {nullptr, nullptr};
    if (block->successor1) {
        successors[successor_count] = block->successor1;
        successor_count++;
    }

    if (block->successor2) {
        successors[successor_count] = block->successor2;
        successor_count++;
    }

    for (int i = 0; i < successor_count; i++) {
        ir_ssa_block_t* succ = successors[i];
        int j = which_pred(block, succ);
        for (ir_instruction_t* F : succ->phi_instructions) {
            std::vector<ir_phi_node_t>* list = (std::vector<ir_phi_node_t>*)F->phi.list;
            ir_phi_node_t& node = (*list)[j];
            node.block = block->actual_block;

            x86_ref_e ref = F->phi.ref;
            node.value = stacks[ref].top();
            node.value->uses += 1;
        }
    }

    for (ir_dominator_tree_node_t* child : node->children) {
        search(child, stacks, counters);
    }

    for (size_t i = 0; i < X86_REF_COUNT; i++) {
        for (int j = 0; j < pop_count[i]; j++) {
            stacks[i].pop();
        }
    }
}

// This is similar to the Cytron rename pass
// Our get_guest instructions are essentially uses, and set_guest instructions are
// defines. We need to replace each use (get_guest) with the appropriate definition, as done
// in Cytron et al. A small catch is, we may encounter a use (get_guest) that is not dominated
// by a definition (either set_guest or phi). In this case, we need to insert a definition
// before the use, by loading the register from memory.
// This should effectively forward sets to gets and get rid of get_guest instructions
static void rename(std::vector<ir_dominator_tree_node_t>& list) {
    std::array<std::stack<ir_instruction_t*>, X86_REF_COUNT> stacks = {};
    std::array<int, X86_REF_COUNT> counters = {};

    search(&list[0], stacks, counters);
}

void ir_ssa_pass(IRFunction* function) {
    size_t count = function->GetBlocks().size();

    std::vector<IRBlock*> rpo;
    reverse_postorder_creation(function, rpo);

    if (rpo[0] != function->GetEntry()) {
        ERROR("Entry block is not the first block");
    }

    rpo[0]->SetImmediateDominator(rpo[0]);
    bool changed = true;

    // Simple fixpoint algorithm to find immediate dominators by Cooper et al.
    // Name: A Simple, Fast Dominance Algorithm
    while (changed) {
        changed = false;

        // For all nodes in reverse postorder, except the start node
        for (size_t i = 1; i < rpo.size(); i++) {
            IRBlock* b = rpo[i];

            auto& predecessors = b->GetPredecessors();
            if (predecessors.empty()) {
                ERROR("Block has no predecessors, this should not happen");
            }

            IRBlock* new_idom = predecessors[0];
            for (size_t j = 1; j < predecessors.size(); j++) {
                IRBlock* p = predecessors[j];
                if (p->GetImmediateDominator()) {
                    new_idom = intersect(p, new_idom);
                }
            }

            if (b->GetImmediateDominator() != new_idom) {
                b->SetImmediateDominator(new_idom);
                changed = true;
            }
        }
    }

    // Now we have immediate dominators, we can find dominance frontiers
    for (size_t i = 0; i < rpo.size(); i++) {
        IRBlock* b = rpo[i];

        auto& predecessors = b->GetPredecessors();
        if (predecessors.size() >= 2) {
            for (size_t j = 0; j < predecessors.size(); j++) {
                IRBlock* p = predecessors[j];
                IRBlock* runner = p;

                while (runner != b->GetImmediateDominator()) {
                    runner->AddDominanceFrontier(b);

                    IRBlock* old_runner = runner;
                    runner = runner->GetImmediateDominator();
                }
            }
        }
    }

    // Now that we have dominance frontiers, step 1 is complete
    // We can now move on to step 2, which is inserting phi instructions
    place_phi_functions(storage);

    // Before we made the entry block have itself as the immediate dominator, we need to undo that
    rpo[0]->immediate_dominator = nullptr;

    // Construct a dominator tree
    std::vector<ir_dominator_tree_node_t> dominator_tree;
    dominator_tree.resize(count);

    for (size_t i = 0; i < storage.size(); i++) {
        ir_ssa_block_t* block = &storage[i];
        dominator_tree[i].block = block;
        if (block->immediate_dominator) {
            dominator_tree[block->immediate_dominator->list_index].children.push_back(
                &dominator_tree[i]);
        }
    }

    // Now rename the variables
    rename(dominator_tree);

    u32 name = 1;
    // Rename the temps too
    for (size_t i = 0; i < storage.size(); i++) {
        ir_ssa_block_t* block = &storage[i];
        for (ir_instruction_t& inst : block->instructions) {
            if (inst.name == 0) {
                inst.name = name;
                name++;
            }
        }
    }
}