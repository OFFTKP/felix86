#include <xbyak/xbyak.h>
#include "biscuit/assembler.hpp"
#include "felix86/common/utility.hpp"

struct Thunks {

private:
    static void* generateTrampoline(const std::string& signature, u64 target);

    static void lock();

    static void unlock();

    // Thunk assembler
    static biscuit::Assembler tas;
    // TODO: semaphore lock
};