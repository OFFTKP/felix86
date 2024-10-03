#include "felix86/ir/passes.hpp"

void ir_copy_propagate_node(const IRDominatorTreeNode* node, std::unordered_map<IRInstruction*, IRInstruction*> map) {
    std::list<IRInstruction>& insts = node->block->GetInstructions();
    auto it = insts.begin();
    auto end = insts.end();
    while (it != end) {
        if (it->GetOpcode() == IROpcode::Mov) {
            map[&*it] = it->GetOperand(0);
            it->Invalidate();
            it = insts.erase(it);
        } else {
            switch (it->GetExpression().index()) {
            case 0: {
                Operands& operands = std::get<Operands>(it->GetExpression());
                for (IRInstruction*& operand : operands.operands) {
                    auto found = map.find(operand);
                    if (found != map.end()) {
                        operand = found->second;
                        operand->AddUse();
                    }
                }
                break;
            }
            case 1: {
                break;
            }
            case 2: {
                break;
            }
            case 3: {
                SetGuest& set_guest = std::get<SetGuest>(it->GetExpression());
                auto found = map.find(set_guest.source);
                if (found != map.end()) {
                    set_guest.source = found->second;
                    set_guest.source->AddUse();
                }
                break;
            }
            case 4: {
                Phi& phi = std::get<Phi>(it->GetExpression());
                for (PhiNode& node : phi.nodes) {
                    auto found = map.find(node.value);
                    if (found != map.end()) {
                        node.value = found->second;
                        node.value->AddUse();
                    }
                }
                break;
            }
            case 5: {
                break;
            }
            case 6: {
                TupleAccess& tuple_access = std::get<TupleAccess>(it->GetExpression());
                auto found = map.find(tuple_access.tuple);
                if (found != map.end()) {
                    tuple_access.tuple = found->second;
                    tuple_access.tuple->AddUse();
                }
                break;
            }
            default: {
                ERROR("Unreachable");
            }
            }
            ++it;
        }
    }

    for (const auto& child : node->children) {
        ir_copy_propagate_node(child, map);
    }
}

void ir_copy_propagation_pass(IRFunction* function) {
    const IRDominatorTree& dominator_tree = function->GetDominatorTree();

    const IRDominatorTreeNode& node = dominator_tree.nodes[0];
    std::unordered_map<IRInstruction*, IRInstruction*> copy_map;
    ir_copy_propagate_node(&node, copy_map);
}