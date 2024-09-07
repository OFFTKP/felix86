#include <catch2/catch_test_macros.hpp>
#include <xbyak/xbyak.h>
#include "felix86/common/utility.h"
#include "felix86/felix86.h"

using namespace Xbyak;
using namespace Xbyak::util;

#define FELIX86_TEST(name) struct Code_##name final : Xbyak::CodeGenerator { \
    Code_##name(); \
    ~Code_##name() { free(data); } \
    void verify(x86_ref_e ref, u64 value) { checks.push_back({ ref, value }); } \
    void verify_memory(void* mem, u64 value, u8 size) { mem_checks.push_back({ mem, value, size }); } \
    void verify_xmm(x86_ref_e ref, xmm_reg_t reg) { xmm_checks.push_back({ ref, reg }); } \
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
    void verify_checks() { \
        for (auto& check : checks) { \
            REQUIRE(felix86_get_guest(recompiler, check.first) == check.second); \
        } \
        if (c.has_value()) REQUIRE(!!(felix86_get_guest(recompiler, X86_REF_CF)) == c.value()); \
        if (p.has_value()) REQUIRE(!!(felix86_get_guest(recompiler, X86_REF_PF)) == p.value()); \
        if (a.has_value()) REQUIRE(!!(felix86_get_guest(recompiler, X86_REF_AF)) == a.value()); \
        if (z.has_value()) REQUIRE(!!(felix86_get_guest(recompiler, X86_REF_ZF)) == z.value()); \
        if (s.has_value()) REQUIRE(!!(felix86_get_guest(recompiler, X86_REF_SF)) == s.value()); \
        if (o.has_value()) REQUIRE(!!(felix86_get_guest(recompiler, X86_REF_OF)) == o.value()); \
        for (auto& check : xmm_checks) { \
            xmm_reg_t has = felix86_get_guest_xmm(recompiler, std::get<0>(check)); \
            xmm_reg_t expected = std::get<1>(check); \
            for (int i = 0; i < sizeof(xmm_reg_t) / sizeof(u64); i++) { \
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
    felix86_recompiler_t* recompiler; \
    std::vector<std::pair<x86_ref_e, u64>> checks; \
    std::vector<std::tuple<void*, u64, u8>> mem_checks; \
    std::vector<std::pair<x86_ref_e, xmm_reg_t>> xmm_checks; \
    std::optional<bool> c,p,a,z,s,o; \
}; \
TEST_CASE(#name, "[felix86]") { \
    Code_##name c; \
} \
Code_##name::Code_##name() : Xbyak::CodeGenerator(0x1000, malloc(0x2000)) { \
    data = (u8*)getCode(); \
    stack = data + 0x2000; \
    mov(rsp, (u64)stack); \
    emit_code(); \
    hlt(); /* emit a hlt instruction to stop the recompiler */ \
    felix86_recompiler_config_t config = { .testing = true, .optimize = true, .print_blocks = true, .use_interpreter = true }; \
    recompiler = felix86_recompiler_create(&config); \
    felix86_set_guest(recompiler, X86_REF_RIP, (u64)data); \
    felix86_recompiler_run(recompiler); \
    verify_checks(); \
    felix86_recompiler_destroy(recompiler); \
} \
void Code_##name::emit_code()
