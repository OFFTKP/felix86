#include "felix86/ir/passes/passes.hpp"

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
            switch (it->GetExpressionType()) {
            case ExpressionType::Operands: {
                Operands& operands = it->AsOperands();
                for (IRInstruction*& operand : operands.operands) {
                    auto found = map.find(operand);
                    if (found != map.end()) {
                        operand = found->second;
                        operand->AddUse();
                    }
                }
                break;
            }
            case ExpressionType::Immediate:
            case ExpressionType::GetGuest:
            case ExpressionType::Comment: {
                break;
            }
            case ExpressionType::SetGuest: {
                SetGuest& set_guest = it->AsSetGuest();
                auto found = map.find(set_guest.source);
                if (found != map.end()) {
                    set_guest.source = found->second;
                    set_guest.source->AddUse();
                }
                break;
            }
            case ExpressionType::Phi: {
                Phi& phi = it->AsPhi();
                for (PhiNode& node : phi.nodes) {
                    auto found = map.find(node.value);
                    if (found != map.end()) {
                        node.value = found->second;
                        node.value->AddUse();
                    }
                }
                break;
            }
            case ExpressionType::TupleAccess: {
                TupleAccess& tuple_access = it->AsTupleAccess();
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