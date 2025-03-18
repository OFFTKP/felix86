#include <cstring>
#include "felix86/common/gdbjit.hpp"
#include "felix86/common/log.hpp"
#include "felix86/common/process_lock.hpp"

extern "C" {
void __attribute__((noinline)) __jit_debug_register_code() {};

struct jit_descriptor __jit_debug_descriptor = {1, 0, 0, 0};
}

felix86_jit_block_t GDBJIT::createBlock() {
    felix86_jit_block_t block;
    memset(&block, 0, sizeof(felix86_jit_block_t));
    int pid = getpid();
    int chars = snprintf(block.filename, sizeof(block.filename), "/tmp/f86_%x", pid);
    ASSERT(chars < sizeof(block.filename));
    if (!std::filesystem::exists(block.filename)) {
        std::filesystem::create_directory(block.filename);
    }
    chars = snprintf(block.filename, sizeof(block.filename), "/tmp/f86_%x/XXXXXX.map", pid);
    ASSERT(chars < sizeof(block.filename));
    block.file = fdopen(mkstemps(block.filename, 4), "w");
    ASSERT(block.file);
    return block;
}

void GDBJIT::fire(const felix86_jit_block_t& block) {
    auto lock = semaphore.lock();
    felix86_jit_block_t* previous = nullptr;
    if (!blocks.empty()) {
        previous = &blocks.back();
    }

    blocks.push_back(block);
    felix86_jit_block_t* new_entry = &blocks.back();

    new_entry->entry.symfile_addr = (const char*)new_entry; // point to itself
    new_entry->entry.symfile_size = sizeof(felix86_jit_block_t);
    new_entry->entry.next_entry = nullptr;

    if (previous) {
        previous->entry.next_entry = &new_entry->entry;
        new_entry->entry.prev_entry = &previous->entry;
    } else {
        new_entry->entry.prev_entry = nullptr;
    }

    if (!__jit_debug_descriptor.first_entry) {
        __jit_debug_descriptor.first_entry = &new_entry->entry;
    }

    __jit_debug_descriptor.relevant_entry = &new_entry->entry;
    __jit_debug_descriptor.action_flag = JIT_REGISTER_FN;
    __jit_debug_register_code(); // push to gdb
}