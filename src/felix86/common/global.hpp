#pragma once

#define SUPPORTED_VLEN 128
extern bool g_verbose;
extern bool g_quiet;
extern bool g_testing;
extern bool g_strace;

struct Extensions {
    static bool G;
    static bool C;
    static bool B;
    static bool V;
    static bool Zacas;
    static bool Zam;
    static bool Zabha;
    static bool Zicond;
    static int VLEN;

    static void Clear();
};