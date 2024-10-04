#include "felix86/ir/passes/passes.hpp"

struct InterferenceGraph {
    InterferenceGraph(size_t node_count) : node_count(node_count) {
        data.resize(node_count * node_count);
    }

    bool IsAdjacent(size_t node1, size_t node2) {
        size_t row = node1 * node_count;
        size_t item = row + node2;
        return data[item];
    }

private:
    std::vector<bool> data;
    size_t node_count;
};

// Hack et al. graph coloring algorithm
void ir_graph_coloring_pass(IRFunction* function) {
    // Build interference graph
}