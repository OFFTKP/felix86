#include <deque>
#include <unordered_set>
#include "felix86/ir/passes/passes.hpp"

struct Node {
    u32 id{};
    std::unordered_set<Node*> edges{};

    Node(u32 id) : id(id) {}
    Node(u32 id, const std::unordered_set<Node*>& edges) : id(id), edges(edges) {}
    Node(const Node& other) = delete;
    Node(Node&& other) = default;
    Node& operator=(const Node& other) = delete;
    Node& operator=(Node&& other) = default;
};

struct CopiedNode {
    u32 id{};
    std::vector<u32> edges{};

    CopiedNode(const Node& node) : id(node.id) {
        for (const Node* edge : node.edges) {
            edges.push_back(edge->id);
        }
    }
};

struct InstructionMetadata {
    BackendInstruction* inst = nullptr;
    u32 spill_cost = 0; // sum of uses + defs (loads and stores that would have to be inserted)
    u32 interferences = 0;
    bool infinite_cost = false;
};

using InstructionMap = std::unordered_map<u32, InstructionMetadata>;

using InstructionList = std::vector<const BackendInstruction*>;

struct InterferenceGraph {
    void AddEdge(u32 a, u32 b) {
        Node* na = graph[a];
        Node* nb = graph[b];

        na->edges.insert(nb);
        nb->edges.insert(na);
    }

    // I forgot why this function was needed, TODO: remove
    void AddEmpty(u32 id) {
        if (graph.find(id) == graph.end())
            graph[id] = new Node{id};
    }

    CopiedNode RemoveNode(u32 id) {
        Node* node = graph[id];
        ASSERT(node);
        for (Node* edge : node->edges) {
            ASSERT(edge);
            ASSERT(edge != node);
            edge->edges.erase(node);
        }
        CopiedNode node_copy{*node};
        graph.erase(id);
        return node_copy;
    }

    u32 Worst(const InstructionMap& instructions) {
        u32 min = std::numeric_limits<u32>::max();
        u32 chosen = 0;
        bool valid = false;
        for (const auto& [id, node] : graph) {
            if (node->edges.size() == 0)
                continue;
            if (instructions.at(id).infinite_cost)
                continue;
            float spill_cost = instructions.at(id).spill_cost;
            float cost = spill_cost / node->edges.size();
            if (cost < min) {
                min = cost;
                chosen = id;
                valid = true;
            }
        }
        ASSERT(valid);
        return chosen;
    }

    bool Interferes(u32 lhs, u32 rhs) {
        ASSERT(graph.find(lhs) != graph.end());
        Node* a = graph[lhs];
        for (Node* b : a->edges) {
            if (b->id == rhs) {
                return true;
            }
        }

        return false;
    }

    Node* GetNode(u32 id) {
        if (graph.find(id) == graph.end())
            graph[id] = new Node{id};
        return graph[id];
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
        for (const auto& [id, node] : graph) {
            if (node->edges.size() < k) {
                return true;
            }
        }
        return false;
    }

private:
    std::unordered_map<u32, Node*> graph;
};

using LivenessSet = std::unordered_set<u32>;

using CoalescingHeuristic = bool (*)(BackendFunction& function, InterferenceGraph& graph, u32 k, u32 lhs, u32 rhs);

static InstructionMap create_instruction_map(BackendFunction& function) {
    InstructionMap instructions;
    for (BackendBlock* block : function.GetBlocks()) {
        for (BackendInstruction& inst : block->GetInstructions()) {
            instructions[inst.GetName()].inst = &inst;
            instructions[inst.GetName()].spill_cost += 1;

            if (inst.IsLocked()) {
                instructions[inst.GetName()].infinite_cost = true;
            }

            for (u8 i = 0; i < inst.GetOperandCount(); i++) {
                instructions[inst.GetOperand(i)].spill_cost += 1;
            }
        }
    }
    return instructions;
}

static void spill(BackendFunction& function, u32 node, u32 location, AllocationType spill_type) {
    VERBOSE("Spilling %s", GetNameString(node).c_str());
    g_spilled_count += 1;
    if (g_spilled_count > 5) {
        WARN("Function %016lx has spilled %d times", function.GetStartAddress(), g_spilled_count);
    }
    for (BackendBlock* block : function.GetBlocks()) {
        auto it = block->GetInstructions().begin();
        while (it != block->GetInstructions().end()) {
            BackendInstruction& inst = *it;
            if (inst.GetName() == node) {
                u32 name = block->GetNextName();
                BackendInstruction store = BackendInstruction::FromStoreSpill(name, node, location);
                // Insert right after this instruction
                auto next = std::next(it);
                block->GetInstructions().insert(next, store);
                it = next;
            } else {
                for (u8 i = 0; i < inst.GetOperandCount(); i++) {
                    if (inst.GetOperand(i) == node) {
                        u32 name = block->GetNextName();
                        BackendInstruction load = BackendInstruction::FromLoadSpill(name, location, spill_type);
                        // Insert right before this instruction
                        it = block->GetInstructions().insert(it, load);

                        // Replace all operands
                        for (u8 j = 0; j < inst.GetOperandCount(); j++) {
                            if (inst.GetOperand(j) == node) {
                                inst.SetOperand(j, name, spill_type);
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

static void liveness_worklist2(BackendFunction& function, std::vector<const BackendBlock*> blocks, std::vector<std::unordered_set<u32>>& in,
                               std::vector<std::unordered_set<u32>>& out, const std::vector<std::unordered_set<u32>>& use,
                               const std::vector<std::unordered_set<u32>>& def) {
    std::deque<u32> worklist;
    for (u32 i = 0; i < blocks.size(); i++) {
        worklist.push_back(blocks[i]->GetIndex());
    }

    while (!worklist.empty()) {
        const BackendBlock* block = &function.GetBlock(worklist.front());
        worklist.pop_front();

        size_t i = block->GetIndex();
        auto in_old = in[i];

        out[i].clear();
        // out[b] = U (in[s]) for all s in succ[b]
        for (u8 k = 0; k < block->GetSuccessorCount(); k++) {
            u32 succ_index = block->GetSuccessor(k)->GetIndex();
            for (u32 item : in[succ_index]) {
                out[i].insert(item);
            }
        }

        in[i].clear();
        // in[b] = use[b] U (out[b] - def[b])
        for (u32 item : out[i]) {
            if (!def[i].contains(item)) {
                in[i].insert(item);
            }
        }

        for (u32 item : use[i]) {
            in[i].insert(item);
        }

        if (in[i] != in_old) {
            for (u8 k = 0; k < block->GetPredecessorCount(); k++) {
                u32 pred_index = block->GetPredecessor(k)->GetIndex();
                if (std::find(worklist.begin(), worklist.end(), pred_index) == worklist.end()) {
                    worklist.push_back(pred_index);
                }
            }
        }
    }
}

static bool should_consider(const BackendInstruction* inst, bool is_vec) {
    if (is_vec && inst->GetDesiredType() == AllocationType::Vec) {
        return true;
    }

    if (!is_vec && inst->GetDesiredType() == AllocationType::GPR) {
        return true;
    }

    return false;
}

static bool should_consider_op(const BackendInstruction* inst, u8 index, bool is_vec) {
    if (is_vec && inst->GetOperandDesiredType(index) == AllocationType::Vec) {
        return true;
    }

    if (!is_vec && inst->GetOperandDesiredType(index) == AllocationType::GPR) {
        return true;
    }

    return false;
}

static void build2(BackendFunction& function, std::vector<const BackendBlock*> blocks, InterferenceGraph& graph, bool is_vec) {
    std::vector<std::unordered_set<u32>> in(blocks.size());
    std::vector<std::unordered_set<u32>> out(blocks.size());
    std::vector<std::unordered_set<u32>> use(blocks.size());
    std::vector<std::unordered_set<u32>> def(blocks.size());

    for (size_t counter = 0; counter < blocks.size(); counter++) {
        const BackendBlock* block = blocks[counter];
        size_t i = block->GetIndex();
        for (const BackendInstruction& inst : block->GetInstructions()) {
            for (u8 j = 0; j < inst.GetOperandCount(); j++) {
                if (should_consider_op(&inst, j, is_vec) && !def[i].contains(inst.GetOperand(j))) {
                    // Not defined in this block ie. upwards exposed, live range goes outside current block
                    use[i].insert(inst.GetOperand(j));
                }
            }

            if (should_consider(&inst, is_vec)) {
                def[i].insert(inst.GetName());
                graph.AddEmpty(inst.GetName());
            }
        }
    }

    liveness_worklist2(function, blocks, in, out, use, def);

    for (const BackendBlock* block : blocks) {
        std::unordered_set<u32> live_now;

        // We are gonna walk the block backwards, first add all definitions that have lifetime
        // that extends past this basic block
        // live_now.insert(out[block->GetIndex()].begin(), out[block->GetIndex()].end());
        for (u32 item : out[block->GetIndex()]) {
            live_now.insert(item);
        }

        const std::list<BackendInstruction>& insts = block->GetInstructions();
        for (auto it = insts.rbegin(); it != insts.rend(); ++it) {
            const BackendInstruction& inst = *it;
            if (should_consider(&inst, is_vec)) {
                // Erase the currently defined variable if it exists in the set
                live_now.erase(inst.GetName());

                // Some instructions, due to RISC-V ISA, can't allocate the same register
                // to destination and source operands. For example, viota, vslideup, vrgather.
                // For those, we make it so the operands interfere with the destination so
                // the register allocator doesn't pick the same register.
                // This function tells the liveness analysis to erase the current instruction from the set
                // after adding interferences.
                switch (inst.GetOpcode()) {
                case IROpcode::VIota:
                case IROpcode::VSlide1Up:
                case IROpcode::VSlideUpZeroesi:
                case IROpcode::VSlideDownZeroesi:
                case IROpcode::VSlideUpi:
                case IROpcode::VWCvtFToS:
                case IROpcode::VNCvtSToF:
                case IROpcode::VNCvtFToS:
                case IROpcode::VWCvtSToF:
                case IROpcode::VWCvtFToSRtz:
                case IROpcode::VNCvtFToSRtz: {
                    for (u8 i = 0; i < inst.GetOperandCount(); i++) {
                        if (should_consider_op(&inst, i, is_vec)) {
                            live_now.insert(inst.GetOperand(i));
                        }
                    }
                    break;
                }
                case IROpcode::VGather: {
                    // Doesn't interfere with the first operand
                    for (u8 i = 1; i < inst.GetOperandCount(); i++) {
                        if (should_consider_op(&inst, i, is_vec)) {
                            live_now.insert(inst.GetOperand(i));
                        }
                    }
                    break;
                }
                default:
                    break;
                }

                // in case there's nothing live (which is possible if nothing is read before written)
                // then we need to add the current instruction to the graph so it gets allocated
                for (u32 item : live_now) {
                    graph.AddEdge(inst.GetName(), item);
                }
            }

            for (u8 i = 0; i < inst.GetOperandCount(); i++) {
                if (should_consider_op(&inst, i, is_vec)) {
                    live_now.insert(inst.GetOperand(i));
                }
            }
        }
    }
}

static u32 choose(const InstructionMap& instructions, const std::deque<CopiedNode>& nodes, bool is_vec) {
    float min = std::numeric_limits<float>::max();
    u32 chosen = 0;

    for (auto& node : nodes) {
        auto& inst = instructions.at(node.id);
        float spill_cost = inst.spill_cost;
        float degree = inst.interferences;
        if (!should_consider(inst.inst, is_vec)) {
            continue;
        }
        if (degree == 0)
            continue;
        if (inst.infinite_cost)
            continue;
        float cost = spill_cost / degree;
        if (cost < min) {
            min = cost;
            chosen = node.id;
        }
    }

    ASSERT(chosen != 0); // all nodes have infinite cost???
    return chosen;
}

bool __attribute__((noinline)) george_coalescing_heuristic(BackendFunction& function, InterferenceGraph& graph, u32 k, u32 lhs, u32 rhs) {
    // A conservative heuristic.
    // Safe to coalesce x and y if for every neighbor t of x, either t already interferes with y or t has degree < k
    u32 u = lhs;
    u32 v = rhs;

    Node* nu = graph.GetNode(u);
    Node* nv = graph.GetNode(v);

    auto& u_neighbors = nu->edges;
    bool u_conquers_v = true;
    for (Node* t : nv->edges) {
        if (std::find(u_neighbors.begin(), u_neighbors.end(), t) == u_neighbors.end() && t->edges.size() >= k) {
            u_conquers_v = false;
            break;
        }
    }

    auto& v_neighbors = nv->edges;
    bool v_conquers_u = true;
    for (Node* t : nu->edges) {
        if (std::find(v_neighbors.begin(), v_neighbors.end(), t) == v_neighbors.end() && t->edges.size() >= k) {
            v_conquers_u = false;
            break;
        }
    }

    return u_conquers_v || v_conquers_u;
}

void coalesce(BackendFunction& function, u32 lhs, u32 rhs, AllocationType rhs_type) {
    for (BackendBlock* block : function.GetBlocks()) {
        for (BackendInstruction& inst : block->GetInstructions()) {
            for (u8 i = 0; i < inst.GetOperandCount(); i++) {
                if (inst.GetOperand(i) == lhs) {
                    inst.SetOperand(i, rhs, rhs_type);
                }
            }
            if (inst.GetName() == lhs) {
                inst.SetName(rhs);
            }
        }
    }
}

bool try_coalesce(BackendFunction& function, InterferenceGraph& graph, bool is_vec, u32 k, CoalescingHeuristic heuristic) {
    bool coalesced = false;
    for (auto& block : function.GetBlocks()) {
        auto it = block->GetInstructions().begin();
        auto end = block->GetInstructions().end();
        while (it != end) {
            BackendInstruction& inst = *it;
            if (inst.GetOpcode() == IROpcode::Mov) {
                if (should_consider(&inst, is_vec) && should_consider_op(&inst, 0, is_vec)) {
                    u32 lhs = inst.GetName();
                    u32 rhs = inst.GetOperand(0);
                    AllocationType rhs_type = inst.GetOperandDesiredType(0);
                    Node* node = graph.GetNode(lhs);
                    if (!graph.Interferes(lhs, rhs)) {
                        if (heuristic(function, graph, k, lhs, rhs)) {
                            coalesce(function, lhs, rhs, rhs_type);
                            it = block->GetInstructions().erase(it);
                            coalesced = true;
                            // Merge interferences into rhs
                            for (Node* neighbor : node->edges) {
                                u32 neighbor_id = neighbor->id;
                                if (neighbor_id != rhs) {
                                    graph.AddEdge(rhs, neighbor_id);
                                }
                            }
                            continue;
                        }
                    }
                }
            }
            ++it;
        }
    }
    return coalesced;
}

static AllocationMap run(BackendFunction& function, AllocationType type, const std::vector<u32>& available_colors, u32& spill_location) {
    g_spilled_count = 0;
    const u32 k = available_colors.size();
    std::vector<const BackendBlock*> blocks = function.GetBlocksPostorder();
    while (true) {
        // Chaitin-Briggs algorithm
        std::deque<CopiedNode> nodes;
        InterferenceGraph graph;
        InstructionMap instructions;
        AllocationMap allocations;
        bool coalesced = false;

        do {
            coalesced = false;
            graph = InterferenceGraph();
            instructions = create_instruction_map(function);
            build2(function, blocks, graph, type == AllocationType::Vec);

            if (g_coalesce) {
                coalesced = try_coalesce(function, graph, type == AllocationType::Vec, k, george_coalescing_heuristic);
            }
        } while (coalesced);

        for (auto& [name, node] : graph) {
            ASSERT_MSG(instructions.find(name) != instructions.end(), "Instruction %s not found in map", GetNameString(name).c_str());
            instructions.at(name).interferences = node->edges.size();
        }

        while (true) {
            // While there's vertices with degree less than k
            while (graph.HasLessThanK(k)) {
                // Pick any node with degree less than k and put it on the stack
                for (auto& [id, node] : graph) {
                    if (node->edges.size() < k) {
                        nodes.push_back(graph.RemoveNode(id));
                        break;
                    }
                }
                // Removing nodes might have created more with degree less than k, repeat
            }

            bool repeat_outer = false;

            // If graph is not empty, all vertices have more than k neighbors
            while (!graph.empty()) {
                // Pick some vertex using a heuristic and remove it.
                // If it causes some node to have less than k neighbors, repeat at step 1, otherwise repeat step 2.
                nodes.push_back(graph.RemoveNode(graph.Worst(instructions)));

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

        auto it = nodes.rbegin();
        for (it = nodes.rbegin(); it != nodes.rend();) {
            CopiedNode& node = *it;

            std::vector<u32> colors = available_colors;
            for (u32 neighbor : node.edges) {
                if (allocations.IsAllocated(neighbor)) {
                    u32 allocation = allocations.GetAllocation(neighbor).GetIndex();
                    std::erase(colors, allocation);
                }
            }

            if (colors.empty()) {
                colored = false;
                it++;

                // According to Briggs thesis on register allocation:
                // Select may discover that it has no color available for some node.
                // In that case it leaves the node uncolored and continues with the next node.
                continue;
            }

            // it's just the erase equivalent when working with rbegin/rend
            it = decltype(it)(nodes.erase(std::next(it).base()));
            allocations.Allocate(node.id, type, colors[0]);
        }

        if (colored) {
            // Allocation has succeeded, they all got colored
            return allocations;
        } else {
            // Must spill one of the nodes
            u32 chosen_node = choose(instructions, nodes, type == AllocationType::Vec);
            spill(function, chosen_node, spill_location, type);
            spill_location += type == AllocationType::Vec ? 16 : 8;
        }
    }
}

AllocationMap ir_graph_coloring_pass(BackendFunction& function) {
    VERBOSE("Register allocation starting");
    AllocationMap allocations;
    u32 spill_location = 0;

    std::vector<u32> available_gprs, available_vecs;
    for (auto& gpr : Registers::GetAllocatableGPRs()) {
        available_gprs.push_back(gpr.Index());
    }

    for (auto& vec : Registers::GetAllocatableVecs()) {
        available_vecs.push_back(vec.Index());
    }

    AllocationMap gpr_map = run(function, AllocationType::GPR, available_gprs, spill_location);
    AllocationMap vec_map = run(function, AllocationType::Vec, available_vecs, spill_location);

    // Merge the maps
    for (auto& [name, allocation] : gpr_map) {
        allocations.Allocate(name, biscuit::GPR(allocation));
    }

    for (auto& [name, allocation] : vec_map) {
        allocations.Allocate(name, biscuit::Vec(allocation));
    }

    for (BackendBlock* block : function.GetBlocks()) {
        for (BackendInstruction& inst : block->GetInstructions()) {
            if (inst.GetOpcode() == IROpcode::GetThreadStatePointer) {
                allocations.Allocate(inst.GetName(), Registers::ThreadStatePointer());
            } else if (inst.GetOpcode() == IROpcode::Immediate && inst.GetImmediateData() == 0) {
                allocations.Allocate(inst.GetName(), Registers::Zero());
            }
        }
    }

    allocations.SetSpillSize(spill_location);

    VERBOSE("Register allocation done");

    return allocations;
}
