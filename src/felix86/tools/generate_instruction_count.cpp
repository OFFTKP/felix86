#include <cstdint>
#include <cstring>
#include <fstream>
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

void gen(Recompiler& rec, nlohmann::json& json, auto func) {
    static Decoder decoder{};
    static bool init = false;
    static ZydisDecoder zydis;
    if (!init) {
        init = true;
        ZydisDecoderInit(&zydis, ZYDIS_MACHINE_MODE_LONG_64, ZYDIS_STACK_WIDTH_64);
        ZydisDecoderEnableMode(&zydis, ZYDIS_DECODER_MODE_AMD_BRANCHES, ZYAN_TRUE);
    }

    // Set bogus vector state so we see the vector state changes
    rec.setVectorState(SEW::E1024, 0);
    rec.assumeLoaded();

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
    g_dont_inline_syscalls = true;
    Extensions::G = true;
    Extensions::B = true;
    Extensions::C = true;
    Extensions::V = true;
    Extensions::VLEN = 256;
    Extensions::Zicond = true;

    Recompiler rec;
    nlohmann::json json;

#define GEN(inst) gen(rec, json, [&](Xbyak::CodeGenerator& x) { x.inst; })

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
    GEN(name(al, -1));                                                                                                                               \
    GEN(name(ah, 1));                                                                                                                                \
    GEN(name(ah, -1));                                                                                                                               \
    GEN(name(ax, 1));                                                                                                                                \
    GEN(name(ax, -1));                                                                                                                               \
    GEN(name(eax, 1));                                                                                                                               \
    GEN(name(eax, -1));                                                                                                                              \
    GEN(name(rax, 1));                                                                                                                               \
    GEN(name(rax, -1));                                                                                                                              \
    GEN(name(byte[rdi], 1));                                                                                                                         \
    GEN(name(word[rdi], 1));                                                                                                                         \
    GEN(name(dword[rdi], 1));                                                                                                                        \
    GEN(name(qword[rdi], 1))

#define GEN_SingleRM(name)                                                                                                                           \
    GEN(name(al));                                                                                                                                   \
    GEN(name(ah));                                                                                                                                   \
    GEN(name(ax));                                                                                                                                   \
    GEN(name(eax));                                                                                                                                  \
    GEN(name(rax));                                                                                                                                  \
    GEN(name(byte[rdi]));                                                                                                                            \
    GEN(name(word[rdi]));                                                                                                                            \
    GEN(name(dword[rdi]));                                                                                                                           \
    GEN(name(qword[rdi]));

#define GEN_Shift(name)                                                                                                                              \
    GEN(name(al, 1));                                                                                                                                \
    GEN(name(ah, 1));                                                                                                                                \
    GEN(name(ax, 1));                                                                                                                                \
    GEN(name(eax, 1));                                                                                                                               \
    GEN(name(rax, 1));                                                                                                                               \
    GEN(name(al, 63));                                                                                                                               \
    GEN(name(ah, 63));                                                                                                                               \
    GEN(name(ax, 63));                                                                                                                               \
    GEN(name(eax, 63));                                                                                                                              \
    GEN(name(rax, 63));                                                                                                                              \
    GEN(name(al, cl));                                                                                                                               \
    GEN(name(ah, cl));                                                                                                                               \
    GEN(name(ax, cl));                                                                                                                               \
    GEN(name(eax, cl));                                                                                                                              \
    GEN(name(rax, cl));                                                                                                                              \
    GEN(name(byte[rdi], 1));                                                                                                                         \
    GEN(name(word[rdi], 1));                                                                                                                         \
    GEN(name(dword[rdi], 1));                                                                                                                        \
    GEN(name(qword[rdi], 1));                                                                                                                        \
    GEN(name(byte[rdi], 63));                                                                                                                        \
    GEN(name(word[rdi], 63));                                                                                                                        \
    GEN(name(dword[rdi], 63));                                                                                                                       \
    GEN(name(qword[rdi], 63));                                                                                                                       \
    GEN(name(byte[rdi], cl));                                                                                                                        \
    GEN(name(word[rdi], cl));                                                                                                                        \
    GEN(name(dword[rdi], cl));                                                                                                                       \
    GEN(name(qword[rdi], cl))

    GEN_Group1(add);
    GEN_Group1(sub);
    GEN_Group1(adc);
    GEN_Group1(sbb);
    GEN_Group1(or_);
    GEN_Group1(and_);
    GEN_Group1(xor_);
    GEN_Group1(cmp);
    GEN_Group1(mov);
    GEN_Shift(shl);
    GEN_Shift(shr);
    GEN_Shift(sar);
    GEN_Shift(rol);
    GEN_Shift(ror);
    GEN_Shift(rcl);
    GEN_Shift(rcr);
    GEN_SingleRM(inc);
    GEN_SingleRM(dec);
    GEN(cwd());
    GEN(cdq());
    GEN(cqo());
    GEN(cbw());
    GEN(cwde());
    GEN(cdqe());
    GEN(cld());
    GEN(std());
    GEN(clc());
    GEN(stc());
    GEN(sahf());
    GEN(lahf());
    GEN(pushfq());
    GEN(popfq());
    GEN(cmpsb());
    GEN(cmpsw());
    GEN(cmpsd());
    GEN(cmpsq());
    GEN(movsb());
    GEN(movsw());
    GEN(movsd());
    GEN(movsq());
    GEN(stosb());
    GEN(stosw());
    GEN(stosd());
    GEN(stosq());
    GEN(lodsb());
    GEN(lodsw());
    GEN(lodsd());
    GEN(lodsq());

    std::ofstream base("counts/Base.json");
    base << json.dump(4);
    json.clear();

#define GEN_SSE(name)                                                                                                                                \
    GEN(name(xmm3, xmm4));                                                                                                                           \
    GEN(name(xmm2, xmm2));                                                                                                                           \
    GEN(name(xmm1, ptr[rdi]))

#define GEN_SSE_MOV(name)                                                                                                                            \
    GEN(name(xmm3, xmm4));                                                                                                                           \
    GEN(name(xmm2, xmm2));                                                                                                                           \
    GEN(name(ptr[rdi], xmm1));                                                                                                                       \
    GEN(name(xmm1, ptr[rdi]))

#define GEN_SSE_CMP(name)                                                                                                                            \
    GEN(name(xmm3, xmm4, 0b000));                                                                                                                    \
    GEN(name(xmm3, xmm4, 0b001));                                                                                                                    \
    GEN(name(xmm3, xmm4, 0b010));                                                                                                                    \
    GEN(name(xmm3, xmm4, 0b011));                                                                                                                    \
    GEN(name(xmm3, xmm4, 0b100));                                                                                                                    \
    GEN(name(xmm3, xmm4, 0b101));                                                                                                                    \
    GEN(name(xmm3, xmm4, 0b110));                                                                                                                    \
    GEN(name(xmm3, xmm4, 0b111));                                                                                                                    \
    GEN(name(xmm2, xmm2, 0b000));                                                                                                                    \
    GEN(name(xmm2, xmm2, 0b001));                                                                                                                    \
    GEN(name(xmm2, xmm2, 0b010));                                                                                                                    \
    GEN(name(xmm2, xmm2, 0b011));                                                                                                                    \
    GEN(name(xmm2, xmm2, 0b100));                                                                                                                    \
    GEN(name(xmm2, xmm2, 0b101));                                                                                                                    \
    GEN(name(xmm2, xmm2, 0b110));                                                                                                                    \
    GEN(name(xmm2, xmm2, 0b111));                                                                                                                    \
    GEN(name(xmm3, ptr[rdi], 0b000));                                                                                                                \
    GEN(name(xmm3, ptr[rdi], 0b001));                                                                                                                \
    GEN(name(xmm3, ptr[rdi], 0b010));                                                                                                                \
    GEN(name(xmm3, ptr[rdi], 0b011));                                                                                                                \
    GEN(name(xmm3, ptr[rdi], 0b100));                                                                                                                \
    GEN(name(xmm3, ptr[rdi], 0b101));                                                                                                                \
    GEN(name(xmm3, ptr[rdi], 0b110));                                                                                                                \
    GEN(name(xmm3, ptr[rdi], 0b111))

    GEN_SSE(addss);
    GEN_SSE(subss);
    GEN_SSE(mulss);
    GEN_SSE(divss);
    GEN_SSE(rcpss);
    GEN_SSE(sqrtss);
    GEN_SSE(minss);
    GEN_SSE(maxss);
    GEN_SSE(rsqrtss);

    GEN_SSE(addps);
    GEN_SSE(subps);
    GEN_SSE(mulps);
    GEN_SSE(divps);
    GEN_SSE(rcpps);
    GEN_SSE(sqrtps);
    GEN_SSE(minps);
    GEN_SSE(maxps);
    GEN_SSE(rsqrtps);
    GEN_SSE(andps);
    GEN_SSE(orps);
    GEN_SSE(xorps);
    GEN_SSE(andnps);

    GEN(cvtsi2ss(xmm3, rax));
    GEN(cvtsi2ss(xmm2, eax));
    GEN(cvtsi2ss(xmm1, dword[rdi]));
    GEN(cvtsi2ss(xmm1, qword[rdi]));
    GEN(cvtss2si(rax, xmm3));
    GEN(cvtss2si(eax, xmm2));
    GEN(cvtss2si(eax, dword[rdi]));
    GEN(cvtss2si(rax, dword[rdi]));
    GEN(cvttss2si(rax, xmm3));
    GEN(cvttss2si(eax, xmm2));
    GEN(cvttss2si(eax, dword[rdi]));
    GEN(cvttss2si(rax, dword[rdi]));

    GEN_SSE(pmulhuw);
    GEN_SSE(psadbw);
    GEN_SSE(pavgb);
    GEN_SSE(pavgw);
    GEN_SSE(pmaxub);
    GEN_SSE(pmaxsw);
    GEN_SSE(pminub);
    GEN_SSE(pminsw);

    GEN_SSE_MOV(movss);
    GEN_SSE_MOV(movaps);
    GEN_SSE_MOV(movups);
    GEN(movlps(ptr[rdi], xmm3));
    GEN(movlps(xmm3, ptr[rdi]));
    GEN(movhps(ptr[rdi], xmm3));
    GEN(movhps(xmm3, ptr[rdi]));
    GEN(movlhps(xmm3, xmm4));
    GEN(movlhps(xmm2, xmm2));
    GEN(movhlps(xmm3, xmm4));
    GEN(movhlps(xmm2, xmm2));
    GEN(movmskps(eax, xmm2));
    GEN(movmskps(rax, xmm2));
    GEN(pmovmskb(eax, xmm2));
    GEN(pmovmskb(rax, xmm2));
    GEN_SSE_CMP(cmpss);
    GEN_SSE_CMP(cmpps);

    for (int i = 0; i < 256; i++) {
        GEN(shufps(xmm3, xmm4, (u8)i));
        GEN(shufps(xmm2, xmm2, (u8)i));
        GEN(shufps(xmm3, ptr[rdi], (u8)i));
    }

    GEN(unpckhps(xmm3, xmm4));
    GEN(unpckhps(xmm2, xmm2));
    GEN(unpckhps(xmm3, ptr[rdi]));
    GEN(unpcklps(xmm3, xmm4));
    GEN(unpcklps(xmm2, xmm2));
    GEN(unpcklps(xmm3, ptr[rdi]));

    std::ofstream sse1("counts/SSE1.json");
    sse1 << json.dump(4);
    json.clear();

    GEN_SSE(addsd);
    GEN_SSE(subsd);
    GEN_SSE(mulsd);
    GEN_SSE(divsd);
    GEN_SSE(sqrtsd);
    GEN_SSE(minsd);
    GEN_SSE(maxsd);

    GEN_SSE(addpd);
    GEN_SSE(subpd);
    GEN_SSE(mulpd);
    GEN_SSE(divpd);
    GEN_SSE(sqrtpd);
    GEN_SSE(minpd);
    GEN_SSE(maxpd);
    GEN_SSE(andpd);
    GEN_SSE(orpd);
    GEN_SSE(xorpd);
    GEN_SSE(andnpd);

    GEN(cvtsi2sd(xmm3, rax));
    GEN(cvtsi2sd(xmm2, eax));
    GEN(cvtsi2sd(xmm1, dword[rdi]));
    GEN(cvtsi2sd(xmm1, qword[rdi]));
    GEN(cvtsd2si(rax, xmm3));
    GEN(cvtsd2si(eax, xmm2));
    GEN(cvtsd2si(eax, dword[rdi]));
    GEN(cvtsd2si(rax, dword[rdi]));
    GEN(cvttsd2si(rax, xmm3));
    GEN(cvttsd2si(eax, xmm2));
    GEN(cvttsd2si(eax, dword[rdi]));
    GEN(cvttsd2si(rax, dword[rdi]));

    GEN_SSE_MOV(movsd);
    GEN_SSE_MOV(movapd);
    GEN_SSE_MOV(movupd);

    std::ofstream sse2("counts/SSE2.json");
    sse2 << json.dump(4);
    json.clear();
}