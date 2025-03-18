#include <cstring>
#include "felix86/common/gdbjit.hpp"
#include "felix86/common/log.hpp"
#include "felix86/common/process_lock.hpp"

extern "C" {
void __attribute__((noinline)) __jit_debug_register_code() {};

struct jit_descriptor __jit_debug_descriptor = {1, 0, 0, 0};
}

felix86_jit_block_t* GDBJIT::createBlock(size_t line_count) {
    // We need to allocate all the memory like so because gdb is going to copy it in its own memory space
    felix86_jit_block_t* block = (felix86_jit_block_t*)calloc(1, sizeof(felix86_jit_block_t) + line_count * sizeof(gdb_line_mapping));
    int pid = getpid();
    int chars = snprintf(block->filename, sizeof(block->filename), "/tmp/f86_%x", pid);
    ASSERT(chars < (int)sizeof(block->filename));
    if (!std::filesystem::exists(block->filename)) {
        std::filesystem::create_directory(block->filename);
    }
    chars = snprintf(block->filename, sizeof(block->filename), "/tmp/f86_%x/XXXXXX.map", pid);
    ASSERT(chars < (int)sizeof(block->filename));
    block->file = fdopen(mkstemps(block->filename, 4), "w");
    ASSERT(block->file);
    return block;
}

void GDBJIT::fire(felix86_jit_block_t* block) {
    auto lock = semaphore.lock();
    jit_code_entry* previous = (jit_code_entry*)__jit_debug_descriptor.relevant_entry;
    felix86_jit_block_t* new_entry = block;

    new_entry->entry.symfile_addr = (const char*)new_entry; // point to itself
    new_entry->entry.symfile_size = sizeof(felix86_jit_block_t) + new_entry->line_count * sizeof(gdb_line_mapping);
    new_entry->entry.next_entry = nullptr;

    if (previous) {
        previous->next_entry = &new_entry->entry;
        new_entry->entry.prev_entry = previous;
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