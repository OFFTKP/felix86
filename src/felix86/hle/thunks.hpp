#include "biscuit/assembler.hpp"
#include "felix86/common/utility.hpp"

struct Recompiler;

struct GuestPointers {
    const char* name;
    u64* func;
};

struct Thunks {
    static void initialize();

    static void* generateTrampoline(Recompiler& rec, Assembler& as, const char* name);

    static void runConstructor(const char* libname, GuestPointers* pointers);
};