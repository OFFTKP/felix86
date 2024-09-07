#include "felix86/ir/passes.h"

#include <array>
#include <cstring>
#include <map>
#include <vector>

#include "felix86/common/log.h"
#include "felix86/ir/instruction.h"
#include "felix86/ir/print.h"

// bool is_read_write(ir_instruction_t* instruction) {
// 	switch (instruction->opcode) {
// 		case IR_READ_BYTE:
// 		case IR_READ_WORD:
// 		case IR_READ_DWORD:
// 		case IR_READ_QWORD:
// 		case IR_WRITE_BYTE:
// 		case IR_WRITE_WORD:
// 		case IR_WRITE_DWORD:
// 		case IR_WRITE_QWORD: {
// 			return true;
// 		}
// 		default: {
// 			break;
// 		}
// 	}
// 	return false;
// }

// struct ssa_state_t {
// 	std::array<std::map<ir_block_t*, ir_instruction_t*>, X86_REF_COUNT> def {};
// };

// ir_instruction_t* read_variable_recursive(ir_function_t* function, ssa_state_t& state, x86_ref_e variable, ir_block_t* block, ir_instruction_list_t* current);

// inline void write_variable(ssa_state_t& state, x86_ref_e variable, ir_block_t* block, ir_instruction_t* value) {
// 	state.def[variable][block] = value;
// }

// inline ir_instruction_t* read_variable(ir_function_t* function, ssa_state_t& state, x86_ref_e variable, ir_block_t* block, ir_instruction_list_t* current) {
// 	if (state.def[variable].find(block) != state.def[variable].end()) {
// 		return state.def[variable][block];
// 	} else {
// 		return read_variable_recursive(function, state, variable, block, current);
// 	}
// }

// inline ir_instruction_t* try_remove_trivial_phi(ir_instruction_t* phi) {
// 	ir_instruction_t* same = nullptr;
// 	ir_phi_node_t* op = phi->phi.list;
// 	while (op) {
// 		if (op->value == same || op->value == phi) {
// 			op = op->next;
// 			continue; // Unique value or self−reference
// 		}

// 		if (same != nullptr) {
// 			return phi; // The phi merges at least two values: not trivial
// 		}

// 		same = op->value;

// 		op = op->next;
// 	}

// 	if (same == nullptr) {
// 		ERROR("The phi is unreachable or in the entry block");
// 	}
	
// 	return same;
// }

// inline ir_instruction_t* add_phi_operands(ir_function_t* function, ssa_state_t& state, ir_block_t* block, x86_ref_e variable, ir_instruction_t* phi, ir_instruction_list_t* current) {
// 	ir_block_list_t* pred = block->predecessors;
// 	while (pred) {
// 		ir_instruction_t* value = read_variable(function, state, variable, pred->block, current);
// 		ir_phi_node_t* node = phi->phi.list;
// 		phi->phi.list = (ir_phi_node_t*)malloc(sizeof(ir_phi_node_t));
// 		phi->phi.list->block = pred->block;
// 		phi->phi.list->value = value;
// 		phi->phi.list->next = node;
// 		value->uses++;
		
// 		pred = pred->next;
// 	}
// 	return try_remove_trivial_phi(phi);
// }

// inline ir_instruction_t* read_variable_recursive(ir_function_t* function, ssa_state_t& state, x86_ref_e variable, ir_block_t* block, ir_instruction_list_t* current) {
// 	u32 predecessor_count = block->predecessors_count;

// 	ir_instruction_t* ret;
// 	if (predecessor_count == 1) {
// 		// Optimize the common case of one predecessor: No phi needed
// 		ret = read_variable(function, state, variable, block->predecessors->block, current);
// 	} else if (predecessor_count > 1) {
// 		ret = ir_ilist_insert_before(current);
// 		ret->type = IR_TYPE_PHI;
// 		ret->opcode = IR_PHI;
// 		ret->phi.list = nullptr;
// 		write_variable(state, variable, block, ret);
// 		ret = add_phi_operands(function, state, block, variable, ret, current);
// 	} else if (predecessor_count == 0) {
// 		// The search has reached our empty entry block without finding a definition
// 		// Which means we actually need to read the guest from memory
// 		// We insert an instruction that reads the guest from memory right before the current instruction
// 		// and return that
// 		ret = ir_ilist_insert_before(current);
// 		ret->type = IR_TYPE_LOAD_GUEST_FROM_MEMORY;
// 		ret->opcode = IR_LOAD_GUEST_FROM_MEMORY;
// 		ret->get_guest.ref = variable;
// 	}

// 	write_variable(state, variable, block, ret);
// 	return ret;
// }

// void ir_ssa_pass_impl(ir_function_t* function, ssa_state_t& state, ir_block_t* block) {
// 	ir_instruction_list_t* current = block->instructions->next;
// 	while(current) {
// 		ir_instruction_list_t* next = current->next;
// 		switch(current->instruction.opcode) {
// 			case IR_SET_GUEST: {
// 				write_variable(state, current->instruction.set_guest.ref, block, &current->instruction);
// 				ir_instruction_t* source = current->instruction.set_guest.source;
// 				ir_clear_instruction(&current->instruction);
// 				current->instruction.type = IR_TYPE_ONE_OPERAND;
// 				current->instruction.opcode = IR_MOV;
// 				current->instruction.operands.args[0] = source;
// 				break;
// 			}
// 			case IR_GET_GUEST: {
// 				ir_instruction_t* value = read_variable(function, state, current->instruction.get_guest.ref, block, current);
// 				ir_clear_instruction(&current->instruction);
// 				current->instruction.type = IR_TYPE_ONE_OPERAND;
// 				current->instruction.opcode = IR_MOV;
// 				current->instruction.operands.args[0] = value;
// 				value->uses++;
// 				break;
// 			}
// 			case IR_CPUID: {
// 				state.def[X86_REF_RAX].clear();
// 				state.def[X86_REF_RCX].clear();
// 				state.def[X86_REF_RDX].clear();
// 				state.def[X86_REF_RBX].clear();
// 				break;
// 			}
// 			case IR_SYSCALL: {
// 				state.def[X86_REF_RAX].clear();
// 				break;
// 			}
// 			case IR_JUMP_REGISTER:
// 			case IR_EXIT: {
// 				// We need to emit a writeback to memory for all variables that are used
// 				for (u8 i = 0; i < X86_REF_COUNT; i++) {
// 					if (!state.def[i].empty()) {
// 						ir_instruction_t* value = read_variable(function, state, (x86_ref_e)i, block, current);
// 						ir_instruction_t* write = ir_ilist_insert_before(current);
// 						write->type = IR_TYPE_STORE_GUEST_TO_MEMORY;
// 						write->opcode = IR_STORE_GUEST_TO_MEMORY;
// 						write->set_guest.ref = (x86_ref_e)i;
// 						write->set_guest.source = value;
// 						value->uses++;
// 					}
// 				}
// 				break;
// 			}
// 			default: {
// 				break;
// 			}
// 		}

// 		current = next;
// 	}
// }

// // This and the above functions are based entirely on Simple and Efficient Construction of Static Single Assignment Form paper
// extern "C" void ir_ssa_pass(ir_function_t* function) {
// 	ssa_state_t state = {};
// 	ir_block_list_t* current = function->first;
// 	while (current) {
// 		ir_ssa_pass_impl(function, state, current->block);
// 		current = current->next;
// 	}
// }

extern "C" void ir_naming_pass(ir_function_t* function) {
    u16 name = 0;
    for (ir_block_list_t* current = function->first; current; current = current->next) {
        for (ir_instruction_list_t* current_instruction = current->block->instructions->next; current_instruction; current_instruction = current_instruction->next) {
            current_instruction->instruction.name = name++;
			if (name == 0xFFFF) {
				ERROR("Too many temporaries");
			}
        }
    }
}

// bool operator<(const ir_instruction_t& a1, const ir_instruction_t& a2) {
// 	int res = memcmp(&a1, &a2, sizeof(ir_instruction_t));
// 	return res < 0;
// }

// bool operates_on_temporaries(ir_instruction_t* instruction) {
// 	switch (instruction->opcode) {
// 		// These instructions directly modify the guest state
// 		case IR_GET_GUEST:
// 		case IR_SET_GUEST:
// 		case IR_CPUID: 
// 		case IR_SYSCALL: {
// 			return false;
// 		}

// 		default: {
// 			return true;
// 		}
// 	}
// }

// extern "C" void ir_local_common_subexpression_elimination_pass_v2(ir_block_t* block) {
// 	std::map<ir_instruction_t, ir_instruction_t*> expressions;
// 	std::array<ir_instruction_t*, X86_REF_COUNT> registers = {};
// 	std::array<ir_instruction_t*, 16> flags = {};

// 	ir_instruction_list_t* current = block->instructions->next;
// 	while (current) {
// 		ir_instruction_t expression = current->instruction;
// 		// we dont wanna hash these in the map
// 		expression.name = 0;
// 		expression.uses = 0;
// 		if (operates_on_temporaries(&current->instruction)) {
// 			if (expressions.find(expression) != expressions.end()) {
// 				ir_clear_instruction(&current->instruction);
// 				current->instruction.type = IR_TYPE_ONE_OPERAND;
// 				current->instruction.opcode = IR_MOV;
// 				current->instruction.operands.args[0] = expressions[expression];
// 			} else {
// 				expressions[expression] = &current->instruction;
// 			}
// 		} else {
// 			// These instructions make the temporaries have a lifetime,
// 			// meaning a temporary is not available again after its guest register is overwritten
// 			switch (current->instruction.opcode) {
// 				case IR_SET_GUEST: {
// 					registers[current->instruction.set_guest.ref] = current->instruction.set_guest.source;
// 					break;
// 				}
// 				case IR_GET_GUEST: {
// 					if (registers[current->instruction.get_guest.ref]) {
// 						registers[current->instruction.get_guest.ref]->uses++;
// 						ir_instruction_t* new_source = registers[current->instruction.get_guest.ref];
// 						ir_clear_instruction(&current->instruction);
// 						current->instruction.type = IR_TYPE_ONE_OPERAND;
// 						current->instruction.opcode = IR_MOV;
// 						current->instruction.operands.args[0] = new_source;
// 					} else {
// 						registers[current->instruction.get_guest.ref] = &current->instruction;
// 					}
// 					break;
// 				}
// 				case IR_CPUID: {
// 					registers[X86_REF_RAX] = nullptr;
// 					registers[X86_REF_RBX] = nullptr;
// 					registers[X86_REF_RCX] = nullptr;
// 					registers[X86_REF_RDX] = nullptr;
// 					break;
// 				}
// 				case IR_SYSCALL: {
// 					registers[X86_REF_RAX] = nullptr;
// 					registers[X86_REF_RDI] = nullptr;
// 					registers[X86_REF_RSI] = nullptr;
// 					registers[X86_REF_RDX] = nullptr;
// 					registers[X86_REF_R10] = nullptr;
// 					registers[X86_REF_R8] = nullptr;
// 					registers[X86_REF_R9] = nullptr;
// 					break;
// 				}
// 				default: {
// 					ERROR("Unreachable");
// 					break;
// 				}
// 			}
// 		}

// 		current = current->next;
// 	}
// }

// extern "C" void ir_copy_propagation_pass_block(std::map<ir_instruction_t*, ir_instruction_t*> copies, ir_block_t* block) {
// 	ir_instruction_list_t* current = block->instructions->next;
// 	while (current) {
// 		ir_instruction_t* instruction = &current->instruction;
// 		if (instruction->opcode == IR_MOV) {
// 			if (copies.find(instruction->operands.args[0]) != copies.end()) {
// 				copies[instruction] = copies[instruction->operands.args[0]];
// 			} else {
// 				copies[instruction] = instruction->operands.args[0];
// 			}

// 			ir_instruction_list_t* next = current->next;
// 			ir_ilist_remove(current);
// 			ir_ilist_free(current);
// 			current = next;
// 		} else {
// 			switch (instruction->type) {
// 				case IR_TYPE_JUMP:
// 				case IR_TYPE_ONE_OPERAND: {
// 					if (copies.find(instruction->operands.args[0]) != copies.end()) {
// 						instruction->operands.args[0] = copies[instruction->operands.args[0]];
// 						instruction->operands.args[0]->uses++;
// 					}
// 					break;
// 				}
// 				case IR_TYPE_TWO_OPERANDS: {
// 					if (copies.find(instruction->operands.args[0]) != copies.end()) {
// 						instruction->operands.args[0] = copies[instruction->operands.args[0]];
// 						instruction->operands.args[0]->uses++;
// 					}
// 					if (copies.find(instruction->operands.args[1]) != copies.end()) {
// 						instruction->operands.args[1] = copies[instruction->operands.args[1]];
// 						instruction->operands.args[1]->uses++;
// 					}
// 					break;
// 				}
// 				case IR_TYPE_THREE_OPERANDS: {
// 					if (copies.find(instruction->operands.args[0]) != copies.end()) {
// 						instruction->operands.args[0] = copies[instruction->operands.args[0]];
// 						instruction->operands.args[0]->uses++;
// 					}
// 					if (copies.find(instruction->operands.args[1]) != copies.end()) {
// 						instruction->operands.args[1] = copies[instruction->operands.args[1]];
// 						instruction->operands.args[1]->uses++;
// 					}
// 					if (copies.find(instruction->operands.args[2]) != copies.end()) {
// 						instruction->operands.args[2] = copies[instruction->operands.args[2]];
// 						instruction->operands.args[2]->uses++;
// 					}
// 					break;
// 				}
// 				case IR_TYPE_FOUR_OPERANDS: {
// 					if (copies.find(instruction->operands.args[0]) != copies.end()) {
// 						instruction->operands.args[0] = copies[instruction->operands.args[0]];
// 						instruction->operands.args[0]->uses++;
// 					}
// 					if (copies.find(instruction->operands.args[1]) != copies.end()) {
// 						instruction->operands.args[1] = copies[instruction->operands.args[1]];
// 						instruction->operands.args[1]->uses++;
// 					}
// 					if (copies.find(instruction->operands.args[2]) != copies.end()) {
// 						instruction->operands.args[2] = copies[instruction->operands.args[2]];
// 						instruction->operands.args[2]->uses++;
// 					}
// 					if (copies.find(instruction->operands.args[3]) != copies.end()) {
// 						instruction->operands.args[3] = copies[instruction->operands.args[3]];
// 						instruction->operands.args[3]->uses++;
// 					}
// 					break;
// 				}
// 				case IR_TYPE_STORE_GUEST_TO_MEMORY:
// 				case IR_TYPE_SET_GUEST: {
// 					if (copies.find(instruction->set_guest.source) != copies.end()) {
// 						instruction->set_guest.source = copies[instruction->set_guest.source];
// 						instruction->set_guest.source->uses++;
// 					}
// 					break;
// 				}
// 				case IR_TYPE_JUMP_CONDITIONAL: {
// 					if (copies.find(instruction->jump_conditional.condition) != copies.end()) {
// 						instruction->jump_conditional.condition = copies[instruction->jump_conditional.condition];
// 						instruction->jump_conditional.condition->uses++;
// 					}
// 					break;
// 				}
// 				case IR_TYPE_PHI: {
// 					ir_phi_node_t* node = instruction->phi.list;
// 					while (node) {
// 						if (copies.find(node->value) != copies.end()) {
// 							node->value = copies[node->value];
// 							node->value->uses++;
// 						}
// 						node = node->next;
// 					}
// 					break;
// 				}
// 				default: {
// 					break;
// 				}
// 			}
// 			current = current->next;
// 		}
// 	}

// 	ir_block_list_t* succ = block->successors;
// 	while (succ) {
// 		if (succ->block != block)
// 			ir_copy_propagation_pass_block(copies, succ->block);
// 		succ = succ->next;
// 	}
// }

// void ir_copy_propagation_pass(ir_function_t* function) {
// 	std::map<ir_instruction_t*, ir_instruction_t*> copies;
// 	ir_block_list_t* block = function->first;
// 	ir_copy_propagation_pass_block(copies, block->block);
// }

// void ir_verifier_pass(ir_block_t* block) {
// 	ir_instruction_list_t* current = block->instructions->next;
// 	while (current) {
// 		if ((i16)current->instruction.uses < 0) {
// 			ERROR("Instruction uses is negative");
// 		}

// 		current = current->next;
// 	}
// }