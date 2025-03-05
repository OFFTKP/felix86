#include "biscuit/assembler.hpp"
#include "felix86/common/utility.hpp"

struct Thunks {
    static void initialize();

    static void* generateTrampoline(Assembler& as, const char* name);
};