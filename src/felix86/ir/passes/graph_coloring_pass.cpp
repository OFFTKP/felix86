#include <stack>
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

    auto begin() {
        return graph.begin();
    }

    auto end() {
        return graph.end();
    }

    auto find(u32 id) {
        return graph.find(id);
    }

    bool empty() {
        return graph.empty();
    }

    void clear() {
        graph.clear();
    }

    size_t size() {
        return graph.size();
    }

private:
    tsl::robin_map<u32, std::unordered_set<u32>> graph;
};

/*
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

// Nothing interferes with these as they are always available
bool IsHardcolored(const BackendInstruction& inst) {
    switch (inst.GetOpcode()) {
    case IROpcode::Immediate: {
        return inst.GetImmediateData() == 0;
    }
    case IROpcode::GetThreadStatePointer:
        return true;
    default:
        return false;
    }
}

using LivenessSet = std::set<u32>;

struct InterferenceGraphs {
    InterferenceGraph gpr_graph;
    InterferenceGraph fpr_graph;
    InterferenceGraph vec_graph;

    void clear() {
        gpr_graph.clear();
        fpr_graph.clear();
        vec_graph.clear();
    }
};

static void build(const BackendFunction& function, InterferenceGraphs& graphs) {
    std::vector<const BackendBlock*> blocks = function.GetBlocksPostorder();
    std::vector<LivenessSet> in(blocks.size());
    std::vector<LivenessSet> out(blocks.size());
    std::vector<LivenessSet> use(blocks.size());
    std::vector<LivenessSet> def(blocks.size());

    // Populate use and def sets
    for (size_t j = 0; j < blocks.size(); j++) {
        const BackendBlock* block = blocks[j];
        size_t i = block->GetIndex();
        for (const BackendInstruction& inst : block->GetInstructions()) {
            if (inst.GetDesiredType() != AllocationType::Null) {
                def[i].insert(inst.GetName());
            }

            for (u8 j = 0; j < inst.GetOperandCount(); j++) {
                if (std::find(def[i].begin(), def[i].end(), inst.GetOperand(j)) == def[i].end()) {
                    // Not defined in this block ie. upwards exposed, live range goes outside current block
                    use[i].insert(inst.GetOperand(j));
                }
            }
        }
    }

    // Calculate in and out sets
    bool changed;
    do {
        changed = false;
        for (size_t j = 0; j < blocks.size(); j++) {
            const BackendBlock* block = blocks[j];

            // j is the index in the postorder list, but we need the index in the blocks list
            size_t i = block->GetIndex();

            LivenessSet in_old = in[i];
            LivenessSet out_old = out[i];

            in[i].clear();

            // in[b] = use[b] U (out[b] - def[b])
            in[i].insert(use[i].begin(), use[i].end());

            LivenessSet out_minus_def = out[i];
            for (u32 inst : def[i]) {
                out_minus_def.erase(inst);
            }

            in[i].insert(out_minus_def.begin(), out_minus_def.end());

            // out[b] = U (in[s]) for all s in succ[b]
            out[i].clear();
            for (u8 k = 0; k < block->GetSuccessorCount(); k++) {
                const BackendBlock* succ = &function.GetBlock(block->GetSuccessor(k));
                u32 succ_index = succ->GetIndex();
                out[i].insert(in[succ_index].begin(), in[succ_index].end());
            }

            // check for changes
            if (!changed) {
                changed = in[i] != in_old || out[i] != out_old;
            }
        }
    } while (changed);

    for (const BackendBlock* block : function.GetBlocksPostorder()) {
        LivenessSet live_now;

        // We are gonna walk the block backwards, first add all definitions that have lifetime
        // that extends past this basic block
        live_now.insert(out[block->GetIndex()].begin(), out[block->GetIndex()].end());

        const std::list<BackendInstruction>& insts = block->GetInstructions();
        for (auto it = insts.rbegin(); it != insts.rend(); ++it) {
            const BackendInstruction& inst = *it;

            // Erase the currently defined variable if it exists in the set
            live_now.erase(inst.GetName());

            for (u8 i = 0; i < inst.GetOperandCount(); i++) {
                live_now.insert(inst.GetOperand(i));
            }

            switch (inst.GetDesiredType()) {
            case AllocationType::GPR: {
                for (auto& live : live_now) {
                    graphs.gpr_graph.AddEdge(inst.GetName(), live);
                }
                break;
            }
            case AllocationType::FPR: {
                for (auto& live : live_now) {
                    graphs.fpr_graph.AddEdge(inst.GetName(), live);
                }
                break;
            }
            case AllocationType::Vec: {
                for (auto& live : live_now) {
                    graphs.vec_graph.AddEdge(inst.GetName(), live);
                }
                break;
            }
            case AllocationType::Null:
                break;
            default:
                UNREACHABLE();
                break;
            }
        }
    }

    // Easier to remove than to not add in the first place...
    // TODO: dont add them in the first place
    for (const BackendBlock& block : function.GetBlocks()) {
        for (const BackendInstruction& inst : block.GetInstructions()) {
            if (IsHardcolored(inst)) {
                graphs.gpr_graph.RemoveNode(inst.GetName());
            }
        }
    }
}

static void spill(BackendFunction& function, u32 node, u32 location) {
    for (BackendBlock& block : function.GetBlocks()) {
        auto it = block.GetInstructions().begin();
        while (it != block.GetInstructions().end()) {
            BackendInstruction& inst = *it;
            if (inst.GetName() == node) {
                BackendInstruction store = BackendInstruction::FromStoreSpill(node, location);
                // Insert right after this instruction
                auto next = std::next(it);
                block.GetInstructions().insert(next, store);
                it = next;
            } else {
                u32 name = 0;
                for (u8 i = 0; i < inst.GetOperandCount(); i++) {
                    if (inst.GetOperand(i) == node) {
                        name = block.GetNextName();
                        BackendInstruction load = BackendInstruction::FromLoadSpill(name, location, inst.GetDesiredType());
                        // Insert right before this instruction
                        it = block.GetInstructions().insert(it, load);
                        break;
                    }
                }

                if (name != 0) {
                    for (u8 i = 0; i < inst.GetOperandCount(); i++) {
                        if (inst.GetOperand(i) == node) {
                            // Use the new name, effectively splitting the live range
                            inst.SetOperand(i, name);
                        }
                    }
                }

                ++it;
            }
        }
    }
}

AllocationMap ir_graph_coloring_pass(BackendFunction& function) {
    AllocationMap allocation_map;

    for (const BackendBlock& block : function.GetBlocks()) {
        for (const BackendInstruction& inst : block.GetInstructions()) {
            if (inst.GetOpcode() == IROpcode::Immediate && inst.GetImmediateData() == 0) {
                allocation_map.Allocate(inst.GetName(), Registers::Zero());
            } else if (inst.GetOpcode() == IROpcode::GetThreadStatePointer) {
                allocation_map.Allocate(inst.GetName(), Registers::ThreadStatePointer());
            }
        }
    }

    bool repeat = false;
    InterferenceGraphs graphs;
    std::stack<Node> colorable;
    const auto& gprs = Registers::GetAllocatableGPRs();
    do {
        repeat = false;
        graphs.clear();
        colorable = {};

        build(function, graphs);

        InterferenceGraph& gpr_graph = graphs.gpr_graph;

        u32 k = gprs.size();

        // Remove nodes with degree < k
        bool less_than_k = false; // while some node has degree < k
        do {
            less_than_k = false;
            for (auto& [id, edges] : gpr_graph) {
                if (edges.size() < k) {
                    colorable.push(gpr_graph.RemoveNode(id));
                    less_than_k = true;
                }
            }
        } while (less_than_k);

        // If there are no nodes with degree < k, we are done
        if (gpr_graph.empty()) {
            break;
        }

        // We need to spill
        repeat = true;

        // Find the node with the highest degree
        u32 max_degree = 0;
        u32 max_degree_node = 0;
        for (auto& [id, edges] : gpr_graph) {
            if (edges.size() > max_degree) {
                max_degree = edges.size();
                max_degree_node = id;
            }
        }

        ASSERT_MSG(max_degree >= k, "max_degree: %d, k: %d", max_degree, k);
        gpr_graph.RemoveNode(max_degree_node);

        u32 spill_location = allocation_map.IncrementSpillSize(8);
        spill(function, max_degree_node, spill_location);
    } while (repeat);

    // All nodes should be colorable now
    while (!colorable.empty()) {
        Node node = colorable.top();
        colorable.pop();

        auto [id, edges] = node;

        // Find the first color that is not in the interference set
        biscuit::GPR color = x0;
        std::array<bool, gprs.size()> used = {false};
        for (u32 edge : edges) {
            if (allocation_map.IsAllocated(edge)) {
                used[Registers::GetGPRIndex(allocation_map.GetAllocation(edge))] = true;
            }
        }

        for (u32 i = 0; i < gprs.size(); i++) {
            if (!used[i]) {
                color = gprs[i];
                break;
            }
        }

        ASSERT(color != x0);

        allocation_map.Allocate(id, color);
    }

    return allocation_map;
}
