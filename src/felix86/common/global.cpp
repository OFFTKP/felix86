#include "felix86/common/global.hpp"

bool g_verbose = false;
bool g_quiet = false;
bool g_testing = false;
bool g_strace = false;

bool Extensions::G = false;
bool Extensions::C = false;
bool Extensions::B = false;
bool Extensions::V = false;
bool Extensions::Zacas = false;
bool Extensions::Zam = false;
bool Extensions::Zabha = false;
bool Extensions::Zicond = false;
int Extensions::VLEN = 0;

void Extensions::Clear() {
    G = false;
    C = false;
    B = false;
    V = false;
    Zacas = false;
    Zam = false;
    Zabha = false;
    Zicond = false;
    VLEN = 0;
}