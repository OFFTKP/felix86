#include <array>
#include <string>
#include <vector>

enum Type {
    Vec_Vec,
    Vec_Vec_Imm,
    Vec_Reg,
    Vec_Reg_Imm,
    Reg_Vec,
    Reg_Vec_Imm,
};

struct InstructionMeta {
    std::string inst;
    Type type;
};

// clang-format off
std::vector<InstructionMeta> insts = {
    {"movlhps", Vec_Vec},
    {"movlhps", Vec_Vec},
    {"movhlps", Vec_Vec},
    {"pmaddwd", Vec_Vec},
    {"psadbw", Vec_Vec},
    {"psllq", Vec_Vec},
    {"pslld", Vec_Vec},
    {"psllw", Vec_Vec},
    {"psrlq", Vec_Vec},
    {"psrld", Vec_Vec},
    {"psrlw", Vec_Vec},
    {"comiss", Vec_Vec},
    {"comisd", Vec_Vec},
    {"pinsrq", Vec_Reg_Imm},
    {"pextrq", Reg_Vec_Imm},
    {"punpcklbw", Vec_Vec},
    {"punpcklwd", Vec_Vec},
    {"punpckldq", Vec_Vec},
    {"punpcklqdq", Vec_Vec},
    {"punpckhbw", Vec_Vec},
    {"punpckhwd", Vec_Vec},
    {"punpckhdq", Vec_Vec},
    {"punpckhqdq", Vec_Vec},
    {"pshufd", Vec_Vec_Imm},
    {"pshuflw", Vec_Vec_Imm},
    {"pshufhw", Vec_Vec_Imm},
    {"pshufb", Vec_Vec},
    {"pblendw", Vec_Vec_Imm},
    {"blendps", Vec_Vec_Imm},
    {"blendpd", Vec_Vec_Imm},
    {"paddb", Vec_Vec},
    {"paddw", Vec_Vec},
    {"paddd", Vec_Vec},
    {"paddq", Vec_Vec},
    {"psubb", Vec_Vec},
    {"psubw", Vec_Vec},
    {"psubd", Vec_Vec},
    {"psubq", Vec_Vec},
    {"por", Vec_Vec},
    {"pxor", Vec_Vec},
    {"pand", Vec_Vec},
    {"pandn", Vec_Vec},
    {"pavgb", Vec_Vec},
    {"pavgw", Vec_Vec},
    {"pmullw", Vec_Vec},
    {"pmulld", Vec_Vec},
    {"pmulhw", Vec_Vec},
    {"pmulhuw", Vec_Vec},
    {"pminsb", Vec_Vec},
    {"pminsw", Vec_Vec},
    {"pminsd", Vec_Vec},
    {"pminub", Vec_Vec},
    {"pminuw", Vec_Vec},
    {"pminud", Vec_Vec},
    {"pmaxsb", Vec_Vec},
    {"pmaxsw", Vec_Vec},
    {"pmaxsd", Vec_Vec},
    {"pmaxub", Vec_Vec},
    {"pmaxuw", Vec_Vec},
    {"pmaxud", Vec_Vec},
    {"packuswb", Vec_Vec},
    {"packusdw", Vec_Vec},
    {"packsswb", Vec_Vec},
    {"packssdw", Vec_Vec},
    {"pmuldq", Vec_Vec},
    {"pmuludq", Vec_Vec},
    {"paddsb", Vec_Vec},
    {"paddsw", Vec_Vec},
    {"psubsb", Vec_Vec},
    {"psubsw", Vec_Vec},
    {"paddusb", Vec_Vec},
    {"paddusw", Vec_Vec},
    {"psubusb", Vec_Vec},
    {"psubusw", Vec_Vec},
    {"movddup", Vec_Vec},
    {"pcmpeqb", Vec_Vec},
    {"pcmpeqw", Vec_Vec},
    {"pcmpeqd", Vec_Vec},
    {"pcmpeqq", Vec_Vec},
    {"pcmpgtb", Vec_Vec},
    {"pcmpgtw", Vec_Vec},
    {"pcmpgtd", Vec_Vec},
    {"pcmpgtq", Vec_Vec},
    {"cvtdq2pd", Vec_Vec},
    {"cvtdq2ps", Vec_Vec},
    {"movss", Vec_Vec},
    {"movsd", Vec_Vec},
    {"unpcklps", Vec_Vec},
    {"unpckhps", Vec_Vec},
    {"unpcklpd", Vec_Vec},
    {"unpckhpd", Vec_Vec},

    // {"pmovmskb", Vec_Vec},
    // {"movmskps", Vec_Vec},
    // {"movmskpd", Vec_Vec},
    // {"addps", Vec_Vec},
    // {"addpd", Vec_Vec},
};
// clang-format on

std::string randominst() {
    auto inst = insts[rand() % insts.size()];
    std::string ret = inst.inst;
    switch (inst.type) {
    case Vec_Vec:
        ret += " xmm" + std::to_string(rand() % 16) + ", xmm" + std::to_string(rand() % 16);
        break;
    case Vec_Vec_Imm:
        ret += " xmm" + std::to_string(rand() % 16) + ", xmm" + std::to_string(rand() % 16) + ", " + std::to_string(rand() % 256);
        break;
    case Vec_Reg:
        ret += " xmm" + std::to_string(rand() % 16) + ", r" + std::to_string(8 + (rand() % 8));
        break;
    case Vec_Reg_Imm:
        ret += " xmm" + std::to_string(rand() % 16) + ", r" + std::to_string(8 + (rand() % 8)) + ", " + std::to_string(rand() % 256);
        break;
    case Reg_Vec:
        ret += " r" + std::to_string(8 + (rand() % 8)) + ", xmm" + std::to_string(rand() % 16);
        break;
    case Reg_Vec_Imm:
        ret += " r" + std::to_string(8 + (rand() % 8)) + ", xmm" + std::to_string(rand() % 16) + ", " + std::to_string(rand() % 256);
        break;
    }

    ret += "\n";
    return ret;
}

int main() {
    std::string as = "bits 64\n";
    as += "global test\n";
    as += "test:\n";
    for (int i = 0; i < 16; i++) {
        as += "mov rax, " + std::to_string((long)rand() << 32 | rand()) + "\n";
        as += "movq xmm" + std::to_string(i) + ", rax\n";
        as += "mov rax, " + std::to_string((long)rand() << 32 | rand()) + "\n";
        as += "pinsrq xmm" + std::to_string(i) + ", rax, 1\n";
    }
    for (int i = 0; i < 8; i++) {
        as += "mov r" + std::to_string(8 + i) + ", " + std::to_string(rand()) + "\n";
    }
    for (int i = 0; i < 30000; i++) {
        as += randominst();
    }
    for (int i = 0; i < 16; i++) {
        as += "movdqa [rdi + " + std::to_string(i * 16) + "], xmm" + std::to_string(i) + "\n";
    }
    as += "ret\n";
    printf("%s", as.c_str());
}