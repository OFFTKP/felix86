#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include "common.h"

bool validate_atomic8(uint8_t* ptr, uint8_t x) {
    uint8_t old_mem = *ptr;
    uint8_t mem = __atomic_exchange_n(ptr, x, __ATOMIC_SEQ_CST);
    uint8_t mem2 = *ptr;
    if (x != mem2) {
        return false;
    }

    if (old_mem != mem) {
        return false;
    }

    return true;
}

bool validate_atomic16(uint16_t* ptr, uint16_t x) {
    uint16_t old_mem = *ptr;
    uint16_t mem = __atomic_exchange_n(ptr, x, __ATOMIC_SEQ_CST);
    uint16_t mem2 = *ptr;
    if (x != mem2) {
        return false;
    }

    if (old_mem != mem) {
        return false;
    }

    return true;
}

int main() {
    uint8_t mem[8198]; // extra space to go out of bounds on last access
    for (int i = 0; i < 8192; i++) {
        mem[i] = rand();
    }

    for (int i = 0; i < 8192; i++) {
        if (!validate_atomic8(&mem[i], rand())) {
            return 1;
        }
    }

    for (int i = 0; i < 8192; i++) {
        if (!validate_atomic16((uint16_t*)&mem[i], rand())) {
            return 1;
        }
    }

    return FELIX86_BTEST_SUCCESS;
}