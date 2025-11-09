#include "biscuit/assembler.hpp"
#include "felix86/common/types.hpp"

struct Recompiler;

struct GuestPointers {
    const char* name;
    u64* func;
};

struct Thunks {
    static void initialize();

    static void* generateTrampoline(Recompiler& rec, const char* name);

    static void* generateTrampoline(Recompiler& rec, const char* name, const char* signature, u64 host_ptr);

    static void runConstructor(const char* libname, GuestPointers* pointers);
};