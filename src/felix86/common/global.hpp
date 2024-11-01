#pragma once

#include "felix86/common/utility.hpp"

#define SUPPORTED_VLEN 128
extern bool g_verbose;
extern bool g_quiet;
extern bool g_testing;
extern bool g_strace;
extern bool g_dont_optimize;
extern bool g_print_blocks;
extern bool g_print_state;
extern bool g_print_disassembly;
extern bool g_extensions_manually_specified;
extern u32 g_spilled_count;

bool parse_extensions(const char* ext);
void initialize_globals();
void initialize_extensions();

struct Extensions {
    static bool G;
    static bool C;
    static bool B;
    static bool V;
    static bool Zacas;
    static bool Zam;
    static bool Zabha;
    static bool Zicond;
    static bool Xtheadcondmov;
    static int VLEN;

    static void Clear();
};