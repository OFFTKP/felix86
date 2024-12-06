#include <deque>
#include <unordered_set>
#include "felix86/ir/passes/passes.hpp"

struct Node {
    u32 id;
    std::unordered_set<u32> edges;
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
        graph[b].edges.insert(a);
        graph[a].edges.insert(b);
    }

    void AddEmpty(u32 id) {
        if (graph.find(id) == graph.end())
            graph[id] = {};
    }

    void RemoveEdge(u32 a, u32 b) {
        graph[a].edges.erase(b);
        graph[b].edges.erase(a);
    }

    Node RemoveNode(u32 id) {
        Node node = {id, graph[id].edges};
        auto& edges = graph[id].edges;
        for (u32 edge : edges) {
            graph[edge].edges.erase(id);
        }
        graph.erase(id);
        return node;
    }

    u32 Worst(const InstructionMap& instructions) {
        u32 min = std::numeric_limits<u32>::max();
        u32 chosen = 0;
        for (const auto& [id, edges] : graph) {
            if (edges.edges.size() == 0)
                continue;
            if (instructions.at(id).infinite_cost)
                continue;
            float spill_cost = instructions.at(id).spill_cost;
            float cost = spill_cost / edges.edges.size();
            if (cost < min) {
                min = cost;
                chosen = id;
            }
        }
        return chosen;
    }

    void AddNode(const Node& node) {
        for (u32 edge : node.edges) {
            AddEdge(node.id, edge);
        }
    }

    const std::unordered_set<u32>& GetInterferences(u32 inst) {
        return graph[inst].edges;
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
            if (edges.edges.size() < k) {
                return true;
            }
        }
        return false;
    }

    void Reserve(size_t size) {
        graph.reserve(size);
    }

    bool operator==(const InterferenceGraph& other) const {
        if (graph.size() != other.graph.size()) {
            return false;
        }

        for (const auto& [id, edges] : graph) {
            if (other.graph.find(id) == other.graph.end()) {
                return false;
            }

            if (edges.edges.size() != other.graph.at(id).edges.size()) {
                return false;
            }

            for (u32 edge : edges.edges) {
                if (other.graph.at(id).edges.find(edge) == other.graph.at(id).edges.end()) {
                    return false;
                }
            }
        }

        return true;
    }

private:
    struct Edges {
        std::unordered_set<u32> edges;
    };

    std::unordered_map<u32, Edges> graph;
};

using LivenessSet = std::unordered_set<u32>;

using CoalescingHeuristic = bool (*)(BackendFunction& function, InterferenceGraph& graph, u32 k, u32 lhs, u32 rhs);

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

static bool should_consider_gpr(const BackendInstruction* instruction) {
    return instruction->GetDesiredType() == AllocationType::GPR && !reserved_gpr(*instruction);
}

static bool should_consider_vec(const BackendInstruction* instruction) {
    return instruction->GetDesiredType() == AllocationType::Vec;
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

static void liveness_iterative(const BackendFunction& function, const std::vector<const BackendBlock*>& blocks, std::vector<LivenessSet>& in,
                               std::vector<LivenessSet>& out, std::vector<LivenessSet>& use, std::vector<LivenessSet>& def) {
    bool changed;
    do {
        changed = false;
        for (size_t j = 0; j < blocks.size(); j++) {
            const BackendBlock* block = blocks[j];

            // j is the index in the postorder list, but we need the index in the blocks list
            size_t i = block->GetIndex();

            LivenessSet in_old = in[i];
            LivenessSet out_old = out[i];

            out[i].clear();
            // out[b] = U (in[s]) for all s in succ[b]
            for (u8 k = 0; k < block->GetSuccessorCount(); k++) {
                u32 succ_index = block->GetSuccessor(k)->GetIndex();
                out[i].insert(in[succ_index].begin(), in[succ_index].end());
            }

            LivenessSet out_minus_def = out[i];
            for (u32 def_inst : def[i]) {
                out_minus_def.erase(def_inst);
            }

            in[i].clear();
            // in[b] = use[b] U (out[b] - def[b])
            in[i].insert(use[i].begin(), use[i].end());
            in[i].insert(out_minus_def.begin(), out_minus_def.end());

            // check for changes
            if (!changed) {
                changed = in[i] != in_old || out[i] != out_old;
            }
        }
    } while (changed);
}

static void liveness_worklist(const BackendFunction& function, const std::vector<const BackendBlock*>& blocks, std::vector<LivenessSet>& in,
                              std::vector<LivenessSet>& out, std::vector<LivenessSet>& use, std::vector<LivenessSet>& def) {
    std::deque<size_t> worklist;
    for (u32 i = 0; i < blocks.size(); i++) {
        worklist.push_back(blocks[i]->GetIndex());
    }

    while (!worklist.empty()) {
        const BackendBlock* block = &function.GetBlock(worklist.front());
        worklist.pop_front();

        size_t i = block->GetIndex();

        LivenessSet in_old = in[i];

        out[i].clear();
        // out[b] = U (in[s]) for all s in succ[b]
        for (u8 k = 0; k < block->GetSuccessorCount(); k++) {
            u32 succ_index = block->GetSuccessor(k)->GetIndex();
            out[i].insert(in[succ_index].begin(), in[succ_index].end());
        }

        in[i].clear();
        // in[b] = use[b] U (out[b] - def[b])
        in[i].insert(out[i].begin(), out[i].end());
        for (u32 def_inst : def[i]) {
            in[i].erase(def_inst);
        }
        in[i].insert(use[i].begin(), use[i].end());

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

static void liveness_worklist2(BackendFunction& function, std::vector<const BackendBlock*> blocks, const InstructionList& insts,
                               std::vector<std::vector<u8>>& in, std::vector<std::vector<u8>>& out, std::vector<std::vector<u8>>& use,
                               std::vector<std::vector<u8>>& def) {
    std::deque<size_t> worklist;
    for (u32 i = 0; i < blocks.size(); i++) {
        worklist.push_back(blocks[i]->GetIndex());
    }

    auto clear_set = [&](std::vector<u8>& vec) { memset(vec.data(), 0, vec.size()); };

    std::vector<u8> in_old(insts.size());

    while (!worklist.empty()) {
        const BackendBlock* block = &function.GetBlock(worklist.front());
        worklist.pop_front();

        size_t i = block->GetIndex();

        memcpy(in_old.data(), in[i].data(), in[i].size());

        clear_set(out[i]);
        // out[b] = U (in[s]) for all s in succ[b]
        for (u8 k = 0; k < block->GetSuccessorCount(); k++) {
            u32 succ_index = block->GetSuccessor(k)->GetIndex();
            for (u32 l = 0; l < in[succ_index].size(); l++) {
                if (in[succ_index][l] == 1) {
                    out[i][l] = 1;
                }
            }
        }

        clear_set(in[i]);
        // in[b] = use[b] U (out[b] - def[b])
        for (u32 l = 0; l < out[i].size(); l++) {
            if (out[i][l] == 1) {
                in[i][l] = 1;
            }
        }

        for (u32 l = 0; l < def[i].size(); l++) {
            if (def[i][l] == 1) {
                in[i][l] = 0;
            }
        }

        for (u32 l = 0; l < use[i].size(); l++) {
            if (use[i][l] == 1) {
                in[i][l] = 1;
            }
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

static void build(BackendFunction& function, std::vector<const BackendBlock*> blocks, InterferenceGraph& graph,
                  bool (*should_consider)(const BackendInstruction*)) {
    InstructionList all_insts;
    std::unordered_map<u32, u32> name_to_index;
    for (const auto& block : function.GetBlocks()) {
        for (const auto& inst : block->GetInstructions()) {
            all_insts.push_back(&inst);
            name_to_index[inst.GetName()] = all_insts.size() - 1;
        }
    }

    auto get_index = [&](u32 name) { return name_to_index.at(name); };

    auto get_instruction = [&](u32 name) { return all_insts.at(get_index(name)); };

    static std::vector<LivenessSet> in(blocks.size());
    static std::vector<LivenessSet> out(blocks.size());
    static std::vector<LivenessSet> use(blocks.size());
    static std::vector<LivenessSet> def(blocks.size());

    if (in.size() < blocks.size()) {
        in.resize(blocks.size());
        out.resize(blocks.size());
        use.resize(blocks.size());
        def.resize(blocks.size());
    }

    for (size_t i = 0; i < blocks.size(); i++) {
        in[i].clear();
        out[i].clear();
        use[i].clear();
        def[i].clear();
    }

    for (size_t counter = 0; counter < blocks.size(); counter++) {
        const BackendBlock* block = blocks[counter];
        size_t i = block->GetIndex();
        for (const BackendInstruction& inst : block->GetInstructions()) {
            for (u8 j = 0; j < inst.GetOperandCount(); j++) {
                if (should_consider(get_instruction(inst.GetOperand(j))) &&
                    std::find(def[i].begin(), def[i].end(), inst.GetOperand(j)) == def[i].end()) {
                    // Not defined in this block ie. upwards exposed, live range goes outside current block
                    use[i].insert(inst.GetOperand(j));
                }
            }

            if (should_consider(get_instruction(inst.GetName()))) {
                def[i].insert(inst.GetName());
            }
        }
    }

    liveness_worklist(function, blocks, in, out, use, def);

    graph.Reserve(all_insts.size());

    for (const BackendBlock* block : blocks) {
        LivenessSet live_now;

        // We are gonna walk the block backwards, first add all definitions that have lifetime
        // that extends past this basic block
        live_now.insert(out[block->GetIndex()].begin(), out[block->GetIndex()].end());

        const std::list<BackendInstruction>& insts = block->GetInstructions();
        for (auto it = insts.rbegin(); it != insts.rend(); ++it) {
            const BackendInstruction& inst = *it;
            if (should_consider(get_instruction(inst.GetName()))) {
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
                case IROpcode::VSlideUpi: {
                    for (u8 i = 0; i < inst.GetOperandCount(); i++) {
                        if (should_consider(get_instruction(inst.GetOperand(i)))) {
                            live_now.insert(inst.GetOperand(i));
                        }
                    }
                    break;
                }
                case IROpcode::VGather: {
                    // Doesn't interfere with the first operand
                    for (u8 i = 1; i < inst.GetOperandCount(); i++) {
                        if (should_consider(get_instruction(inst.GetOperand(i)))) {
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
                graph.AddEmpty(inst.GetName());
                for (u32 live : live_now) {
                    graph.AddEdge(inst.GetName(), live);
                }
            }

            for (u8 i = 0; i < inst.GetOperandCount(); i++) {
                if (should_consider(get_instruction(inst.GetOperand(i)))) {
                    live_now.insert(inst.GetOperand(i));
                }
            }
        }
    }
}

static void build2(BackendFunction& function, std::vector<const BackendBlock*> blocks, InterferenceGraph& graph,
                   bool (*should_consider)(const BackendInstruction*)) {
    InstructionList all_insts;
    std::unordered_map<u32, u32> name_to_index;
    for (const auto& block : function.GetBlocks()) {
        for (const auto& inst : block->GetInstructions()) {
            all_insts.push_back(&inst);
            name_to_index[inst.GetName()] = all_insts.size() - 1;
        }
    }

    auto get_index = [&](u32 name) { return name_to_index.at(name); };

    auto get_instruction = [&](u32 name) { return all_insts.at(get_index(name)); };

    static std::vector<std::vector<u8>> in(blocks.size());
    static std::vector<std::vector<u8>> out(blocks.size());
    static std::vector<std::vector<u8>> use(blocks.size());
    static std::vector<std::vector<u8>> def(blocks.size());

    if (in.size() < blocks.size()) {
        in.resize(blocks.size());
        out.resize(blocks.size());
        use.resize(blocks.size());
        def.resize(blocks.size());
    }

    for (size_t i = 0; i < blocks.size(); i++) {
        in[i].resize(all_insts.size());
        out[i].resize(all_insts.size());
        use[i].resize(all_insts.size());
        def[i].resize(all_insts.size());

        std::fill(in[i].begin(), in[i].end(), 0);
        std::fill(out[i].begin(), out[i].end(), 0);
        std::fill(use[i].begin(), use[i].end(), 0);
        std::fill(def[i].begin(), def[i].end(), 0);
    }

    for (size_t counter = 0; counter < blocks.size(); counter++) {
        const BackendBlock* block = blocks[counter];
        size_t i = block->GetIndex();
        for (const BackendInstruction& inst : block->GetInstructions()) {
            for (u8 j = 0; j < inst.GetOperandCount(); j++) {
                if (get_instruction(inst.GetOperand(j)) == nullptr) {
                    ERROR("Null operand %d for instruction %s", j, inst.Print().c_str());
                }

                if (should_consider(get_instruction(inst.GetOperand(j))) && def[i][get_index(inst.GetOperand(j))] == 0) {
                    // Not defined in this block ie. upwards exposed, live range goes outside current block
                    u32 operand_index = get_index(inst.GetOperand(j));
                    use[i][operand_index] = 1;
                }
            }

            if (should_consider(get_instruction(inst.GetName()))) {
                u32 name_index = get_index(inst.GetName());
                def[i][name_index] = 1;
            }
        }
    }

    liveness_worklist2(function, blocks, all_insts, in, out, use, def);

    graph.Reserve(all_insts.size());

    std::vector<u8> live_now;
    live_now.resize(all_insts.size());

    for (const BackendBlock* block : blocks) {
        std::fill(live_now.begin(), live_now.end(), 0);

        // We are gonna walk the block backwards, first add all definitions that have lifetime
        // that extends past this basic block
        // live_now.insert(out[block->GetIndex()].begin(), out[block->GetIndex()].end());
        for (u32 i = 0; i < out[block->GetIndex()].size(); i++) {
            if (out[block->GetIndex()][i] == 1) {
                live_now[i] = 1;
            }
        }

        const std::list<BackendInstruction>& insts = block->GetInstructions();
        for (auto it = insts.rbegin(); it != insts.rend(); ++it) {
            const BackendInstruction& inst = *it;
            if (should_consider(get_instruction(inst.GetName()))) {
                // Erase the currently defined variable if it exists in the set
                u32 inst_index = get_index(inst.GetName());
                live_now[inst_index] = 0;

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
                case IROpcode::VSlideUpi: {
                    for (u8 i = 0; i < inst.GetOperandCount(); i++) {
                        if (should_consider(get_instruction(inst.GetOperand(i)))) {
                            u32 operand_index = get_index(inst.GetOperand(i));
                            live_now[operand_index] = 1;
                        }
                    }
                    break;
                }
                case IROpcode::VGather: {
                    // Doesn't interfere with the first operand
                    for (u8 i = 1; i < inst.GetOperandCount(); i++) {
                        if (should_consider(get_instruction(inst.GetOperand(i)))) {
                            u32 operand_index = get_index(inst.GetOperand(i));
                            live_now[operand_index] = 1;
                        }
                    }
                    break;
                }
                default:
                    break;
                }

                // in case there's nothing live (which is possible if nothing is read before written)
                // then we need to add the current instruction to the graph so it gets allocated
                graph.AddEmpty(inst.GetName());
                for (u32 i = 0; i < live_now.size(); i++) {
                    if (live_now[i] == 1) {
                        graph.AddEdge(inst.GetName(), all_insts[i]->GetName());
                    }
                }
            }

            for (u8 i = 0; i < inst.GetOperandCount(); i++) {
                if (should_consider(get_instruction(inst.GetOperand(i)))) {
                    u32 operand_index = get_index(inst.GetOperand(i));
                    live_now[operand_index] = 1;
                }
            }
        }
    }
}

static u32 choose(const InstructionMap& instructions, const std::deque<Node>& nodes) {
    float min = std::numeric_limits<float>::max();
    u32 chosen = 0;

    for (auto& node : nodes) {
        float spill_cost = instructions.at(node.id).spill_cost;
        float degree = instructions.at(node.id).interferences;
        if (degree == 0)
            continue;
        if (instructions.at(node.id).infinite_cost)
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

bool george_coalescing_heuristic(BackendFunction& function, InterferenceGraph& graph, u32 k, u32 lhs, u32 rhs) {
    // A conservative heuristic.
    // Safe to coalesce x and y if for every neighbor t of x, either t already interferes with y or t has degree < k
    u32 u = lhs;
    u32 v = rhs;

    auto& u_neighbors = graph.GetInterferences(u);
    ASSERT(u_neighbors.find(v) == u_neighbors.end());
    bool u_conquers_v = true;
    for (u32 t : graph.GetInterferences(v)) {
        if (u_neighbors.find(t) == u_neighbors.end() && graph.GetInterferences(t).size() >= k) {
            u_conquers_v = false;
            break;
        }
    }

    auto& v_neighbors = graph.GetInterferences(v);
    bool v_conquers_u = true;
    for (u32 t : graph.GetInterferences(u)) {
        if (v_neighbors.find(t) == v_neighbors.end() && graph.GetInterferences(t).size() >= k) {
            v_conquers_u = false;
            break;
        }
    }

    return u_conquers_v || v_conquers_u;
}

bool aggressive_coalescing_heuristic(BackendFunction& function, InterferenceGraph& graph, u32 k, u32 lhs, u32 rhs) {
    // An aggressive heuristic.
    // Coalesce every move that doesn't interfere.
    auto& edges = graph.GetInterferences(lhs);
    ASSERT(edges.find(rhs) == edges.end());
    return true;
}

void coalesce(BackendFunction& function, u32 lhs, u32 rhs) {
    for (BackendBlock* block : function.GetBlocks()) {
        for (BackendInstruction& inst : block->GetInstructions()) {
            for (u8 i = 0; i < inst.GetOperandCount(); i++) {
                if (inst.GetOperand(i) == lhs) {
                    inst.SetOperand(i, rhs);
                }
            }
            if (inst.GetName() == lhs) {
                inst.SetName(rhs);
            }
        }
    }
}

bool try_coalesce(BackendFunction& function, InstructionMap& map, InterferenceGraph& graph, bool (*should_consider)(const BackendInstruction*), u32 k,
                  CoalescingHeuristic heuristic) {
    bool coalesced = false;
    for (auto& block : function.GetBlocks()) {
        auto it = block->GetInstructions().begin();
        auto end = block->GetInstructions().end();
        while (it != end) {
            BackendInstruction& inst = *it;
            if (inst.GetOpcode() == IROpcode::Mov) {
                if (should_consider(map.at(inst.GetName()).inst), should_consider(map.at(inst.GetOperand(0)).inst)) {
                    u32 lhs = inst.GetName();
                    u32 rhs = inst.GetOperand(0);
                    auto& edges = graph.GetInterferences(lhs);
                    if (edges.find(rhs) == edges.end()) {
                        if (heuristic(function, graph, k, lhs, rhs)) {
                            coalesce(function, lhs, rhs);
                            it = block->GetInstructions().erase(it);
                            coalesced = true;
                            // Merge interferences into rhs
                            for (u32 neighbor : edges) {
                                if (neighbor != rhs) {
                                    graph.AddEdge(rhs, neighbor);
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

static AllocationMap run(BackendFunction& function, AllocationType type, bool (*should_consider)(const BackendInstruction*),
                         const std::vector<u32>& available_colors, u32& spill_location) {
    g_spilled_count = 0;
    const u32 k = available_colors.size();
    std::vector<const BackendBlock*> blocks = function.GetBlocksPostorder();
    while (true) {
        // Chaitin-Briggs algorithm
        std::deque<Node> nodes;
        InterferenceGraph graph;
        InstructionMap instructions;
        AllocationMap allocations;
        bool coalesced = false;

        do {
            coalesced = false;
            graph = InterferenceGraph();
            instructions = create_instruction_map(function);
            build2(function, blocks, graph, should_consider);

            if (g_coalesce) {
                coalesced = try_coalesce(function, instructions, graph, should_consider, k, george_coalescing_heuristic);
            }
        } while (coalesced);

        for (auto& [name, edges] : graph) {
            ASSERT_MSG(instructions.find(name) != instructions.end(), "Instruction %s not found in map", GetNameString(name).c_str());
            instructions.at(name).interferences = edges.edges.size();
        }

        while (true) {
            // While there's vertices with degree less than k
            while (graph.HasLessThanK(k)) {
                // Pick any node with degree less than k and put it on the stack
                for (auto& [id, edges] : graph) {
                    if (edges.edges.size() < k) {
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
            Node& node = *it;

            std::vector<u32> colors = available_colors;
            for (u32 neighbor : node.edges) {
                if (allocations.IsAllocated(neighbor)) {
                    u32 allocation = allocations.GetAllocationIndex(neighbor);
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
            u32 chosen_node = choose(instructions, nodes);
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

    AllocationMap gpr_map = run(function, AllocationType::GPR, should_consider_gpr, available_gprs, spill_location);
    AllocationMap vec_map = run(function, AllocationType::Vec, should_consider_vec, available_vecs, spill_location);

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
