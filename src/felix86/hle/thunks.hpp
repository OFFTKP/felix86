#include "biscuit/assembler.hpp"
#include "felix86/common/utility.hpp"

struct Recompiler;

struct Thunks {
    static void initialize();

    static void* generateTrampoline(Recompiler& rec, Assembler& as, const char* name);
};