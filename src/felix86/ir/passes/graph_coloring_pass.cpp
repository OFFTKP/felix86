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

    LiveIn(B) = PhiDefs(B) U UpwardExposed(B) U (LiveOut(B) \ Defs(B))
    LiveOut(B) = ⋃ S∈succs(B)(LiveIn(S) \ PhiDefs(S)) ∪ PhiUses(B)

    there's other non fixpoint algos but they are more complex
*/
void liveness(IRFunction* function, std::vector<LivenessList>& in, std::vector<LivenessList>& out) {
    const std::vector<IRBlock*>& blocks = function->GetBlocksPostorder();
    std::vector<LivenessList> use(blocks.size());
    std::vector<LivenessList> def(blocks.size());
    std::vector<LivenessList> phi_def(blocks.size());
    std::vector<LivenessList> phi_use(blocks.size());

    in.resize(blocks.size());
    out.resize(blocks.size());

    // Populate use and def sets
    for (size_t j = 0; j < blocks.size(); j++) {
        IRBlock* block = blocks[j];
        size_t i = block->GetIndex();
        for (IRInstruction& instr : block->GetInstructions()) {
            if (instr.IsPhi()) {
                phi_def[i].push_back(&instr);
            } else {
                if (!instr.IsVoid()) {
                    def[i].push_back(&instr);
                }

                std::list<IRInstruction*> used_instructions = instr.GetUsedInstructions();
                for (IRInstruction* used_instr : used_instructions) {
                    // Not defined in this block ie. upwards exposed, live range goes outside
                    // current block
                    if (std::find(def[i].begin(), def[i].end(), used_instr) == def[i].end()) {
                        use[i].push_back(used_instr);
                    }
                }
            }

            IRBlock* successor1 = block->GetSuccessor(0);
            if (successor1 && successor1->IsUsedInPhi(&instr)) {
                phi_use[i].push_back(&instr);
            }

            IRBlock* successor2 = block->GetSuccessor(1);
            if (successor2 && successor2->IsUsedInPhi(&instr)) {
                phi_use[i].push_back(&instr);
            }
        }
    }

    // for (int i = 0; i < blocks.size(); i++) {
    //     printf("Block %d\n", i);
    //     printf("Use count: %d\n", use[i].size());
    //     printf("{\n");
    //     for (IRInstruction* instr : use[i]) {
    //         printf("    %s\n", instr->Print().c_str());
    //     }
    //     printf("}\n");
    //     printf("Def count: %d\n", def[i].size());
    //     printf("{\n");
    //     for (IRInstruction* instr : def[i]) {
    //         printf("    %s\n", instr->Print().c_str());
    //     }
    //     printf("}\n");
    // }

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

            // in[b] = phi_def U use[b] U (out[b] - def[b])
            in[i].insert(in[i].end(), use[i].begin(), use[i].end());
            in[i].insert(in[i].end(), phi_def[i].begin(), phi_def[i].end());

            LivenessList out_minus_def = out[i];
            for (IRInstruction* instr : def[i]) {
                out_minus_def.remove(instr);
            }

            in[i].insert(in[i].end(), out_minus_def.begin(), out_minus_def.end());

            // out[b] = U (in[s]) for all s in succ[b]
            out[i].clear();
            out[i].insert(out[i].end(), phi_use[i].begin(), phi_use[i].end());

            const IRBlock* successor1 = block->GetSuccessor(0);
            const IRBlock* successor2 = block->GetSuccessor(1);
            if (successor1 != nullptr) {
                size_t s = successor1->GetIndex();
                LivenessList in_minus_phi_def = in[s];
                for (IRInstruction* instr : phi_def[s]) {
                    in_minus_phi_def.remove(instr);
                }
                out[i].insert(out[i].end(), in_minus_phi_def.begin(), in_minus_phi_def.end());
            }

            if (successor2 != nullptr) {
                size_t s = successor2->GetIndex();
                LivenessList in_minus_phi_def = in[s];
                for (IRInstruction* instr : phi_def[s]) {
                    in_minus_phi_def.remove(instr);
                }
                out[i].insert(out[i].end(), in_minus_phi_def.begin(), in_minus_phi_def.end());
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

    for (auto& block : function->GetBlocks()) {
        printf("Block %d live in count: %d\n", block->GetIndex(), in[block->GetIndex()].size());
        printf("{\n");
        for (auto& instr : in[block->GetIndex()]) {
            printf("    %s\n", instr->Print().c_str());
        }
        printf("}\n");
        printf("Block %d live out count: %d\n", block->GetIndex(), out[block->GetIndex()].size());
        printf("{\n");
        for (auto& instr : out[block->GetIndex()]) {
            printf("    %s\n", instr->Print().c_str());
        }
        printf("}\n");
    }
}