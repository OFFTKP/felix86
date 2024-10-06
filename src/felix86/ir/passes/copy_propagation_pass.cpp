#include "felix86/ir/passes/passes.hpp"

void ir_copy_propagate_node(const IRDominatorTreeNode* node, std::unordered_map<IRInstruction*, IRInstruction*> map) {
    std::list<IRInstruction>& insts = node->block->GetInstructions();
    auto it = insts.begin();
    auto end = insts.end();
    while (it != end) {
        if (it->GetOpcode() == IROpcode::Mov) {
            IRInstruction* value_final = it->GetOperand(0);
            bool found_once = false;
            while (map.find(value_final) != map.end()) {
                found_once = true;
                value_final = map[value_final];
            }
            map[&*it] = value_final;
            // If it's the mov operand was already in the map, that means it was also removed
            // This can happen if you have sequential movs like so:
            // mov a, b
            // mov c, a
            // When the pass go through, it will remove `a`. So we don't need to remove a use
            // from a as it's removed from the list and it would be invalid to do so anyway.
            // We still need to keep it in the map though for instructions that could be using
            // it further down the line.
            if (!found_once) {
                it->Invalidate();
            }
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
                for (auto& value : phi.values) {
                    auto found = map.find(value);
                    if (found != map.end()) {
                        value = found->second;
                        value->AddUse();
                    }
                }
                break;
            }
            default: {
                UNREACHABLE();
            }
            }
            ++it;
        }
    }

    for (const auto& child : node->children) {
        ir_copy_propagate_node(child, map);
    }
}

void replace_setguest_with_mov(IRFunction* function) {
    for (IRBlock* block : function->GetBlocksPostorder()) {
        std::list<IRInstruction>& insts = block->GetInstructions();
        auto it = insts.begin();
        auto end = insts.end();
        while (it != end) {
            if (it->GetOpcode() == IROpcode::SetGuest) {
                it->ReplaceExpressionWithMov(it->AsSetGuest().source);
            }
            ++it;
        }
    }
}

void ir_copy_propagation_pass(IRFunction* function) {
    replace_setguest_with_mov(function);

    const IRDominatorTree& dominator_tree = function->GetDominatorTree();

    const IRDominatorTreeNode& node = dominator_tree.nodes[0];
    std::unordered_map<IRInstruction*, IRInstruction*> copy_map;
    ir_copy_propagate_node(&node, copy_map);
}
