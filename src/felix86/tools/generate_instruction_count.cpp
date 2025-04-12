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

void to_json(ordered_json& j, const Instruction& p) {
    if (!p.disassembly.empty())
        j = ordered_json{{"instruction_count", p.count}, {"expected_asm", p.expected_asm}, {"disassembly", p.disassembly}};
    else
        j = ordered_json{{"instruction_count", p.count}, {"expected_asm", p.expected_asm}};
}

void from_json(const ordered_json& j, Instruction& p) {
    j.at("instruction_count").get_to(p.count);
    j.at("expected_asm").get_to(p.expected_asm);
    if (!p.disassembly.empty())
        j.at("disassembly").get_to(p.disassembly);
}

void gen_many(Recompiler& rec, const std::string& name, nlohmann::ordered_json& json, void (*func)(Xbyak::CodeGenerator&)) {
    static Decoder decoder{};
    static bool init = false;
    static ZydisDecoder zydis;
    if (!init) {
        init = true;
        ZydisDecoderInit(&zydis, ZYDIS_MACHINE_MODE_LONG_64, ZYDIS_STACK_WIDTH_64);
        ZydisDecoderEnableMode(&zydis, ZYDIS_DECODER_MODE_AMD_BRANCHES, ZYAN_TRUE);
    }

    rec.setVectorState(SEW::E1024, 0);
    rec.assumeLoaded();
    DecodedInstruction instruction;
    DecodedOperand operands[4];
    Xbyak::CodeGenerator x;
    auto x86_start = x.getCurr();
    func(x);

    auto bisc = rec.getAssembler().GetCursorPointer();
    rec.compileSequence((u64)x86_start);
    auto after = rec.getAssembler().GetCursorPointer();
    int count = 0;
    Instruction inst;
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

    inst.count = count;
    json[name] = inst;
}

void gen(Recompiler& rec, nlohmann::ordered_json& json, void (*func)(Xbyak::CodeGenerator&), bool flags = false) {
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
    rec.setFlagMode(flags ? FlagMode::AlwaysEmit : FlagMode::NeverEmit);

    DecodedInstruction instruction;
    DecodedOperand operands[4];
    Xbyak::CodeGenerator x;
    auto x86_start = x.getCurr();
    func(x);
    auto x86_end = x.getCurr();
    auto bisc = rec.getAssembler().GetCursorPointer();
    ZydisDecodedInstruction zinstruction;
    ZydisDecodedOperand zoperands[10];
    u64 rip = (u64)x86_start;
    rec.decode(rip, zinstruction, zoperands);
    rec.compileInstruction(zinstruction, zoperands, rip);
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

    ZydisDisassembledInstruction dinstruction;
    ZydisDisassembleIntel(ZYDIS_MACHINE_MODE_LONG_64, (u64)x86_start, x86_start, 15, &dinstruction);

    inst.count = count;
    inst.disassembly = dinstruction.text;
    json[bytes] = inst;
}

int main() {
    g_config.inline_syscalls = false;
    Extensions::G = true;
    Extensions::B = true;
    Extensions::C = true;
    Extensions::V = true;
    Extensions::VLEN = 256;
    Extensions::Zicond = true;

    Recompiler rec;
    nlohmann::ordered_json json;

#define GEN(inst) gen(rec, json, [](Xbyak::CodeGenerator& x) { x.inst; }, flags)

    for (int i = 0; i < 2; i++) {
        std::string name = i != 0 ? "Base.json" : "Base_NoFlags.json";
        bool flags = i;

#define GEN_Group1(name)                                                                                                                             \
    GEN(name(dl, bl));                                                                                                                               \
    GEN(name(dl, bh));                                                                                                                               \
    GEN(name(dh, bl));                                                                                                                               \
    GEN(name(dh, bh));                                                                                                                               \
    GEN(name(dx, bx));                                                                                                                               \
    GEN(name(edx, ebx));                                                                                                                             \
    GEN(name(rdx, rbx));                                                                                                                             \
    GEN(name(dl, byte[rdi]));                                                                                                                        \
    GEN(name(dh, byte[rdi]));                                                                                                                        \
    GEN(name(dx, word[rdi]));                                                                                                                        \
    GEN(name(edx, dword[rdi]));                                                                                                                      \
    GEN(name(rdx, qword[rdi]));                                                                                                                      \
    GEN(name(byte[rdi], dl));                                                                                                                        \
    GEN(name(byte[rdi], dh));                                                                                                                        \
    GEN(name(word[rdi], dx));                                                                                                                        \
    GEN(name(dword[rdi], edx));                                                                                                                      \
    GEN(name(qword[rdi], rdx));                                                                                                                      \
    GEN(name(dl, 1));                                                                                                                                \
    GEN(name(dl, -1));                                                                                                                               \
    GEN(name(dh, 1));                                                                                                                                \
    GEN(name(dh, -1));                                                                                                                               \
    GEN(name(dx, 1));                                                                                                                                \
    GEN(name(dx, -1));                                                                                                                               \
    GEN(name(edx, 1));                                                                                                                               \
    GEN(name(edx, -1));                                                                                                                              \
    GEN(name(rdx, 1));                                                                                                                               \
    GEN(name(rdx, -1));                                                                                                                              \
    GEN(name(byte[rdi], 1));                                                                                                                         \
    GEN(name(word[rdi], 1));                                                                                                                         \
    GEN(name(dword[rdi], 1));                                                                                                                        \
    GEN(name(qword[rdi], 1))

#define GEN_SingleRM(name)                                                                                                                           \
    GEN(name(dl));                                                                                                                                   \
    GEN(name(dh));                                                                                                                                   \
    GEN(name(dx));                                                                                                                                   \
    GEN(name(edx));                                                                                                                                  \
    GEN(name(rdx));                                                                                                                                  \
    GEN(name(byte[rdi]));                                                                                                                            \
    GEN(name(word[rdi]));                                                                                                                            \
    GEN(name(dword[rdi]));                                                                                                                           \
    GEN(name(qword[rdi]));

#define GEN_Shift(name)                                                                                                                              \
    GEN(name(dl, 1));                                                                                                                                \
    GEN(name(dh, 1));                                                                                                                                \
    GEN(name(dx, 1));                                                                                                                                \
    GEN(name(edx, 1));                                                                                                                               \
    GEN(name(rdx, 1));                                                                                                                               \
    GEN(name(dl, 63));                                                                                                                               \
    GEN(name(dh, 63));                                                                                                                               \
    GEN(name(dx, 63));                                                                                                                               \
    GEN(name(edx, 63));                                                                                                                              \
    GEN(name(rdx, 63));                                                                                                                              \
    GEN(name(dl, cl));                                                                                                                               \
    GEN(name(dh, cl));                                                                                                                               \
    GEN(name(dx, cl));                                                                                                                               \
    GEN(name(edx, cl));                                                                                                                              \
    GEN(name(rdx, cl));                                                                                                                              \
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
        GEN_SingleRM(imul);
        GEN_SingleRM(mul);
        GEN(div(dl));
        GEN(div(dh));
        GEN(div(dx));
        GEN(div(edx));
        GEN(div(byte[rdi]));
        GEN(div(word[rdi]));
        GEN(div(dword[rdi]));
        GEN(idiv(dl));
        GEN(idiv(dh));
        GEN(idiv(dx));
        GEN(idiv(edx));
        GEN(idiv(byte[rdi]));
        GEN(idiv(word[rdi]));
        GEN(idiv(dword[rdi]));
        GEN(imul(dx, bx));
        GEN(imul(edx, ebx));
        GEN(imul(rdx, rbx));
        GEN(imul(dx, word[rdi]));
        GEN(imul(edx, dword[rdi]));
        GEN(imul(rdx, qword[rdi]));
        GEN(imul(dx, bx, -1));
        GEN(imul(edx, ebx, -1));
        GEN(imul(rdx, rbx, -1));
        GEN(imul(dx, word[rdi], -1));
        GEN(imul(edx, dword[rdi], -1));
        GEN(imul(rdx, qword[rdi], -1));
        GEN(putSeg(fs); x.lea(rax, ptr[(void*)1]));
        GEN(putSeg(fs); x.lea(rax, ptr[rdi]));
        GEN(putSeg(fs); x.lea(rax, ptr[rdi + rsi]));
        GEN(putSeg(fs); x.lea(rax, ptr[2 * rsi]));
        GEN(putSeg(fs); x.lea(rax, ptr[rdi + 2 * rsi]));
        GEN(putSeg(fs); x.lea(rax, ptr[rdi + 4 * rsi]));
        GEN(putSeg(fs); x.lea(rax, ptr[rdi + 8 * rsi]));
        GEN(putSeg(fs); x.lea(rax, ptr[rdi + 8 * rsi + 0x12345678]));
        GEN(lea(rax, ptr[(void*)0x12345678]));
        GEN(lea(rax, ptr[rdi]));
        GEN(lea(rax, ptr[rdi + 0x12345678]));
        GEN(lea(rax, ptr[2 * rsi]));
        GEN(lea(rax, ptr[2 * rsi + 0x12345678]));
        GEN(lea(rax, ptr[rdi + 4 * rsi]));
        GEN(lea(rax, ptr[rdi + 4 * rsi + 0x12345678]));
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
        GEN(scasb());
        GEN(scasw());
        GEN(scasd());
        GEN(scasq());
        GEN(lodsb());
        GEN(lodsw());
        GEN(lodsd());
        GEN(lodsq());
        if (flags) {
            GEN(rep(); x.movsb());
            GEN(rep(); x.movsw());
            GEN(rep(); x.movsd());
            GEN(rep(); x.movsq());
            GEN(rep(); x.stosb());
            GEN(rep(); x.stosw());
            GEN(rep(); x.stosd());
            GEN(rep(); x.stosq());
            GEN(rep(); x.cmpsb());
            GEN(rep(); x.cmpsw());
            GEN(rep(); x.cmpsd());
            GEN(rep(); x.cmpsq());
            GEN(rep(); x.scasb());
            GEN(rep(); x.scasw());
            GEN(rep(); x.scasd());
            GEN(rep(); x.scasq());
            GEN(repnz(); x.cmpsb());
            GEN(repnz(); x.cmpsw());
            GEN(repnz(); x.cmpsd());
            GEN(repnz(); x.cmpsq());
            GEN(repnz(); x.scasb());
            GEN(repnz(); x.scasw());
            GEN(repnz(); x.scasd());
            GEN(repnz(); x.scasq());
        }

        std::ofstream base("counts/" + name);
        base << json.dump(4);
        json.clear();
    }
#undef GEN

#define GEN(inst) gen(rec, json, [](Xbyak::CodeGenerator& x) { x.inst; })
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

    GEN_SSE(comiss);
    GEN_SSE(ucomiss);

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

    GEN(shufps(xmm3, xmm4, (u8)0));
    GEN(shufps(xmm2, xmm2, (u8)0));
    GEN(shufps(xmm3, ptr[rdi], (u8)0));
    GEN(shufps(xmm3, xmm4, (u8)0xE4));
    GEN(shufps(xmm2, xmm2, (u8)0xE4));
    GEN(shufps(xmm3, ptr[rdi], (u8)0xE4));

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

    GEN_SSE(comisd);
    GEN_SSE(ucomisd);

    GEN_SSE(pcmpeqb);
    GEN_SSE(pcmpeqw);
    GEN_SSE(pcmpeqd);

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
    GEN(cvtps2pd(xmm3, xmm4));
    GEN(cvtps2pd(xmm2, xmm2));
    GEN(cvtps2pd(xmm3, ptr[rdi]));
    GEN(cvtpd2dq(xmm3, xmm4));
    GEN(cvtpd2dq(xmm2, xmm2));
    GEN(cvtpd2dq(xmm3, ptr[rdi]));
    GEN(cvtpd2ps(xmm3, xmm4));
    GEN(cvtpd2ps(xmm2, xmm2));
    GEN(cvtpd2ps(xmm3, ptr[rdi]));
    GEN(cvtps2dq(xmm3, xmm4));
    GEN(cvtps2dq(xmm2, xmm2));
    GEN(cvtps2dq(xmm3, ptr[rdi]));
    GEN(cvttps2dq(xmm3, xmm4));
    GEN(cvttps2dq(xmm2, xmm2));
    GEN(cvttps2dq(xmm3, ptr[rdi]));
    GEN(cvttpd2dq(xmm3, xmm4));
    GEN(cvttpd2dq(xmm2, xmm2));
    GEN(cvttpd2dq(xmm3, ptr[rdi]));

    GEN(unpckhpd(xmm3, xmm4));
    GEN(unpckhpd(xmm2, xmm2));
    GEN(unpckhpd(xmm3, ptr[rdi]));
    GEN(unpcklpd(xmm3, xmm4));
    GEN(unpcklpd(xmm2, xmm2));
    GEN(unpcklpd(xmm3, ptr[rdi]));

    GEN(pshufd(xmm3, xmm4, (u8)0));
    GEN(pshufd(xmm2, xmm2, (u8)0));
    GEN(pshufd(xmm3, ptr[rdi], (u8)0));
    GEN(pshufd(xmm3, xmm4, (u8)0xE4));
    GEN(pshufd(xmm2, xmm2, (u8)0xE4));
    GEN(pshufd(xmm3, ptr[rdi], (u8)0xE4));
    GEN(pshuflw(xmm3, xmm4, (u8)0));
    GEN(pshuflw(xmm2, xmm2, (u8)0));
    GEN(pshuflw(xmm3, ptr[rdi], (u8)0));
    GEN(pshuflw(xmm3, xmm4, (u8)0xE4));
    GEN(pshuflw(xmm2, xmm2, (u8)0xE4));
    GEN(pshuflw(xmm3, ptr[rdi], (u8)0xE4));
    GEN(pshufhw(xmm3, xmm4, (u8)0));
    GEN(pshufhw(xmm2, xmm2, (u8)0));
    GEN(pshufhw(xmm3, ptr[rdi], (u8)0));
    GEN(pshufhw(xmm3, xmm4, (u8)0xE4));
    GEN(pshufhw(xmm2, xmm2, (u8)0xE4));
    GEN(pshufhw(xmm3, ptr[rdi], (u8)0xE4));
    GEN(pslldq(xmm2, 5));
    GEN(psrldq(xmm2, 5));
    GEN_SSE_CMP(cmpsd);
    GEN_SSE_CMP(cmppd);

    GEN(shufpd(xmm3, xmm4, (u8)0));
    GEN(shufpd(xmm2, xmm2, (u8)0));
    GEN(shufpd(xmm3, ptr[rdi], (u8)0));
    GEN(shufpd(xmm3, xmm4, (u8)0xE4));
    GEN(shufpd(xmm2, xmm2, (u8)0xE4));
    GEN(shufpd(xmm3, ptr[rdi], (u8)0xE4));

    GEN_SSE(punpcklbw);
    GEN_SSE(punpcklwd);
    GEN_SSE(punpckldq);
    GEN_SSE(punpcklqdq);
    GEN_SSE(punpckhbw);
    GEN_SSE(punpckhwd);
    GEN_SSE(punpckhdq);
    GEN_SSE(punpckhqdq);
    GEN_SSE(pmaddwd);

    GEN(movlpd(ptr[rdi], xmm3));
    GEN(movlpd(xmm3, ptr[rdi]));
    GEN(movhpd(ptr[rdi], xmm3));
    GEN(movhpd(xmm3, ptr[rdi]));
    GEN_SSE_MOV(movsd);
    GEN_SSE_MOV(movapd);
    GEN_SSE_MOV(movupd);
    GEN_SSE_MOV(movdqa);
    GEN_SSE_MOV(movdqu);
    GEN(movmskpd(eax, xmm2));
    GEN(movmskpd(rax, xmm2));

    std::ofstream sse2("counts/SSE2.json");
    sse2 << json.dump(4);
    json.clear();

    GEN_SSE(addsubps);
    GEN_SSE(addsubpd);
    // GEN_SSE(haddps);
    GEN_SSE(haddpd);
    // GEN_SSE(hsubps);
    GEN_SSE(hsubpd);
    GEN_SSE(movshdup);
    GEN_SSE(movsldup);
    GEN_SSE(movddup);

    std::ofstream sse3("counts/SSE3.json");
    sse3 << json.dump(4);
    json.clear();

    // GEN_SSE(pabsb);
    // GEN_SSE(pabsw);
    // GEN_SSE(pabsd);
    GEN_SSE(psignb);
    GEN_SSE(psignw);
    GEN_SSE(psignd);
    GEN_SSE(pshufb);
    GEN(palignr(xmm2, xmm3, 10));
    GEN(palignr(xmm2, xmm3, 16));

    std::ofstream ssse3("counts/SSSE3.json");
    ssse3 << json.dump(4);
    json.clear();

    GEN_SSE(pmulld);
    GEN_SSE(pmuldq);
    GEN(dpps(xmm2, xmm3, 0b11110000));
    // GEN_SSE(dppd);
    GEN(blendpd(xmm2, xmm3, 0b10101010));
    GEN(blendps(xmm2, xmm3, 0b10101010));
    GEN(pblendw(xmm2, xmm3, 0b10101010));
    GEN(blendvpd(xmm2, xmm3));
    GEN(blendvps(xmm2, xmm3));
    GEN(pblendvb(xmm2, xmm3));

    GEN(pminuw(xmm2, xmm3));
    GEN(pminud(xmm2, xmm3));
    GEN(pminsb(xmm2, xmm3));
    GEN(pminsd(xmm2, xmm3));
    GEN(pmaxuw(xmm2, xmm3));
    GEN(pmaxud(xmm2, xmm3));
    GEN(pmaxsb(xmm2, xmm3));
    GEN(pmaxsd(xmm2, xmm3));

    GEN(roundss(xmm2, xmm3, 0b00000011));
    GEN(roundsd(xmm2, xmm3, 0b00000011));
    // GEN(roundps(xmm2, xmm3, 0b00000011));
    // GEN(roundpd(xmm2, xmm3, 0b00000011));

    GEN(pinsrb(xmm2, ptr[rdi], 5));
    GEN(pinsrw(xmm2, ptr[rdi], 4));
    GEN(pinsrd(xmm2, ptr[rdi], 3));
    GEN(pextrb(ptr[rdi], xmm2, 5));
    GEN(pextrw(ptr[rdi], xmm2, 4));
    GEN(pextrd(ptr[rdi], xmm2, 3));

    GEN_SSE(pmovsxbw);
    GEN_SSE(pmovsxbd);
    GEN_SSE(pmovsxbq);
    GEN_SSE(pmovsxwd);
    GEN_SSE(pmovsxwq);
    GEN_SSE(pmovsxdq);
    GEN_SSE(pmovzxbw);
    GEN_SSE(pmovzxbd);
    GEN_SSE(pmovzxbq);
    GEN_SSE(pmovzxwd);
    GEN_SSE(pmovzxwq);
    GEN_SSE(pmovzxdq);

    GEN_SSE(pcmpeqq);
    GEN_SSE(ptest);

    std::ofstream sse4_1("counts/SSE4_1.json");
    sse4_1 << json.dump(4);
    json.clear();

    gen_many(rec, "llvmpipe_shader", json, [](Xbyak::CodeGenerator& x) {
        x.mov(r14, ptr[rsi]);
        x.mov(r14d, ptr[r14 + 0x04]);
        x.movaps(xmm5, ptr[rsp + 0x70]);
        x.movaps(xmm8, ptr[rsp + 0x60]);
        x.movaps(xmm0, ptr[rsp + 0x50]);
        x.movaps(xmm1, ptr[rsp + 0x30]);
        x.movaps(xmm2, ptr[rsp + 0x10]);
        x.movaps(xmm3, ptr[rsp + 0x40]);
        x.movaps(xmm7, ptr[rsp + 0x20]);
        x.movaps(xmm9, ptr[rsp]);
        x.movaps(xmm4, ptr[rdx]);
        x.movdqa(ptr[rsp - 0x80], xmm11);
        x.shufps(xmm5, xmm5, 0x00);
        x.shufps(xmm8, xmm8, 0x00);
        x.shufps(xmm0, xmm0, 0xFF);
        x.shufps(xmm1, xmm1, 0xFF);
        x.shufps(xmm2, xmm2, 0xFF);
        x.movaps(xmm12, xmm7);
        x.movaps(xmm15, xmm7);
        x.movaps(xmm6, xmm7);
        x.addps(xmm5, ptr[rsp + rcx * 1 + 0xA0]);
        x.addps(xmm8, ptr[rsp + rcx * 1 + 0xE0]);
        x.shufps(xmm12, xmm7, 0x00);
        x.shufps(xmm15, xmm7, 0x55);
        x.shufps(xmm6, xmm7, 0xAA);
        x.mulps(xmm0, xmm5);
        x.mulps(xmm1, xmm8);
        x.mulps(xmm12, xmm8);
        x.mulps(xmm15, xmm8);
        x.mulps(xmm6, xmm8);
        x.addps(xmm0, xmm2);
        x.movaps(xmm2, xmm9);
        x.addps(xmm1, xmm0);
        x.movaps(xmm0, xmm3);
        x.shufps(xmm2, xmm9, 0x00);
        x.shufps(xmm0, xmm3, 0x00);
        x.divps(xmm4, xmm1);
        x.movaps(xmm1, xmm9);
        x.mulps(xmm0, xmm5);
        x.shufps(xmm1, xmm9, 0x55);
        x.addps(xmm0, xmm2);
        x.movaps(xmm2, xmm9);
        x.addps(xmm12, xmm0);
        x.movaps(xmm0, xmm3);
        x.shufps(xmm2, xmm9, 0xFF);
        x.shufps(xmm0, xmm3, 0x55);
        x.mulps(xmm0, xmm5);
        x.addps(xmm0, xmm1);
        x.movaps(xmm1, xmm9);
        x.addps(xmm15, xmm0);
        x.movaps(xmm0, xmm3);
        x.shufps(xmm1, xmm9, 0xAA);
        x.shufps(xmm0, xmm3, 0xAA);
        x.mulps(xmm0, xmm5);
        x.addps(xmm0, xmm1);
        x.movaps(xmm1, xmm3);
        x.mulps(xmm12, xmm4);
        x.mulps(xmm15, xmm4);
        x.addps(xmm6, xmm0);
        x.shufps(xmm1, xmm3, 0xFF);
        x.movaps(xmm0, xmm7);
        x.shufps(xmm0, xmm7, 0xFF);
        x.mulps(xmm1, xmm5);
        x.mulps(xmm6, xmm4);
        x.addps(xmm1, xmm2);
        x.mulps(xmm0, xmm8);
        x.addps(xmm0, xmm1);
        x.movd(xmm1, r14d);
        x.pshufd(xmm2, xmm1, 0x00);
        x.mulps(xmm0, xmm4);
        x.movdqa(xmm1, xmm2);
        x.pand(xmm1, xmm11);
        x.pcmpeqd(xmm1, xmm10);
        x.movmskps(r14d, xmm1);
        x.xor_(r14d, 0x0F);
        u8* curr = x.getCurr<u8*>();
        x.jz((void*)(curr + 0xcafe));
    });

    std::ofstream many("counts/HotBlocks.json");
    many << json.dump(4);
    json.clear();
}