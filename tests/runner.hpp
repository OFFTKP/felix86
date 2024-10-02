#define CATCH_CONFIG_PREFIX_MESSAGES
#include <catch2/catch_test_macros.hpp>
#include <xbyak/xbyak.h>
#include "felix86/common/utility.hpp"
#include "felix86/emulator.hpp"

using namespace Xbyak;
using namespace Xbyak::util;

#define FELIX86_TEST(name) struct Code_##name final : Xbyak::CodeGenerator { \
    Code_##name(); \
    void verify(x86_ref_e ref, u64 value) { checks.push_back({ ref, value }); } \
    void verify_memory(void* mem, u64 value, u8 size) { mem_checks.push_back({ mem, value, size }); } \
    void verify_xmm(x86_ref_e ref, XmmReg reg) { xmm_checks.push_back({ ref, reg }); } \
    void verify_c(bool value) { c = value; } \
    void verify_p(bool value) { p = value; } \
    void verify_a(bool value) { a = value; } \
    void verify_z(bool value) { z = value; } \
    void verify_s(bool value) { s = value; } \
    void verify_o(bool value) { o = value; } \
    u8* data; \
    u8* stack; \
private: \
    void emit_code(); \
    void verify_checks(const Emulator& emulator) { \
        for (auto& check : checks) { \
            REQUIRE(emulator.GetGpr(check.first) == check.second); \
        } \
        if (c.has_value()) REQUIRE(!!(emulator.GetFlag(X86_REF_CF)) == c.value()); \
        if (p.has_value()) REQUIRE(!!(emulator.GetFlag(X86_REF_PF)) == p.value()); \
        if (a.has_value()) REQUIRE(!!(emulator.GetFlag(X86_REF_AF)) == a.value()); \
        if (z.has_value()) REQUIRE(!!(emulator.GetFlag(X86_REF_ZF)) == z.value()); \
        if (s.has_value()) REQUIRE(!!(emulator.GetFlag(X86_REF_SF)) == s.value()); \
        if (o.has_value()) REQUIRE(!!(emulator.GetFlag(X86_REF_OF)) == o.value()); \
        for (auto& check : xmm_checks) { \
            XmmReg has = emulator.GetXmmReg(std::get<0>(check)); \
            XmmReg expected = std::get<1>(check); \
            for (size_t i = 0; i < sizeof(XmmReg) / sizeof(u64); i++) { \
                REQUIRE(has.data[i] == expected.data[i]); \
            } \
        } \
        for (auto& check : mem_checks) { \
            void* mem = std::get<0>(check); \
            u64 value = std::get<1>(check); \
            u8 size = std::get<2>(check); \
            switch (size) { \
                case 1: REQUIRE(*(u8*)mem == (u8)value); break; \
                case 2: REQUIRE(*(u16*)mem == (u16)value); break; \
                case 4: REQUIRE(*(u32*)mem == (u32)value); break; \
                case 8: REQUIRE(*(u64*)mem == value); break; \
            } \
        } \
    } \
    std::vector<std::pair<x86_ref_e, u64>> checks; \
    std::vector<std::tuple<void*, u64, u8>> mem_checks; \
    std::vector<std::pair<x86_ref_e, XmmReg>> xmm_checks; \
    std::optional<bool> c,p,a,z,s,o; \
}; \
TEST_CASE(#name, "[felix86]") { \
    Code_##name c; \
} \
Code_##name::Code_##name() : Xbyak::CodeGenerator(0x4000) { \
    data = (u8*)getCode(); \
    stack = data + 0x2000; \
    emit_code(); \
    hlt(); /* emit a hlt instruction to stop the recompiler */ \
    Config config = {}; \
    config.testing = true; \
    Emulator emulator(config); \
    emulator.SetGpr(X86_REF_RSP, (u64)stack); \
    emulator.SetRip((u64)data); \
    emulator.Run(); \
    verify_checks(emulator); \
} \
void Code_##name::emit_code()
