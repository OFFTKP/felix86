#include "felix86/ir/print.h"
#include "felix86/common/log.h"

#include <stdio.h>

#define OPC_BEGIN "<font color=\"#c586c0\">"
#define OPC_END "</font>"
#define IMM_BEGIN "<font color=\"#b5cba8\">"
#define IMM_END "</font>"
#define VAR_BEGIN "<font color=\"#9cdcfe\">"
#define VAR_END "</font>"
#define GUEST_BEGIN "<font color=\"#4fc1ff\">"
#define GUEST_END "</font>"
#define VAR VAR_BEGIN "t%d" VAR_END
#define IMM IMM_BEGIN "0x%016llx" IMM_END
#define OP OPC_BEGIN "&nbsp;%s" OPC_END
#define EQUALS "&nbsp;="

void print_guest(x86_ref_e guest) {
    printf(GUEST_BEGIN);
    switch (guest) {
        case X86_REF_RAX: printf("rax"); break;
        case X86_REF_RCX: printf("rcx"); break;
        case X86_REF_RDX: printf("rdx"); break;
        case X86_REF_RBX: printf("rbx"); break;
        case X86_REF_RSP: printf("rsp"); break;
        case X86_REF_RBP: printf("rbp"); break;
        case X86_REF_RSI: printf("rsi"); break;
        case X86_REF_RDI: printf("rdi"); break;
        case X86_REF_R8: printf("r8"); break;
        case X86_REF_R9: printf("r9"); break;
        case X86_REF_R10: printf("r10"); break;
        case X86_REF_R11: printf("r11"); break;
        case X86_REF_R12: printf("r12"); break;
        case X86_REF_R13: printf("r13"); break;
        case X86_REF_R14: printf("r14"); break;
        case X86_REF_R15: printf("r15"); break;
        case X86_REF_CF: printf("cf"); break;
        case X86_REF_PF: printf("pf"); break;
        case X86_REF_AF: printf("af"); break;
        case X86_REF_ZF: printf("zf"); break;
        case X86_REF_SF: printf("sf"); break;
        case X86_REF_OF: printf("of"); break;
        case X86_REF_RIP: printf("rip"); break;
        case X86_REF_FS: printf("fs"); break;
        case X86_REF_GS: printf("gs"); break;
        case X86_REF_XMM0 ... X86_REF_XMM15: printf("xmm%d", guest - X86_REF_XMM0); break;
        default: printf("Unknown guest"); break;
    }
    printf(GUEST_END);
}

void print_one_op(ir_instruction_t* instruction, const char* op) {
    printf(VAR EQUALS OP VAR, instruction->name, op, instruction->operands.args[0]->name);
}

void print_two_op(ir_instruction_t* instruction, const char* op) {
    printf(VAR EQUALS VAR OP VAR, instruction->name, instruction->operands.args[0]->name, op, instruction->operands.args[1]->name);
}

void ir_print_instruction(ir_instruction_t* instruction, ir_block_t* block) {
    if (instruction->opcode == IR_START_OF_BLOCK) {
        return;
    }

    switch (instruction->opcode) {
        case IR_IMMEDIATE: {
            printf(VAR EQUALS IMM, instruction->name, (unsigned long long)instruction->load_immediate.immediate);
            break;
        }
        case IR_ADD: {
            print_two_op(instruction, "+");
            break;
        }
        case IR_SUB: {
            print_two_op(instruction, "-");
            break;
        }
        case IR_SHIFT_LEFT: {
            print_two_op(instruction, "&lt;&lt;");
            break;
        }
        case IR_SHIFT_RIGHT: {
            print_two_op(instruction, "&gt;&gt;");
            break;
        }
        case IR_SHIFT_RIGHT_ARITHMETIC: {
            print_two_op(instruction, "&gt;&gt;");
            break;
        }
        case IR_AND: {
            print_two_op(instruction, "&amp;");
            break;
        }
        case IR_OR: {
            print_two_op(instruction, "|");
            break;
        }
        case IR_XOR: {
            print_two_op(instruction, "^");
            break;
        }
        case IR_POPCOUNT: {
            print_one_op(instruction, "popcount");
            break;
        }
        case IR_EQUAL: {
            print_two_op(instruction, "==");
            break;
        }
        case IR_NOT_EQUAL: {
            print_two_op(instruction, "!=");
            break;
        }
        case IR_GREATER_THAN_SIGNED: {
            print_two_op(instruction, "s&gt;");
            break;
        }
        case IR_LESS_THAN_SIGNED: {
            print_two_op(instruction, "s&lt;");
            break;
        }
        case IR_GREATER_THAN_UNSIGNED: {
            print_two_op(instruction, "u&gt;");
            break;
        }
        case IR_LESS_THAN_UNSIGNED: {
            print_two_op(instruction, "u&lt;");
            break;
        }
        case IR_MOV: {
            printf(VAR EQUALS VAR, instruction->name, instruction->operands.args[0]->name);
            break;
        }
        case IR_SEXT_GPR8: {
            print_one_op(instruction, "sext8");
            break;
        }
        case IR_SEXT_GPR16: {
            print_one_op(instruction, "sext16");
            break;
        }
        case IR_SEXT_GPR32: {
            print_one_op(instruction, "sext32");
            break;
        }
        case IR_LEA: {
            printf("t%d = ptr[t%d + t%d * t%d + t%d]", instruction->name, instruction->operands.args[0]->name, instruction->operands.args[1]->name, instruction->operands.args[2]->name, instruction->operands.args[3]->name);
            break;
        }
        case IR_GET_GUEST: {
            printf(VAR EQUALS OP, instruction->name, "get_guest");
            print_guest(instruction->get_guest.ref);
            break;
        }
        case IR_SET_GUEST: {
            printf(VAR EQUALS OP, instruction->name, "set_guest");
            print_guest(instruction->set_guest.ref);
            printf(",&nbsp;" VAR, instruction->set_guest.source->name);
            break;
        }
        case IR_READ_BYTE: {
            printf("t%d = byte[t%d]", instruction->name, instruction->operands.args[0]->name);
            break;
        }
        case IR_READ_WORD: {
            printf("t%d = word[t%d]", instruction->name, instruction->operands.args[0]->name);
            break;
        }
        case IR_READ_DWORD: {
            printf("t%d = dword[t%d]", instruction->name, instruction->operands.args[0]->name);
            break;
        }
        case IR_READ_QWORD: {
            printf("t%d = qword[t%d]", instruction->name, instruction->operands.args[0]->name);
            break;
        }
        case IR_READ_XMMWORD: {
            printf("t%d = xmmword[t%d]", instruction->name, instruction->operands.args[0]->name);
            break;
        }
        case IR_WRITE_BYTE: {
            printf("byte[t%d] = t%d", instruction->operands.args[0]->name, instruction->operands.args[1]->name);
            break;
        }
        case IR_WRITE_WORD: {
            printf("word[t%d] = t%d", instruction->operands.args[0]->name, instruction->operands.args[1]->name);
            break;
        }
        case IR_WRITE_DWORD: {
            printf("dword[t%d] = t%d", instruction->operands.args[0]->name, instruction->operands.args[1]->name);
            break;
        }
        case IR_WRITE_QWORD: {
            printf("qword[t%d] = t%d", instruction->operands.args[0]->name, instruction->operands.args[1]->name);
            break;
        }
        case IR_SYSCALL: {
            printf("syscall");
            break;
        }
        case IR_CPUID: {
            printf("cpuid");
            break;
        }
        case IR_JUMP: {
            printf("jump %p", instruction->jump.target);
            break;
        }
        case IR_EXIT: {
            printf("exit");
            break;
        }
        case IR_PHI: {
            printf("t%d = φ&lt;", instruction->name);
            ir_phi_node_t* node = instruction->phi.list;
            while (node) {
                if (!node->value || !node->block) {
                    printf("NULL");
                } else {
                    printf("t%d @ %p", node->value->name, node->block);
                }
                node = node->next;
                if (node) {
                    printf(", ");
                }
            }
            printf("&gt;");
            break;
        }
        case IR_JUMP_CONDITIONAL: {
            printf("jump t%d ? %p : %p", instruction->jump_conditional.condition->name, instruction->jump_conditional.target_true, instruction->jump_conditional.target_false);
            break;
        }
        case IR_JUMP_REGISTER: {
            printf(OP VAR, "jump", instruction->operands.args[0]->name);
            break;
        }
        default: {
            printf("Unknown opcode: %d", instruction->opcode);
            break;
        }
    }

    // printf("\t\t\t\t(uses: %d)", instruction->uses);
}

void ir_print_block(ir_block_t* block) {
    ir_instruction_list_t* node = block->instructions;
    while (node) {
        ir_print_instruction(&node->instruction, block);
        node = node->next;
    }
}

extern "C" void ir_print_function_graphviz(u64 program_entrypoint, ir_function_t* function) {
    {
        ir_block_list_t* blocks = function->first;
        while (blocks) {
            printf("block: %p\n", blocks->block);
            ir_instruction_list_t* node = blocks->block->instructions;
            while (node) {
                printf("\tinstruction: %p\n", &node->instruction);
                node = node->next;
            }
            blocks = blocks->next;
        }
    }

    printf("digraph function_%p {\n", function);
    printf("\tgraph [splines=true, nodesep=0.8, overlap=false]\n");
    printf("\tnode ["
              "style=filled,"
              "shape=rect,"
              "pencolor=\"#00000044\","
              "fontname=\"Helvetica,Arial,sans-serif\","
              "shape=plaintext"
              "]\n");
    printf("\tedge ["
                "arrowsize=0.5,"
                "fontname=\"Helvetica,Arial,sans-serif\","
                "labeldistance=3,"
                "labelfontcolor=\"#00000080\","
                "penwidth=2"
                "]\n");
    
    ir_block_list_t* blocks = function->first;
    while (blocks) {
        printf("\tblock_%p [", blocks->block);
        printf("\t\tfontcolor=\"#ffffff\"");
        printf("\t\tfillcolor=\"#1e1e1e\"");
        printf("\t\tlabel=<<table border=\"0\" cellborder=\"1\" cellspacing=\"0\" cellpadding=\"3\">\n");
        printf("\t\t<tr><td port=\"top\"><b>%016lx</b></td> </tr>\n", (u64)(blocks->block->start_address - program_entrypoint));
        
        ir_instruction_list_t* node = blocks->block->instructions;
        ir_instruction_t* last = NULL;
        while (node) {
            if (node->instruction.opcode != IR_START_OF_BLOCK) {
                if (node->next == NULL) {
                    printf("\t\t<tr><td align=\"left\" port=\"exit\" sides=\"lbr\">"); // draw the bottom border too
                    last = &node->instruction;
                } else {
                    printf("\t\t<tr><td align=\"left\" sides=\"lr\">");
                }

                ir_instruction_t* instruction = &node->instruction;
                ir_print_instruction(instruction, blocks->block);

                printf("</td></tr>\n");
            }
            node = node->next;
        }
        
        printf("\t\t</table>>\n");
        printf("\t\tshape=plain\n");
        printf("\t];\n");
        
        if (last->opcode == IR_JUMP_CONDITIONAL) {
            printf("\tblock_%p:exit -> block_%p:top [color=\"#00ff00\" tailport=s headport=n]\n", blocks->block, last->jump_conditional.target_true);
            printf("\tblock_%p:exit -> block_%p:top [color=\"#ff0000\" tailport=s headport=n]\n", blocks->block, last->jump_conditional.target_false);
        } else if (last->opcode == IR_JUMP) {
            printf("\tblock_%p:exit -> block_%p:top\n", blocks->block, last->jump.target);
        }
        
        blocks = blocks->next;
    }
    
    
    printf("}\n");
    fflush(stdout);
}