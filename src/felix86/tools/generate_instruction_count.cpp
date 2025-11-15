// TODO: This file is a big mess, refactor
#include <cstdint>
#include <cstring>
#include <fstream>
#include <nlohmann/json.hpp>
#include <xbyak/xbyak.h>
#include "Zydis/Disassembler.h"
#include "felix86/v2/handlers.hpp"
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
    static bool init = false;
    static ZydisDecoder zydis;
    if (!init) {
        init = true;
        ZydisDecoderInit(&zydis, ZYDIS_MACHINE_MODE_LONG_64, ZYDIS_STACK_WIDTH_64);
        ZydisDecoderEnableMode(&zydis, ZYDIS_DECODER_MODE_AMD_BRANCHES, ZYAN_TRUE);
    }

    rec.setVectorState(SEW::E1024, 0);
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
        i += 4;
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

    Xbyak::CodeGenerator x(8192);
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
        i += 4;
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

u8 dotnet_block[] = {
    0x44, 0x89, 0xD8, 0xC1, 0xC0, 0x05, 0x44, 0x89, 0xD1, 0x44, 0x31, 0xC1, 0x44, 0x21, 0xF1, 0x44, 0x31, 0xC1, 0x41, 0x01, 0xC9, 0x41, 0x01, 0xC1,
    0x8B, 0x85, 0x90, 0xFE, 0xFF, 0xFF, 0x46, 0x8D, 0x3C, 0x08, 0x41, 0x81, 0xC7, 0x99, 0x79, 0x82, 0x5A, 0x41, 0xC1, 0xC6, 0x1E, 0x44, 0x89, 0xF9,
    0xC1, 0xC1, 0x05, 0x44, 0x89, 0xF2, 0x44, 0x31, 0xD2, 0x44, 0x21, 0xDA, 0x44, 0x31, 0xD2, 0x44, 0x01, 0xC2, 0x03, 0x95, 0x94, 0xFE, 0xFF, 0xFF,
    0x44, 0x8D, 0x0C, 0x11, 0x41, 0x81, 0xC1, 0x99, 0x79, 0x82, 0x5A, 0x41, 0xC1, 0xC3, 0x1E, 0x44, 0x89, 0xC9, 0xC1, 0xC1, 0x05, 0x44, 0x8B, 0x85,
    0xA4, 0xFE, 0xFF, 0xFF, 0x44, 0x89, 0xDA, 0x44, 0x31, 0xF2, 0x44, 0x21, 0xFA, 0x44, 0x31, 0xF2, 0x44, 0x03, 0x95, 0x98, 0xFE, 0xFF, 0xFF, 0x41,
    0x01, 0xD2, 0x42, 0x8D, 0x04, 0x11, 0x05, 0x99, 0x79, 0x82, 0x5A, 0x41, 0xC1, 0xC7, 0x1E, 0x89, 0xC1, 0xC1, 0xC1, 0x05, 0x44, 0x89, 0xFE, 0x44,
    0x31, 0xDE, 0x44, 0x21, 0xCE, 0x44, 0x03, 0xB5, 0x9C, 0xFE, 0xFF, 0xFF, 0x44, 0x31, 0xDE, 0x41, 0x01, 0xF6, 0x41, 0xC1, 0xC1, 0x1E, 0x42, 0x8D,
    0x34, 0x31, 0x81, 0xC6, 0x99, 0x79, 0x82, 0x5A, 0x89, 0xF1, 0xC1, 0xC1, 0x05, 0x44, 0x89, 0xCB, 0x44, 0x31, 0xFB, 0x21, 0xC3, 0x44, 0x31, 0xFB,
    0x44, 0x03, 0x9D, 0xA0, 0xFE, 0xFF, 0xFF, 0x41, 0x01, 0xDB, 0x46, 0x8D, 0x14, 0x19, 0x41, 0x81, 0xC2, 0x99, 0x79, 0x82, 0x5A, 0xC1, 0xC0, 0x1E,
    0x44, 0x89, 0xD1, 0xC1, 0xC1, 0x05, 0x89, 0xC2, 0x44, 0x31, 0xCA, 0x21, 0xF2, 0x44, 0x31, 0xCA, 0x44, 0x01, 0xFA, 0x01, 0xCA, 0x45, 0x8D, 0x04,
    0x10, 0x41, 0x81, 0xC0, 0x99, 0x79, 0x82, 0x5A, 0xC1, 0xC6, 0x1E, 0x44, 0x89, 0xC2, 0xC1, 0xC2, 0x05, 0x89, 0xF1, 0x31, 0xC1, 0x44, 0x21, 0xD1,
    0x31, 0xC1, 0x44, 0x01, 0xC9, 0x03, 0x8D, 0xA8, 0xFE, 0xFF, 0xFF, 0x44, 0x8D, 0x0C, 0x0A, 0x41, 0x81, 0xC1, 0x99, 0x79, 0x82, 0x5A, 0x41, 0xC1,
    0xC2, 0x1E, 0x44, 0x89, 0xC9, 0xC1, 0xC1, 0x05, 0x44, 0x89, 0xD2, 0x31, 0xF2, 0x44, 0x21, 0xC2, 0x31, 0xF2, 0x03, 0x85, 0xAC, 0xFE, 0xFF, 0xFF,
    0x01, 0xD0, 0x8D, 0x1C, 0x01, 0x81, 0xC3, 0x99, 0x79, 0x82, 0x5A, 0x41, 0xC1, 0xC0, 0x1E, 0x89, 0xD9, 0xC1, 0xC1, 0x05, 0x44, 0x89, 0xC2, 0x44,
    0x31, 0xD2, 0x44, 0x21, 0xCA, 0x44, 0x31, 0xD2, 0x03, 0xB5, 0xB0, 0xFE, 0xFF, 0xFF, 0x01, 0xD6, 0x8D, 0x14, 0x31, 0x81, 0xC2, 0x99, 0x79, 0x82,
    0x5A, 0x41, 0xC1, 0xC1, 0x1E, 0x89, 0xD0, 0xC1, 0xC0, 0x05, 0x44, 0x89, 0xCE, 0x44, 0x31, 0xC6, 0x21, 0xDE, 0x44, 0x31, 0xC6, 0x44, 0x03, 0x95,
    0xB4, 0xFE, 0xFF, 0xFF, 0x41, 0x01, 0xF2, 0x46, 0x8D, 0x1C, 0x10, 0x41, 0x81, 0xC3, 0x99, 0x79, 0x82, 0x5A, 0xC1, 0xC3, 0x1E, 0x44, 0x89, 0xD9,
    0xC1, 0xC1, 0x05, 0x89, 0xD8, 0x44, 0x31, 0xC8, 0x21, 0xD0, 0x44, 0x31, 0xC8, 0x44, 0x8B, 0x95, 0xB8, 0xFE, 0xFF, 0xFF, 0x44, 0x01, 0xC0, 0x01,
    0xC8, 0x45, 0x8D, 0x14, 0x02, 0x41, 0x81, 0xC2, 0x99, 0x79, 0x82, 0x5A, 0xC1, 0xC2, 0x1E, 0x44, 0x89, 0xD0, 0xC1, 0xC0, 0x05, 0x89, 0xD1, 0x31,
    0xD9, 0x44, 0x21, 0xD9, 0x31, 0xD9, 0x44, 0x01, 0xC9, 0x03, 0x8D, 0xBC, 0xFE, 0xFF, 0xFF, 0x44, 0x8D, 0x04, 0x08, 0x41, 0x81, 0xC0, 0x99, 0x79,
    0x82, 0x5A, 0x41, 0xC1, 0xC3, 0x1E, 0x44, 0x89, 0xC0, 0xC1, 0xC0, 0x05, 0x44, 0x89, 0xD9, 0x31, 0xD1, 0x44, 0x21, 0xD1, 0x31, 0xD1, 0x03, 0x9D,
    0xC0, 0xFE, 0xFF, 0xFF, 0x01, 0xCB, 0x8D, 0x34, 0x18, 0x81, 0xC6, 0x99, 0x79, 0x82, 0x5A, 0x41, 0xC1, 0xC2, 0x1E, 0x89, 0xF1, 0xC1, 0xC1, 0x05,
    0x44, 0x89, 0xD3, 0x44, 0x31, 0xDB, 0x44, 0x21, 0xC3, 0x44, 0x31, 0xDB, 0x03, 0x95, 0xC4, 0xFE, 0xFF, 0xFF, 0x01, 0xDA, 0x8D, 0x14, 0x11, 0x81,
    0xC2, 0x99, 0x79, 0x82, 0x5A, 0x41, 0xC1, 0xC0, 0x1E, 0x89, 0xD0, 0xC1, 0xC0, 0x05, 0x44, 0x89, 0xC3, 0x44, 0x31, 0xD3, 0x21, 0xF3, 0x44, 0x31,
    0xD3, 0x44, 0x03, 0x9D, 0xC8, 0xFE, 0xFF, 0xFF, 0x41, 0x01, 0xDB, 0x46, 0x8D, 0x1C, 0x18, 0x41, 0x81, 0xC3, 0x99, 0x79, 0x82, 0x5A, 0xC1, 0xC6,
    0x1E, 0x44, 0x89, 0xD9, 0xC1, 0xC1, 0x05, 0x89, 0xF0, 0x44, 0x31, 0xC0, 0x21, 0xD0, 0x44, 0x31, 0xC0, 0x44, 0x8B, 0x8D, 0xCC, 0xFE, 0xFF, 0xFF,
    0x44, 0x01, 0xD0, 0x01, 0xC8, 0x45, 0x8D, 0x0C, 0x01, 0x41, 0x81, 0xC1, 0x99, 0x79, 0x82, 0x5A, 0xC1, 0xC2, 0x1E, 0x44, 0x89, 0xC8, 0xC1, 0xC0,
    0x05, 0x89, 0xD1, 0x31, 0xF1, 0x44, 0x21, 0xD9, 0x31, 0xF1, 0x44, 0x01, 0xC1, 0x03, 0x8D, 0xD0, 0xFE, 0xFF, 0xFF, 0x44, 0x8D, 0x04, 0x08, 0x41,
    0x81, 0xC0, 0x99, 0x79, 0x82, 0x5A, 0x41, 0xC1, 0xC3, 0x1E, 0x44, 0x89, 0xC0, 0xC1, 0xC0, 0x05, 0x44, 0x89, 0xD9, 0x31, 0xD1, 0x44, 0x21, 0xC9,
    0x31, 0xD1, 0x03, 0xB5, 0xD4, 0xFE, 0xFF, 0xFF, 0x01, 0xCE, 0x44, 0x8D, 0x14, 0x30, 0x41, 0x81, 0xC2, 0x99, 0x79, 0x82, 0x5A, 0x41, 0xC1, 0xC1,
    0x1E, 0x44, 0x89, 0xD1, 0xC1, 0xC1, 0x05, 0x44, 0x89, 0xCE, 0x44, 0x31, 0xDE, 0x44, 0x21, 0xC6, 0x44, 0x31, 0xDE, 0x03, 0x95, 0xD8, 0xFE, 0xFF,
    0xFF, 0x01, 0xF2, 0x8D, 0x1C, 0x11, 0x81, 0xC3, 0x99, 0x79, 0x82, 0x5A, 0x41, 0xC1, 0xC0, 0x1E, 0x89, 0xDA, 0xC1, 0xC2, 0x05, 0x44, 0x89, 0xC6,
    0x44, 0x31, 0xCE, 0x44, 0x21, 0xD6, 0x44, 0x31, 0xCE, 0x44, 0x03, 0x9D, 0xDC, 0xFE, 0xFF, 0xFF, 0x41, 0x01, 0xF3, 0x46, 0x8D, 0x1C, 0x1A, 0x41,
    0x81, 0xC3, 0x99, 0x79, 0x82, 0x5A, 0x41, 0xC1, 0xC2, 0x1E, 0x44, 0x89, 0xDE, 0xC1, 0xC6, 0x05, 0x44, 0x89, 0xD0, 0x44, 0x31, 0xC0, 0x31, 0xD8,
    0x8B, 0x8D, 0xE0, 0xFE, 0xFF, 0xFF, 0x44, 0x01, 0xC8, 0x01, 0xF0, 0xC1, 0xC3, 0x1E, 0x44, 0x8D, 0x0C, 0x01, 0x41, 0x81, 0xC1, 0xA1, 0xEB, 0xD9,
    0x6E, 0x44, 0x89, 0xC8, 0xC1, 0xC0, 0x05, 0x89, 0xD9, 0x44, 0x31, 0xD1, 0x44, 0x31, 0xD9, 0x44, 0x01, 0xC1, 0x03, 0x8D, 0xE4, 0xFE, 0xFF, 0xFF,
    0x44, 0x8D, 0x04, 0x08, 0x41, 0x81, 0xC0, 0xA1, 0xEB, 0xD9, 0x6E, 0x41, 0xC1, 0xC3, 0x1E, 0x44, 0x89, 0xC0, 0xC1, 0xC0, 0x05, 0x44, 0x89, 0xD9,
    0x31, 0xD9, 0x44, 0x31, 0xC9, 0x44, 0x03, 0x95, 0xE8, 0xFE, 0xFF, 0xFF, 0x41, 0x01, 0xCA, 0x42, 0x8D, 0x14, 0x10, 0x81, 0xC2, 0xA1, 0xEB, 0xD9,
    0x6E, 0x41, 0xC1, 0xC1, 0x1E, 0x89, 0xD1, 0xC1, 0xC1, 0x05, 0x44, 0x89, 0xCE, 0x44, 0x31, 0xDE, 0x44, 0x31, 0xC6, 0x03, 0x9D, 0xEC, 0xFE, 0xFF,
    0xFF, 0x01, 0xF3, 0x8D, 0x34, 0x19, 0x81, 0xC6, 0xA1, 0xEB, 0xD9, 0x6E, 0x41, 0xC1, 0xC0, 0x1E, 0x89, 0xF0, 0xC1, 0xC0, 0x05, 0x44, 0x89, 0xC3,
    0x44, 0x31, 0xCB, 0x31, 0xD3, 0x44, 0x03, 0x9D, 0xF0, 0xFE, 0xFF, 0xFF, 0x41, 0x01, 0xDB, 0x46, 0x8D, 0x1C, 0x18, 0x41, 0x81, 0xC3, 0xA1, 0xEB,
    0xD9, 0x6E, 0xC1, 0xC2, 0x1E, 0x44, 0x89, 0xD9, 0xC1, 0xC1, 0x05, 0x89, 0xD0, 0x44, 0x31, 0xC0, 0x31, 0xF0, 0x44, 0x8B, 0x95, 0xF4, 0xFE, 0xFF,
    0xFF, 0x44, 0x01, 0xC8, 0x01, 0xC8, 0xC1, 0xC6, 0x1E, 0x45, 0x8D, 0x0C, 0x02, 0x41, 0x81, 0xC1, 0xA1, 0xEB, 0xD9, 0x6E, 0x44, 0x89, 0xC8, 0xC1,
    0xC0, 0x05, 0x89, 0xF1, 0x31, 0xD1, 0x44, 0x31, 0xD9, 0x44, 0x01, 0xC1, 0x03, 0x8D, 0xF8, 0xFE, 0xFF, 0xFF, 0x44, 0x8D, 0x04, 0x08, 0x41, 0x81,
    0xC0, 0xA1, 0xEB, 0xD9, 0x6E, 0x41, 0xC1, 0xC3, 0x1E, 0x44, 0x89, 0xC0, 0xC1, 0xC0, 0x05, 0x44, 0x89, 0xD9, 0x31, 0xF1, 0x44, 0x31, 0xC9, 0x03,
    0x95, 0xFC, 0xFE, 0xFF, 0xFF, 0x01, 0xCA, 0x8D, 0x1C, 0x10, 0x81, 0xC3, 0xA1, 0xEB, 0xD9, 0x6E, 0x41, 0xC1, 0xC1, 0x1E, 0x89, 0xD9, 0xC1, 0xC1,
    0x05, 0x44, 0x89, 0xCA, 0x44, 0x31, 0xDA, 0x44, 0x31, 0xC2, 0x03, 0xB5, 0x00, 0xFF, 0xFF, 0xFF, 0x01, 0xD6, 0x8D, 0x0C, 0x31, 0x81, 0xC1, 0xA1,
    0xEB, 0xD9, 0x6E, 0x41, 0xC1, 0xC0, 0x1E, 0x89, 0xCA, 0xC1, 0xC2, 0x05, 0x44, 0x89, 0xC6, 0x44, 0x31, 0xCE, 0x31, 0xDE, 0x44, 0x03, 0x9D, 0x04,
    0xFF, 0xFF, 0xFF, 0x41, 0x01, 0xF3, 0x46, 0x8D, 0x1C, 0x1A, 0x41, 0x81, 0xC3, 0xA1, 0xEB, 0xD9, 0x6E, 0xC1, 0xC3, 0x1E, 0x44, 0x89, 0xDE, 0xC1,
    0xC6, 0x05, 0x89, 0xD8, 0x44, 0x31, 0xC0, 0x31, 0xC8, 0x44, 0x8B, 0x95, 0x08, 0xFF, 0xFF, 0xFF, 0x44, 0x01, 0xC8, 0x01, 0xF0, 0xC1, 0xC1, 0x1E,
    0x45, 0x8D, 0x0C, 0x02, 0x41, 0x81, 0xC1, 0xA1, 0xEB, 0xD9, 0x6E, 0x44, 0x89, 0xC8, 0xC1, 0xC0, 0x05, 0x89, 0xCE, 0x31, 0xDE, 0x44, 0x31, 0xDE,
    0x44, 0x01, 0xC6, 0x03, 0xB5, 0x0C, 0xFF, 0xFF, 0xFF, 0x44, 0x8D, 0x04, 0x30, 0x41, 0x81, 0xC0, 0xA1, 0xEB, 0xD9, 0x6E, 0x41, 0xC1, 0xC3, 0x1E,
    0x44, 0x89, 0xC0, 0xC1, 0xC0, 0x05, 0x44, 0x89, 0xDE, 0x31, 0xCE, 0x44, 0x31, 0xCE, 0x03, 0x9D, 0x10, 0xFF, 0xFF, 0xFF, 0x01, 0xF3, 0x8D, 0x14,
    0x18, 0x81, 0xC2, 0xA1, 0xEB, 0xD9, 0x6E, 0x41, 0xC1, 0xC1, 0x1E, 0x89, 0xD6, 0xC1, 0xC6, 0x05, 0x44, 0x89, 0xCB, 0x44, 0x31, 0xDB, 0x44, 0x31,
    0xC3, 0x03, 0x8D, 0x14, 0xFF, 0xFF, 0xFF, 0x01, 0xD9, 0x8D, 0x0C, 0x0E, 0x81, 0xC1, 0xA1, 0xEB, 0xD9, 0x6E, 0x41, 0xC1, 0xC0, 0x1E, 0x89, 0xC8,
    0xC1, 0xC0, 0x05, 0x44, 0x89, 0xC3, 0x44, 0x31, 0xCB, 0x31, 0xD3, 0x44, 0x03, 0x9D, 0x18, 0xFF, 0xFF, 0xFF, 0x41, 0x01, 0xDB, 0x42, 0x8D, 0x1C,
    0x18, 0x81, 0xC3, 0xA1, 0xEB, 0xD9, 0x6E, 0xC1, 0xC2, 0x1E, 0x89, 0xDE, 0xC1, 0xC6, 0x05, 0x89, 0xD0, 0x44, 0x31, 0xC0, 0x31, 0xC8, 0x44, 0x8B,
    0x95, 0x1C, 0xFF, 0xFF, 0xFF, 0x44, 0x01, 0xC8, 0x01, 0xF0, 0xC1, 0xC1, 0x1E, 0x45, 0x8D, 0x0C, 0x02, 0x41, 0x81, 0xC1, 0xA1, 0xEB, 0xD9, 0x6E,
    0x44, 0x89, 0xC8, 0xC1, 0xC0, 0x05, 0x89, 0xCE, 0x31, 0xD6, 0x31, 0xDE, 0x44, 0x01, 0xC6, 0x03, 0xB5, 0x20, 0xFF, 0xFF, 0xFF, 0x44, 0x8D, 0x04,
    0x30, 0x41, 0x81, 0xC0, 0xA1, 0xEB, 0xD9, 0x6E, 0xC1, 0xC3, 0x1E, 0x44, 0x89, 0xC0, 0xC1, 0xC0, 0x05, 0x89, 0xDE, 0x31, 0xCE, 0x44, 0x31, 0xCE,
    0x03, 0x95, 0x24, 0xFF, 0xFF, 0xFF, 0x01, 0xF2, 0x44, 0x8D, 0x14, 0x10, 0x41, 0x81, 0xC2, 0xA1, 0xEB, 0xD9, 0x6E, 0x41, 0xC1, 0xC1, 0x1E, 0x44,
    0x89, 0xD0, 0xC1, 0xC0, 0x05, 0x44, 0x89, 0xCA, 0x31, 0xDA, 0x44, 0x31, 0xC2, 0x03, 0x8D, 0x28, 0xFF, 0xFF, 0xFF, 0x01, 0xD1, 0x8D, 0x14, 0x08,
    0x81, 0xC2, 0xA1, 0xEB, 0xD9, 0x6E, 0x41, 0xC1, 0xC0, 0x1E, 0x89, 0xD0, 0xC1, 0xC0, 0x05, 0x44, 0x89, 0xC1, 0x44, 0x31, 0xC9, 0x44, 0x31, 0xD1,
    0x03, 0x9D, 0x2C, 0xFF, 0xFF, 0xFF, 0x01, 0xCB, 0x44, 0x8D, 0x1C, 0x18, 0x41, 0x81, 0xC3, 0xA1, 0xEB, 0xD9, 0x6E, 0x41, 0xC1, 0xC2, 0x1E, 0x44,
    0x89, 0xD9, 0xC1, 0xC1, 0x05, 0x89, 0xD6, 0x44, 0x09, 0xC6, 0x44, 0x21, 0xD6, 0x89, 0xD3, 0x44, 0x21, 0xC3, 0x09, 0xF3, 0x8B, 0xB5, 0x30, 0xFF,
    0xFF, 0xFF, 0x44, 0x01, 0xC9, 0x01, 0xD9, 0x44, 0x8D, 0x0C, 0x0E, 0x41, 0x81, 0xC1, 0xDC, 0xBC, 0x1B, 0x8F, 0xC1, 0xC2, 0x1E, 0x44, 0x89, 0xC9,
    0xC1, 0xC1, 0x05, 0x44, 0x89, 0xDE, 0x44, 0x09, 0xD6, 0x21, 0xD6, 0x44, 0x89, 0xD8, 0x44, 0x21, 0xD0, 0x09, 0xF0, 0x44, 0x01, 0xC0, 0x03, 0x85,
    0x34, 0xFF, 0xFF, 0xFF, 0x44, 0x8D, 0x04, 0x01, 0x41, 0x81, 0xC0, 0xDC, 0xBC, 0x1B, 0x8F, 0x41, 0xC1, 0xC3, 0x1E, 0x44, 0x89, 0xC0, 0xC1, 0xC0,
    0x05, 0x44, 0x89, 0xC9, 0x09, 0xD1, 0x44, 0x21, 0xD9, 0x44, 0x89, 0xCE, 0x21, 0xD6, 0x09, 0xCE, 0x44, 0x03, 0x95, 0x38, 0xFF, 0xFF, 0xFF, 0x41,
    0x01, 0xF2, 0x42, 0x8D, 0x0C, 0x10, 0x81, 0xC1, 0xDC, 0xBC, 0x1B, 0x8F, 0x41, 0xC1, 0xC1, 0x1E, 0x89, 0xC8, 0xC1, 0xC0, 0x05, 0x44, 0x89, 0xC6,
    0x44, 0x09, 0xDE, 0x44, 0x21, 0xCE, 0x44, 0x89, 0xC3, 0x44, 0x21, 0xDB, 0x09, 0xF3, 0x03, 0x95, 0x3C, 0xFF, 0xFF, 0xFF, 0x01, 0xDA, 0x8D, 0x14,
    0x10, 0x81, 0xC2, 0xDC, 0xBC, 0x1B, 0x8F, 0x41, 0xC1, 0xC0, 0x1E, 0x89, 0xD0, 0xC1, 0xC0, 0x05, 0x89, 0xCE, 0x44, 0x09, 0xCE, 0x44, 0x21, 0xC6,
    0x89, 0xCB, 0x44, 0x21, 0xCB, 0x09, 0xF3, 0x44, 0x03, 0x9D, 0x40, 0xFF, 0xFF, 0xFF, 0x41, 0x01, 0xDB, 0x46, 0x8D, 0x14, 0x18, 0x41, 0x81, 0xC2,
    0xDC, 0xBC, 0x1B, 0x8F, 0xC1, 0xC1, 0x1E, 0x44, 0x89, 0xD6, 0xC1, 0xC6, 0x05, 0x89, 0xD3, 0x44, 0x09, 0xC3, 0x21, 0xCB, 0x89, 0xD0, 0x44, 0x21,
    0xC0, 0x09, 0xD8, 0x8B, 0x9D, 0x44, 0xFF, 0xFF, 0xFF, 0x44, 0x01, 0xCE, 0x01, 0xC6, 0x44, 0x8D, 0x0C, 0x33, 0x41, 0x81, 0xC1, 0xDC, 0xBC, 0x1B,
    0x8F, 0xC1, 0xC2, 0x1E, 0x44, 0x89, 0xC8, 0xC1, 0xC0, 0x05, 0x44, 0x89, 0xD3, 0x09, 0xCB, 0x21, 0xD3, 0x44, 0x89, 0xD6, 0x21, 0xCE, 0x09, 0xDE,
    0x44, 0x01, 0xC6, 0x03, 0xB5, 0x48, 0xFF, 0xFF, 0xFF, 0x44, 0x8D, 0x04, 0x30, 0x41, 0x81, 0xC0, 0xDC, 0xBC, 0x1B, 0x8F, 0x41, 0xC1, 0xC2, 0x1E,
    0x44, 0x89, 0xC0, 0xC1, 0xC0, 0x05, 0x44, 0x89, 0xCE, 0x09, 0xD6, 0x44, 0x21, 0xD6, 0x44, 0x89, 0xCB, 0x21, 0xD3, 0x09, 0xF3, 0x03, 0x8D, 0x4C,
    0xFF, 0xFF, 0xFF, 0x01, 0xD9, 0x8D, 0x0C, 0x08, 0x81, 0xC1, 0xDC, 0xBC, 0x1B, 0x8F, 0x41, 0xC1, 0xC1, 0x1E, 0x89, 0xC8, 0xC1, 0xC0, 0x05, 0x44,
    0x89, 0xC6, 0x44, 0x09, 0xD6, 0x44, 0x21, 0xCE, 0x44, 0x89, 0xC3, 0x44, 0x21, 0xD3, 0x09, 0xF3, 0x03, 0x95, 0x50, 0xFF, 0xFF, 0xFF, 0x01, 0xDA,
    0x8D, 0x14, 0x10, 0x81, 0xC2, 0xDC, 0xBC, 0x1B, 0x8F, 0x41, 0xC1, 0xC0, 0x1E, 0x89, 0xD0, 0xC1, 0xC0, 0x05, 0x89, 0xCE, 0x44, 0x09, 0xCE, 0x44,
    0x21, 0xC6, 0x89, 0xCB, 0x44, 0x21, 0xCB, 0x09, 0xF3, 0x44, 0x03, 0x95, 0x54, 0xFF, 0xFF, 0xFF, 0x41, 0x01, 0xDA, 0x46, 0x8D, 0x14, 0x10, 0x41,
    0x81, 0xC2, 0xDC, 0xBC, 0x1B, 0x8F, 0xC1, 0xC1, 0x1E, 0x44, 0x89, 0xD6, 0xC1, 0xC6, 0x05, 0x89, 0xD3, 0x44, 0x09, 0xC3, 0x21, 0xCB, 0x89, 0xD0,
    0x44, 0x21, 0xC0, 0x09, 0xD8, 0x8B, 0x9D, 0x58, 0xFF, 0xFF, 0xFF, 0x44, 0x01, 0xCE, 0x01, 0xC6, 0x44, 0x8D, 0x0C, 0x33, 0x41, 0x81, 0xC1, 0xDC,
    0xBC, 0x1B, 0x8F, 0xC1, 0xC2, 0x1E, 0x44, 0x89, 0xC8, 0xC1, 0xC0, 0x05, 0x44, 0x89, 0xD3, 0x09, 0xCB, 0x21, 0xD3, 0x44, 0x89, 0xD6, 0x21, 0xCE,
    0x09, 0xDE, 0x44, 0x01, 0xC6, 0x03, 0xB5, 0x5C, 0xFF, 0xFF, 0xFF, 0x44, 0x8D, 0x04, 0x30, 0x41, 0x81, 0xC0, 0xDC, 0xBC, 0x1B, 0x8F, 0x41, 0xC1,
    0xC2, 0x1E, 0x44, 0x89, 0xC0, 0xC1, 0xC0, 0x05, 0x44, 0x89, 0xCE, 0x09, 0xD6, 0x44, 0x21, 0xD6, 0x44, 0x89, 0xCB, 0x21, 0xD3, 0x09, 0xF3, 0x03,
    0x8D, 0x60, 0xFF, 0xFF, 0xFF, 0x01, 0xD9, 0x8D, 0x0C, 0x08, 0x81, 0xC1, 0xDC, 0xBC, 0x1B, 0x8F, 0x41, 0xC1, 0xC1, 0x1E, 0x89, 0xC8, 0xC1, 0xC0,
    0x05, 0x44, 0x89, 0xC6, 0x44, 0x09, 0xD6, 0x44, 0x21, 0xCE, 0x44, 0x89, 0xC3, 0x44, 0x21, 0xD3, 0x09, 0xF3, 0x03, 0x95, 0x64, 0xFF, 0xFF, 0xFF,
    0x01, 0xDA, 0x8D, 0x14, 0x10, 0x81, 0xC2, 0xDC, 0xBC, 0x1B, 0x8F, 0x41, 0xC1, 0xC0, 0x1E, 0x89, 0xD0, 0xC1, 0xC0, 0x05, 0x89, 0xCE, 0x44, 0x09,
    0xCE, 0x44, 0x21, 0xC6, 0x89, 0xCB, 0x44, 0x21, 0xCB, 0x09, 0xF3, 0x44, 0x03, 0x95, 0x68, 0xFF, 0xFF, 0xFF, 0x41, 0x01, 0xDA, 0x46, 0x8D, 0x14,
    0x10, 0x41, 0x81, 0xC2, 0xDC, 0xBC, 0x1B, 0x8F, 0xC1, 0xC1, 0x1E, 0x44, 0x89, 0xD6, 0xC1, 0xC6, 0x05, 0x89, 0xD3, 0x44, 0x09, 0xC3, 0x21, 0xCB,
    0x89, 0xD0, 0x44, 0x21, 0xC0, 0x09, 0xD8, 0x8B, 0x9D, 0x6C, 0xFF, 0xFF, 0xFF, 0x44, 0x01, 0xCE, 0x01, 0xC6, 0x44, 0x8D, 0x0C, 0x33, 0x41, 0x81,
    0xC1, 0xDC, 0xBC, 0x1B, 0x8F, 0xC1, 0xC2, 0x1E, 0x44, 0x89, 0xC8, 0xC1, 0xC0, 0x05, 0x44, 0x89, 0xD3, 0x09, 0xCB, 0x21, 0xD3, 0x44, 0x89, 0xD6,
    0x21, 0xCE, 0x09, 0xDE, 0x44, 0x01, 0xC6, 0x03, 0xB5, 0x70, 0xFF, 0xFF, 0xFF, 0x44, 0x8D, 0x04, 0x30, 0x41, 0x81, 0xC0, 0xDC, 0xBC, 0x1B, 0x8F,
    0x41, 0xC1, 0xC2, 0x1E, 0x44, 0x89, 0xC0, 0xC1, 0xC0, 0x05, 0x44, 0x89, 0xCE, 0x09, 0xD6, 0x44, 0x21, 0xD6, 0x44, 0x89, 0xCB, 0x21, 0xD3, 0x09,
    0xF3, 0x03, 0x8D, 0x74, 0xFF, 0xFF, 0xFF, 0x01, 0xD9, 0x8D, 0x04, 0x08, 0x05, 0xDC, 0xBC, 0x1B, 0x8F, 0x41, 0xC1, 0xC1, 0x1E, 0x89, 0xC1, 0xC1,
    0xC1, 0x05, 0x44, 0x89, 0xC6, 0x44, 0x09, 0xD6, 0x44, 0x21, 0xCE, 0x44, 0x89, 0xC3, 0x44, 0x21, 0xD3, 0x09, 0xF3, 0x03, 0x95, 0x78, 0xFF, 0xFF,
    0xFF, 0x01, 0xDA, 0x44, 0x8D, 0x1C, 0x11, 0x41, 0x81, 0xC3, 0xDC, 0xBC, 0x1B, 0x8F, 0x41, 0xC1, 0xC0, 0x1E, 0x44, 0x89, 0xD9, 0xC1, 0xC1, 0x05,
    0x89, 0xC6, 0x44, 0x09, 0xCE, 0x44, 0x21, 0xC6, 0x89, 0xC3, 0x44, 0x21, 0xCB, 0x09, 0xF3, 0x44, 0x03, 0x95, 0x7C, 0xFF, 0xFF, 0xFF, 0x41, 0x01,
    0xDA, 0x46, 0x8D, 0x34, 0x11, 0x41, 0x81, 0xC6, 0xDC, 0xBC, 0x1B, 0x8F, 0xC1, 0xC0, 0x1E, 0x44, 0x89, 0xF6, 0xC1, 0xC6, 0x05, 0x89, 0xC1, 0x44,
    0x31, 0xC1, 0x44, 0x31, 0xD9, 0x44, 0x8B, 0x55, 0x80, 0x44, 0x01, 0xC9, 0x01, 0xF1, 0x41, 0xC1, 0xC3, 0x1E, 0x45, 0x8D, 0x0C, 0x0A, 0x41, 0x81,
    0xC1, 0xD6, 0xC1, 0x62, 0xCA, 0x44, 0x89, 0xC9, 0xC1, 0xC1, 0x05, 0x44, 0x89, 0xDA, 0x31, 0xC2, 0x44, 0x31, 0xF2, 0x44, 0x01, 0xC2, 0x03, 0x55,
    0x84, 0x44, 0x8D, 0x04, 0x11, 0x41, 0x81, 0xC0, 0xD6, 0xC1, 0x62, 0xCA, 0x41, 0xC1, 0xC6, 0x1E, 0x44, 0x89, 0xC1, 0xC1, 0xC1, 0x05, 0x44, 0x89,
    0xF2, 0x44, 0x31, 0xDA, 0x44, 0x31, 0xCA, 0x03, 0x45, 0x88, 0x01, 0xD0, 0x8D, 0x04, 0x01, 0x05, 0xD6, 0xC1, 0x62, 0xCA, 0x41, 0xC1, 0xC1, 0x1E,
    0x89, 0xC1, 0xC1, 0xC1, 0x05, 0x44, 0x89, 0xCA, 0x44, 0x31, 0xF2, 0x44, 0x31, 0xC2, 0x44, 0x03, 0x5D, 0x8C, 0x41, 0x01, 0xD3, 0x42, 0x8D, 0x1C,
    0x19, 0x81, 0xC3, 0xD6, 0xC1, 0x62, 0xCA, 0x41, 0xC1, 0xC0, 0x1E, 0x89, 0xD9, 0xC1, 0xC1, 0x05, 0x44, 0x89, 0xC6, 0x44, 0x31, 0xCE, 0x31, 0xC6,
    0x44, 0x03, 0x75, 0x90, 0x41, 0x01, 0xF6, 0x42, 0x8D, 0x34, 0x31, 0x81, 0xC6, 0xD6, 0xC1, 0x62, 0xCA, 0xC1, 0xC0, 0x1E, 0x89, 0xF2, 0xC1, 0xC2,
    0x05, 0x89, 0xC1, 0x44, 0x31, 0xC1, 0x31, 0xD9, 0x44, 0x8B, 0x55, 0x94, 0x44, 0x01, 0xC9, 0x01, 0xD1, 0xC1, 0xC3, 0x1E, 0x45, 0x8D, 0x0C, 0x0A,
    0x41, 0x81, 0xC1, 0xD6, 0xC1, 0x62, 0xCA, 0x44, 0x89, 0xC9, 0xC1, 0xC1, 0x05, 0x89, 0xDA, 0x31, 0xC2, 0x31, 0xF2, 0x44, 0x01, 0xC2, 0x03, 0x55,
    0x98, 0x44, 0x8D, 0x04, 0x11, 0x41, 0x81, 0xC0, 0xD6, 0xC1, 0x62, 0xCA, 0xC1, 0xC6, 0x1E, 0x44, 0x89, 0xC1, 0xC1, 0xC1, 0x05, 0x89, 0xF2, 0x31,
    0xDA, 0x44, 0x31, 0xCA, 0x03, 0x45, 0x9C, 0x01, 0xD0, 0x8D, 0x04, 0x01, 0x05, 0xD6, 0xC1, 0x62, 0xCA, 0x41, 0xC1, 0xC1, 0x1E, 0x89, 0xC1, 0xC1,
    0xC1, 0x05, 0x44, 0x89, 0xCA, 0x31, 0xF2, 0x44, 0x31, 0xC2, 0x03, 0x5D, 0xA0, 0x01, 0xD3, 0x8D, 0x14, 0x19, 0x81, 0xC2, 0xD6, 0xC1, 0x62, 0xCA,
    0x41, 0xC1, 0xC0, 0x1E, 0x89, 0xD1, 0xC1, 0xC1, 0x05, 0x44, 0x89, 0xC3, 0x44, 0x31, 0xCB, 0x31, 0xC3, 0x03, 0x75, 0xA4, 0x01, 0xDE, 0x8D, 0x1C,
    0x31, 0x81, 0xC3, 0xD6, 0xC1, 0x62, 0xCA, 0xC1, 0xC0, 0x1E, 0x89, 0xDE, 0xC1, 0xC6, 0x05, 0x89, 0xC1, 0x44, 0x31, 0xC1, 0x31, 0xD1, 0x44, 0x8B,
    0x55, 0xA8, 0x44, 0x01, 0xC9, 0x01, 0xF1, 0xC1, 0xC2, 0x1E, 0x45, 0x8D, 0x0C, 0x0A, 0x41, 0x81, 0xC1, 0xD6, 0xC1, 0x62, 0xCA, 0x44, 0x89, 0xC9,
    0xC1, 0xC1, 0x05, 0x89, 0xD6, 0x31, 0xC6, 0x31, 0xDE, 0x44, 0x01, 0xC6, 0x03, 0x75, 0xAC, 0x44, 0x8D, 0x04, 0x31, 0x41, 0x81, 0xC0, 0xD6, 0xC1,
    0x62, 0xCA, 0xC1, 0xC3, 0x1E, 0x44, 0x89, 0xC1, 0xC1, 0xC1, 0x05, 0x89, 0xDE, 0x31, 0xD6, 0x44, 0x31, 0xCE, 0x03, 0x45, 0xB0, 0x01, 0xF0, 0x8D,
    0x04, 0x01, 0x05, 0xD6, 0xC1, 0x62, 0xCA, 0x41, 0xC1, 0xC1, 0x1E, 0x89, 0xC6, 0xC1, 0xC6, 0x05, 0x44, 0x89, 0xC9, 0x31, 0xD9, 0x44, 0x31, 0xC1,
    0x03, 0x55, 0xB4, 0x01, 0xCA, 0x8D, 0x0C, 0x16, 0x81, 0xC1, 0xD6, 0xC1, 0x62, 0xCA, 0x41, 0xC1, 0xC0, 0x1E, 0x89, 0xCE, 0xC1, 0xC6, 0x05, 0x44,
    0x89, 0xC2, 0x44, 0x31, 0xCA, 0x31, 0xC2, 0x03, 0x5D, 0xB8, 0x01, 0xD3, 0x8D, 0x1C, 0x1E, 0x81, 0xC3, 0xD6, 0xC1, 0x62, 0xCA, 0xC1, 0xC0, 0x1E,
    0x89, 0xDA, 0xC1, 0xC2, 0x05, 0x89, 0xC6, 0x44, 0x31, 0xC6, 0x31, 0xCE, 0x44, 0x8B, 0x55, 0xBC, 0x44, 0x01, 0xCE, 0x01, 0xD6, 0xC1, 0xC1, 0x1E,
    0x45, 0x8D, 0x0C, 0x32, 0x41, 0x81, 0xC1, 0xD6, 0xC1, 0x62, 0xCA, 0x44, 0x89, 0xCA, 0xC1, 0xC2, 0x05, 0x89, 0xCE, 0x31, 0xC6, 0x31, 0xDE, 0x44,
    0x01, 0xC6, 0x03, 0x75, 0xC0, 0x44, 0x8D, 0x04, 0x32, 0x41, 0x81, 0xC0, 0xD6, 0xC1, 0x62, 0xCA, 0xC1, 0xC3, 0x1E, 0x44, 0x89, 0xC6, 0xC1, 0xC6,
    0x05, 0x89, 0xDA, 0x31, 0xCA, 0x44, 0x31, 0xCA, 0x03, 0x45, 0xC4, 0x01, 0xD0, 0x8D, 0x34, 0x06, 0x81, 0xC6, 0xD6, 0xC1, 0x62, 0xCA, 0x41, 0xC1,
    0xC1, 0x1E, 0x89, 0xF0, 0xC1, 0xC0, 0x05, 0x44, 0x89, 0xCA, 0x31, 0xDA, 0x03, 0x4D, 0xC8, 0x44, 0x31, 0xC2, 0x01, 0xD1, 0x41, 0xC1, 0xC0, 0x1E,
    0x8D, 0x04, 0x08, 0x05, 0xD6, 0xC1, 0x62, 0xCA, 0x66, 0x0F, 0x6E, 0xC0, 0xC1, 0xC0, 0x05, 0x66, 0x41, 0x0F, 0x6E, 0xC8, 0x45, 0x31, 0xC8, 0x03,
    0x5D, 0xCC, 0x41, 0x31, 0xF0, 0x44, 0x01, 0xC3, 0xC1, 0xC6, 0x1E, 0x8D, 0x04, 0x18, 0x05, 0xD6, 0xC1, 0x62, 0xCA, 0xF3, 0x0F, 0x6F, 0x57, 0x44,
    0x66, 0x0F, 0x6E, 0xD8, 0x66, 0x0F, 0x62, 0xD8, 0x66, 0x0F, 0x6E, 0xC6, 0x66, 0x0F, 0x62, 0xC1, 0x66, 0x0F, 0x6C, 0xD8, 0x66, 0x0F, 0xFE, 0xDA,
    0xF3, 0x0F, 0x7F, 0x5F, 0x44, 0x44, 0x01, 0x4F, 0x54, 0x64, 0x48, 0x8B, 0x04, 0x25, 0x28, 0x00, 0x00, 0x00, 0x48, 0x3B, 0x45, 0xD8, 0x0F, 0x85,
    0x7F, 0x03, 0x9A, 0xB0};

u8 z7_block[] = {0x48, 0x8b, 0x1f, 0x48, 0x8b, 0x77, 0x28, 0x0f, 0xb6, 0x13, 0x44, 0x0f, 0xb6, 0x53, 0x02, 0x0f, 0xb6, 0x43, 0x01, 0x41,
                 0xc1, 0xe2, 0x08, 0x33, 0x44, 0x97, 0x78, 0x8b, 0x57, 0x08, 0x41, 0x31, 0xc2, 0x25, 0xff, 0x03, 0x00, 0x00, 0x4c, 0x8d,
                 0x0c, 0x86, 0x41, 0x0f, 0xb7, 0xc2, 0x89, 0xd7, 0x89, 0xd1, 0x4c, 0x8d, 0x84, 0x86, 0x00, 0x10, 0x00, 0x00, 0x0f, 0xb6,
                 0x43, 0x03, 0x41, 0x2b, 0x39, 0x41, 0x2b, 0x08, 0x41, 0x8b, 0x44, 0x84, 0x78, 0xc1, 0xe0, 0x05, 0x44, 0x31, 0xd0, 0x41,
                 0x23, 0x44, 0x24, 0x38, 0x48, 0x8d, 0x84, 0x86, 0x00, 0x10, 0x04, 0x00, 0x8b, 0x30, 0x41, 0x89, 0x11, 0x41, 0x89, 0x10,
                 0x89, 0x10, 0x41, 0x8b, 0x44, 0x24, 0x1c, 0x39, 0xc2, 0x0f, 0x47, 0xd0, 0x39, 0xd7, 0x73, 0x17};

u8 outlast_camera[] = {
    0x8b, 0x4f, 0x10, 0x4c, 0x8b, 0x7f, 0x08, 0x4c, 0x8d, 0x34, 0x06, 0x0f, 0x28, 0x65, 0x00, 0x49, 0x8d, 0x0c, 0x8f, 0x4c, 0x8b, 0x7f, 0x20,
    0x44, 0x0f, 0x10, 0x01, 0x8b, 0x4f, 0x28, 0x41, 0x0f, 0x28, 0xc8, 0x49, 0x8d, 0x0c, 0x8f, 0x4c, 0x8b, 0x7f, 0x38, 0x0f, 0x10, 0x19, 0x8b,
    0x4f, 0x40, 0x0f, 0x16, 0xcb, 0x44, 0x0f, 0xc6, 0xc3, 0xee, 0x49, 0x8d, 0x0c, 0x8f, 0x4c, 0x8b, 0x7f, 0x50, 0x45, 0x0f, 0x28, 0xc8, 0x0f,
    0x10, 0x01, 0x8b, 0x4f, 0x58, 0x0f, 0x28, 0xd8, 0x49, 0x8d, 0x0c, 0x8f, 0x0f, 0x10, 0x11, 0x0f, 0x16, 0xda, 0x0f, 0xc6, 0xc2, 0xee, 0x0f,
    0x28, 0xd1, 0x0f, 0xc6, 0xcb, 0xdd, 0x0f, 0xc6, 0xd3, 0x88, 0x41, 0x0f, 0x28, 0x1b, 0x44, 0x0f, 0xc6, 0xc8, 0x88, 0x44, 0x0f, 0xc6, 0xc0,
    0xdd, 0x0f, 0x28, 0x03, 0x0f, 0x59, 0xd4, 0x0f, 0x59, 0xd8, 0x0f, 0x59, 0xcc, 0x44, 0x0f, 0x59, 0xcc, 0x0f, 0x58, 0xda, 0x44, 0x0f, 0x59,
    0xc4, 0x0f, 0x28, 0xd3, 0x0f, 0x59, 0xd0, 0x0f, 0x58, 0xd1, 0x0f, 0x28, 0xca, 0x0f, 0x59, 0xc8, 0x41, 0x0f, 0x58, 0xc9, 0x0f, 0x59, 0xc1,
    0x0f, 0x28, 0xe1, 0x41, 0x0f, 0x58, 0xc0, 0x44, 0x0f, 0x28, 0xc3, 0x0f, 0xc6, 0xda, 0xee, 0x44, 0x0f, 0x16, 0xc2, 0x0f, 0x16, 0xe0, 0x41,
    0x0f, 0x28, 0xd0, 0x0f, 0xc6, 0xc8, 0xee, 0x44, 0x0f, 0xc6, 0xc4, 0xdd, 0x0f, 0xc6, 0xd4, 0x88, 0x0f, 0x28, 0xe3, 0x0f, 0x29, 0x44, 0x24,
    0xc8, 0x0f, 0xc6, 0xe1, 0x88, 0x44, 0x0f, 0x28, 0xca, 0x0f, 0xc6, 0xd9, 0xdd, 0x41, 0x0f, 0x28, 0xc0, 0x44, 0x0f, 0x58, 0xcc, 0x41, 0x0f,
    0x10, 0x0e, 0x0f, 0x58, 0xc3, 0x41, 0x0f, 0x58, 0xc9, 0x0f, 0x5c, 0xc8, 0x0f, 0x28, 0xc2, 0x41, 0x0f, 0x58, 0xc0, 0x41, 0x0f, 0x11, 0x0e,
    0x8b, 0x4f, 0x10, 0x4c, 0x8b, 0x77, 0x08, 0x0f, 0x58, 0xc4, 0x0f, 0x10, 0x08, 0x48, 0x83, 0xc0, 0x10, 0x49, 0x8d, 0x0c, 0x8e, 0x0f, 0x58,
    0xc3, 0x0f, 0x59, 0xc5, 0x0f, 0x58, 0xc1, 0x44, 0x0f, 0x58, 0xc0, 0x0f, 0x58, 0xe0, 0x0f, 0x58, 0xd8, 0x44, 0x0f, 0x11, 0x01, 0x8b, 0x4f,
    0x28, 0x4c, 0x8b, 0x7f, 0x20, 0x0f, 0x58, 0xc2, 0x83, 0x47, 0x10, 0x04, 0x49, 0x8d, 0x0c, 0x8f, 0x0f, 0x11, 0x21, 0x8b, 0x4f, 0x40, 0x4c,
    0x8b, 0x77, 0x38, 0x83, 0x47, 0x28, 0x04, 0x49, 0x8d, 0x0c, 0x8e, 0x0f, 0x11, 0x19, 0x8b, 0x4f, 0x58, 0x4c, 0x8b, 0x7f, 0x50, 0x83, 0x47,
    0x40, 0x04, 0x49, 0x8d, 0x0c, 0x8f, 0x0f, 0x11, 0x01, 0x83, 0x47, 0x58, 0x04, 0x49, 0x39, 0xc0, 0x0f, 0x85, 0x91, 0xfe, 0xff, 0xff};

u8 outlast_camera2[] = {
    0x44, 0x0f, 0xb7, 0xa8, 0x14, 0x02, 0x00, 0x00, 0xf3, 0x0f, 0x10, 0x42, 0x14, 0x0f, 0xc6, 0xc0, 0x00, 0x44, 0x0f, 0xb7, 0xa0, 0x16, 0x02, 0x00,
    0x00, 0x0f, 0xb7, 0xa8, 0x18, 0x02, 0x00, 0x00, 0x0f, 0xb7, 0x98, 0x1a, 0x02, 0x00, 0x00, 0x41, 0x83, 0xc1, 0x04, 0x44, 0x0f, 0xb7, 0x98, 0x94,
    0x02, 0x00, 0x00, 0x44, 0x0f, 0xb7, 0x90, 0x96, 0x02, 0x00, 0x00, 0x45, 0x0f, 0xb7, 0xf5, 0x44, 0x0f, 0xb7, 0x80, 0x98, 0x02, 0x00, 0x00, 0x0f,
    0xb7, 0xb8, 0x9a, 0x02, 0x00, 0x00, 0x42, 0x0f, 0x59, 0x04, 0xb1, 0x45, 0x0f, 0xb7, 0xf4, 0x41, 0x83, 0xc5, 0x04, 0x41, 0x83, 0xc4, 0x04, 0x0f,
    0x58, 0xc2, 0xf3, 0x0f, 0x10, 0x52, 0x18, 0x0f, 0xc6, 0xd2, 0x00, 0x42, 0x0f, 0x59, 0x14, 0xb1, 0x44, 0x0f, 0xb7, 0xf5, 0x83, 0xc5, 0x04, 0x0f,
    0x58, 0xd0, 0xf3, 0x0f, 0x10, 0x42, 0x1c, 0x0f, 0xc6, 0xc0, 0x00, 0x42, 0x0f, 0x59, 0x04, 0xb1, 0x44, 0x0f, 0xb7, 0xf3, 0x83, 0xc3, 0x04, 0x0f,
    0x58, 0xc2, 0xf3, 0x0f, 0x10, 0x52, 0x20, 0x0f, 0xc6, 0xd2, 0x00, 0x42, 0x0f, 0x59, 0x14, 0xb1, 0x45, 0x0f, 0xb7, 0xf3, 0x41, 0x83, 0xc3, 0x04,
    0x0f, 0x58, 0xd0, 0xf3, 0x0f, 0x10, 0x82, 0x14, 0x01, 0x00, 0x00, 0x0f, 0xc6, 0xc0, 0x00, 0x42, 0x0f, 0x59, 0x04, 0xb1, 0x45, 0x0f, 0xb7, 0xf2,
    0x41, 0x83, 0xc2, 0x04, 0x0f, 0x58, 0xc1, 0xf3, 0x0f, 0x10, 0x8a, 0x18, 0x01, 0x00, 0x00, 0x0f, 0xc6, 0xc9, 0x00, 0x42, 0x0f, 0x59, 0x0c, 0xb1,
    0x45, 0x0f, 0xb7, 0xf0, 0x41, 0x83, 0xc0, 0x04, 0x0f, 0x58, 0xc8, 0xf3, 0x0f, 0x10, 0x82, 0x1c, 0x01, 0x00, 0x00, 0x0f, 0xc6, 0xc0, 0x00, 0x42,
    0x0f, 0x59, 0x04, 0xb1, 0x44, 0x0f, 0xb7, 0xf7, 0x83, 0xc7, 0x04, 0x0f, 0x58, 0xc1, 0xf3, 0x0f, 0x10, 0x8a, 0x20, 0x01, 0x00, 0x00, 0x48, 0x83,
    0xc2, 0x10, 0x0f, 0xc6, 0xc9, 0x00, 0x42, 0x0f, 0x59, 0x0c, 0xb1, 0x66, 0x44, 0x89, 0xa8, 0x14, 0x02, 0x00, 0x00, 0x66, 0x44, 0x89, 0xa0, 0x16,
    0x02, 0x00, 0x00, 0x66, 0x89, 0xa8, 0x18, 0x02, 0x00, 0x00, 0x66, 0x89, 0x98, 0x1a, 0x02, 0x00, 0x00, 0x66, 0x44, 0x89, 0x98, 0x94, 0x02, 0x00,
    0x00, 0x66, 0x44, 0x89, 0x90, 0x96, 0x02, 0x00, 0x00, 0x66, 0x44, 0x89, 0x80, 0x98, 0x02, 0x00, 0x00, 0x66, 0x89, 0xb8, 0x9a, 0x02, 0x00, 0x00,
    0x48, 0x83, 0xc0, 0x08, 0x45, 0x39, 0xcf, 0x0f, 0x58, 0xc8, 0x0f, 0x87, 0xa0, 0xfe, 0xff, 0xff};

int main() {
    g_config.inline_syscalls = false;
    g_config.scan_ahead_multi = false;
    Extensions::G = true;
    Extensions::B = true;
    Extensions::C = true;
    Extensions::V = true;
    Extensions::VLEN = 256;
    Extensions::Zicond = true;
    Handlers::initialize();

    std::unique_ptr<Recompiler> rec_storage = std::make_unique<Recompiler>();
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
        GEN(cmpxchg(ptr[rdi], bl));
        GEN(cmpxchg(ptr[rdi], bh));
        GEN(cmpxchg(ptr[rdi], bx));
        GEN(cmpxchg(ptr[rdi], ebx));
        GEN(cmpxchg(ptr[rdi], rbx));
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

        std::ofstream base("counts/Lock.json");
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

    GEN(cvtss2sd(xmm3, xmm2));
    GEN(cvtss2sd(xmm3, ptr[rdi]));
    GEN(cvtsd2ss(xmm3, xmm2));
    GEN(cvtsd2ss(xmm3, ptr[rdi]));
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

    GEN_SSE(phminposuw);

    GEN_SSE(pcmpeqq);
    GEN_SSE(ptest);

    std::ofstream sse4_1("counts/SSE4_1.json");
    sse4_1 << json.dump(4);
    json.clear();

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

    std::ofstream x87("counts/X87.json");
    x87 << json.dump(4);
    json.clear();

    rec.setFlagMode(FlagMode::Default);
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

    gen_many(rec, "Crysis", json, [](Xbyak::CodeGenerator& x) {
        x.movaps(xmm0, ptr[rdi - 0x20]);
        x.movaps(xmm2, ptr[rdi - 0x40]);
        x.movaps(xmm9, ptr[rdi + 0x20]);
        x.movaps(xmm11, ptr[r9]);
        x.movaps(xmm3, ptr[rdi]);
        x.movaps(xmm7, ptr[rdi - 0x10]);
        x.movaps(xmm8, ptr[r9 + 0x10]);
        x.movaps(xmm1, xmm0);
        x.movaps(xmm5, xmm11);
        x.subps(xmm1, xmm9);
        x.addps(xmm9, xmm0);
        x.movaps(xmm0, xmm2);
        x.mulps(xmm5, xmm1);
        x.subps(xmm0, xmm3);
        x.addps(xmm3, xmm2);
        x.subps(xmm5, xmm9);
        x.movaps(xmm2, ptr[rdi - 0x30]);
        x.movaps(xmm6, xmm3);
        x.movaps(xmm1, xmm0);
        x.subps(xmm1, xmm5);
        x.addps(xmm5, xmm0);
        x.subps(xmm6, xmm9);
        x.movaps(xmm0, ptr[rdi + 0x10]);
        x.movaps(xmm4, xmm0);
        x.movaps(ptr[r10], xmm1);
        x.addps(xmm9, xmm3);
        x.subps(xmm4, xmm7);
        x.addps(xmm7, xmm0);
        x.movaps(xmm3, ptr[rdi + 0x30]);
        x.movaps(xmm0, xmm2);
        x.movaps(xmm10, xmm9);
        x.mulps(xmm8, xmm4);
        x.subps(xmm0, xmm3);
        x.addps(xmm3, xmm2);
        x.movaps(xmm2, xmm4);
        x.movaps(xmm4, xmm5);
        x.movaps(xmm1, xmm3);
        x.subps(xmm1, xmm7);
        x.addps(xmm7, xmm3);
        x.subps(xmm2, xmm0);
        x.movaps(xmm3, ptr[r9 + 0x30]);
        x.mulps(xmm3, xmm0);
        x.movaps(xmm0, ptr[r10]);
        x.mulps(xmm2, ptr[r9 + 0x40]);
        x.mulps(xmm11, xmm1);
        x.movaps(xmm1, xmm0);
        x.subps(xmm10, xmm7);
        x.subps(xmm3, xmm2);
        x.subps(xmm8, xmm2);
        x.movaps(xmm2, xmm6);
        x.subps(xmm3, xmm7);
        x.addps(xmm7, xmm9);
        x.movss(dword[r11 - 0x60], xmm7);
        x.subps(xmm11, xmm3);
        x.subps(xmm4, xmm3);
        x.shufps(xmm7, xmm7, 0xe5);
        x.movss(dword[r11 - 0x40], xmm7);
        x.subps(xmm8, xmm11);
        x.addps(xmm3, xmm5);
        x.shufps(xmm7, xmm7, 0xe6);
        x.movss(dword[rax + rdi], xmm7);
        x.subps(xmm1, xmm11);
        x.addps(xmm11, xmm0);
        x.shufps(xmm7, xmm7, 0xe7);
        x.movss(dword[r11], xmm7);
        x.movss(dword[r11 - 0x5c], xmm3);
        x.subps(xmm2, xmm8);
        x.shufps(xmm3, xmm3, 0xe5);
        x.movss(dword[r11 - 0x3c], xmm3);
        x.addps(xmm8, xmm6);
        x.shufps(xmm3, xmm3, 0xe6);
        x.movss(dword[r11 - 0x1c], xmm3);
        x.shufps(xmm3, xmm3, 0xe7);
        x.movss(dword[r11 + 4], xmm3);
        x.movss(dword[r11 - 0x58], xmm11);
        x.shufps(xmm11, xmm11, 0xe5);
        x.movss(dword[r11 - 0x38], xmm11);
        x.shufps(xmm11, xmm11, 0xe6);
        x.movss(dword[r11 - 0x18], xmm11);
        x.shufps(xmm11, xmm11, 0xe7);
        x.movss(dword[r11 + 8], xmm11);
        x.movss(dword[r11 - 0x54], xmm8);
        x.shufps(xmm8, xmm8, 0xe5);
        x.sub(r11, -0x80);
        x.sub(rdi, -0x80);
        x.sub(rcx, 1);
        x.movss(dword[r11 - 0xb4], xmm8);
        x.shufps(xmm8, xmm8, 0xe6);
        x.movss(dword[r11 - 0x94], xmm8);
        x.shufps(xmm8, xmm8, 0xe7);
        x.movss(dword[r11 - 0x74], xmm8);
        x.movss(dword[r11 - 0xd0], xmm2);
        x.shufps(xmm2, xmm2, 0xe5);
        x.movss(dword[r11 - 0xb0], xmm2);
        x.shufps(xmm2, xmm2, 0xe6);
        x.movss(dword[r11 - 0x90], xmm2);
        x.shufps(xmm2, xmm2, 0xe7);
        x.movss(dword[r11 - 0x70], xmm2);
        x.movss(dword[r11 - 0xcc], xmm1);
        x.shufps(xmm1, xmm1, 0xe5);
        x.movss(dword[r11 - 0xac], xmm1);
        x.shufps(xmm1, xmm1, 0xe6);
        x.movss(dword[r11 - 0x8c], xmm1);
        x.shufps(xmm1, xmm1, 0xe7);
        x.movss(dword[r11 - 0x6c], xmm1);
        x.movss(dword[r11 - 0xc8], xmm4);
        x.shufps(xmm4, xmm4, 0xe5);
        x.movss(dword[r11 - 0xa8], xmm4);
        x.shufps(xmm4, xmm4, 0xe6);
        x.movss(dword[r11 - 0x88], xmm4);
        x.shufps(xmm4, xmm4, 0xe7);
        x.movss(dword[r11 - 0x68], xmm4);
        x.movss(dword[r11 - 0xc4], xmm10);
        x.shufps(xmm10, xmm10, 0xe5);
        x.movss(dword[r11 - 0xa4], xmm10);
        x.shufps(xmm10, xmm10, 0xe6);
        x.movss(dword[r11 - 0x84], xmm10);
        x.shufps(xmm10, xmm10, 0xe7);
        x.movss(dword[r11 - 0x64], xmm10);
        x.hlt();
    });

    gen_many(rec, "XXHash", json, [](Xbyak::CodeGenerator& x) {
        x.prefetcht0(ptr[r9 + r10 * 8 + 0x140]);
        x.movdqu(xmm11, ptr[r9 + r10 * 8]);
        x.movdqu(xmm12, ptr[r9 + r10 * 8 + 0x10]);
        x.movdqu(xmm13, ptr[r9 + r10 * 8 + 0x20]);
        x.movdqu(xmm9, ptr[r9 + r10 * 8 + 0x30]);
        x.movdqu(xmm14, ptr[r10 + r8 * 1]);
        x.movdqu(xmm15, ptr[r10 + r8 * 1 + 0x10]);
        x.movdqu(xmm4, ptr[r10 + r8 * 1 + 0x20]);
        x.movdqu(xmm10, ptr[r10 + r8 * 1 + 0x30]);
        x.pxor(xmm14, xmm11);
        x.pshufd(xmm6, xmm14, 0xF5);
        x.pmuludq(xmm6, xmm14);
        x.pshufd(xmm11, xmm11, 0x4E);
        x.paddq(xmm3, xmm11);
        x.paddq(xmm3, xmm6);
        x.pxor(xmm15, xmm12);
        x.pshufd(xmm6, xmm15, 0xF5);
        x.pmuludq(xmm6, xmm15);
        x.pshufd(xmm11, xmm12, 0x4E);
        x.paddq(xmm2, xmm11);
        x.paddq(xmm2, xmm6);
        x.pxor(xmm4, xmm13);
        x.pshufd(xmm6, xmm4, 0xF5);
        x.pmuludq(xmm6, xmm4);
        x.pshufd(xmm4, xmm13, 0x4E);
        x.paddq(xmm1, xmm4);
        x.paddq(xmm1, xmm6);
        x.pxor(xmm10, xmm9);
        x.pshufd(xmm4, xmm10, 0xF5);
        x.pmuludq(xmm4, xmm10);
        x.pshufd(xmm6, xmm9, 0x4E);
        x.paddq(xmm0, xmm6);
        x.paddq(xmm0, xmm4);
        x.add(r10, 0x08);
        x.cmp(r10, 0x80);
        x.jmp(rax);
    });

    gen_many(rec, "Baldur 2 FMV", json, [](Xbyak::CodeGenerator& x) {
        x.mov(rax, rsp);
        x.and_(rsp, 0xFFFFFFF0);
        x.lea(rsp, ptr[rsp - 0x08]);
        x.push(rax);
        x.sub(rsp, 0x100);
        x.lea(rax, ptr[rdi + rsi * 1]);
        x.lea(r9, ptr[rsi + rsi * 2]);
        x.lea(r10, ptr[r9 + rsi * 2]);
        x.lea(r11, ptr[r9 + rsi * 4]);
        x.movdqa(xmm0, ptr[rdi + rsi * 8]);
        x.movdqa(xmm1, xmm0);
        x.punpcklbw(xmm0, ptr[rax + rsi * 8]);
        x.punpckhbw(xmm1, ptr[rax + rsi * 8]);
        x.movdqa(xmm2, ptr[rdi + r10 * 2]);
        x.movdqa(xmm3, xmm2);
        x.punpcklbw(xmm2, ptr[rax + r10 * 2]);
        x.punpckhbw(xmm3, ptr[rax + r10 * 2]);
        x.movdqa(xmm4, xmm0);
        x.punpcklwd(xmm0, xmm2);
        x.punpckhwd(xmm4, xmm2);
        x.movdqa(xmm2, xmm1);
        x.punpcklwd(xmm1, xmm3);
        x.punpckhwd(xmm2, xmm3);
        x.movdqa(xmm3, ptr[rdi + r9 * 4]);
        x.movdqa(xmm5, xmm3);
        x.punpcklbw(xmm3, ptr[rax + r9 * 4]);
        x.punpckhbw(xmm5, ptr[rax + r9 * 4]);
        x.movdqa(xmm6, ptr[rdi + r11 * 2]);
        x.movdqa(xmm7, xmm6);
        x.punpcklbw(xmm6, ptr[rax + r11 * 2]);
        x.punpckhbw(xmm7, ptr[rax + r11 * 2]);
        x.movdqa(xmm8, xmm3);
        x.punpcklwd(xmm3, xmm6);
        x.punpckhwd(xmm8, xmm6);
        x.movdqa(xmm6, xmm5);
        x.punpcklwd(xmm5, xmm7);
        x.punpckhwd(xmm6, xmm7);
        x.movdqa(xmm7, xmm0);
        x.punpckldq(xmm0, xmm3);
        x.punpckhdq(xmm7, xmm3);
        x.movdqa(xmm3, xmm4);
        x.punpckldq(xmm4, xmm8);
        x.punpckhdq(xmm3, xmm8);
        x.movdqa(xmm8, xmm1);
        x.punpckldq(xmm1, xmm5);
        x.punpckhdq(xmm8, xmm5);
        x.movdqa(xmm5, xmm2);
        x.punpckldq(xmm2, xmm6);
        x.punpckhdq(xmm5, xmm6);
        x.movdqa(ptr[rsp], xmm0);
        x.movdqa(ptr[rsp + 0x10], xmm7);
        x.movdqa(ptr[rsp + 0x20], xmm4);
        x.movdqa(ptr[rsp + 0x30], xmm3);
        x.movdqa(ptr[rsp + 0x40], xmm1);
        x.movdqa(ptr[rsp + 0x50], xmm8);
        x.movdqa(ptr[rsp + 0x60], xmm2);
        x.movdqa(ptr[rsp + 0x70], xmm5);
        x.movdqa(xmm0, ptr[rdi]);
        x.movdqa(xmm1, xmm0);
        x.punpcklbw(xmm0, ptr[rax]);
        x.punpckhbw(xmm1, ptr[rax]);
        x.movdqa(xmm2, ptr[rdi + rsi * 2]);
        x.movdqa(xmm3, xmm2);
        x.punpcklbw(xmm2, ptr[rax + rsi * 2]);
        x.punpckhbw(xmm3, ptr[rax + rsi * 2]);
        x.movdqa(xmm4, xmm0);
        x.punpcklwd(xmm0, xmm2);
        x.punpckhwd(xmm4, xmm2);
        x.movdqa(xmm2, xmm1);
        x.punpcklwd(xmm1, xmm3);
        x.punpckhwd(xmm2, xmm3);
        x.movdqa(xmm3, ptr[rdi + rsi * 4]);
        x.movdqa(xmm5, xmm3);
        x.punpcklbw(xmm3, ptr[rax + rsi * 4]);
        x.punpckhbw(xmm5, ptr[rax + rsi * 4]);
        x.movdqa(xmm6, ptr[rdi + r9 * 2]);
        x.movdqa(xmm7, xmm6);
        x.punpcklbw(xmm6, ptr[rax + r9 * 2]);
        x.punpckhbw(xmm7, ptr[rax + r9 * 2]);
        x.movdqa(xmm8, xmm3);
        x.punpcklwd(xmm3, xmm6);
        x.punpckhwd(xmm8, xmm6);
        x.movdqa(xmm6, xmm5);
        x.punpcklwd(xmm5, xmm7);
        x.punpckhwd(xmm6, xmm7);
        x.movdqa(xmm7, xmm0);
        x.punpckldq(xmm0, xmm3);
        x.punpckhdq(xmm7, xmm3);
        x.movdqa(xmm3, xmm4);
        x.punpckldq(xmm4, xmm8);
        x.punpckhdq(xmm3, xmm8);
        x.movdqa(xmm8, xmm1);
        x.punpckldq(xmm1, xmm5);
        x.punpckhdq(xmm8, xmm5);
        x.movdqa(xmm5, xmm2);
        x.punpckldq(xmm2, xmm6);
        x.punpckhdq(xmm5, xmm6);
        x.movdqa(xmm6, xmm0);
        x.punpcklqdq(xmm0, ptr[rsp]);
        x.punpckhqdq(xmm6, ptr[rsp]);
        x.movdqa(xmm9, xmm7);
        x.punpcklqdq(xmm7, ptr[rsp + 0x10]);
        x.punpckhqdq(xmm9, ptr[rsp + 0x10]);
        x.movdqa(xmm10, xmm4);
        x.punpcklqdq(xmm4, ptr[rsp + 0x20]);
        x.punpckhqdq(xmm10, ptr[rsp + 0x20]);
        x.movdqa(xmm11, xmm3);
        x.punpcklqdq(xmm3, ptr[rsp + 0x30]);
        x.punpckhqdq(xmm11, ptr[rsp + 0x30]);
        x.movdqa(xmm12, xmm1);
        x.punpcklqdq(xmm1, ptr[rsp + 0x40]);
        x.punpckhqdq(xmm12, ptr[rsp + 0x40]);
        x.movdqa(xmm13, xmm8);
        x.punpcklqdq(xmm8, ptr[rsp + 0x50]);
        x.punpckhqdq(xmm13, ptr[rsp + 0x50]);
        x.movdqa(xmm14, xmm2);
        x.punpcklqdq(xmm2, ptr[rsp + 0x60]);
        x.punpckhqdq(xmm14, ptr[rsp + 0x60]);
        x.movdqa(xmm15, xmm5);
        x.punpcklqdq(xmm5, ptr[rsp + 0x70]);
        x.punpckhqdq(xmm15, ptr[rsp + 0x70]);
        x.movdqa(ptr[rsp], xmm0);
        x.movdqa(ptr[rsp + 0x10], xmm6);
        x.movdqa(ptr[rsp + 0x20], xmm7);
        x.movdqa(ptr[rsp + 0x30], xmm9);
        x.movdqa(ptr[rsp + 0x40], xmm4);
        x.movdqa(ptr[rsp + 0x50], xmm10);
        x.movdqa(ptr[rsp + 0x60], xmm3);
        x.movdqa(ptr[rsp + 0x70], xmm11);
        x.movdqa(ptr[rsp + 0x80], xmm1);
        x.movdqa(ptr[rsp + 0x90], xmm12);
        x.movdqa(ptr[rsp + 0xA0], xmm8);
        x.movdqa(ptr[rsp + 0xB0], xmm13);
        x.movdqa(ptr[rsp + 0xC0], xmm2);
        x.movdqa(ptr[rsp + 0xD0], xmm14);
        x.movdqa(ptr[rsp + 0xE0], xmm5);
        x.movdqa(ptr[rsp + 0xF0], xmm15);
        x.movdqa(xmm12, xmm6);
        x.movdqa(xmm13, xmm7);
        x.pxor(xmm7, xmm7);
        x.movdqa(xmm5, xmm12);
        x.psubusb(xmm5, xmm0);
        x.psubusb(xmm0, xmm12);
        x.por(xmm0, xmm5);
        x.movdqa(xmm5, xmm13);
        x.psubusb(xmm5, xmm12);
        x.psubusb(xmm12, xmm13);
        x.por(xmm12, xmm5);
        x.pmaxub(xmm0, xmm12);
        x.movdqa(xmm6, xmm13);
        x.movdqa(xmm5, xmm9);
        x.psubusb(xmm5, xmm6);
        x.psubusb(xmm6, xmm9);
        x.por(xmm6, xmm5);
        x.movdqa(xmm5, xmm4);
        x.psubusb(xmm5, xmm9);
        x.psubusb(xmm9, xmm4);
        x.por(xmm9, xmm5);
        x.movdqa(xmm5, xmm10);
        x.psubusb(xmm5, xmm4);
        x.psubusb(xmm4, xmm10);
        x.por(xmm4, xmm5);
        x.pmaxub(xmm4, xmm6);
        x.pmaxub(xmm0, xmm4);
        x.movdqa(xmm5, xmm10);
        x.psubusb(xmm5, xmm13);
        x.psubusb(xmm13, xmm10);
        x.por(xmm13, xmm5);
        x.movdqa(xmm5, xmm3);
        x.psubusb(xmm5, xmm10);
        x.psubusb(xmm10, xmm3);
        x.por(xmm10, xmm5);
        x.pmaxub(xmm0, xmm10);
        x.movdqa(xmm5, xmm11);
        x.psubusb(xmm5, xmm3);
        x.psubusb(xmm3, xmm11);
        x.por(xmm3, xmm5);
        x.pmaxub(xmm0, xmm3);
        x.paddusb(xmm9, xmm9);
        x.pand(xmm13, ptr[rdi]);
        x.psrlw(xmm13, 0x01);
        x.paddusb(xmm9, xmm13);
        x.psubusb(xmm0, ptr[rcx]);
        x.psubusb(xmm9, ptr[rdx]);
        x.por(xmm0, xmm9);
        x.pcmpeqb(xmm0, xmm7);
        x.psubusb(xmm4, ptr[r8]);
        x.pcmpeqb(xmm4, xmm7);
        x.movdqa(xmm1, ptr[rsp + 0x20]);
        x.movdqa(xmm2, ptr[rsp + 0x30]);
        x.movdqa(xmm8, ptr[rsp + 0x40]);
        x.movdqa(xmm9, ptr[rsp + 0x50]);
        x.movdqa(xmm6, xmm4);
        x.pxor(xmm1, ptr[rdi]);
        x.pxor(xmm9, ptr[rdi]);
        x.movdqa(xmm5, xmm1);
        x.psubsb(xmm5, xmm9);
        x.pandn(xmm6, xmm5);
        x.pxor(xmm2, ptr[rdi]);
        x.pxor(xmm8, ptr[rdi]);
        x.movdqa(xmm5, xmm8);
        x.psubsb(xmm5, xmm2);
        x.paddsb(xmm6, xmm5);
        x.paddsb(xmm6, xmm5);
        x.paddsb(xmm6, xmm5);
        x.pand(xmm0, xmm6);
        x.movdqa(xmm6, xmm0);
        x.paddsb(xmm0, ptr[rdi]);
        x.paddsb(xmm6, ptr[rdi]);
        x.movdqa(xmm5, xmm7);
        x.pcmpgtb(xmm5, xmm0);
        x.psrlw(xmm0, 0x03);
        x.pand(xmm5, ptr[rdi]);
        x.pand(xmm0, ptr[rdi]);
        x.por(xmm0, xmm5);
        x.psubsb(xmm8, xmm0);
        x.pxor(xmm8, ptr[rdi]);
        x.movdqa(xmm5, xmm7);
        x.pcmpgtb(xmm5, xmm6);
        x.psrlw(xmm6, 0x03);
        x.pand(xmm5, ptr[rdi]);
        x.pand(xmm6, ptr[rdi]);
        x.por(xmm6, xmm5);
        x.paddsb(xmm2, xmm6);
        x.pxor(xmm2, ptr[rdi]);
        x.paddsb(xmm0, ptr[rdi]);
        x.movdqa(xmm5, xmm7);
        x.pcmpgtb(xmm5, xmm0);
        x.psrlw(xmm0, 0x01);
        x.pand(xmm5, ptr[rdi]);
        x.pand(xmm0, ptr[rdi]);
        x.por(xmm0, xmm5);
        x.pand(xmm0, xmm4);
        x.psubsb(xmm9, xmm0);
        x.pxor(xmm9, ptr[rdi]);
        x.paddsb(xmm1, xmm0);
        x.pxor(xmm1, ptr[rdi]);
        x.movdqa(ptr[rsp + 0x20], xmm1);
        x.movdqa(ptr[rsp + 0x30], xmm2);
        x.movdqa(ptr[rsp + 0x40], xmm8);
        x.movdqa(ptr[rsp + 0x50], xmm9);
        x.movdqa(xmm0, ptr[rsp + 0x60]);
        x.movdqa(xmm1, ptr[rsp + 0x70]);
        x.movdqa(xmm2, ptr[rsp + 0x80]);
        x.movdqa(xmm4, ptr[rsp + 0x90]);
        x.movdqa(xmm10, ptr[rsp + 0xA0]);
        x.movdqa(xmm11, ptr[rsp + 0xB0]);
        x.movdqa(xmm5, xmm9);
        x.psubusb(xmm5, xmm8);
        x.psubusb(xmm8, xmm9);
        x.por(xmm8, xmm5);
        x.movdqa(xmm5, xmm0);
        x.psubusb(xmm5, xmm9);
        x.psubusb(xmm9, xmm0);
        x.por(xmm9, xmm5);
        x.pmaxub(xmm8, xmm9);
        x.movdqa(xmm5, xmm2);
        x.psubusb(xmm5, xmm1);
        x.psubusb(xmm1, xmm2);
        x.por(xmm1, xmm5);
        x.movdqa(xmm5, xmm4);
        x.psubusb(xmm5, xmm2);
        x.psubusb(xmm2, xmm4);
        x.por(xmm2, xmm5);
        x.pmaxub(xmm2, xmm3);
        x.pmaxub(xmm8, xmm2);
        x.movdqa(xmm5, xmm4);
        x.psubusb(xmm5, xmm0);
        x.psubusb(xmm0, xmm4);
        x.por(xmm0, xmm5);
        x.movdqa(xmm5, xmm10);
        x.psubusb(xmm5, xmm4);
        x.psubusb(xmm4, xmm10);
        x.por(xmm4, xmm5);
        x.pmaxub(xmm8, xmm4);
        x.movdqa(xmm5, xmm11);
        x.psubusb(xmm5, xmm10);
        x.psubusb(xmm10, xmm11);
        x.por(xmm10, xmm5);
        x.pmaxub(xmm8, xmm10);
        x.paddusb(xmm1, xmm1);
        x.pand(xmm0, ptr[rdi]);
        x.psrlw(xmm0, 0x01);
        x.paddusb(xmm1, xmm0);
        x.psubusb(xmm8, ptr[rcx]);
        x.psubusb(xmm1, ptr[rdx]);
        x.por(xmm8, xmm1);
        x.pcmpeqb(xmm8, xmm7);
        x.psubusb(xmm2, ptr[r8]);
        x.pcmpeqb(xmm2, xmm7);
        x.movdqa(xmm0, ptr[rsp + 0x60]);
        x.movdqa(xmm1, ptr[rsp + 0x70]);
        x.movdqa(xmm3, ptr[rsp + 0x80]);
        x.movdqa(xmm4, ptr[rsp + 0x90]);
        x.movdqa(xmm6, xmm2);
        x.pxor(xmm0, ptr[rdi]);
        x.pxor(xmm4, ptr[rdi]);
        x.movdqa(xmm5, xmm0);
        x.psubsb(xmm5, xmm4);
        x.pandn(xmm6, xmm5);
        x.pxor(xmm1, ptr[rdi]);
        x.pxor(xmm3, ptr[rdi]);
        x.movdqa(xmm5, xmm3);
        x.psubsb(xmm5, xmm1);
        x.paddsb(xmm6, xmm5);
        x.paddsb(xmm6, xmm5);
        x.paddsb(xmm6, xmm5);
        x.pand(xmm8, xmm6);
        x.movdqa(xmm6, xmm8);
        x.paddsb(xmm8, ptr[rdi]);
        x.paddsb(xmm6, ptr[rdi]);
        x.movdqa(xmm5, xmm7);
        x.pcmpgtb(xmm5, xmm8);
        x.psrlw(xmm8, 0x03);
        x.pand(xmm5, ptr[rdi]);
        x.pand(xmm8, ptr[rdi]);
        x.por(xmm8, xmm5);
        x.psubsb(xmm3, xmm8);
        x.pxor(xmm3, ptr[rdi]);
        x.movdqa(xmm5, xmm7);
        x.pcmpgtb(xmm5, xmm6);
        x.psrlw(xmm6, 0x03);
        x.pand(xmm5, ptr[rdi]);
        x.pand(xmm6, ptr[rdi]);
        x.por(xmm6, xmm5);
        x.paddsb(xmm1, xmm6);
        x.pxor(xmm1, ptr[rdi]);
        x.paddsb(xmm8, ptr[rdi]);
        x.movdqa(xmm5, xmm7);
        x.pcmpgtb(xmm5, xmm8);
        x.psrlw(xmm8, 0x01);
        x.pand(xmm5, ptr[rdi]);
        x.pand(xmm8, ptr[rdi]);
        x.por(xmm8, xmm5);
        x.pand(xmm8, xmm2);
        x.psubsb(xmm4, xmm8);
        x.pxor(xmm4, ptr[rdi]);
        x.paddsb(xmm0, xmm8);
        x.pxor(xmm0, ptr[rdi]);
        x.movdqa(ptr[rsp + 0x60], xmm0);
        x.movdqa(ptr[rsp + 0x70], xmm1);
        x.movdqa(ptr[rsp + 0x80], xmm3);
        x.movdqa(ptr[rsp + 0x90], xmm4);
        x.movdqa(xmm0, ptr[rsp + 0xA0]);
        x.movdqa(xmm1, ptr[rsp + 0xB0]);
        x.movdqa(xmm2, ptr[rsp + 0xC0]);
        x.movdqa(xmm8, ptr[rsp + 0xD0]);
        x.movdqa(xmm9, ptr[rsp + 0xE0]);
        x.movdqa(xmm11, ptr[rsp + 0xF0]);
        x.movdqa(xmm5, xmm4);
        x.psubusb(xmm5, xmm3);
        x.psubusb(xmm3, xmm4);
        x.por(xmm3, xmm5);
        x.movdqa(xmm5, xmm0);
        x.psubusb(xmm5, xmm4);
        x.psubusb(xmm4, xmm0);
        x.por(xmm4, xmm5);
        x.pmaxub(xmm3, xmm4);
        x.movdqa(xmm5, xmm2);
        x.psubusb(xmm5, xmm1);
        x.psubusb(xmm1, xmm2);
        x.por(xmm1, xmm5);
        x.movdqa(xmm5, xmm8);
        x.psubusb(xmm5, xmm2);
        x.psubusb(xmm2, xmm8);
        x.por(xmm2, xmm5);
        x.pmaxub(xmm2, xmm10);
        x.pmaxub(xmm3, xmm2);
        x.movdqa(xmm5, xmm8);
        x.psubusb(xmm5, xmm0);
        x.psubusb(xmm0, xmm8);
        x.por(xmm0, xmm5);
        x.movdqa(xmm5, xmm9);
        x.psubusb(xmm5, xmm8);
        x.psubusb(xmm8, xmm9);
        x.por(xmm8, xmm5);
        x.pmaxub(xmm3, xmm8);
        x.movdqa(xmm5, xmm11);
        x.psubusb(xmm5, xmm9);
        x.psubusb(xmm9, xmm11);
        x.por(xmm9, xmm5);
        x.pmaxub(xmm3, xmm9);
        x.paddusb(xmm1, xmm1);
        x.pand(xmm0, ptr[rdi]);
        x.psrlw(xmm0, 0x01);
        x.paddusb(xmm1, xmm0);
        x.psubusb(xmm3, ptr[rcx]);
        x.psubusb(xmm1, ptr[rdx]);
        x.por(xmm3, xmm1);
        x.pcmpeqb(xmm3, xmm7);
        x.psubusb(xmm2, ptr[r8]);
        x.pcmpeqb(xmm2, xmm7);
        x.movdqa(xmm0, ptr[rsp + 0xA0]);
        x.movdqa(xmm1, ptr[rsp + 0xB0]);
        x.movdqa(xmm4, ptr[rsp + 0xC0]);
        x.movdqa(xmm8, ptr[rsp + 0xD0]);
        x.movdqa(xmm6, xmm2);
        x.pxor(xmm0, ptr[rdi]);
        x.pxor(xmm8, ptr[rdi]);
        x.movdqa(xmm5, xmm0);
        x.psubsb(xmm5, xmm8);
        x.pandn(xmm6, xmm5);
        x.pxor(xmm1, ptr[rdi]);
        x.pxor(xmm4, ptr[rdi]);
        x.movdqa(xmm5, xmm4);
        x.psubsb(xmm5, xmm1);
        x.paddsb(xmm6, xmm5);
        x.paddsb(xmm6, xmm5);
        x.paddsb(xmm6, xmm5);
        x.pand(xmm3, xmm6);
        x.movdqa(xmm6, xmm3);
        x.paddsb(xmm3, ptr[rdi]);
        x.paddsb(xmm6, ptr[rdi]);
        x.movdqa(xmm5, xmm7);
        x.pcmpgtb(xmm5, xmm3);
        x.psrlw(xmm3, 0x03);
        x.pand(xmm5, ptr[rdi]);
        x.pand(xmm3, ptr[rdi]);
        x.por(xmm3, xmm5);
        x.psubsb(xmm4, xmm3);
        x.pxor(xmm4, ptr[rdi]);
        x.movdqa(xmm5, xmm7);
        x.pcmpgtb(xmm5, xmm6);
        x.psrlw(xmm6, 0x03);
        x.pand(xmm5, ptr[rdi]);
        x.pand(xmm6, ptr[rdi]);
        x.por(xmm6, xmm5);
        x.paddsb(xmm1, xmm6);
        x.pxor(xmm1, ptr[rdi]);
        x.paddsb(xmm3, ptr[rdi]);
        x.movdqa(xmm5, xmm7);
        x.pcmpgtb(xmm5, xmm3);
        x.psrlw(xmm3, 0x01);
        x.pand(xmm5, ptr[rdi]);
        x.pand(xmm3, ptr[rdi]);
        x.por(xmm3, xmm5);
        x.pand(xmm3, xmm2);
        x.psubsb(xmm8, xmm3);
        x.pxor(xmm8, ptr[rdi]);
        x.paddsb(xmm0, xmm3);
        x.pxor(xmm0, ptr[rdi]);
        x.movdqa(ptr[rsp + 0xA0], xmm0);
        x.movdqa(ptr[rsp + 0xB0], xmm1);
        x.movdqa(ptr[rsp + 0xC0], xmm4);
        x.movdqa(ptr[rsp + 0xD0], xmm8);
        x.movdqa(xmm0, ptr[rsp + 0x80]);
        x.movdqa(xmm1, xmm0);
        x.punpcklbw(xmm0, ptr[rsp + 0x90]);
        x.punpckhbw(xmm1, ptr[rsp + 0x90]);
        x.movdqa(xmm2, ptr[rsp + 0xA0]);
        x.movdqa(xmm3, xmm2);
        x.punpcklbw(xmm2, ptr[rsp + 0xB0]);
        x.punpckhbw(xmm3, ptr[rsp + 0xB0]);
        x.movdqa(xmm4, xmm0);
        x.punpcklwd(xmm0, xmm2);
        x.punpckhwd(xmm4, xmm2);
        x.movdqa(xmm2, xmm1);
        x.punpcklwd(xmm1, xmm3);
        x.punpckhwd(xmm2, xmm3);
        x.movdqa(xmm3, ptr[rsp + 0xC0]);
        x.movdqa(xmm5, xmm3);
        x.punpcklbw(xmm3, ptr[rsp + 0xD0]);
        x.punpckhbw(xmm5, ptr[rsp + 0xD0]);
        x.movdqa(xmm6, ptr[rsp + 0xE0]);
        x.movdqa(xmm7, xmm6);
        x.punpcklbw(xmm6, ptr[rsp + 0xF0]);
        x.punpckhbw(xmm7, ptr[rsp + 0xF0]);
        x.movdqa(xmm8, xmm3);
        x.punpcklwd(xmm3, xmm6);
        x.punpckhwd(xmm8, xmm6);
        x.movdqa(xmm6, xmm5);
        x.punpcklwd(xmm5, xmm7);
        x.punpckhwd(xmm6, xmm7);
        x.movdqa(xmm7, xmm0);
        x.punpckldq(xmm0, xmm3);
        x.punpckhdq(xmm7, xmm3);
        x.movdqa(xmm3, xmm4);
        x.punpckldq(xmm4, xmm8);
        x.punpckhdq(xmm3, xmm8);
        x.movdqa(xmm8, xmm1);
        x.punpckldq(xmm1, xmm5);
        x.punpckhdq(xmm8, xmm5);
        x.movdqa(xmm5, xmm2);
        x.punpckldq(xmm2, xmm6);
        x.punpckhdq(xmm5, xmm6);
        x.movdqa(ptr[rsp + 0x80], xmm0);
        x.movdqa(ptr[rsp + 0x90], xmm7);
        x.movdqa(ptr[rsp + 0xA0], xmm4);
        x.movdqa(ptr[rsp + 0xB0], xmm3);
        x.movdqa(ptr[rsp + 0xC0], xmm1);
        x.movdqa(ptr[rsp + 0xD0], xmm8);
        x.movdqa(ptr[rsp + 0xE0], xmm2);
        x.movdqa(ptr[rsp + 0xF0], xmm5);
        x.movdqa(xmm0, ptr[rsp]);
        x.movdqa(xmm1, xmm0);
        x.punpcklbw(xmm0, ptr[rsp + 0x10]);
        x.punpckhbw(xmm1, ptr[rsp + 0x10]);
        x.movdqa(xmm2, ptr[rsp + 0x20]);
        x.movdqa(xmm3, xmm2);
        x.punpcklbw(xmm2, ptr[rsp + 0x30]);
        x.punpckhbw(xmm3, ptr[rsp + 0x30]);
        x.movdqa(xmm4, xmm0);
        x.punpcklwd(xmm0, xmm2);
        x.punpckhwd(xmm4, xmm2);
        x.movdqa(xmm2, xmm1);
        x.punpcklwd(xmm1, xmm3);
        x.punpckhwd(xmm2, xmm3);
        x.movdqa(xmm3, ptr[rsp + 0x40]);
        x.movdqa(xmm5, xmm3);
        x.punpcklbw(xmm3, ptr[rsp + 0x50]);
        x.punpckhbw(xmm5, ptr[rsp + 0x50]);
        x.movdqa(xmm6, ptr[rsp + 0x60]);
        x.movdqa(xmm7, xmm6);
        x.punpcklbw(xmm6, ptr[rsp + 0x70]);
        x.punpckhbw(xmm7, ptr[rsp + 0x70]);
        x.movdqa(xmm8, xmm3);
        x.punpcklwd(xmm3, xmm6);
        x.punpckhwd(xmm8, xmm6);
        x.movdqa(xmm6, xmm5);
        x.punpcklwd(xmm5, xmm7);
        x.punpckhwd(xmm6, xmm7);
        x.movdqa(xmm7, xmm0);
        x.punpckldq(xmm0, xmm3);
        x.punpckhdq(xmm7, xmm3);
        x.movdqa(xmm3, xmm4);
        x.punpckldq(xmm4, xmm8);
        x.punpckhdq(xmm3, xmm8);
        x.movdqa(xmm8, xmm1);
        x.punpckldq(xmm1, xmm5);
        x.punpckhdq(xmm8, xmm5);
        x.movdqa(xmm5, xmm2);
        x.punpckldq(xmm2, xmm6);
        x.punpckhdq(xmm5, xmm6);
        x.movdqa(xmm6, xmm0);
        x.punpcklqdq(xmm0, ptr[rsp + 0x80]);
        x.punpckhqdq(xmm6, ptr[rsp + 0x80]);
        x.movdqa(xmm9, xmm7);
        x.punpcklqdq(xmm7, ptr[rsp + 0x90]);
        x.punpckhqdq(xmm9, ptr[rsp + 0x90]);
        x.movdqa(xmm10, xmm4);
        x.punpcklqdq(xmm4, ptr[rsp + 0xA0]);
        x.punpckhqdq(xmm10, ptr[rsp + 0xA0]);
        x.movdqa(xmm11, xmm3);
        x.punpcklqdq(xmm3, ptr[rsp + 0xB0]);
        x.punpckhqdq(xmm11, ptr[rsp + 0xB0]);
        x.movdqa(xmm12, xmm1);
        x.punpcklqdq(xmm1, ptr[rsp + 0xC0]);
        x.punpckhqdq(xmm12, ptr[rsp + 0xC0]);
        x.movdqa(xmm13, xmm8);
        x.punpcklqdq(xmm8, ptr[rsp + 0xD0]);
        x.punpckhqdq(xmm13, ptr[rsp + 0xD0]);
        x.movdqa(xmm14, xmm2);
        x.punpcklqdq(xmm2, ptr[rsp + 0xE0]);
        x.punpckhqdq(xmm14, ptr[rsp + 0xE0]);
        x.movdqa(xmm15, xmm5);
        x.punpcklqdq(xmm5, ptr[rsp + 0xF0]);
        x.punpckhqdq(xmm15, ptr[rsp + 0xF0]);
        x.movdqa(ptr[rdi], xmm0);
        x.movdqa(ptr[rax], xmm6);
        x.movdqa(ptr[rdi + rsi * 2], xmm7);
        x.movdqa(ptr[rax + rsi * 2], xmm9);
        x.movdqa(ptr[rdi + rsi * 4], xmm4);
        x.movdqa(ptr[rax + rsi * 4], xmm10);
        x.movdqa(ptr[rdi + r9 * 2], xmm3);
        x.movdqa(ptr[rax + r9 * 2], xmm11);
        x.movdqa(ptr[rdi + rsi * 8], xmm1);
        x.movdqa(ptr[rax + rsi * 8], xmm12);
        x.movdqa(ptr[rdi + r10 * 2], xmm8);
        x.movdqa(ptr[rax + r10 * 2], xmm13);
        x.movdqa(ptr[rdi + r9 * 4], xmm2);
        x.movdqa(ptr[rax + r9 * 4], xmm14);
        x.movdqa(ptr[rdi + r11 * 2], xmm5);
        x.movdqa(ptr[rax + r11 * 2], xmm15);
        x.add(rsp, 0x100);
        x.pop(rsp);
        x.ret();
    });

    gen_many(rec, "dotnet rotate", json, [](Xbyak::CodeGenerator& x) { std::memcpy((u8*)x.getCurr(), dotnet_block, sizeof(dotnet_block)); });
    gen_many(rec, "7z block", json, [](Xbyak::CodeGenerator& x) { std::memcpy((u8*)x.getCurr(), z7_block, sizeof(z7_block)); });
    gen_many(rec, "Outlast camera", json, [](Xbyak::CodeGenerator& x) { std::memcpy((u8*)x.getCurr(), outlast_camera, sizeof(outlast_camera)); });
    gen_many(rec, "Outlast camera 2", json, [](Xbyak::CodeGenerator& x) { std::memcpy((u8*)x.getCurr(), outlast_camera2, sizeof(outlast_camera2)); });

    std::ofstream many("counts/HotBlocks.json");
    many << json.dump(4);
    json.clear();
}