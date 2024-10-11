#include <algorithm>
#include <array>
#include <cstdio>
#include <list>
#include <stack>
#include <vector>
#include "felix86/common/log.hpp"
#include "felix86/ir/passes/passes.hpp"

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

   SSA form is destroyed during IRFunction -> BackendFunction conversion
   See BackendFunction::FromIRFunction
*/

// See Cytron et al. paper figure 11
static void place_phi_functions(IRFunction* function) {
    auto& list = function->GetBlocks();
    std::vector<IRBlock*> worklist = {};
    worklist.reserve(list.size());
    std::vector<int> work;        // indicates whether X has ever been added to worklist
                                  // during the current iteration of the outer loop.
    std::vector<int> has_already; // indicates whether a phi function has already
                                  // been placed in X
    work.resize(list.size());
    has_already.resize(list.size());

    std::array<std::vector<IRBlock*>, X86_REF_COUNT> assignments = {};

    for (size_t i = 0; i < list.size(); i++) {
        IRBlock* block = list[i];
        std::list<SSAInstruction>& instructions = block->GetInstructions();
        for (const SSAInstruction& inst : instructions) {
            // Make sure it wasn't already added in this list of instructions
            if (inst.GetOpcode() == IROpcode::SetGuest) {
                x86_ref_e ref = inst.AsSetGuest().ref;
                if (assignments[ref].empty() || assignments[ref].back() != block) {
                    assignments[ref].push_back(block);
                }
            }
        }
    }

    // Placement of phi functions
    int iter_count = 0;
    for (size_t i = 0; i < X86_REF_COUNT; i++) {
        iter_count += 1;

        for (const auto& block : assignments[i]) {
            work[block->GetIndex()] = iter_count;
            worklist.push_back(block);
        }

        while (!worklist.empty()) {
            IRBlock* X = worklist.back();
            worklist.pop_back();

            for (auto& df : X->GetDominanceFrontiers()) {
                if (has_already[df->GetIndex()] < iter_count) {
                    Phi phi;
                    phi.ref = static_cast<x86_ref_e>(i);

                    size_t pred_count = df->GetPredecessors().size();
                    phi.values.resize(pred_count);
                    phi.blocks.resize(pred_count);

                    SSAInstruction instruction(std::move(phi));
                    df->AddPhi(std::move(instruction));

                    has_already[df->GetIndex()] = iter_count;
                    if (work[df->GetIndex()] < iter_count) {
                        work[df->GetIndex()] = iter_count;
                        worklist.push_back(df);
                    }
                }
            }
        }
    }
}

int which_pred(IRBlock* pred, IRBlock* block) {
    for (size_t i = 0; i < block->GetPredecessors().size(); i++) {
        if (block->GetPredecessors()[i] == pred) {
            return i;
        }
    }

    ERROR("Block is not a predecessor of the other block");
    return -1;
}

static void search(IRDominatorTreeNode* node, std::array<std::stack<SSAInstruction*>, X86_REF_COUNT>& stacks) {
    IRBlock* block = node->block;
    std::array<int, X86_REF_COUNT> pop_count = {};
    for (auto it = block->GetInstructions().begin(); it != block->GetInstructions().end();) {
        SSAInstruction& inst = *it;
        // These are the only instructions we care about moving to SSA.
        if (inst.GetOpcode() == IROpcode::SetGuest) {
            int ref = inst.AsSetGuest().ref;
            stacks[ref].push(&inst);
            pop_count[ref]++;
        } else if (inst.GetOpcode() == IROpcode::Phi) {
            int ref = inst.AsPhi().ref;
            stacks[ref].push(&inst);
            pop_count[ref]++;
        } else if (inst.GetOpcode() == IROpcode::GetGuest) {
            SSAInstruction* def = stacks[inst.AsGetGuest().ref].top();
            inst.ReplaceExpressionWithMov(def);
        }

        it++;
    }

    for (IRBlock* succesor : block->GetSuccessors()) {
        int j = which_pred(block, succesor);
        for (SSAInstruction& inst : succesor->GetInstructions()) {
            if (!inst.IsPhi()) {
                break;
            }

            Phi& phi = inst.AsPhi();
            phi.blocks[j] = block;
            phi.values[j] = stacks[phi.ref].top();
            phi.values[j]->AddUse();
        }
    }

    for (IRDominatorTreeNode* child : node->children) {
        search(child, stacks);
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
static void rename(std::vector<IRDominatorTreeNode>& list) {
    std::array<std::stack<SSAInstruction*>, X86_REF_COUNT> stacks = {};

    search(&list[0], stacks);
}

static IRBlock* intersect(IRBlock* a, IRBlock* b, const std::vector<u32>& postorder_index, std::vector<IRBlock*>& doms) {
    IRBlock* finger1 = a;
    IRBlock* finger2 = b;

    while (postorder_index[finger1->GetIndex()] != postorder_index[finger2->GetIndex()]) {
        while (postorder_index[finger1->GetIndex()] < postorder_index[finger2->GetIndex()]) {
            if (doms[finger1->GetIndex()] == nullptr) {
                ERROR("finger1 (%d) has no immediate dominator", finger1->GetIndex());
            }

            finger1 = doms[finger1->GetIndex()];
        }

        while (postorder_index[finger2->GetIndex()] < postorder_index[finger1->GetIndex()]) {
            if (doms[finger2->GetIndex()] == nullptr) {
                ERROR("finger2 (%d) has no immediate dominator", finger2->GetIndex());
            }

            finger2 = doms[finger2->GetIndex()];
        }
    }

    return finger1;
}

[[nodiscard]] std::vector<IRBlock*> fast_dominance_algorithm(const std::vector<IRBlock*>& rpo, const std::vector<u32>& postorder_indices) {
    std::vector<IRBlock*> doms(rpo.size());
    std::fill(doms.begin(), doms.end(), nullptr);

    doms[0] = rpo[0];

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

            IRBlock* new_idom = nullptr;
            for (IRBlock* block : predecessors) {
                if (doms[block->GetIndex()] != nullptr) {
                    new_idom = block;
                    break;
                }
            }

            if (!new_idom) {
                ERROR("Could not find processed predecessor for block %d", b->GetIndex());
            }

            for (IRBlock* p : predecessors) {
                if (p == new_idom) {
                    continue;
                }

                if (doms[p->GetIndex()] != nullptr) {
                    new_idom = intersect(p, new_idom, postorder_indices, doms);
                }
            }

            if (doms[b->GetIndex()] != new_idom) {
                doms[b->GetIndex()] = new_idom;
                changed = true;
            }
        }
    }

    doms[0] = nullptr;

    return doms;
}

void ir_ssa_pass(IRFunction* function) {
    size_t count = function->GetBlocks().size();

    std::vector<IRBlock*> rpo = function->GetBlocksPostorder();
    std::vector<u32> postorder_index(count);
    for (size_t i = 0; i < count; i++) {
        postorder_index[rpo[i]->GetIndex()] = i;
    }

    std::reverse(rpo.begin(), rpo.end());

    if (rpo[0] != function->GetEntry()) {
        ERROR("Entry block is not the first block");
    }

    std::vector<IRBlock*> result_fast = fast_dominance_algorithm(rpo, postorder_index);

    // std::vector<IRBlock*> result_slow = slow_dominance_algorithm(rpo);
    // for (size_t i = 0; i < count; i++) {
    //     if (result_fast[i] != result_slow[i]) {
    //         ERROR("Fast and slow dominance algorithms do not match for block: %d", i);
    //     }
    // }

    for (size_t i = 0; i < count; i++) {
        IRBlock* block = function->GetBlocks()[i];
        block->SetImmediateDominator(result_fast[i]);
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
                    runner = runner->GetImmediateDominator();
                }
            }
        }
    }

    // Now that we have dominance frontiers, step 1 is complete
    // We can now move on to step 2, which is inserting phi instructions
    place_phi_functions(function);

    // Construct a dominator tree
    IRDominatorTree dominator_tree;
    dominator_tree.nodes.resize(count);

    auto& blocks = function->GetBlocks();
    for (size_t i = 0; i < blocks.size(); i++) {
        IRBlock* block = blocks[i];
        dominator_tree.nodes[i].block = block;
        if (block->GetImmediateDominator()) {
            dominator_tree.nodes[block->GetImmediateDominator()->GetIndex()].children.push_back(&dominator_tree.nodes[i]);
        }
    }

    // Now rename the variables
    rename(dominator_tree.nodes);

    function->SetDominatorTree(std::move(dominator_tree));
}