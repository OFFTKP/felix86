#include "felix86/common/log.hpp"
#include "felix86/ir/instruction.hpp"

void ir_clear_instruction(ir_instruction_t* instruction) {
    instruction->raw_data[0] = 0;
    instruction->raw_data[1] = 0;
    instruction->raw_data[2] = 0;
    instruction->raw_data[3] = 0;
}

// Copies just the expression, zero initializing everything else
// This is useful for common subexpression elimination and other optimizations
// for hashing in maps
ir_instruction_t ir_copy_expression(ir_instruction_t* expression) {
    ir_instruction_t ret = {0};
    ret.type = expression->type;
    ret.opcode = expression->opcode;

    switch (ret.type) {
    case IR_TYPE_NO_OPERANDS: {
        break;
    }

    case IR_TYPE_ONE_OPERAND: {
        ret.operands.args[0] = expression->operands.args[0];
        break;
    }

    case IR_TYPE_TWO_OPERANDS: {
        ret.operands.args[0] = expression->operands.args[0];
        ret.operands.args[1] = expression->operands.args[1];
        break;
    }

    case IR_TYPE_THREE_OPERANDS: {
        ret.operands.args[0] = expression->operands.args[0];
        ret.operands.args[1] = expression->operands.args[1];
        ret.operands.args[2] = expression->operands.args[2];
        break;
    }

    case IR_TYPE_FOUR_OPERANDS: {
        ret.operands.args[0] = expression->operands.args[0];
        ret.operands.args[1] = expression->operands.args[1];
        ret.operands.args[2] = expression->operands.args[2];
        ret.operands.args[3] = expression->operands.args[3];
        break;
    }

    case IR_TYPE_LOAD_IMMEDIATE: {
        ret.load_immediate.immediate = expression->load_immediate.immediate;
        break;
    }

    case IR_TYPE_GET_GUEST: {
        ret.get_guest.ref = expression->get_guest.ref;
        break;
    }

    case IR_TYPE_SET_GUEST: {
        ret.set_guest.source = expression->set_guest.source;
        ret.set_guest.ref = expression->set_guest.ref;
        break;
    }

    case IR_TYPE_PHI: {
        ret.phi.list = expression->phi.list;
        break;
    }

    default: {
        ERROR("Unknown type: %d", ret.type);
        break;
    }
    }

    return ret;
}