#include <cstdio>
#include <cstdlib>
#include "felix86/common/exit.hpp"
#include "felix86/common/print.hpp"
#include "felix86/common/state.hpp"

void felix86_exit(int code) {
    if (g_dump_regs) {
        ThreadState* state = ThreadState::Get();
        if (state) {
            print_state(state);
        }
    }

    exit(code);
}