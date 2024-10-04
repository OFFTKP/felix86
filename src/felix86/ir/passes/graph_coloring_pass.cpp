#include <unordered_set>
#include "felix86/ir/passes/passes.hpp"

// This has got to be super slow, lots of heap allocations, please profile me

struct InterferenceGraph {
    void AddEdge(IRInstruction* a, IRInstruction* b) {
        graph[a].insert(b);
        graph[b].insert(a);
    }

    void RemoveEdge(IRInstruction* a, IRInstruction* b) {
        graph[a].erase(b);
        graph[b].erase(a);
    }

private:
    tsl::robin_map<IRInstruction*, std::unordered_set<IRInstruction*>> graph;
};

using LivenessList = std::list<IRInstruction*>;

/*
    Based on this common algorithm that I couldn't find a paper for:
    for each block b
        in[b] = {}
        out[b] = {}
    
    repeat
        for each block b
            in_old = in[b]
            out_old = out[b]
            in[b] = use[b] U (out[b] - def[b])
            out[b] = U (in[s]) for all s in succ[b]
        
        while in[b] != in_old or out[b] != out_old

    To incorporate phi functions, this is recommended in SSA book:

    LiveIn(B) = PhiDefs(B) ∪ UpwardExposed(B) ∪ (LiveOut(B) \ Defs(B))
    LiveOut(B) = ⋃ S∈succs(B)(LiveIn(S) \ PhiDefs(S)) ∪ PhiUses(B)
    
    there's other non fixpoint algos but they are more complex
*/
void liveness(IRFunction* function, std::vector<LivenessList>& in, std::vector<LivenessList>& out) {
    const std::vector<IRBlock*>& blocks = function->GetBlocksPostorder();
    std::vector<LivenessList> use(blocks.size());
    std::vector<LivenessList> def(blocks.size());

    in.resize(blocks.size());
    out.resize(blocks.size());

    // Populate use and def sets
    for (size_t i = 0; i < blocks.size(); i++) {
        IRBlock* block = blocks[i];
        for (IRInstruction& instr : block->GetInstructions()) {
            def[i].push_back(&instr);

            std::list<IRInstruction*> used_instructions = instr.GetUsedInstructions();
            use[i].insert(use[i].end(), used_instructions.begin(), used_instructions.end());
        }
    }

    // Calculate in and out sets
    bool changed;
    do {
        changed = false;
        for (size_t j = 0; j < blocks.size(); j++) {
            const IRBlock* block = blocks[j];

            // j is the index in the postorder list, but we need the index in the blocks list
            size_t i = block->GetIndex();

            LivenessList in_old = in[i];
            LivenessList out_old = out[i];

            in[i].clear();
            
            // in[b] = use[b] U (out[b] - def[b])
            in[i].insert(in[i].end(), use[i].begin(), use[i].end());

            LivenessList out_minus_def = out[i];
            for (IRInstruction* instr : def[i]) {
                out_minus_def.remove(instr);
            }

            in[i].insert(in[i].end(), out_minus_def.begin(), out_minus_def.end());

            // out[b] = U (in[s]) for all s in succ[b]
            out[i].clear();
            const IRBlock* successor1 = block->GetSuccessor(0);
            const IRBlock* successor2 = block->GetSuccessor(1);
            if (successor1 != nullptr) {
                out[i].insert(out[i].end(), in[successor1->GetIndex()].begin(), in[successor1->GetIndex()].end());
            }

            if (successor2 != nullptr) {
                out[i].insert(out[i].end(), in[successor2->GetIndex()].begin(), in[successor2->GetIndex()].end());
            }

            // check for changes
            if (in[i].size() != in_old.size() || out[i].size() != out_old.size()) {
                changed = true;
            } else {
                for (IRInstruction* instr : in[i]) {
                    if (std::find(in_old.begin(), in_old.end(), instr) == in_old.end()) {
                        changed = true;
                        break;
                    }
                }

                for (IRInstruction* instr : out[i]) {
                    if (std::find(out_old.begin(), out_old.end(), instr) == out_old.end()) {
                        changed = true;
                        break;
                    }
                }
            }
        }
    } while (changed);
}

// Hack et al. graph coloring algorithm
void ir_graph_coloring_pass(IRFunction* function) {
    std::vector<LivenessList> in, out;
    liveness(function, in, out);

    // Create interference graph
    InterferenceGraph graph;

}