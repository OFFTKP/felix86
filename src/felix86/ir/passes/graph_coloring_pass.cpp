#include <unordered_set>
#include <fmt/base.h>
#include <fmt/format.h>
#include "felix86/ir/passes/passes.hpp"

// This has got to be super slow, lots of heap allocations, please profile me

struct Node {
    u32 id;
    std::unordered_set<u32> edges;
};

struct InterferenceGraph {
    void AddEdge(u32 a, u32 b) {
        graph[b].insert(a);
        graph[a].insert(b);
    }

    void RemoveEdge(u32 a, u32 b) {
        graph[a].erase(b);
        graph[b].erase(a);
    }

    Node RemoveNode(u32 id) {
        Node node = {id, graph[id]};
        for (u32 edge : graph[id]) {
            graph[edge].erase(id);
        }

        graph.erase(id);
        return node;
    }

    void AddNode(const Node& node) {
        for (u32 edge : node.edges) {
            AddEdge(node.id, edge);
        }
    }

    const std::unordered_set<u32>& GetInterferences(u32 inst) {
        return graph[inst];
    }

private:
    tsl::robin_map<u32, std::unordered_set<u32>> graph;
};

// using LivenessSet = std::list<SSAInstruction*>;

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
*/

using LivenessSet = std::unordered_set<u32>;
struct InOutLiveness {
    LivenessSet in;
    LivenessSet out;
};

// void liveness(const BackendFunction& function, std::vector<LivenessSet>& out) {
//     std::vector<const BackendBlock*> blocks = function.GetBlocksPostorder();
//     std::vector<LivenessSet> in(blocks.size());
//     std::vector<LivenessSet> use(blocks.size());
//     std::vector<LivenessSet> def(blocks.size());

//     out.resize(blocks.size());

//     // Populate use and def sets
//     for (size_t j = 0; j < blocks.size(); j++) {
//         const BackendBlock* block = blocks[j];
//         size_t i = block->GetIndex();
//         for (const BackendInstruction& instr : block->GetInstructions()) {
//             if (instr.GetDesiredType() != AllocationType::Null) {
//                 def[i].insert(instr.GetName());
//             }

//             for (u8 i = 0; i < instr.GetOperandCount(); i++) {
//                 if (std::find(def[i].begin(), def[i].end(), instr.GetOperand(i)) == def[i].end()) {
//                     // Not defined in this block ie. upwards exposed, live range goes outside
//                     // current block
//                     use[i].insert(instr.GetOperand(i));
//                 }
//             }
//         }
//     }

//     // for (int i = 0; i < blocks.size(); i++) {
//     //     printf("Block %d\n", i);
//     //     printf("Use count: %d\n", use[i].size());
//     //     printf("{\n");
//     //     for (SSAInstruction* instr : use[i]) {
//     //         printf("    %s\n", instr->Print().c_str());
//     //     }
//     //     printf("}\n");
//     //     printf("Def count: %d\n", def[i].size());
//     //     printf("{\n");
//     //     for (SSAInstruction* instr : def[i]) {
//     //         printf("    %s\n", instr->Print().c_str());
//     //     }
//     //     printf("}\n");
//     // }

//     // Calculate in and out sets
//     bool changed;
//     do {
//         changed = false;
//         for (size_t j = 0; j < blocks.size(); j++) {
//             const BackendBlock* block = blocks[j];

//             // j is the index in the postorder list, but we need the index in the blocks list
//             size_t i = block->GetIndex();

//             LivenessSet in_old = in[i];
//             LivenessSet out_old = out[i];

//             in[i].clear();

//             // in[b] = use[b] U (out[b] - def[b])
//             in[i].insert(use[i].begin(), use[i].end());

//             LivenessSet out_minus_def = out[i];
//             for (u32 instr : def[i]) {
//                 out_minus_def.erase(instr);
//             }

//             in[i].insert(out_minus_def.begin(), out_minus_def.end());

//             // out[b] = U (in[s]) for all s in succ[b]
//             out[i].clear();
//             for (u8 i = 0; i < block->GetSuccessorCount(); i++) {
//                 const BackendBlock* succ = &function.GetBlock(block->GetSuccessor(i));
//                 u32 succ_index = succ->GetIndex();
//                 out[i].insert(in[succ_index].begin(), in[succ_index].end());
//             }

//             // check for changes
//             if (!changed) {
//                 if (in[i].size() != in_old.size() || out[i].size() != out_old.size()) {
//                     changed = true;
//                 } else {
//                     for (u32 instr : in[i]) {
//                         if (in_old.find(instr) == in_old.end()) {
//                             changed = true;
//                             break;
//                         }
//                     }

//                     if (!changed) {
//                         for (u32 instr : out[i]) {
//                             if (out_old.find(instr) == out_old.end()) {
//                                 changed = true;
//                                 break;
//                             }
//                         }
//                     }
//                 }
//             }
//         }
//     } while (changed);
// }

// void construct_interference_graph(const BackendFunction& function, const AllocationMap& allocation_map, std::array<InterferenceGraph, 3>& graph,
//                                   const std::vector<LivenessSet>& out) {

//     InterferenceGraph& gpr_graph = graph[0];
//     InterferenceGraph& fpr_graph = graph[1];
//     InterferenceGraph& vec_graph = graph[2];

//     for (const BackendBlock* block : function.GetBlocksPostorder()) {
//         LivenessSet live_now;

//         // We are gonna walk the block backwards, first add all definitions that have lifetime
//         // that extends past this basic block
//         live_now.insert(out[block->GetIndex()].begin(), out[block->GetIndex()].end());

//         const std::list<BackendInstruction>& insts = block->GetInstructions();
//         for (auto it = insts.rbegin(); it != insts.rend(); ++it) {
//             const BackendInstruction& inst = *it;

//             // Erase the currently defined variable if it exists in the set
//             live_now.erase(inst.GetName());

//             for (u8 i = 0; i < inst.GetOperandCount(); i++) {
//                 live_now.insert(inst.GetOperand(i));
//             }

//             switch (inst.GetDesiredType()) {
//             case AllocationType::GPR: {
//                 for (auto& live : live_now) {
//                     if (!allocation_map.IsAllocated(live)) {
//                         gpr_graph.AddEdge(inst.GetName(), live);
//                     }
//                 }
//                 break;
//             }
//             case AllocationType::FPR: {
//                 for (auto& live : live_now) {
//                     if (!allocation_map.IsAllocated(live)) {
//                         fpr_graph.AddEdge(inst.GetName(), live);
//                     }
//                 }
//                 break;
//             }
//             case AllocationType::Vec: {
//                 for (auto& live : live_now) {
//                     if (!allocation_map.IsAllocated(live)) {
//                         vec_graph.AddEdge(inst.GetName(), live);
//                     }
//                 }
//                 break;
//             }
//             case AllocationType::Null:
//                 break;
//             default:
//                 UNREACHABLE();
//                 break;
//             }
//         }
//     }
// }

// AllocationMap ir_graph_coloring_pass(const BackendFunction& function) {
//     AllocationMap allocation_map;

//     // Pre-color some special values
//     for (const BackendBlock* block : function.GetBlocksPostorder()) {
//         for (const BackendInstruction& inst : block->GetInstructions()) {
//             switch (inst.GetOpcode()) {
//             case IROpcode::GetThreadStatePointer: {
//                 allocation_map.Allocate(inst.GetName(), Registers::ThreadStatePointer());
//                 break;
//             }
//             case IROpcode::Immediate: {
//                 if (inst.GetImmediateData() == 0) {
//                     allocation_map.Allocate(inst.GetName(), Registers::Zero());
//                 }
//                 break;
//             }
//             default:
//                 break;
//             }
//         }
//     }

//     // Computes variables live at the exit of a block
//     std::vector<LivenessSet> out;
//     liveness(function, out);

//     bool was_spilled = false;

//     std::array<InterferenceGraph, 3> graphs;
//     InterferenceGraph& gpr_graph = graphs[0];
//     InterferenceGraph& fpr_graph = graphs[1];
//     InterferenceGraph& vec_graph = graphs[2];
//     construct_interference_graph(function, allocation_map, graphs, out);
// }