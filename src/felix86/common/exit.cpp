#include <cstdio>
#include <cstdlib>
#include "felix86/common/exit.hpp"
#include "felix86/common/print.hpp"
#include "felix86/common/state.hpp"

void felix86_exit(int code) {
    if (g_config.dump_regs) {
        ThreadState* state = ThreadState::Get();
        if (state) {
            print_state(state);
        }
    }

    if (g_config.sleep_on_error) {
        int pid = getpid();
        PLAIN("I have crashed! My PID is %d, you have 40 seconds to attach gdb using `gdb -p %d` to find out why!", pid, pid);
        sleep(40);
    }

    exit(code);
}