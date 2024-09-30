#include "felix86/common/global.hpp"
#include "felix86/common/log.hpp"
#include "felix86/common/print.hpp"
#include "felix86/ir/print.hpp"

#include <vector>
#include <cstdio>

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
    print_guest_register(guest);
    printf(GUEST_END);
}

void print_one_op(const IRInstruction& instruction, const char* op) {
    printf(VAR EQUALS OP VAR, instruction.GetName(), op, instruction.GetOperandName(0));
}

void print_two_op(const IRInstruction& instruction, const char* op) {
    printf(VAR EQUALS VAR OP VAR, instruction.GetName(), instruction.GetOperandName(0), op, instruction.GetOperandName(1));
}

void ir_print_instruction(const IRInstruction& instruction, const IRBlock& block) {
    switch (instruction.GetOpcode()) {
    case IROpcode::IR_COMMENT: {
        printf("%s", instruction.AsComment().comment.c_str());
        break;
    }
    case IROpcode::IR_IMMEDIATE: {
        printf(VAR EQUALS IMM, instruction.GetName(), (unsigned long long)instruction.AsImmediate().immediate);
        break;
    }
    case IROpcode::IR_ADD: {
        print_two_op(instruction, "+");
        break;
    }
    case IROpcode::IR_SUB: {
        print_two_op(instruction, "-");
        break;
    }
    case IROpcode::IR_SHIFT_LEFT: {
        print_two_op(instruction, "&lt;&lt;");
        break;
    }
    case IROpcode::IR_SHIFT_RIGHT: {
        print_two_op(instruction, "&gt;&gt;");
        break;
    }
    case IROpcode::IR_SHIFT_RIGHT_ARITHMETIC: {
        print_two_op(instruction, "&gt;&gt;");
        break;
    }
    case IROpcode::IR_AND: {
        print_two_op(instruction, "&amp;");
        break;
    }
    case IROpcode::IR_OR: {
        print_two_op(instruction, "|");
        break;
    }
    case IROpcode::IR_XOR: {
        print_two_op(instruction, "^");
        break;
    }
    case IROpcode::IR_POPCOUNT: {
        print_one_op(instruction, "popcount");
        break;
    }
    case IROpcode::IR_EQUAL: {
        print_two_op(instruction, "==");
        break;
    }
    case IROpcode::IR_NOT_EQUAL: {
        print_two_op(instruction, "!=");
        break;
    }
    case IROpcode::IR_GREATER_THAN_SIGNED: {
        print_two_op(instruction, "s&gt;");
        break;
    }
    case IROpcode::IR_LESS_THAN_SIGNED: {
        print_two_op(instruction, "s&lt;");
        break;
    }
    case IROpcode::IR_GREATER_THAN_UNSIGNED: {
        print_two_op(instruction, "u&gt;");
        break;
    }
    case IROpcode::IR_LESS_THAN_UNSIGNED: {
        print_two_op(instruction, "u&lt;");
        break;
    }
    case IROpcode::IR_MOV: {
        printf(VAR EQUALS VAR, instruction.GetName(), instruction.GetOperandName(0));
        break;
    }
    case IROpcode::IR_SEXT8: {
        print_one_op(instruction, "sext8");
        break;
    }
    case IROpcode::IR_SEXT16: {
        print_one_op(instruction, "sext16");
        break;
    }
    case IROpcode::IR_SEXT32: {
        print_one_op(instruction, "sext32");
        break;
    }
    case IROpcode::IR_LEA: {
        printf("t%d = ptr[t%d + t%d * t%d + t%d]", instruction.GetName(), instruction.GetOperandName(0), instruction.GetOperandName(1),
               instruction.GetOperandName(2), instruction.GetOperandName(3));
        break;
    }
    case IROpcode::IR_GET_GUEST: {
        printf(VAR EQUALS OP, instruction.GetName(), "get_guest");
        print_guest(instruction.AsGetGuest().ref);
        break;
    }
    case IROpcode::IR_SET_GUEST: {
        printf(VAR EQUALS OP, instruction.GetName(), "set_guest");
        print_guest(instruction.AsSetGuest().ref);
        printf(",&nbsp;" VAR, instruction.AsSetGuest().source->GetName());
        break;
    }
    case IROpcode::IR_READ_BYTE: {
        printf("t%d = byte[t%d]", instruction.GetName(), instruction.GetOperandName(0));
        break;
    }
    case IROpcode::IR_READ_WORD: {
        printf("t%d = word[t%d]", instruction.GetName(), instruction.GetOperandName(0));
        break;
    }
    case IROpcode::IR_READ_DWORD: {
        printf("t%d = dword[t%d]", instruction.GetName(), instruction.GetOperandName(0));
        break;
    }
    case IROpcode::IR_READ_QWORD: {
        printf("t%d = qword[t%d]", instruction.GetName(), instruction.GetOperandName(0));
        break;
    }
    case IROpcode::IR_READ_XMMWORD: {
        printf("t%d = xmmword[t%d]", instruction.GetName(), instruction.GetOperandName(0));
        break;
    }
    case IROpcode::IR_WRITE_BYTE: {
        printf("byte[t%d] = t%d", instruction.GetOperandName(0), instruction.GetOperandName(1));
        break;
    }
    case IROpcode::IR_WRITE_WORD: {
        printf("word[t%d] = t%d", instruction.GetOperandName(0), instruction.GetOperandName(1));
        break;
    }
    case IROpcode::IR_WRITE_DWORD: {
        printf("dword[t%d] = t%d", instruction.GetOperandName(0), instruction.GetOperandName(1));
        break;
    }
    case IROpcode::IR_WRITE_QWORD: {
        printf("qword[t%d] = t%d", instruction.GetOperandName(0), instruction.GetOperandName(1));
        break;
    }
    case IROpcode::IR_SYSCALL: {
        printf("syscall");
        break;
    }
    case IROpcode::IR_CPUID: {
        printf("cpuid");
        break;
    }
    case IROpcode::IR_PHI: {
        printf("t%d = φ&lt;", instruction.GetName());
        const Phi& phi = instruction.AsPhi();
        for (auto& node : phi.nodes) {
            printf("t%d @ %p", node.value->GetName(), node.block);
            if (&node != &phi.nodes.back()) {
                printf(", ");
            }
        }
        printf("&gt;");
        break;
    }
    default: {
        printf("Unknown opcode: %d", (int)instruction.GetOpcode());
        break;
    }
    }

    // printf("\t\t\t\t(uses: %d)", instruction->uses);
}

void ir_print_block(const IRBlock& block) {
    for (auto& instruction : block.GetInstructions()) {
        ir_print_instruction(instruction, block);
    }
}

void ir_print_function_graphviz(const IRFunction& function) {
    printf("digraph function_%016lx {\n", function.GetStartAddress());
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

    auto& blocks = function.GetBlocks();
    for (auto& block : blocks) {
        u64 address = block.GetStartAddress() - g_base_address;
        printf("\tblock_%p [", &block);
        printf("\t\tfontcolor=\"#ffffff\"");
        printf("\t\tfillcolor=\"#1e1e1e\"");
        printf("\t\tlabel=<<table border=\"0\" cellborder=\"1\" cellspacing=\"0\" "
               "cellpadding=\"3\">\n");
        printf("\t\t<tr><td port=\"top\"><b>%016lx</b></td> </tr>\n", (u64)(address));

        for (auto& instruction : block.GetInstructions()) {
            printf("\t\t<tr><td align=\"left\" sides=\"lr\">");
            ir_print_instruction(instruction, block);
            printf("</td></tr>\n");
        }

        printf("\t\t</table>>\n");
        printf("\t\tshape=plain\n");
        printf("\t];\n");

        if (block.GetTermination() == Termination::JumpConditional) {
            printf("\tblock_%p:exit -> block_%p:top [color=\"#00ff00\" tailport=s "
                   "headport=n]\n",
                   &block, block.GetSuccessor(0));
            printf("\tblock_%p:exit -> block_%p:top [color=\"#ff0000\" tailport=s "
                   "headport=n]\n",
                   &block, block.GetSuccessor(1));
        } else if (block.GetTermination() == Termination::Jump) {
            printf("\tblock_%p:exit -> block_%p:top\n", &block, block.GetSuccessor(0));
        }
    }

    printf("}\n");
    fflush(stdout);
}