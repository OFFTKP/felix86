#include "felix86/common/log.h"
#include "felix86/ir/passes.h"
#include <algorithm>
#include <array>
#include <cstdio>
#include <deque>
#include <stack>
#include <unordered_map>
#include <vector>

/*
    This file is in C++ instead of C to not have to reimplement some very useful
   standard C++ containers

    This is written with my current understanding of SSA at this time, parts of
   it could be wrong.

    We want to convert registers (rax, rcx, ...) to SSA form. At this time we do
   not care about moving memory to SSA. This is because when there's code like
   this:

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
    (immediately after the last usage of rax, store it back to memory)

    This is not hard, but there's a tiny gotcha. Imagine the first occurence of
   a usage of a variable. Imagine that this first usage is not dominated by a
   definition. So for example if your very first basic block in a CFG started
   with this instruction:

    mov rbx, rax

    rax here has a usage but is not dominated by a definition. So we need to
   insert a definition when converting to SSA:

    rax_0 = load rax from memory (our actual state struct)
    rbx_0 = rax_0

    Furthermore, we need to ensure that a load like this, even though it defines
   a variable named rax_0, it doesn't actually write it back because rax is not
   changed.

    If these rules are ensured, then dead load/store elimination should be
   simple. Stores and loads with no uses will get eliminated.

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

struct ir_ssa_block_t
{
    ir_block_t* actual_block = nullptr;
    std::vector<ir_ssa_block_t*> predecessors = {};
    std::vector<ir_ssa_block_t*> dominance_frontiers = {};
    ir_ssa_block_t* immediate_dominator = nullptr;

    std::deque<ir_instruction_t> instructions = {};
    ir_ssa_block_t* successor1 = nullptr;
    ir_ssa_block_t* successor2 = nullptr;
    bool visited = false;
    int list_index = 0;
    int postorder_index = 0;
};

struct ir_dominator_tree_node_t {
    ir_ssa_block_t* block = nullptr;
    std::vector<ir_dominator_tree_node_t*> children = {};
};

static void postorder(ir_ssa_block_t* block, std::vector<ir_ssa_block_t*>& output)
{
    if (block->visited)
    {
        return;
    }

    block->visited = true;

    if (block->successor1)
    {
        postorder(block->successor1, output);
    }

    if (block->successor2)
    {
        postorder(block->successor2, output);
    }

    output.push_back(block);
}

static void reverse_postorder_vector_creation(ir_function_t* function, std::vector<ir_ssa_block_t>& list,
                                       std::vector<ir_ssa_block_t*>& output, size_t size)
{
    list.resize(size);

    std::vector<std::pair<ir_block_t*, ir_block_t*>> successors;
    successors.resize(size);

    std::unordered_map<ir_block_t*, int> block_to_index;
    ir_block_list_t* block = function->list;
    size_t index = 0;
    while (block)
    {
        list[index].actual_block = block->block;
        list[index].visited = false;
        block_to_index[block->block] = index;
        list[index].list_index = index;

        ir_instruction_list_t* inst = block->block->instructions;
        while (inst)
        {
            list[index].instructions.push_back(inst->instruction);
            inst = inst->next;
        }

        successors[index].first =
            block->block->successors ? block->block->successors->block : nullptr;
        successors[index].second =
            block->block->successors
                ? block->block->successors->next ? block->block->successors->next->block : nullptr
                : nullptr;
        index++;
        block = block->next;
    }

    for (size_t i = 0; i < list.size(); i++)
    {
        list[i].successor1 =
            successors[i].first ? &list[block_to_index[successors[i].first]] : nullptr;
        list[i].successor2 =
            successors[i].second ? &list[block_to_index[successors[i].second]] : nullptr;

        ir_block_list_t* pred = list[i].actual_block->predecessors;
        while (pred)
        {
            list[i].predecessors.push_back(&list[block_to_index[pred->block]]);
            pred = pred->next;
        }
    }

    ir_ssa_block_t* entry = &list[0];
    postorder(entry, output);

    for (size_t i = 0; i < output.size(); i++)
    {
        // Set the postorder number before reversing
        output[i]->postorder_index = i;
        output[i]->visited = false;
    }

    std::reverse(output.begin(), output.end());

    if (output.size() != size)
    {
        ERROR("Postorder traversal did not visit all blocks");
    }
}

static ir_ssa_block_t* intersect(ir_ssa_block_t* a, ir_ssa_block_t* b)
{
    ir_ssa_block_t* finger1 = a;
    ir_ssa_block_t* finger2 = b;

    while (finger1->postorder_index != finger2->postorder_index)
    {
        while (finger1->postorder_index < finger2->postorder_index)
        {
            finger1 = finger1->immediate_dominator;
        }

        while (finger2->postorder_index < finger1->postorder_index)
        {
            finger2 = finger2->immediate_dominator;
        }
    }

    return finger1;
}

// See Cytron et al. paper figure 11
static void place_phi_functions(std::vector<ir_ssa_block_t>& list)
{
    std::vector<ir_ssa_block_t*> worklist = {};
    worklist.reserve(list.size());
    std::vector<int> work;        // indicates whether X has ever been added to worklist
                                  // during the current iteration of the outer loop.
    std::vector<int> has_already; // indicates whether a phi function has already
                                  // been placed in X
    work.resize(list.size());
    has_already.resize(list.size());

    std::array<std::vector<ir_ssa_block_t*>, X86_REF_COUNT> assignments = {};

    for (size_t i = 0; i < list.size(); i++)
    {
        ir_ssa_block_t* block = &list[i];
        std::deque<ir_instruction_t>& instructions = block->instructions;
        for (const ir_instruction_t& inst : instructions)
        {
            // Make sure it wasn't already added in this list of instructions
            if (inst.opcode == IR_SET_GUEST)
            {
                if (assignments[inst.set_guest.ref].empty() ||
                    assignments[inst.set_guest.ref].back() != block)
                {
                    assignments[inst.set_guest.ref].push_back(block);
                }
            } else if (inst.opcode == IR_HINT_OUTPUTS) {
                for (u8 j = 0; j < inst.side_effect.count; j++) {
                    x86_ref_e ref = inst.side_effect.registers_affected[j];
                    if (assignments[ref].empty() ||
                        assignments[ref].back() != block)
                    {
                        assignments[ref].push_back(block);
                    }
                }
            }
        }
    }

    // Placement of phi functions
    int iter_count = 0;
    for (size_t i = 0; i < X86_REF_COUNT; i++)
    {
        iter_count += 1;

        for (const auto& block : assignments[i])
        {
            work[block->list_index] = iter_count;
            worklist.push_back(block);
        }

        while (!worklist.empty())
        {
            ir_ssa_block_t* X = worklist.back();
            worklist.pop_back();

            for (auto& df : X->dominance_frontiers)
            {
                if (has_already[df->list_index] < iter_count)
                {
                    ir_instruction_t phi = {0};
                    phi.type = IR_TYPE_PHI;
                    phi.opcode = IR_PHI;
                    phi.phi.ref = static_cast<x86_ref_e>(i);
                    std::vector<ir_phi_node_t>* list = new std::vector<ir_phi_node_t>();
                    list->resize(df->predecessors.size());
                    phi.phi.list = list;

                    df->instructions.push_front(phi);
                    has_already[df->list_index] = iter_count;
                    if (work[df->list_index] < iter_count)
                    {
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
        name |= ref << 20; // store here which register this is

        if (count >= ((1 << 20) - 1)) {
            ERROR("Too many names");
        }

        name |= count;
    } else {
        name = count;
    }

    instruction->name = name;
}

static void search(ir_dominator_tree_node_t* node, std::array<std::stack<int>, X86_REF_COUNT>& stacks, std::array<int, X86_REF_COUNT + 1>& counters) {
    ir_ssa_block_t* block = node->block;
    for (auto& inst : block->instructions) {
        if (inst.opcode == IR_SET_GUEST || inst.opcode == IR_GET_GUEST) {
            int i = counters[inst.get_guest.ref];
            name(&inst, &inst.get_guest.ref, i);
            stacks[inst.get_guest.ref].push(i);
            counters[inst.get_guest.ref] = i + 1;
        }
    }

    if (block->successor1) {
        
    }
}

static void rename(std::vector<ir_dominator_tree_node_t>& list) {
    std::array<std::stack<int>, X86_REF_COUNT> stacks = {};

    // The one at counters[X86_REF_COUNT] is the counter for temporaries
    std::array<int, X86_REF_COUNT + 1> counters = {};

    search(&list[0], stacks, counters);
}

void ir_ssa_pass(ir_function_t* function)
{
    size_t count = 0;
    ir_block_list_t* block = function->list;
    while (block)
    {
        count++;
        block = block->next;
    }

    std::vector<ir_ssa_block_t> storage;
    std::vector<ir_ssa_block_t*> rpo_vector; // points to storage
    rpo_vector.reserve(count);

    reverse_postorder_vector_creation(function, storage, rpo_vector, count);

    if (rpo_vector[0]->actual_block != function->entry)
    {
        ERROR("Entry block is not the first block");
    }

    rpo_vector[0]->immediate_dominator = rpo_vector[0];
    bool changed = true;

    // Simple fixpoint algorithm to find immediate dominators by Cooper et al.
    // Name: A Simple, Fast Dominance Algorithm
    while (changed)
    {
        changed = false;

        // For all nodes in reverse postorder, except the start node
        for (size_t i = 1; i < rpo_vector.size(); i++)
        {
            ir_ssa_block_t* b = rpo_vector[i];

            if (b->predecessors.empty())
            {
                ERROR("Block has no predecessors, this should not happen");
            }

            ir_ssa_block_t* new_idom = b->predecessors[0];
            for (size_t j = 1; j < b->predecessors.size(); j++)
            {
                ir_ssa_block_t* p = b->predecessors[j];
                if (p->immediate_dominator)
                {
                    new_idom = intersect(p, new_idom);
                }
            }

            if (b->immediate_dominator != new_idom)
            {
                b->immediate_dominator = new_idom;
                changed = true;
            }
        }
    }

    // Now we have immediate dominators, we can find dominance frontiers
    for (size_t i = 0; i < rpo_vector.size(); i++)
    {
        ir_ssa_block_t* b = rpo_vector[i];

        if (b->predecessors.size() >= 2)
        {
            for (size_t j = 0; j < b->predecessors.size(); j++)
            {
                ir_ssa_block_t* p = b->predecessors[j];
                ir_ssa_block_t* runner = p;

                while (runner != b->immediate_dominator)
                {
                    runner->dominance_frontiers.push_back(b);

                    ir_ssa_block_t* old_runner = runner;
                    runner = runner->immediate_dominator;
                }
            }
        }
    }

    // Now that we have dominance frontiers, step 1 is complete
    // We can now move on to step 2, which is inserting phi instructions
    place_phi_functions(storage);

    // Construct a dominator tree
    std::vector<ir_dominator_tree_node_t> dominator_tree;
    dominator_tree.resize(count);

    for (size_t i = 0; i < rpo_vector.size(); i++)
    {
        ir_ssa_block_t* b = rpo_vector[i];
        if (b->immediate_dominator)
        {
            dominator_tree[b->immediate_dominator->list_index].children.push_back(&dominator_tree[i]);
        }
    }

    // Now rename the variables
    rename(dominator_tree);
}