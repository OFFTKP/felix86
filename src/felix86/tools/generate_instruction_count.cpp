#include <cstdint>
#include <cstring>
#include <nlohmann/json.hpp>
#include <xbyak/xbyak.h>
#include "Zydis/Disassembler.h"
#include "biscuit/decoder.hpp"
#include "felix86/v2/recompiler.hpp"
#include "fmt/format.h"
#include "rv64_printer.h"

using namespace nlohmann;

using namespace Xbyak::util;

struct Instruction {
    int count;
    std::string disassembly;
    std::vector<std::string> expected_asm;
};

void to_json(json& j, const Instruction& p) {
    j = json{{"instruction_count", p.count}, {"expected_asm", p.expected_asm}, {"disassembly", p.disassembly}};
}

void from_json(const json& j, Instruction& p) {
    j.at("instruction_count").get_to(p.count);
    j.at("expected_asm").get_to(p.expected_asm);
    j.at("disassembly").get_to(p.disassembly);
}

// #define MAKE_TEST(inst, dest, src) \
//     {\
//     Xbyak::CodeGenerator x;\
//     auto here = x.getCurr();\
//     x.inst(dst, src);\

//     }

void gen(Recompiler& rec, nlohmann::json& json, auto func) {
    static Decoder decoder{};
    static bool init = false;
    static ZydisDecoder zydis;
    if (!init) {
        init = true;
        ZydisDecoderInit(&zydis, ZYDIS_MACHINE_MODE_LONG_64, ZYDIS_STACK_WIDTH_64);
        ZydisDecoderEnableMode(&zydis, ZYDIS_DECODER_MODE_AMD_BRANCHES, ZYAN_TRUE);
    }

    DecodedInstruction instruction;
    DecodedOperand operands[4];
    Xbyak::CodeGenerator x;
    auto x86_start = x.getCurr();
    func(x);
    auto x86_end = x.getCurr();
    auto bisc = rec.getAssembler().GetCursorPointer();
    HandlerMetadata meta;
    meta.rip = HostAddress{(u64)x86_start};
    rec.compileInstruction(meta);
    auto after = rec.getAssembler().GetCursorPointer();
    int count = 0;
    Instruction inst;
    std::string bytes;
    for (int i = 0; i < x86_end - x86_start; i++) {
        bytes += fmt::format("{:02x}", x86_start[i]);
    }

    for (int i = 0; i < after - bisc;) {
        void* address = bisc + i;
        auto status = decoder.Decode(bisc, 4, instruction, operands);
        if (status == biscuit::DecoderStatus::Ok) {
            i += instruction.length;
        } else if (status == biscuit::DecoderStatus::UnknownInstructionCompressed) {
            i += 2;
        } else {
            i += 4;
        }
        u32 data = 0;
        memcpy(&data, address, 4);
        const char* out = rv64_print(data, (u64)address);
        inst.expected_asm.push_back(out);
        count++;
    }

    ZydisDisassembledInstruction zinstruction;
    ZydisDisassembleIntel(ZYDIS_MACHINE_MODE_LONG_64, (u64)x86_start, x86_start, 15, &zinstruction);

    inst.count = count;
    inst.disassembly = zinstruction.text;
    json[bytes] = inst;
}

int main() {
    Extensions::G = true;
    Extensions::B = true;
    Extensions::C = true;
    Extensions::V = true;
    Extensions::VLEN = 256;
    Extensions::Zicond = true;

    Recompiler rec;
    nlohmann::json json;

#define GEN(inst) gen(rec, json, [](Xbyak::CodeGenerator& x) { x.inst; })

#define GEN_Group1(name)                                                                                                                             \
    GEN(name(al, bl));                                                                                                                               \
    GEN(name(al, bh));                                                                                                                               \
    GEN(name(ah, bl));                                                                                                                               \
    GEN(name(ah, bh));                                                                                                                               \
    GEN(name(ax, bx));                                                                                                                               \
    GEN(name(eax, ebx));                                                                                                                             \
    GEN(name(rax, rbx));                                                                                                                             \
    GEN(name(al, byte[rdi]));                                                                                                                        \
    GEN(name(ah, byte[rdi]));                                                                                                                        \
    GEN(name(ax, word[rdi]));                                                                                                                        \
    GEN(name(eax, dword[rdi]));                                                                                                                      \
    GEN(name(rax, qword[rdi]));                                                                                                                      \
    GEN(name(byte[rdi], al));                                                                                                                        \
    GEN(name(byte[rdi], ah));                                                                                                                        \
    GEN(name(word[rdi], ax));                                                                                                                        \
    GEN(name(dword[rdi], eax));                                                                                                                      \
    GEN(name(qword[rdi], rax));                                                                                                                      \
    GEN(name(al, 1));                                                                                                                                \
    GEN(name(ah, 1));                                                                                                                                \
    GEN(name(ax, 1));                                                                                                                                \
    GEN(name(eax, 1));                                                                                                                               \
    GEN(name(rax, 1));                                                                                                                               \
    GEN(name(byte[rdi], 1));                                                                                                                         \
    GEN(name(word[rdi], 1));                                                                                                                         \
    GEN(name(dword[rdi], 1));                                                                                                                        \
    GEN(name(qword[rdi], 1))

    GEN_Group1(add);
    GEN_Group1(sub);
    GEN_Group1(adc);
    GEN_Group1(sbb);
    GEN_Group1(or_);
    GEN_Group1(and_);
    GEN_Group1(xor_);
    GEN_Group1(cmp);
    GEN_Group1(mov);

    printf("%s\n", json.dump(4).c_str());
}