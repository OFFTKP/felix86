// TODO: This file is a big mess, refactor
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <nlohmann/json.hpp>
#define XBYAK_NO_EXCEPTION
#include <xbyak/xbyak.h>
#include "Zydis/Disassembler.h"
#include "felix86/common/disassembler.h"
#include "felix86/v2/handlers.hpp"
#include "felix86/v2/recompiler.hpp"
#include "fmt/format.h"

using namespace nlohmann;

using namespace Xbyak::util;

struct Instruction {
    int count;
    std::string disassembly;
    std::vector<std::string> expected_asm;
};

static void to_json(ordered_json& j, const Instruction& p) {
    if (!p.disassembly.empty())
        j = ordered_json{{"instruction_count", p.count}, {"expected_asm", p.expected_asm}, {"disassembly", p.disassembly}};
    else
        j = ordered_json{{"instruction_count", p.count}, {"expected_asm", p.expected_asm}};
}

static void from_json(const ordered_json& j, Instruction& p) {
    j.at("instruction_count").get_to(p.count);
    j.at("expected_asm").get_to(p.expected_asm);
    if (!p.disassembly.empty())
        j.at("disassembly").get_to(p.disassembly);
}

static void gen_many(Recompiler& rec, const std::string& name, nlohmann::ordered_json& json, void (*func)(Xbyak::CodeGenerator&)) {
    rec.setVectorState(SEW::E1024, 0);
    Xbyak::CodeGenerator x;
    auto x86_start = x.getCurr();
    func(x);

    auto bisc = rec.getAssembler().GetCursorPointer();
    rec.compileSequence(false, (u64)x86_start);
    auto after = rec.getAssembler().GetCursorPointer();
    int count = 0;
    Instruction inst;
    for (int i = 0; i < after - bisc;) {
        void* address = bisc + i;
        i += 4;
        u32 data = 0;
        memcpy(&data, address, 4);
        std::string out = riscv_disassemble(data, (u64)address);
        inst.expected_asm.push_back(out);
        count++;
    }

    inst.count = count;
    json[name] = inst;
}

static void gen_sequence(Recompiler& rec, nlohmann::ordered_json& json, const char* name, bool mode32) {
    std::string command = "nasm -f bin counts/sequences/" + std::string(name) + ".asm -o /dev/stdout";
    FILE* pipe = popen(command.c_str(), "r");
    ASSERT_MSG(pipe, "Failed to run nasm for %s", name);
    std::vector<u8> bytes;
    u8 buffer[4096];
    size_t read = 0;
    while ((read = fread(buffer, 1, sizeof(buffer), pipe)) > 0) {
        bytes.insert(bytes.end(), buffer, buffer + read);
    }
    int status = pclose(pipe);
    ASSERT_MSG(status == 0 && !bytes.empty(), "nasm failed for %s", name);

    rec.setVectorState(SEW::E1024, 0);
    auto bisc = rec.getAssembler().GetCursorPointer();
    rec.compileSequence(mode32, (u64)bytes.data());
    auto after = rec.getAssembler().GetCursorPointer();
    int count = 0;
    Instruction inst;
    for (int i = 0; i < after - bisc;) {
        void* address = bisc + i;
        i += 4;
        u32 data = 0;
        memcpy(&data, address, 4);
        std::string out = riscv_disassemble(data, (u64)address);
        inst.expected_asm.push_back(out);
        count++;
    }

    inst.count = count;
    json[name] = inst;
}

static void gen(Recompiler& rec, nlohmann::ordered_json& json, void (*func)(Xbyak::CodeGenerator&), bool flags = false) {
    static bool init = false;
    static ZydisDecoder zydis;
    if (!init) {
        init = true;
        ZydisDecoderInit(&zydis, ZYDIS_MACHINE_MODE_LONG_64, ZYDIS_STACK_WIDTH_64);
        ZydisDecoderEnableMode(&zydis, ZYDIS_DECODER_MODE_AMD_BRANCHES, ZYAN_TRUE);
    }

    // Set bogus vector state so we see the vector state changes
    rec.setVectorState(SEW::E1024, 0);
    rec.setFlagMode(flags ? FlagMode::AlwaysEmit : FlagMode::NeverEmit);
    rec.resetX87();
    rec.v0Modified();

    Xbyak::CodeGenerator x(8192);
    auto x86_start = x.getCurr();
    func(x);
    auto x86_end = x.getCurr();
    auto bisc = rec.getAssembler().GetCursorPointer();
    ZydisDecodedInstruction zinstruction;
    ZydisDecodedOperand zoperands[10];
    u64 rip = (u64)x86_start;
    rec.setCurrentRipregValue(rip);
    rec.decode(rip, zinstruction, zoperands);
    rec.setCurrentMode32(false);
    rec.compileInstruction(zinstruction, zoperands, rip);
    rec.resetScratch();
    rec.flushX87();
    auto after = rec.getAssembler().GetCursorPointer();
    int count = 0;
    Instruction inst;
    std::string bytes;
    for (int i = 0; i < x86_end - x86_start; i++) {
        bytes += fmt::format("{:02x}", x86_start[i]);
    }

    for (int i = 0; i < after - bisc;) {
        void* address = bisc + i;
        i += 4;
        u32 data = 0;
        memcpy(&data, address, 4);
        std::string out = riscv_disassemble(data, (u64)address);
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
    riscv_set_felix86_allocations(true);
    g_config.inline_syscalls = false;
    g_config.scan_ahead_multi = false;
    Extensions::G = true;
    Extensions::Zba = true;
    Extensions::Zbb = true;
    Extensions::Zbs = true;
    Extensions::Zbc = true;
    Extensions::Zvbc = true;
    Extensions::C = true;
    Extensions::V = true;
    Extensions::VLEN = 256;
    Extensions::Zicond = true;
    Extensions::Zvkned = true;
    Handlers::initialize();

    std::unique_ptr<Recompiler> rec_storage = std::make_unique<Recompiler>(true /* relocatable code */);
    Recompiler& rec = *rec_storage;
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

#define GEN_BitTest(name)                                                                                                                            \
    GEN(name(dx, bx));                                                                                                                               \
    GEN(name(edx, ebx));                                                                                                                             \
    GEN(name(rdx, rbx));                                                                                                                             \
    GEN(name(word[rdi], dx));                                                                                                                        \
    GEN(name(dword[rdi], edx));                                                                                                                      \
    GEN(name(qword[rdi], rdx));                                                                                                                      \
    GEN(name(dx, 3));                                                                                                                                \
    GEN(name(edx, 3));                                                                                                                               \
    GEN(name(rdx, 3));                                                                                                                               \
    GEN(name(word[rdi], 3));                                                                                                                         \
    GEN(name(dword[rdi], 3));                                                                                                                        \
    GEN(name(qword[rdi], 3))

#define GEN_BitScan(name)                                                                                                                            \
    GEN(name(ax, bx));                                                                                                                               \
    GEN(name(eax, ebx));                                                                                                                             \
    GEN(name(rax, rbx));                                                                                                                             \
    GEN(name(ax, word[rdi]));                                                                                                                        \
    GEN(name(eax, dword[rdi]));                                                                                                                      \
    GEN(name(rax, qword[rdi]))

#define GEN_DoubleShift(name)                                                                                                                        \
    GEN(name(dx, bx, 5));                                                                                                                            \
    GEN(name(edx, ebx, 5));                                                                                                                          \
    GEN(name(rdx, rbx, 5));                                                                                                                          \
    GEN(name(dx, bx, cl));                                                                                                                           \
    GEN(name(edx, ebx, cl));                                                                                                                         \
    GEN(name(rdx, rbx, cl));                                                                                                                         \
    GEN(name(word[rdi], dx, 5));                                                                                                                     \
    GEN(name(dword[rdi], edx, 5));                                                                                                                   \
    GEN(name(qword[rdi], rdx, 5));                                                                                                                   \
    GEN(name(word[rdi], dx, cl));                                                                                                                    \
    GEN(name(dword[rdi], edx, cl));                                                                                                                  \
    GEN(name(qword[rdi], rdx, cl))

#define GEN_SetCC(name)                                                                                                                              \
    GEN(name(dl));                                                                                                                                   \
    GEN(name(dh));                                                                                                                                   \
    GEN(name(byte[rdi]))

#define GEN_BMI1_2(name)                                                                                                                             \
    GEN(name(eax, ebx));                                                                                                                             \
    GEN(name(rax, rbx));                                                                                                                             \
    GEN(name(eax, dword[rdi]));                                                                                                                      \
    GEN(name(rax, qword[rdi]))

#define GEN_BMI1_3(name)                                                                                                                             \
    GEN(name(eax, ebx, ecx));                                                                                                                        \
    GEN(name(rax, rbx, rcx));                                                                                                                        \
    GEN(name(eax, dword[rdi], ecx));                                                                                                                 \
    GEN(name(rax, qword[rdi], rcx))

#define GEN_BMI2_3(name)                                                                                                                             \
    GEN(name(eax, ebx, ecx));                                                                                                                        \
    GEN(name(rax, rbx, rcx));                                                                                                                        \
    GEN(name(eax, ebx, dword[rdi]));                                                                                                                 \
    GEN(name(rax, rbx, qword[rdi]))

        GEN_Group1(add);
        GEN_Group1(sub);
        GEN_Group1(adc);
        GEN_Group1(sbb);
        GEN_Group1(or_);
        GEN_Group1(and_);
        GEN_Group1(xor_);
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
        GEN_SingleRM(neg);
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
        GEN(cmpxchg(ptr[rdi], bl));
        GEN(cmpxchg(ptr[rdi], bh));
        GEN(cmpxchg(ptr[rdi], bx));
        GEN(cmpxchg(ptr[rdi], ebx));
        GEN(cmpxchg(ptr[rdi], rbx));
        GEN(tzcnt(ax, bx));
        GEN(tzcnt(ax, word[rdi]));
        GEN(tzcnt(eax, ebx));
        GEN(tzcnt(eax, dword[rdi]));
        GEN(tzcnt(rax, rbx));
        GEN(tzcnt(rax, qword[rdi]));
        GEN(lzcnt(ax, bx));
        GEN(lzcnt(ax, word[rdi]));
        GEN(lzcnt(eax, ebx));
        GEN(lzcnt(eax, dword[rdi]));
        GEN(lzcnt(rax, rbx));
        GEN(lzcnt(rax, qword[rdi]));
        GEN_BitScan(bsf);
        GEN_BitScan(bsr);
        GEN_BitScan(popcnt);
        // GEN_BitTest(bt);
        // GEN_BitTest(btc);
        // GEN_BitTest(btr);
        // GEN_BitTest(bts);
        GEN_DoubleShift(shld);
        GEN_DoubleShift(shrd);
        GEN(adcx(eax, ebx));
        GEN(adcx(rax, rbx));
        GEN(adcx(eax, dword[rdi]));
        GEN(adcx(rax, qword[rdi]));
        GEN(adox(eax, ebx));
        GEN(adox(rax, rbx));
        GEN(adox(eax, dword[rdi]));
        GEN(adox(rax, qword[rdi]));
        GEN_BMI1_2(blsi);
        GEN_BMI1_2(blsmsk);
        GEN_BMI1_2(blsr);
        GEN_BMI1_3(bextr);
        GEN_BMI1_3(bzhi);
        GEN_BMI2_3(andn);
        GEN(cmpxchg8b(qword[rdi]));
        GEN(cmc());
        if (flags) {
            GEN_Group1(mov);
            GEN(mov(rax, qword[rdi + 128]));
            GEN(mov(qword[rdi + 128], 1));
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
            GEN(putSeg(fs); x.lea(rax, ptr[(void*)1]));
            GEN(putSeg(fs); x.lea(rax, ptr[rdi]));
            GEN(putSeg(fs); x.lea(rax, ptr[rdi + rsi]));
            GEN(putSeg(fs); x.lea(rax, ptr[2 * rsi]));
            GEN(putSeg(fs); x.lea(rax, ptr[rdi + 2 * rsi]));
            GEN(putSeg(fs); x.lea(rax, ptr[rdi + 4 * rsi]));
            GEN(putSeg(fs); x.lea(rax, ptr[rdi + 8 * rsi]));
            GEN(putSeg(fs); x.lea(rax, ptr[rdi + 8 * rsi + 0x40]));
            GEN(putSeg(fs); x.lea(rax, ptr[rdi + 8 * rsi + 0x12345678]));
            GEN(lea(rax, ptr[(void*)0x12345678]));
            GEN(lea(rax, ptr[rdi]));
            GEN(lea(rax, ptr[rdi + 0x12345678]));
            GEN(lea(rax, ptr[2 * rsi]));
            GEN(lea(rax, ptr[2 * rsi + 0x12345678]));
            GEN(lea(rax, ptr[rdi + 4 * rsi]));
            GEN(lea(rax, ptr[rdi + 4 * rsi + 0x40]));
            GEN(lea(rax, ptr[rdi + 4 * rsi + 0x12345678]));
            GEN(cmovo(rcx, rbx));
            GEN(cmovno(rcx, rbx));
            GEN(cmovb(rcx, rbx));
            GEN(cmovnb(rcx, rbx));
            GEN(cmovz(rcx, rbx));
            GEN(cmovnz(rcx, rbx));
            GEN(cmovbe(rcx, rbx));
            GEN(cmovnbe(rcx, rbx));
            GEN(cmovp(rcx, rbx));
            GEN(cmovnp(rcx, rbx));
            GEN(cmovs(rcx, rbx));
            GEN(cmovns(rcx, rbx));
            GEN(cmovl(rcx, rbx));
            GEN(cmovnl(rcx, rbx));
            GEN(cmovle(rcx, rbx));
            GEN(cmovnle(rcx, rbx));
            GEN(cmovo(ecx, ebx));
            GEN(cmovno(ecx, ebx));
            GEN(cmovb(ecx, ebx));
            GEN(cmovnb(ecx, ebx));
            GEN(cmovz(ecx, ebx));
            GEN(cmovnz(ecx, ebx));
            GEN(cmovbe(ecx, ebx));
            GEN(cmovnbe(ecx, ebx));
            GEN(cmovp(ecx, ebx));
            GEN(cmovnp(ecx, ebx));
            GEN(cmovs(ecx, ebx));
            GEN(cmovns(ecx, ebx));
            GEN(cmovl(ecx, ebx));
            GEN(cmovnl(ecx, ebx));
            GEN(cmovle(ecx, ebx));
            GEN(cmovnle(ecx, ebx));
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
            GEN(push(rsp));
            GEN(push(qword[rsp + 32]));
            GEN(pop(qword[rsp + 32]));
            GEN(pop(rsp));
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
            GEN_Group1(cmp);
            GEN(test(al, bl));
            GEN(test(al, bh));
            GEN(test(ah, bl));
            GEN(test(ah, bh));
            GEN(test(ax, bx));
            GEN(test(eax, ebx));
            GEN(test(rax, rbx));
            GEN(test(ptr[rdi], bl));
            GEN(test(ptr[rdi], bh));
            GEN(test(ptr[rdi], bx));
            GEN(test(ptr[rdi], ebx));
            GEN(test(ptr[rdi], rbx));
            GEN(test(byte[rdi], -1));
            GEN(test(word[rdi], -1));
            GEN(test(dword[rdi], -1));
            GEN(test(qword[rdi], -1));
            GEN(test(byte[rdi], 0));
            GEN(test(word[rdi], 0));
            GEN(test(dword[rdi], 0));
            GEN(test(qword[rdi], 0));
            GEN(xchg(al, bl));
            GEN(xchg(al, bh));
            GEN(xchg(ah, bl));
            GEN(xchg(ah, bh));
            GEN(xchg(ax, bx));
            GEN(xchg(eax, ebx));
            GEN(xchg(rax, rbx));
            GEN(xchg(ptr[rdi], bl));
            GEN(xchg(ptr[rdi], bh));
            GEN(xchg(ptr[rdi], bx));
            GEN(xchg(ptr[rdi], ebx));
            GEN(xchg(ptr[rdi], rbx));
            GEN(movzx(ax, bl));
            GEN(movzx(ax, bh));
            GEN(movzx(eax, bl));
            GEN(movzx(eax, bh));
            GEN(movzx(eax, bx));
            GEN(movzx(rax, bl));
            GEN(movzx(rax, bx));
            GEN(movzx(ax, byte[rdi]));
            GEN(movzx(eax, byte[rdi]));
            GEN(movzx(eax, word[rdi]));
            GEN(movzx(rax, byte[rdi]));
            GEN(movzx(rax, word[rdi]));
            GEN(movsx(ax, bl));
            GEN(movsx(ax, bh));
            GEN(movsx(eax, bl));
            GEN(movsx(eax, bh));
            GEN(movsx(eax, bx));
            GEN(movsx(rax, bl));
            GEN(movsx(rax, bx));
            GEN(movsx(ax, byte[rdi]));
            GEN(movsx(eax, byte[rdi]));
            GEN(movsx(eax, word[rdi]));
            GEN(movsx(rax, byte[rdi]));
            GEN(movsx(rax, word[rdi]));
            GEN(leave());
            GEN(bswap(ecx));
            GEN(bswap(rcx));
            GEN_SingleRM(not_);
            GEN_SetCC(seto);
            GEN_SetCC(setno);
            GEN_SetCC(setb);
            GEN_SetCC(setnb);
            GEN_SetCC(setz);
            GEN_SetCC(setnz);
            GEN_SetCC(setbe);
            GEN_SetCC(setnbe);
            GEN_SetCC(sets);
            GEN_SetCC(setns);
            GEN_SetCC(setp);
            GEN_SetCC(setnp);
            GEN_SetCC(setl);
            GEN_SetCC(setnl);
            GEN_SetCC(setle);
            GEN_SetCC(setnle);
            GEN(movsxd(rax, ebx));
            GEN(movsxd(rax, dword[rdi]));
            GEN(movbe(ax, word[rdi]));
            GEN(movbe(eax, dword[rdi]));
            GEN(movbe(rax, qword[rdi]));
            GEN(movbe(word[rdi], ax));
            GEN(movbe(dword[rdi], eax));
            GEN(movbe(qword[rdi], rax));
            GEN(xlatb());
            GEN(enter(16, 0));
            GEN(cpuid());
            GEN(rdtsc());
            GEN(rdtscp());
            GEN(rdfsbase(eax));
            GEN(rdfsbase(rax));
            GEN(rdgsbase(eax));
            GEN(rdgsbase(rax));
            GEN(wrfsbase(eax));
            GEN(wrfsbase(rax));
            GEN(wrgsbase(eax));
            GEN(wrgsbase(rax));
            GEN(clflush(ptr[rdi]));
            GEN_BMI2_3(mulx);
            GEN_BMI2_3(pdep);
            GEN_BMI2_3(pext);
            GEN_BMI1_3(sarx);
            GEN_BMI1_3(shlx);
            GEN_BMI1_3(shrx);
            GEN(rorx(eax, ebx, 5));
            GEN(rorx(rax, rbx, 5));
            GEN(rorx(eax, dword[rdi], 5));
            GEN(rorx(rax, qword[rdi], 5));
        }

        std::ofstream base("counts/" + name);
        base << json.dump(4);
        json.clear();
    }

    {
        bool flags = false;
        GEN(lock(); x.add(ptr[rdi], ah));
        GEN(lock(); x.add(ptr[rdi], ax));
        GEN(lock(); x.add(ptr[rdi], eax));
        GEN(lock(); x.add(ptr[rdi], rax));
        GEN(lock(); x.xadd(ptr[rdi], eax));
        GEN(lock(); x.xadd(ptr[rdi], rax));
        GEN(lock(); x.or_(ptr[rdi], ah));
        GEN(lock(); x.or_(ptr[rdi], ax));
        GEN(lock(); x.or_(ptr[rdi], eax));
        GEN(lock(); x.or_(ptr[rdi], rax));
        GEN(lock(); x.and_(ptr[rdi], ah));
        GEN(lock(); x.and_(ptr[rdi], ax));
        GEN(lock(); x.and_(ptr[rdi], eax));
        GEN(lock(); x.and_(ptr[rdi], rax));
        GEN(lock(); x.xor_(ptr[rdi], ah));
        GEN(lock(); x.xor_(ptr[rdi], ax));
        GEN(lock(); x.xor_(ptr[rdi], eax));
        GEN(lock(); x.xor_(ptr[rdi], rax));
        GEN(lock(); x.xchg(ptr[rdi], ah));
        GEN(lock(); x.xchg(ptr[rdi], ax));
        GEN(lock(); x.xchg(ptr[rdi], eax));
        GEN(lock(); x.xchg(ptr[rdi], rax));
        GEN(lock(); x.cmpxchg(ptr[rdi], ah));
        GEN(lock(); x.cmpxchg(ptr[rdi], ax));
        GEN(lock(); x.cmpxchg(ptr[rdi], eax));
        GEN(lock(); x.cmpxchg(ptr[rdi], rax));

        GEN(lock(); x.adc(ptr[rdi], ah));
        GEN(lock(); x.adc(ptr[rdi], ax));
        GEN(lock(); x.adc(ptr[rdi], eax));
        GEN(lock(); x.adc(ptr[rdi], rax));
        GEN(lock(); x.sbb(ptr[rdi], ah));
        GEN(lock(); x.sbb(ptr[rdi], ax));
        GEN(lock(); x.sbb(ptr[rdi], eax));
        GEN(lock(); x.sbb(ptr[rdi], rax));
        GEN(lock(); x.sub(ptr[rdi], ah));
        GEN(lock(); x.sub(ptr[rdi], ax));
        GEN(lock(); x.sub(ptr[rdi], eax));
        GEN(lock(); x.sub(ptr[rdi], rax));
        GEN(lock(); x.xadd(ptr[rdi], ah));
        GEN(lock(); x.xadd(ptr[rdi], ax));
        GEN(lock(); x.inc(byte[rdi]));
        GEN(lock(); x.inc(word[rdi]));
        GEN(lock(); x.inc(dword[rdi]));
        GEN(lock(); x.inc(qword[rdi]));
        GEN(lock(); x.dec(byte[rdi]));
        GEN(lock(); x.dec(word[rdi]));
        GEN(lock(); x.dec(dword[rdi]));
        GEN(lock(); x.dec(qword[rdi]));
        GEN(lock(); x.neg(byte[rdi]));
        GEN(lock(); x.neg(word[rdi]));
        GEN(lock(); x.neg(dword[rdi]));
        GEN(lock(); x.neg(qword[rdi]));
        GEN(lock(); x.not_(byte[rdi]));
        GEN(lock(); x.not_(word[rdi]));
        GEN(lock(); x.not_(dword[rdi]));
        GEN(lock(); x.not_(qword[rdi]));
        GEN(lock(); x.cmpxchg8b(qword[rdi]));

        std::ofstream base("counts/Lock.json");
        base << json.dump(4);
        json.clear();
    }

#undef GEN

#define GEN(inst) gen(rec, json, [](Xbyak::CodeGenerator& x) { x.inst; }, true)
#define GEN_SSE(name)                                                                                                                                \
    GEN(name(xmm3, xmm4));                                                                                                                           \
    GEN(name(xmm2, xmm2));                                                                                                                           \
    GEN(name(xmm1, ptr[rdi]))

#define GEN_AVX(name)                                                                                                                                \
    GEN(name(xmm3, xmm4, xmm5));                                                                                                                     \
    GEN(name(xmm1, xmm2, ptr[rdi]));                                                                                                                 \
    GEN(name(ymm3, ymm4, ymm5));                                                                                                                     \
    GEN(name(ymm1, ymm2, ptr[rdi]))

#define GEN_AVX_YMM4(name)                                                                                                                           \
    GEN(name(xmm3, xmm4, xmm5, xmm0));                                                                                                               \
    GEN(name(xmm1, xmm2, ptr[rdi], xmm0));                                                                                                           \
    GEN(name(ymm3, ymm4, ymm5, ymm0));                                                                                                               \
    GEN(name(ymm1, ymm2, ptr[rdi], ymm0))

#define GEN_AVX_XMM3(name)                                                                                                                           \
    GEN(name(xmm3, xmm4, xmm5));                                                                                                                     \
    GEN(name(xmm3, xmm2, xmm2));                                                                                                                     \
    GEN(name(xmm1, xmm2, ptr[rdi]))

#define GEN_AVX_XMM2(name)                                                                                                                           \
    GEN(name(xmm3, xmm4));                                                                                                                           \
    GEN(name(xmm1, ptr[rdi]))

#define GEN_AVX_YMM2(name)                                                                                                                           \
    GEN(name(ymm3, ymm4));                                                                                                                           \
    GEN(name(ymm1, ptr[rdi]));                                                                                                                       \
    GEN(name(xmm3, xmm4));                                                                                                                           \
    GEN(name(xmm1, ptr[rdi]))

#define GEN_AVX_YMM2_ONLY(name)                                                                                                                      \
    GEN(name(ymm3, ymm4));                                                                                                                           \
    GEN(name(ymm1, ptr[rdi]))

#define GEN_AVX_YMM3_ONLY(name)                                                                                                                      \
    GEN(name(ymm3, ymm4, ymm5));                                                                                                                     \
    GEN(name(ymm1, ymm2, ptr[rdi]))

#define GEN_AVX_MOV(name)                                                                                                                            \
    GEN(name(xmm3, xmm4));                                                                                                                           \
    GEN(name(xmm1, ptr[rdi]));                                                                                                                       \
    GEN(name(ptr[rdi], xmm1));                                                                                                                       \
    GEN(name(ymm3, ymm4));                                                                                                                           \
    GEN(name(ymm1, ptr[rdi]));                                                                                                                       \
    GEN(name(ptr[rdi], ymm1))

#define GEN_AVX_IMM(name)                                                                                                                            \
    GEN(name(xmm3, xmm4, xmm5, 0b10101010));                                                                                                         \
    GEN(name(xmm1, xmm2, ptr[rdi], 0b10101010));                                                                                                     \
    GEN(name(ymm3, ymm4, ymm5, 0b10101010));                                                                                                         \
    GEN(name(ymm1, ymm2, ptr[rdi], 0b10101010))

#define GEN_AVX_XMM3_IMM(name)                                                                                                                       \
    GEN(name(xmm3, xmm4, xmm5, 0b10101010));                                                                                                         \
    GEN(name(xmm1, xmm2, ptr[rdi], 0b10101010))

#define GEN_AVX_XMM2_IMM(name)                                                                                                                       \
    GEN(name(xmm3, xmm4, 0b10101010));                                                                                                               \
    GEN(name(xmm1, ptr[rdi], 0b10101010))

#define GEN_AVX_YMM2_IMM(name)                                                                                                                       \
    GEN(name(ymm3, ymm4, 0b10101010));                                                                                                               \
    GEN(name(ymm1, ptr[rdi], 0b10101010));                                                                                                           \
    GEN(name(xmm3, xmm4, 0b10101010));                                                                                                               \
    GEN(name(xmm1, ptr[rdi], 0b10101010))

#define GEN_AVX_YMM3_IMM(name)                                                                                                                       \
    GEN(name(ymm3, ymm4, ymm5, 0b10101010));                                                                                                         \
    GEN(name(ymm1, ymm2, ptr[rdi], 0b10101010));                                                                                                     \
    GEN(name(xmm3, xmm4, xmm5, 0b10101010));                                                                                                         \
    GEN(name(xmm1, xmm2, ptr[rdi], 0b10101010))

#define GEN_AVX_YMM2_ONLY_IMM(name)                                                                                                                  \
    GEN(name(ymm3, ymm4, 0b10101010));                                                                                                               \
    GEN(name(ymm1, ptr[rdi], 0b10101010))

#define GEN_AVX_CMP(name)                                                                                                                            \
    GEN(name(xmm3, xmm4, xmm5, 0));                                                                                                                  \
    GEN(name(xmm3, xmm4, xmm5, 7));                                                                                                                  \
    GEN(name(xmm1, xmm2, ptr[rdi], 0));                                                                                                              \
    GEN(name(xmm1, xmm2, ptr[rdi], 7));                                                                                                              \
    GEN(name(ymm3, ymm4, ymm5, 0));                                                                                                                  \
    GEN(name(ymm3, ymm4, ymm5, 7));                                                                                                                  \
    GEN(name(ymm1, ymm2, ptr[rdi], 0));                                                                                                              \
    GEN(name(ymm1, ymm2, ptr[rdi], 7))

#define GEN_AVX_CMP_XMM(name)                                                                                                                        \
    GEN(name(xmm3, xmm4, xmm5, 0));                                                                                                                  \
    GEN(name(xmm3, xmm4, xmm5, 7));                                                                                                                  \
    GEN(name(xmm1, xmm2, ptr[rdi], 0));                                                                                                              \
    GEN(name(xmm1, xmm2, ptr[rdi], 7))

#define GEN_MMX(name)                                                                                                                                \
    GEN(name(mm3, mm4));                                                                                                                             \
    GEN(name(mm2, mm2));                                                                                                                             \
    GEN(name(mm1, ptr[rdi]))

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
    GEN(cvtss2si(rax, qword[rdi]));
    GEN(cvttss2si(rax, xmm3));
    GEN(cvttss2si(eax, xmm2));
    GEN(cvttss2si(eax, dword[rdi]));
    GEN(cvttss2si(rax, qword[rdi]));

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

    GEN(pshufw(mm2, mm3, 0b10101010));
    GEN(pshufw(mm1, ptr[rdi], 0b10101010));
    GEN(cvtpi2ps(xmm2, mm3));
    GEN(cvtpi2ps(xmm1, ptr[rdi]));
    GEN(cvtps2pi(mm2, xmm3));
    GEN(cvtps2pi(mm1, ptr[rdi]));
    GEN(cvttps2pi(mm2, xmm3));
    GEN(cvttps2pi(mm1, ptr[rdi]));
    GEN(maskmovq(mm2, mm3));
    GEN(movntq(ptr[rdi], mm1));
    GEN(movntps(ptr[rdi], xmm1));
    GEN(ldmxcsr(ptr[rdi]));
    GEN(stmxcsr(ptr[rdi]));
    GEN(emms());

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

    GEN(cvtss2sd(xmm3, xmm2));
    GEN(cvtss2sd(xmm2, xmm2));
    GEN(cvtss2sd(xmm3, ptr[rdi]));
    GEN(cvtsd2ss(xmm3, xmm2));
    GEN(cvtsd2ss(xmm2, xmm2));
    GEN(cvtsd2ss(xmm3, ptr[rdi]));
    GEN(cvtsi2sd(xmm3, rax));
    GEN(cvtsi2sd(xmm2, eax));
    GEN(cvtsi2sd(xmm1, dword[rdi]));
    GEN(cvtsi2sd(xmm1, qword[rdi]));
    GEN(cvtsd2si(rax, xmm3));
    GEN(cvtsd2si(eax, xmm2));
    GEN(cvtsd2si(eax, dword[rdi]));
    GEN(cvtsd2si(rax, qword[rdi]));
    GEN(cvttsd2si(rax, xmm3));
    GEN(cvttsd2si(eax, xmm2));
    GEN(cvttsd2si(eax, dword[rdi]));
    GEN(cvttsd2si(rax, qword[rdi]));
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

    GEN_SSE(packuswb);
    GEN_SSE(packusdw);
    GEN_SSE(packsswb);
    GEN_SSE(packssdw);

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

    GEN_SSE(pmuludq);

    GEN_SSE(paddb);
    GEN_SSE(paddw);
    GEN_SSE(paddd);
    GEN_SSE(paddq);
    GEN_SSE(paddsb);
    GEN_SSE(paddsw);
    GEN_SSE(paddusb);
    GEN_SSE(paddusw);
    GEN_SSE(psubb);
    GEN_SSE(psubw);
    GEN_SSE(psubd);
    GEN_SSE(psubq);
    GEN_SSE(psubsb);
    GEN_SSE(psubsw);
    GEN_SSE(psubusb);
    GEN_SSE(psubusw);
    GEN_SSE(pand);
    GEN_SSE(pandn);
    GEN_SSE(por);
    GEN_SSE(pxor);
    GEN_SSE(pcmpgtb);
    GEN_SSE(pcmpgtw);
    GEN_SSE(pcmpgtd);
    GEN_SSE(pmullw);
    GEN_SSE(pmulhw);
    GEN_MMX(paddb);
    GEN_MMX(paddw);
    GEN_MMX(paddd);
    GEN_MMX(paddq);
    GEN_MMX(paddsb);
    GEN_MMX(paddsw);
    GEN_MMX(paddusb);
    GEN_MMX(paddusw);
    GEN_MMX(psubb);
    GEN_MMX(psubw);
    GEN_MMX(psubd);
    GEN_MMX(psubq);
    GEN_MMX(psubsb);
    GEN_MMX(psubsw);
    GEN_MMX(psubusb);
    GEN_MMX(psubusw);
    GEN_MMX(pand);
    GEN_MMX(pandn);
    GEN_MMX(por);
    GEN_MMX(pxor);
    GEN_MMX(pcmpgtb);
    GEN_MMX(pcmpgtw);
    GEN_MMX(pcmpgtd);
    GEN_MMX(pcmpeqb);
    GEN_MMX(pcmpeqw);
    GEN_MMX(pcmpeqd);
    GEN_MMX(pmullw);
    GEN_MMX(pmulhw);
    GEN_MMX(pmulhuw);
    GEN_MMX(pmaddwd);
    GEN_MMX(pmuludq);
    GEN_MMX(psadbw);
    GEN_MMX(pavgb);
    GEN_MMX(pavgw);
    GEN_MMX(pmaxub);
    GEN_MMX(pmaxsw);
    GEN_MMX(pminub);
    GEN_MMX(pminsw);
    GEN_MMX(packuswb);
    GEN_MMX(packsswb);
    GEN_MMX(packssdw);
    GEN_MMX(punpcklbw);
    GEN_MMX(punpcklwd);
    GEN_MMX(punpckldq);
    GEN_MMX(punpckhbw);
    GEN_MMX(punpckhwd);
    GEN_MMX(punpckhdq);

#define GEN_SSE_SHIFT(name)                                                                                                                          \
    GEN(name(xmm2, xmm3));                                                                                                                           \
    GEN(name(xmm1, ptr[rdi]));                                                                                                                       \
    GEN(name(xmm2, 5));                                                                                                                              \
    GEN(name(mm2, mm3));                                                                                                                             \
    GEN(name(mm1, ptr[rdi]));                                                                                                                        \
    GEN(name(mm2, 5))

    GEN_SSE_SHIFT(psllw);
    GEN_SSE_SHIFT(pslld);
    GEN_SSE_SHIFT(psllq);
    GEN_SSE_SHIFT(psrlw);
    GEN_SSE_SHIFT(psrld);
    GEN_SSE_SHIFT(psrlq);
    GEN_SSE_SHIFT(psraw);
    GEN_SSE_SHIFT(psrad);

    GEN(pmovmskb(eax, mm2));

    GEN(movd(xmm1, eax));
    GEN(movd(eax, xmm1));
    GEN(movd(xmm1, ptr[rdi]));
    GEN(movd(ptr[rdi], xmm1));
    GEN(movd(mm1, eax));
    GEN(movd(eax, mm1));
    GEN(movd(mm1, ptr[rdi]));
    GEN(movd(ptr[rdi], mm1));
    GEN(movq(xmm1, rax));
    GEN(movq(rax, xmm1));
    GEN(movq(xmm1, xmm2));
    GEN(movq(xmm1, ptr[rdi]));
    GEN(movq(ptr[rdi], xmm1));
    GEN(movq(mm1, rax));
    GEN(movq(rax, mm1));
    GEN(movq(mm1, mm2));
    GEN(movq(mm1, ptr[rdi]));
    GEN(movq(ptr[rdi], mm1));
    GEN(movq2dq(xmm1, mm2));
    GEN(movdq2q(mm1, xmm2));

    GEN(cvtdq2pd(xmm2, xmm3));
    GEN(cvtdq2pd(xmm1, ptr[rdi]));
    GEN(cvtdq2ps(xmm2, xmm3));
    GEN(cvtdq2ps(xmm1, ptr[rdi]));
    GEN(cvtpi2pd(xmm2, mm3));
    GEN(cvtpi2pd(xmm1, ptr[rdi]));
    GEN(cvtpd2pi(mm2, xmm3));
    GEN(cvtpd2pi(mm1, ptr[rdi]));
    GEN(cvttpd2pi(mm2, xmm3));
    GEN(cvttpd2pi(mm1, ptr[rdi]));

    GEN(maskmovdqu(xmm2, xmm3));
    GEN(movntdq(ptr[rdi], xmm1));
    GEN(movntpd(ptr[rdi], xmm1));
    GEN(movnti(ptr[rdi], eax));
    GEN(movnti(ptr[rdi], rax));

    std::ofstream sse2("counts/SSE2.json");
    sse2 << json.dump(4);
    json.clear();

    GEN_SSE(addsubps);
    GEN_SSE(addsubpd);
    GEN_SSE(haddps);
    GEN_SSE(haddpd);
    GEN_SSE(hsubps);
    GEN_SSE(hsubpd);
    GEN_SSE(movshdup);
    GEN_SSE(movsldup);
    GEN_SSE(movddup);
    GEN(lddqu(xmm3, ptr[rdi]));

    std::ofstream sse3("counts/SSE3.json");
    sse3 << json.dump(4);
    json.clear();

    GEN_SSE(pabsb);
    GEN_SSE(pabsw);
    GEN_SSE(pabsd);
    GEN_SSE(psignb);
    GEN_SSE(psignw);
    GEN_SSE(psignd);
    GEN_SSE(pshufb);
    GEN_SSE(pmulhrsw);
    GEN_SSE(pmaddubsw);
    GEN_SSE(phsubw);
    GEN_SSE(phsubd);
    GEN_SSE(phsubsw);
    GEN_SSE(phaddw);
    GEN_SSE(phaddd);
    GEN_SSE(phaddsw);
    GEN(palignr(xmm2, xmm3, 10));
    GEN(palignr(xmm2, xmm3, 16));

    GEN_MMX(pabsb);
    GEN_MMX(pabsw);
    GEN_MMX(pabsd);
    GEN_MMX(psignb);
    GEN_MMX(psignw);
    GEN_MMX(psignd);
    GEN_MMX(pshufb);
    GEN_MMX(pmulhrsw);
    GEN_MMX(pmaddubsw);
    GEN_MMX(phsubw);
    GEN_MMX(phsubd);
    GEN_MMX(phsubsw);
    GEN_MMX(phaddw);
    GEN_MMX(phaddd);
    GEN_MMX(phaddsw);
    GEN(palignr(mm2, mm3, 5));
    GEN(palignr(mm2, mm3, 16));

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
    GEN(pinsrb(xmm2, eax, 5));
    GEN(pinsrw(xmm2, eax, 4));
    GEN(pinsrd(xmm2, eax, 3));
    GEN(pinsrb(xmm2, ptr[rdi], 0));
    GEN(pinsrw(xmm2, ptr[rdi], 0));
    GEN(pinsrd(xmm2, ptr[rdi], 0));
    GEN(pinsrb(xmm2, eax, 0));
    GEN(pinsrw(xmm2, eax, 0));
    GEN(pinsrd(xmm2, eax, 0));
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

    GEN_SSE(phminposuw);

    GEN_SSE(pcmpeqq);
    GEN_SSE(ptest);

    GEN(mpsadbw(xmm0, ptr[rdi], 0b111));

    GEN(dppd(xmm2, xmm3, 0b00110011));
    GEN(dppd(xmm1, ptr[rdi], 0b00110011));
    GEN(roundps(xmm2, xmm3, 0b00000011));
    GEN(roundps(xmm1, ptr[rdi], 0b00000011));
    GEN(roundpd(xmm2, xmm3, 0b00000011));
    GEN(roundpd(xmm1, ptr[rdi], 0b00000011));
    GEN(insertps(xmm2, xmm3, 0b00010010));
    GEN(insertps(xmm1, ptr[rdi], 0b00010010));
    GEN(extractps(eax, xmm2, 2));
    GEN(extractps(ptr[rdi], xmm2, 2));
    GEN(pinsrq(xmm2, rax, 1));
    GEN(pinsrq(xmm2, ptr[rdi], 1));
    GEN(pinsrq(xmm2, rax, 0));
    GEN(pextrb(eax, xmm2, 5));
    GEN(pextrw(eax, xmm2, 4));
    GEN(pextrd(eax, xmm2, 3));
    GEN(pextrq(rax, xmm2, 1));
    GEN(pextrq(ptr[rdi], xmm2, 1));
    GEN(movntdqa(xmm1, ptr[rdi]));

    std::ofstream sse4_1("counts/SSE4_1.json");
    sse4_1 << json.dump(4);
    json.clear();

    GEN_SSE(pcmpgtq);
    GEN(pcmpistri(xmm2, xmm3, 0b00000000));
    GEN(pcmpistri(xmm2, xmm3, 0b01000101));
    GEN(pcmpistri(xmm1, ptr[rdi], 0b00000000));
    GEN(pcmpistrm(xmm2, xmm3, 0b00000000));
    GEN(pcmpistrm(xmm2, xmm3, 0b01000101));
    GEN(pcmpistrm(xmm1, ptr[rdi], 0b00000000));
    GEN(pcmpestri(xmm2, xmm3, 0b00000000));
    GEN(pcmpestri(xmm2, xmm3, 0b01000101));
    GEN(pcmpestri(xmm1, ptr[rdi], 0b00000000));
    GEN(pcmpestrm(xmm2, xmm3, 0b00000000));
    GEN(pcmpestrm(xmm2, xmm3, 0b01000101));
    GEN(pcmpestrm(xmm1, ptr[rdi], 0b00000000));
    GEN(crc32(eax, bl));
    GEN(crc32(eax, bx));
    GEN(crc32(eax, ebx));
    GEN(crc32(rax, rbx));
    GEN(crc32(eax, byte[rdi]));
    GEN(crc32(eax, word[rdi]));
    GEN(crc32(eax, dword[rdi]));
    GEN(crc32(rax, qword[rdi]));

    std::ofstream sse4_2("counts/SSE4_2.json");
    sse4_2 << json.dump(4);
    json.clear();

    GEN_SSE(aesenc);
    GEN_SSE(aesenclast);
    GEN_SSE(aesdec);
    GEN_SSE(aesdeclast);
    GEN_SSE(aesimc);
    GEN(aeskeygenassist(xmm2, xmm3, 0b10101010));
    GEN(aeskeygenassist(xmm1, ptr[rdi], 0b10101010));
    GEN(pclmulqdq(xmm2, xmm3, 0x00));
    GEN(pclmulqdq(xmm2, xmm3, 0x01));
    GEN(pclmulqdq(xmm2, xmm3, 0x10));
    GEN(pclmulqdq(xmm2, xmm3, 0x11));
    GEN(pclmulqdq(xmm1, ptr[rdi], 0x00));

    std::ofstream aes("counts/AES.json");
    aes << json.dump(4);
    json.clear();

    GEN_AVX_XMM3(vaddss);
    GEN_AVX_XMM3(vsubss);
    GEN_AVX_XMM3(vmulss);
    GEN_AVX_XMM3(vdivss);
    GEN_AVX_XMM3(vsqrtss);
    GEN_AVX_XMM3(vaddsd);
    GEN_AVX_XMM3(vsubsd);
    GEN_AVX_XMM3(vmulsd);
    GEN_AVX_XMM3(vdivsd);
    GEN_AVX_XMM3(vsqrtsd);
    GEN_AVX(vaddps);
    GEN_AVX(vsubps);
    GEN_AVX(vmulps);
    GEN_AVX(vdivps);
    GEN_AVX_YMM2(vsqrtps);
    GEN_AVX(vaddpd);
    GEN_AVX(vsubpd);
    GEN_AVX(vmulpd);
    GEN_AVX(vdivpd);
    GEN_AVX_YMM2(vsqrtpd);
    GEN_AVX(vpmullw);
    GEN_AVX(vpmulhw);
    GEN_AVX(vpmulhuw);
    GEN_AVX(vpmulld);
    GEN_AVX(vpmuldq);
    GEN_AVX(vpmuludq);
    GEN_AVX(vpmulhrsw);
    GEN_AVX(vandps);
    GEN_AVX(vandpd);
    GEN_AVX(vpand);
    GEN_AVX(vandnps);
    GEN_AVX(vandnpd);
    GEN_AVX(vpandn);
    GEN_AVX(vorps);
    GEN_AVX(vorpd);
    GEN_AVX(vpor);
    GEN_AVX(vxorps);
    GEN_AVX(vxorpd);
    GEN_AVX(vpxor);
    GEN_AVX(vpaddb);
    GEN_AVX(vpaddw);
    GEN_AVX(vpaddd);
    GEN_AVX(vpaddq);
    GEN_AVX(vpaddsb);
    GEN_AVX(vpaddsw);
    GEN_AVX(vpaddusb);
    GEN_AVX(vpaddusw);
    GEN_AVX(vpavgb);
    GEN_AVX(vpavgw);
    GEN_AVX_YMM2_IMM(vpshuflw);
    GEN_AVX_YMM2_IMM(vpshufhw);
    GEN_AVX(vpacksswb);
    GEN_AVX(vpackssdw);
    GEN_AVX(vpackuswb);
    GEN_AVX(vpackusdw);
    GEN_AVX(vpshufb);
    GEN_AVX(vpsubb);
    GEN_AVX(vpsubw);
    GEN_AVX(vpsubd);
    GEN_AVX(vpsubq);
    GEN_AVX(vpsubsb);
    GEN_AVX(vpsubsw);
    GEN_AVX(vpsubusb);
    GEN_AVX(vpsubusw);

    GEN(vpmovmskb(eax, xmm2));
    GEN(vpmovmskb(rax, xmm2));
    GEN(vpmovmskb(eax, ymm2));
    GEN(vpmovmskb(rax, ymm2));

    GEN_AVX(vpminsb);
    GEN_AVX(vpminsw);
    GEN_AVX(vpminsd);
    GEN_AVX(vpminub);
    GEN_AVX(vpminuw);
    GEN_AVX(vpminud);
    GEN_AVX(vpmaxsb);
    GEN_AVX(vpmaxsw);
    GEN_AVX(vpmaxsd);
    GEN_AVX(vpmaxub);
    GEN_AVX(vpmaxuw);
    GEN_AVX(vpmaxud);
    GEN_AVX(vmaxps);
    GEN_AVX(vmaxpd);
    GEN_AVX(vminps);
    GEN_AVX(vminpd);
    GEN_AVX_XMM3(vmaxss);
    GEN_AVX_XMM3(vmaxsd);
    GEN_AVX_XMM3(vminss);
    GEN_AVX_XMM3(vminsd);
    GEN_AVX_YMM2(vpabsb);
    GEN_AVX_YMM2(vpabsw);
    GEN_AVX_YMM2(vpabsd);
    GEN_AVX_XMM3(vrcpss);
    GEN_AVX_YMM2(vrcpps);
    GEN_AVX_XMM3(vrsqrtss);
    GEN_AVX_YMM2(vrsqrtps);
    GEN_AVX(vpcmpeqb);
    GEN_AVX(vpcmpeqw);
    GEN_AVX(vpcmpeqd);
    GEN_AVX(vpcmpeqq);
    GEN_AVX(vpcmpgtb);
    GEN_AVX(vpcmpgtw);
    GEN_AVX(vpcmpgtd);
    GEN_AVX(vpcmpgtq);

    GEN_AVX(vpsrlw);
    GEN_AVX(vpsrld);
    GEN_AVX(vpsrlq);
    GEN_AVX(vpsllw);
    GEN_AVX(vpslld);
    GEN_AVX(vpsllq);
    GEN(vpsrlw(ymm2, ymm3, 5));
    GEN(vpsrlw(xmm2, xmm3, 5));
    GEN(vpsrld(ymm2, ymm3, 5));
    GEN(vpsrld(xmm2, xmm3, 5));
    GEN(vpsrlq(ymm2, ymm3, 5));
    GEN(vpsrlq(xmm2, xmm3, 5));
    GEN(vpslldq(xmm2, xmm3, 5));
    GEN(vpsrldq(xmm2, xmm3, 5));
    GEN(vpslldq(ymm2, ymm3, 5));
    GEN(vpsrldq(ymm2, ymm3, 5));
    GEN_AVX(vpsllvd);
    GEN_AVX(vpsllvq);
    GEN_AVX(vpsrlvd);
    GEN_AVX(vpsrlvq);
    GEN_AVX(vpsravd);
    GEN_AVX(vpsraw);
    GEN_AVX(vpsrad);
    GEN(vpsraw(ymm2, ymm3, 5));
    GEN(vpsraw(xmm2, xmm3, 5));
    GEN(vpsrad(ymm2, ymm3, 5));
    GEN(vpsrad(xmm2, xmm3, 5));

    GEN_AVX_IMM(vblendps);
    GEN_AVX_IMM(vblendpd);
    GEN_AVX_YMM4(vblendvps);
    GEN_AVX_YMM4(vblendvpd);
    GEN_AVX_YMM4(vpblendvb);
    GEN_AVX_YMM3_IMM(vpblendw);
    GEN_AVX_YMM3_IMM(vpblendd);

    GEN(vbroadcastss(xmm1, xmm2));
    GEN(vbroadcastss(ymm1, xmm2));
    GEN(vbroadcastss(xmm1, ptr[rdi]));
    GEN(vbroadcastss(ymm1, ptr[rdi]));
    GEN(vbroadcastsd(ymm1, xmm2));
    GEN(vbroadcastsd(ymm1, ptr[rdi]));
    GEN(vbroadcastf128(ymm1, ptr[rdi]));
    GEN(vbroadcasti128(ymm1, ptr[rdi]));

    GEN_AVX_IMM(vshufps);
    GEN_AVX_IMM(vshufpd);
    GEN_AVX_YMM2_IMM(vpshufd);

    GEN(vmovmskps(eax, xmm2));
    GEN(vmovmskps(rax, xmm2));
    GEN(vmovmskps(eax, ymm2));
    GEN(vmovmskps(rax, ymm2));
    GEN(vmovmskpd(eax, xmm2));
    GEN(vmovmskpd(rax, xmm2));
    GEN(vmovmskpd(eax, ymm2));
    GEN(vmovmskpd(rax, ymm2));

    GEN_AVX(vpunpcklbw);
    GEN_AVX(vpunpcklwd);
    GEN_AVX(vpunpckldq);
    GEN_AVX(vpunpcklqdq);
    GEN_AVX(vpunpckhbw);
    GEN_AVX(vpunpckhwd);
    GEN_AVX(vpunpckhdq);
    GEN_AVX(vpunpckhqdq);
    GEN_AVX_IMM(vpalignr);

    GEN_AVX_XMM3_IMM(vroundss);
    GEN_AVX_XMM3_IMM(vroundsd);
    GEN_AVX_YMM2_IMM(vroundps);
    GEN_AVX_YMM2_IMM(vroundpd);

    GEN_AVX(vphaddw);
    GEN_AVX_XMM2(vphminposuw);
    GEN_AVX(vpmaddwd);
    GEN_AVX(vpmaddubsw);
    GEN_AVX(vphaddsw);
    GEN_AVX(vphaddd);
    GEN_AVX(vphsubw);
    GEN_AVX(vphsubsw);
    GEN_AVX(vphsubd);

    GEN_AVX_CMP_XMM(vcmpss);
    GEN_AVX_CMP_XMM(vcmpsd);
    GEN_AVX_CMP(vcmpps);
    GEN_AVX_CMP(vcmppd);

    GEN_AVX_XMM2(vcomiss);
    GEN_AVX_XMM2(vucomiss);
    GEN_AVX_XMM2(vcomisd);
    GEN_AVX_XMM2(vucomisd);

    GEN(vcvtdq2pd(xmm1, xmm2));
    GEN(vcvtdq2pd(xmm1, ptr[rdi]));
    GEN(vcvtdq2pd(ymm1, xmm2));
    GEN(vcvtdq2pd(ymm1, ptr[rdi]));
    GEN(vcvtpd2dq(xmm1, xmm2));
    GEN(vcvtpd2dq(xmm1, ptr[rdi]));
    GEN(vcvtpd2dq(xmm1, ymm2));
    GEN(vcvttpd2dq(xmm1, xmm2));
    GEN(vcvttpd2dq(xmm1, ptr[rdi]));
    GEN(vcvttpd2dq(xmm1, ymm2));
    GEN(vcvtpd2ps(xmm1, xmm2));
    GEN(vcvtpd2ps(xmm1, ptr[rdi]));
    GEN(vcvtpd2ps(xmm1, ymm2));
    GEN(vcvtdq2ps(xmm1, xmm2));
    GEN(vcvtdq2ps(xmm1, ptr[rdi]));
    GEN(vcvtdq2ps(xmm1, ymm2));
    GEN(vcvtps2dq(xmm1, xmm2));
    GEN(vcvtps2dq(xmm1, ptr[rdi]));
    GEN(vcvtps2dq(xmm1, ymm2));
    GEN(vcvtps2pd(xmm1, xmm2));
    GEN(vcvtps2pd(xmm1, ptr[rdi]));
    GEN(vcvtps2pd(ymm1, xmm2));
    GEN(vcvttps2dq(xmm1, xmm2));
    GEN(vcvttps2dq(xmm1, ptr[rdi]));
    GEN(vcvttps2dq(ymm1, xmm2));

    GEN(vcvttss2si(rax, xmm3));
    GEN(vcvttss2si(eax, xmm2));
    GEN(vcvttss2si(eax, dword[rdi]));
    GEN(vcvttss2si(rax, dword[rdi]));
    GEN(vcvttsd2si(rax, xmm3));
    GEN(vcvttsd2si(eax, xmm2));
    GEN(vcvttsd2si(eax, qword[rdi]));
    GEN(vcvttsd2si(rax, qword[rdi]));
    GEN(vcvtsd2si(rax, xmm3));
    GEN(vcvtsd2si(eax, xmm2));
    GEN(vcvtsd2si(eax, qword[rdi]));
    GEN(vcvtsd2si(rax, qword[rdi]));
    GEN(vcvtss2si(rax, xmm3));
    GEN(vcvtss2si(eax, xmm2));
    GEN(vcvtss2si(eax, dword[rdi]));
    GEN(vcvtss2si(rax, dword[rdi]));

    GEN(vcvtsi2ss(xmm3, xmm4, rax));
    GEN(vcvtsi2ss(xmm2, xmm3, eax));
    GEN(vcvtsi2ss(xmm1, xmm2, dword[rdi]));
    GEN(vcvtsi2ss(xmm1, xmm2, qword[rdi]));
    GEN(vcvtsi2sd(xmm3, xmm4, rax));
    GEN(vcvtsi2sd(xmm2, xmm3, eax));
    GEN(vcvtsi2sd(xmm1, xmm2, dword[rdi]));
    GEN(vcvtsi2sd(xmm1, xmm2, qword[rdi]));

    GEN_AVX_XMM3(vcvtss2sd);
    GEN_AVX_XMM3(vcvtsd2ss);

    GEN(vldmxcsr(ptr[rdi]));
    GEN(vstmxcsr(ptr[rdi]));

    GEN_AVX_IMM(vdpps);
    GEN_AVX_XMM3_IMM(vdppd);

    GEN(vextractf128(xmm1, ymm2, 0));
    GEN(vextractf128(ptr[rdi], ymm2, 0));
    GEN(vextracti128(xmm1, ymm2, 0));
    GEN(vextracti128(ptr[rdi], ymm2, 0));
    GEN(vextractps(eax, xmm2, 0));
    GEN(vextractps(ptr[rdi], xmm2, 0));

    GEN_AVX_MOV(vmovaps);
    GEN_AVX_MOV(vmovapd);
    GEN_AVX_MOV(vmovups);
    GEN_AVX_MOV(vmovupd);
    GEN_AVX_MOV(vmovdqu);
    GEN_AVX_MOV(vmovdqa);

    GEN(vmovhlps(xmm1, xmm2, xmm3));
    GEN(vmovlhps(xmm1, xmm2, xmm3));
    GEN(vmovhpd(xmm1, xmm2, ptr[rdi]));
    GEN(vmovhpd(ptr[rdi], xmm1));
    GEN(vmovhps(xmm1, xmm2, ptr[rdi]));
    GEN(vmovhps(ptr[rdi], xmm1));
    GEN(vmovlpd(xmm1, xmm2, ptr[rdi]));
    GEN(vmovlpd(ptr[rdi], xmm1));
    GEN(vmovlps(xmm1, xmm2, ptr[rdi]));
    GEN(vmovlps(ptr[rdi], xmm1));

    GEN(vmovd(xmm1, eax));
    GEN(vmovd(eax, xmm1));
    GEN(vmovd(xmm1, ptr[rdi]));
    GEN(vmovd(ptr[rdi], xmm1));
    GEN(vmovq(xmm1, rax));
    GEN(vmovq(rax, xmm1));
    GEN(vmovq(xmm1, ptr[rdi]));
    GEN(vmovq(ptr[rdi], xmm1));
    GEN(vmovq(xmm1, xmm2));

    GEN(vmovss(xmm1, xmm2, xmm3));
    GEN(vmovss(ptr[rdi], xmm1));
    GEN(vmovss(xmm1, ptr[rdi]));
    GEN(vmovsd(xmm1, xmm2, xmm3));
    GEN(vmovsd(ptr[rdi], xmm1));
    GEN(vmovsd(xmm1, ptr[rdi]));
    GEN_AVX_YMM2(vmovsldup);
    GEN_AVX_YMM2(vmovshdup);
    GEN(vmovntdq(ptr[rdi], xmm1));
    GEN(vmovntdq(ptr[rdi], ymm1));
    GEN(vmovntdqa(xmm1, ptr[rdi]));
    GEN(vmovntdqa(ymm1, ptr[rdi]));
    GEN(vmovntps(ptr[rdi], xmm1));
    GEN(vmovntps(ptr[rdi], ymm1));
    GEN(vmovntpd(ptr[rdi], xmm1));
    GEN(vmovntpd(ptr[rdi], ymm1));

    GEN_AVX(vaddsubps);
    GEN_AVX(vaddsubpd);

    GEN(vcvtps2ph(xmm1, xmm2, 0));
    GEN(vcvtps2ph(ptr[rdi], xmm1, 0));
    GEN(vcvtps2ph(xmm1, ymm2, 0));
    GEN(vcvtps2ph(ptr[rdi], ymm1, 0));
    GEN(vcvtph2ps(xmm1, xmm2));
    GEN(vcvtph2ps(xmm1, ptr[rdi]));
    GEN(vcvtph2ps(ymm1, xmm2));
    GEN(vcvtph2ps(ymm1, ptr[rdi]));

    GEN_AVX_YMM2(vpmovzxbq);
    GEN_AVX_YMM2(vpmovzxbd);
    GEN_AVX_YMM2(vpmovzxbw);
    GEN_AVX_YMM2(vpmovzxwd);
    GEN_AVX_YMM2(vpmovzxwq);
    GEN_AVX_YMM2(vpmovzxdq);
    GEN_AVX_YMM2(vpmovsxbq);
    GEN_AVX_YMM2(vpmovsxbd);
    GEN_AVX_YMM2(vpmovsxbw);
    GEN_AVX_YMM2(vpmovsxwd);
    GEN_AVX_YMM2(vpmovsxwq);
    GEN_AVX_YMM2(vpmovsxdq);

    GEN(vpinsrb(xmm1, xmm2, eax, 1));
    GEN(vpinsrb(xmm1, xmm2, ptr[rdi], 1));
    GEN(vpinsrw(xmm1, xmm2, eax, 1));
    GEN(vpinsrw(xmm1, xmm2, ptr[rdi], 1));
    GEN(vpinsrd(xmm1, xmm2, eax, 1));
    GEN(vpinsrd(xmm1, xmm2, ptr[rdi], 1));
    GEN(vpinsrq(xmm1, xmm2, rax, 1));
    GEN(vpinsrq(xmm1, xmm2, ptr[rdi], 1));
    GEN(vpextrb(eax, xmm2, 1));
    GEN(vpextrb(ptr[rdi], xmm2, 1));
    GEN(vpextrw(eax, xmm2, 1));
    GEN(vpextrw(ptr[rdi], xmm2, 1));
    GEN(vpextrd(eax, xmm2, 1));
    GEN(vpextrd(ptr[rdi], xmm2, 1));
    GEN(vpextrq(rax, xmm2, 1));
    GEN(vpextrq(ptr[rdi], xmm2, 1));

    GEN_AVX(vhaddps);
    GEN_AVX(vhaddpd);
    GEN_AVX(vhsubps);
    GEN_AVX(vhsubpd);

    GEN(vinsertf128(ymm1, ymm2, xmm3, 0));
    GEN(vinsertf128(ymm1, ymm2, ptr[rdi], 0));
    GEN(vinserti128(ymm1, ymm2, xmm3, 0));
    GEN(vinserti128(ymm1, ymm2, ptr[rdi], 0));
    GEN(vinsertps(xmm1, xmm2, xmm3, 0));
    GEN(vinsertps(xmm1, xmm2, ptr[rdi], 0));

    GEN(vlddqu(xmm1, ptr[rdi]));
    GEN(vlddqu(ymm1, ptr[rdi]));

    GEN(vmaskmovps(xmm1, xmm2, ptr[rdi]));
    GEN(vmaskmovps(ymm1, ymm2, ptr[rdi]));
    GEN(vmaskmovps(ptr[rdi], xmm1, xmm2));
    GEN(vmaskmovps(ptr[rdi], ymm1, ymm2));
    GEN(vmaskmovpd(xmm1, xmm2, ptr[rdi]));
    GEN(vmaskmovpd(ymm1, ymm2, ptr[rdi]));
    GEN(vmaskmovpd(ptr[rdi], xmm1, xmm2));
    GEN(vmaskmovpd(ptr[rdi], ymm1, ymm2));
    GEN(vpmaskmovd(xmm1, xmm2, ptr[rdi]));
    GEN(vpmaskmovd(ymm1, ymm2, ptr[rdi]));
    GEN(vpmaskmovd(ptr[rdi], xmm1, xmm2));
    GEN(vpmaskmovd(ptr[rdi], ymm1, ymm2));
    GEN(vpmaskmovq(xmm1, xmm2, ptr[rdi]));
    GEN(vpmaskmovq(ymm1, ymm2, ptr[rdi]));
    GEN(vpmaskmovq(ptr[rdi], xmm1, xmm2));
    GEN(vpmaskmovq(ptr[rdi], ymm1, ymm2));

    GEN(vmaskmovdqu(xmm1, xmm2));

    GEN(vgatherdps(xmm1, ptr[rdi + xmm2 * 4], xmm3));
    GEN(vgatherdps(ymm1, ptr[rdi + ymm2 * 4], ymm3));
    GEN(vgatherdpd(xmm1, ptr[rdi + xmm2 * 4], xmm3));
    GEN(vgatherdpd(ymm1, ptr[rdi + xmm2 * 4], ymm3));
    GEN(vgatherqps(xmm1, ptr[rdi + xmm2 * 4], xmm3));
    GEN(vgatherqps(xmm1, ptr[rdi + ymm2 * 4], xmm3));
    GEN(vgatherqpd(xmm1, ptr[rdi + xmm2 * 4], xmm3));
    GEN(vgatherqpd(ymm1, ptr[rdi + ymm2 * 4], ymm3));
    GEN(vpgatherdd(xmm1, ptr[rdi + xmm2 * 4], xmm3));
    GEN(vpgatherdd(ymm1, ptr[rdi + ymm2 * 4], ymm3));
    GEN(vpgatherdq(xmm1, ptr[rdi + xmm2 * 4], xmm3));
    GEN(vpgatherdq(ymm1, ptr[rdi + xmm2 * 4], ymm3));
    GEN(vpgatherqd(xmm1, ptr[rdi + xmm2 * 4], xmm3));
    GEN(vpgatherqd(xmm1, ptr[rdi + ymm2 * 4], xmm3));
    GEN(vpgatherqq(xmm1, ptr[rdi + xmm2 * 4], xmm3));
    GEN(vpgatherqq(ymm1, ptr[rdi + ymm2 * 4], ymm3));

    GEN_AVX_YMM3_ONLY(vpermd);
    GEN_AVX_YMM2_ONLY_IMM(vpermq);
    GEN_AVX_YMM3_ONLY(vpermps);
    GEN_AVX_YMM2_ONLY_IMM(vpermpd);
    GEN_AVX(vaesenc);
    GEN_AVX(vaesenclast);
    GEN_AVX(vaesdec);
    GEN_AVX(vaesdeclast);
    GEN_AVX_XMM2(vaesimc);
    GEN_AVX_XMM2_IMM(vaeskeygenassist);

    GEN_AVX(vpsadbw);
    GEN_AVX(vpsignb);
    GEN_AVX(vpsignw);
    GEN_AVX(vpsignd);
    GEN_AVX(vunpcklps);
    GEN_AVX(vunpckhps);
    GEN_AVX(vunpcklpd);
    GEN_AVX(vunpckhpd);

    GEN_AVX_YMM2(vmovddup);
    GEN_AVX_IMM(vmpsadbw);
    GEN_AVX_XMM2(vptest);
    GEN_AVX_YMM2(vtestps);
    GEN_AVX_YMM2(vtestpd);

    GEN(vzeroall());
    GEN(vzeroupper());

    GEN_AVX_XMM2_IMM(vpcmpistri);
    GEN_AVX_XMM2_IMM(vpcmpistrm);
    GEN_AVX_XMM2_IMM(vpcmpestri);
    GEN_AVX_XMM2_IMM(vpcmpestrm);

    GEN_AVX_YMM2_IMM(vpermilps);
    GEN_AVX_YMM2_IMM(vpermilpd);
    GEN_AVX(vpermilps);
    GEN_AVX(vpermilpd);

    GEN(vperm2f128(ymm1, ymm2, ymm3, 0));
    GEN(vperm2i128(ymm1, ymm2, ymm3, 0));

    GEN(vpbroadcastb(xmm1, xmm2));
    GEN(vpbroadcastb(ymm1, xmm2));
    GEN(vpbroadcastw(xmm1, xmm2));
    GEN(vpbroadcastw(ymm1, xmm2));
    GEN(vpbroadcastd(xmm1, xmm2));
    GEN(vpbroadcastd(ymm1, xmm2));
    GEN(vpbroadcastq(xmm1, xmm2));
    GEN(vpbroadcastq(ymm1, xmm2));
    GEN(vpbroadcastb(xmm1, ptr[rdi]));
    GEN(vpbroadcastb(ymm1, ptr[rdi]));
    GEN(vpbroadcastw(xmm1, ptr[rdi]));
    GEN(vpbroadcastw(ymm1, ptr[rdi]));
    GEN(vpbroadcastd(xmm1, ptr[rdi]));
    GEN(vpbroadcastd(ymm1, ptr[rdi]));
    GEN(vpbroadcastq(xmm1, ptr[rdi]));
    GEN(vpbroadcastq(ymm1, ptr[rdi]));

    GEN_AVX_YMM3_IMM(vpclmulqdq);

#define GEN_FMA_PACKED(prefix)                                                                                                                       \
    GEN_AVX(prefix##132ps);                                                                                                                          \
    GEN_AVX(prefix##213ps);                                                                                                                          \
    GEN_AVX(prefix##231ps);                                                                                                                          \
    GEN_AVX(prefix##132pd);                                                                                                                          \
    GEN_AVX(prefix##213pd);                                                                                                                          \
    GEN_AVX(prefix##231pd)

#define GEN_FMA_SCALAR(prefix)                                                                                                                       \
    GEN_AVX_XMM3(prefix##132ss);                                                                                                                     \
    GEN_AVX_XMM3(prefix##213ss);                                                                                                                     \
    GEN_AVX_XMM3(prefix##231ss);                                                                                                                     \
    GEN_AVX_XMM3(prefix##132sd);                                                                                                                     \
    GEN_AVX_XMM3(prefix##213sd);                                                                                                                     \
    GEN_AVX_XMM3(prefix##231sd)

    GEN_FMA_PACKED(vfmadd);
    GEN_FMA_SCALAR(vfmadd);
    GEN_FMA_PACKED(vfmsub);
    GEN_FMA_SCALAR(vfmsub);
    GEN_FMA_PACKED(vfnmadd);
    GEN_FMA_SCALAR(vfnmadd);
    GEN_FMA_PACKED(vfnmsub);
    GEN_FMA_SCALAR(vfnmsub);
    GEN_FMA_PACKED(vfmaddsub);
    GEN_FMA_PACKED(vfmsubadd);

    std::ofstream avx("counts/AVX.json");
    avx << json.dump(4);
    json.clear();

    for (int i = 0; i < 2; i++) {
        g_config.reduced_precision = i;
        Handlers::initialize();
        GEN(fadd(st3, st0));
        GEN(fadd(st0, st3));
        GEN(fadd(dword[rdi]));
        GEN(fadd(qword[rdi]));
        GEN(faddp(st3, st0));
        GEN(fiadd(word[rdi]));
        GEN(fiadd(dword[rdi]));

        GEN(fsub(st3, st0));
        GEN(fsub(st0, st3));
        GEN(fsub(dword[rdi]));
        GEN(fsub(qword[rdi]));
        GEN(fsubp(st3, st0));
        GEN(fisub(word[rdi]));
        GEN(fisub(dword[rdi]));

        GEN(fsubr(st3, st0));
        GEN(fsubr(st0, st3));
        GEN(fsubr(dword[rdi]));
        GEN(fsubr(qword[rdi]));
        GEN(fsubrp(st3, st0));
        GEN(fisubr(word[rdi]));
        GEN(fisubr(dword[rdi]));

        GEN(fmul(st3, st0));
        GEN(fmul(st0, st3));
        GEN(fmul(dword[rdi]));
        GEN(fmul(qword[rdi]));
        GEN(fmulp(st3, st0));
        GEN(fimul(dword[rdi]));
        GEN(fimul(word[rdi]));

        GEN(fdiv(st3, st0));
        GEN(fdiv(st0, st3));
        GEN(fdiv(dword[rdi]));
        GEN(fdiv(qword[rdi]));
        GEN(fdivp(st3, st0));
        GEN(fidiv(dword[rdi]));
        GEN(fidiv(word[rdi]));

        GEN(fdivr(st3, st0));
        GEN(fdivr(st0, st3));
        GEN(fdivr(dword[rdi]));
        GEN(fdivr(qword[rdi]));
        GEN(fdivrp(st3, st0));
        GEN(fidivr(dword[rdi]));
        GEN(fidivr(word[rdi]));

        GEN(fcomi(st0, st3));
        GEN(fcomip(st0, st3));
        GEN(fucomi(st0, st3));
        GEN(fucomip(st0, st3));

        GEN(fist(word[rdi]));
        GEN(fist(dword[rdi]));

        GEN(fistp(word[rdi]));
        GEN(fistp(dword[rdi]));
        GEN(fistp(qword[rdi]));

        GEN(fisttp(word[rdi]));
        GEN(fisttp(dword[rdi]));
        GEN(fisttp(qword[rdi]));

        GEN(fabs());
        GEN(fsin());
        GEN(fcos());
        GEN(fld1());
        GEN(fldl2t());
        GEN(fldl2e());
        GEN(fldpi());
        GEN(fldlg2());
        GEN(fldln2());
        GEN(fldz());
        GEN(fchs());
        GEN(frndint());
        GEN(fprem());
        GEN(fsqrt());
        GEN(fxch(st3));
        GEN(fnstsw(ptr[rdi]));
        GEN(fldenv(ptr[rdi]));
        GEN(fnstenv(ptr[rdi]));

        GEN(fcmovb(st0, st3));
        GEN(fcmove(st0, st3));
        GEN(fcmovbe(st0, st3));
        GEN(fcmovu(st0, st3));
        GEN(fcmovnb(st0, st3));
        GEN(fcmovne(st0, st3));
        GEN(fcmovnbe(st0, st3));
        GEN(fcmovnu(st0, st3));

        std::string name = "counts/X87.json";
        if (g_config.reduced_precision) {
            name = "counts/X87_F64.json";
        }

        std::ofstream x87(name);
        x87 << json.dump(4);
        json.clear();
    }

    Extensions::Zicclsm = true;
    g_config.reduced_precision = true;
    rec.setFlagMode(FlagMode::Default);
    gen_sequence(rec, json, "crysis1", false);
    gen_sequence(rec, json, "crysis2", false);
    gen_sequence(rec, json, "crysis3", false);
    gen_sequence(rec, json, "tombraider1", false);
    gen_sequence(rec, json, "7z1", false);

    std::ofstream many("counts/HotBlocks.json");
    many << json.dump(4);
    json.clear();
}
