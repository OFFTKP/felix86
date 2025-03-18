#pragma once

#include <cstdint>
#include <list>
#include "felix86/common/gdbjitblock.h"
#include "felix86/common/process_lock.hpp"
#include "felix86/common/utility.hpp"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum { JIT_NOACTION = 0, JIT_REGISTER_FN, JIT_UNREGISTER_FN } jit_actions_t;

struct jit_descriptor {
    uint32_t version;
    uint32_t action_flag;
    struct jit_code_entry* relevant_entry;
    struct jit_code_entry* first_entry;
};

#ifdef __cplusplus
}
#endif

struct GDBJIT {
    static felix86_jit_block_t* createBlock(size_t line_count);

    void fire(felix86_jit_block_t* block);

private:
    Semaphore semaphore;
};