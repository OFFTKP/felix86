#include <deque>
#include <stack>
#include <unordered_set>
#include "felix86/ir/passes/passes.hpp"

struct Node {
    u32 id;
    std::unordered_set<u32> edges;
};

struct InterferenceGraph {
    void AddEdge(u32 a, u32 b) {
        graph[b].insert(a);
        graph[a].insert(b);
    }

    void AddEmpty(u32 id) {
        if (graph.find(id) == graph.end())
            graph[id] = {};
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

    bool HasLessThanK(u32 k) {
        for (const auto& [id, edges] : graph) {
            if (edges.size() < k) {
                return true;
            }
        }
        return false;
    }

private:
    tsl::robin_map<u32, std::unordered_set<u32>> graph;
};

using LivenessSet = std::unordered_set<u32>;

using InstructionMap = tsl::robin_map<u32, BackendInstruction*>;

static bool reserved_gpr(const BackendInstruction& inst) {
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

static InstructionMap create_instruction_map(BackendFunction& function) {
    InstructionMap instructions;
    for (BackendBlock& block : function.GetBlocks()) {
        for (BackendInstruction& inst : block.GetInstructions()) {
            instructions[inst.GetName()] = &inst;
        }
    }
    return instructions;
}

static bool should_consider_gpr(const InstructionMap& map, u32 inst) {
    const BackendInstruction& instruction = *map.at(inst);
    return instruction.GetDesiredType() == AllocationType::GPR && !reserved_gpr(instruction);
}

static bool should_consider_fpr(const InstructionMap& map, u32 inst) {
    const BackendInstruction& instruction = *map.at(inst);
    return instruction.GetDesiredType() == AllocationType::FPR;
}

static bool should_consider_vec(const InstructionMap& map, u32 inst) {
    const BackendInstruction& instruction = *map.at(inst);
    return instruction.GetDesiredType() == AllocationType::Vec;
}

static void spill(BackendFunction& function, u32 node, u32 location, AllocationType spill_type) {
    printf("Spilling %s\n", GetNameString(node).c_str());
    for (BackendBlock& block : function.GetBlocks()) {
        auto it = block.GetInstructions().begin();
        while (it != block.GetInstructions().end()) {
            BackendInstruction& inst = *it;
            if (inst.GetName() == node) {
                u32 name = block.GetNextName();
                BackendInstruction store = BackendInstruction::FromStoreSpill(name, node, location);
                // Insert right after this instruction
                auto next = std::next(it);
                block.GetInstructions().insert(next, store);
                it = next;
            } else {
                for (u8 i = 0; i < inst.GetOperandCount(); i++) {
                    if (inst.GetOperand(i) == node) {
                        u32 name = block.GetNextName();
                        BackendInstruction load = BackendInstruction::FromLoadSpill(name, location, spill_type);
                        // Insert right before this instruction
                        it = block.GetInstructions().insert(it, load);

                        // Replace all operands
                        for (u8 j = 0; j < inst.GetOperandCount(); j++) {
                            if (inst.GetOperand(j) == node) {
                                inst.SetOperand(j, name);
                            }
                        }
                        break;
                    }
                }

                ++it;
            }
        }
    }
}

static void build(const BackendFunction& function, InterferenceGraph& graph, bool (*should_consider)(const InstructionMap&, u32),
                  const InstructionMap& instructions) {
    std::vector<const BackendBlock*> blocks = function.GetBlocksPostorder();

    std::vector<LivenessSet> in(blocks.size());
    std::vector<LivenessSet> out(blocks.size());
    std::vector<LivenessSet> use(blocks.size());
    std::vector<LivenessSet> def(blocks.size());

    for (size_t counter = 0; counter < blocks.size(); counter++) {
        const BackendBlock* block = blocks[counter];
        size_t i = block->GetIndex();
        for (const BackendInstruction& inst : block->GetInstructions()) {
            if (should_consider(instructions, inst.GetName())) {
                def[i].insert(inst.GetName());
            }

            for (u8 j = 0; j < inst.GetOperandCount(); j++) {
                if (should_consider(instructions, inst.GetOperand(j)) &&
                    std::find(def[i].begin(), def[i].end(), inst.GetOperand(j)) == def[i].end()) {
                    // Not defined in this block ie. upwards exposed, live range goes outside current block
                    use[i].insert(inst.GetOperand(j));
                }
            }
        }
    }

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
            for (u32 def_inst : def[i]) {
                out_minus_def.erase(def_inst);
            }

            in[i].insert(out_minus_def.begin(), out_minus_def.end());

            out[i].clear();
            // out[b] = U (in[s]) for all s in succ[b]
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

    for (const BackendBlock* block : blocks) {
        LivenessSet live_now;

        // We are gonna walk the block backwards, first add all definitions that have lifetime
        // that extends past this basic block
        live_now.insert(out[block->GetIndex()].begin(), out[block->GetIndex()].end());

        const std::list<BackendInstruction>& insts = block->GetInstructions();
        for (auto it = insts.rbegin(); it != insts.rend(); ++it) {
            const BackendInstruction& inst = *it;
            if (should_consider(instructions, inst.GetName())) {
                // Erase the currently defined variable if it exists in the set
                live_now.erase(inst.GetName());

                // in case there's nothing live (which is possible if nothing is read before written)
                // then we need to add the current instruction to the graph so it gets allocated
                graph.AddEmpty(inst.GetName());
                for (u32 live : live_now) {
                    graph.AddEdge(inst.GetName(), live);
                }
            }

            for (u8 i = 0; i < inst.GetOperandCount(); i++) {
                if (should_consider(instructions, inst.GetOperand(i))) {
                    live_now.insert(inst.GetOperand(i));
                }
            }
        }
    }
}

static AllocationMap run(BackendFunction& function, const InstructionMap& instructions, AllocationType type,
                         bool (*should_consider)(const InstructionMap&, u32), const std::vector<u32>& available_colors, u32& spill_location) {
    const u32 k = available_colors.size();
    while (true) {
        // Chaitin-Briggs algorithm
        std::deque<Node> nodes;
        InterferenceGraph graph;
        AllocationMap allocations;
        build(function, graph, should_consider, instructions);

        while (true) {
            // While there's vertices with degree less than k
            while (graph.HasLessThanK(k)) {
                // Pick any node with degree less than k and put it on the stack
                std::stack<u32> to_remove;
                for (auto& [id, edges] : graph) {
                    if (edges.size() < k) {
                        to_remove.push(id);
                    }
                }

                while (!to_remove.empty()) {
                    u32 id = to_remove.top();
                    to_remove.pop();
                    nodes.push_back(graph.RemoveNode(id));
                }
                // Removing nodes might have created more with degree less than k, repeat
            }

            bool repeat_outer = false;

            // If graph is not empty, all vertices have more than k neighbors
            while (!graph.empty()) {
                // Pick some vertex using a heuristic and remove it.
                // If it causes some node to have less than k neighbors, repeat at step 1, otherwise repeat step 2.
                nodes.push_back(graph.RemoveNode(graph.begin()->first));

                if (graph.HasLessThanK(k)) {
                    repeat_outer = true;
                    break; // break, return to top of while loop
                }
            }

            if (!repeat_outer) {
                // Graph is empty, try to color
                break;
            }
        }

        // Try to color the nodes
        bool colored = true;

        while (!nodes.empty()) {
            Node node = nodes.back();

            std::vector<u32> colors = available_colors;
            for (u32 neighbor : node.edges) {
                if (allocations.IsAllocated(neighbor)) {
                    u32 allocation = allocations.GetAllocationIndex(neighbor);
                    std::erase(colors, allocation);
                }
            }

            if (colors.empty()) {
                colored = false;
                break;
            }

            allocations.Allocate(node.id, type, colors[0]);
            nodes.pop_back();
        }

        if (colored) {
            return allocations;
        } else {
            // Must spill one of the nodes
            spill(function, nodes.back().id, spill_location, type);
            spill_location += 8;
        }
    }
}

AllocationMap ir_graph_coloring_pass(BackendFunction& function) {
    AllocationMap allocations;
    u32 spill_location = 0;
    InstructionMap instructions = create_instruction_map(function);

    std::vector<u32> available_gprs, available_fprs, available_vecs;
    for (auto& gpr : Registers::GetAllocatableGPRs()) {
        available_gprs.push_back(gpr.Index());
    }

    for (auto& fpr : Registers::GetAllocatableFPRs()) {
        available_fprs.push_back(fpr.Index());
    }

    for (auto& vec : Registers::GetAllocatableVecs()) {
        available_vecs.push_back(vec.Index());
    }

    AllocationMap gpr_map = run(function, instructions, AllocationType::GPR, should_consider_gpr, available_gprs, spill_location);
    AllocationMap fpr_map = run(function, instructions, AllocationType::FPR, should_consider_fpr, available_fprs, spill_location);
    AllocationMap vec_map = run(function, instructions, AllocationType::Vec, should_consider_vec, available_vecs, spill_location);

    // Merge the maps
    for (auto& [name, allocation] : gpr_map) {
        allocations.Allocate(name, biscuit::GPR(allocation));
    }

    for (auto& [name, allocation] : fpr_map) {
        allocations.Allocate(name, biscuit::FPR(allocation));
    }

    for (auto& [name, allocation] : vec_map) {
        allocations.Allocate(name, biscuit::Vec(allocation));
    }

    for (BackendBlock& block : function.GetBlocks()) {
        for (BackendInstruction& inst : block.GetInstructions()) {
            if (inst.GetOpcode() == IROpcode::GetThreadStatePointer) {
                allocations.Allocate(inst.GetName(), Registers::ThreadStatePointer());
            } else if (inst.GetOpcode() == IROpcode::Immediate && inst.GetImmediateData() == 0) {
                allocations.Allocate(inst.GetName(), Registers::Zero());
            }
        }
    }

    allocations.SetSpillSize(spill_location);

    return allocations;
}