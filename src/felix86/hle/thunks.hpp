#pragma once

#include "felix86/common/types.hpp"

namespace biscuit {
class Assembler;
}

struct GuestPointers {
    const char* name;
    u64* func;
};

struct Thunks {
    static void initialize();

    static void* generateTrampoline(bool relocatable, biscuit::Assembler& as, const char* name);

    static void* generateTrampoline(bool relocatable, biscuit::Assembler& as, const char* name, const char* signature, u64 host_ptr);

    static void runConstructor(const char* libname, GuestPointers* pointers);
};
