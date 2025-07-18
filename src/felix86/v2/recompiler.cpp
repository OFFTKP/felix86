#include <algorithm>
#include <sys/file.h>
#include <sys/mman.h>
#include <unistd.h>
#include "Zydis/Disassembler.h"
#include "felix86/common/frame.hpp"
#include "felix86/common/gdbjit.hpp"
#include "felix86/common/perf.hpp"
#include "felix86/common/types.hpp"
#include "felix86/common/utility.hpp"
#include "felix86/emulator.hpp"
#include "felix86/hle/syscall.hpp"
#include "felix86/v2/handlers.hpp"
#include "felix86/v2/recompiler.hpp"
#include "fmt/format.h"

// TODO: benchmark to find best arrangement?
constexpr static u64 code_cache_sizes[] = {
    8 * 1024 * 1024,
    16 * 1024 * 1024,
    32 * 1024 * 1024,
    64 * 1042 * 1024,
};

constexpr static u64 code_cache_sizes_count = std::size(code_cache_sizes);
constexpr static u64 max_code_cache_size = code_cache_sizes[code_cache_sizes_count - 1];

// TODO: move to header file
BlockMetadata* get_block_metadata(ThreadState* state, u64 host_pc);

static void incorrect_magic(void* sp) {
    ERROR("Incorrect magic in frame (sp: %lx)", sp);
}

static void incorrect_stack(void* sp_expected, void* sp_actual) {
    ERROR("Incorrect stack in frame, expected %lx, but got %lx", sp_expected, sp_actual);
}

// Some instructions modify the flags conditionally or sometimes they don't modify them at all.
// This needs to be marked as a usage of the flag as it can be passed through if they don't modify,
// and previous instructions need to know that.
// TODO: actually this might be unnecessary if Zydis doesn't mark these flags as modified, test it
static bool flag_passthrough(ZydisMnemonic mnemonic, x86_ref_e flag) {
    switch (mnemonic) {
    case ZYDIS_MNEMONIC_SHL:
    case ZYDIS_MNEMONIC_SHLD:
    case ZYDIS_MNEMONIC_SHR:
    case ZYDIS_MNEMONIC_SHRD:
    case ZYDIS_MNEMONIC_SAR:
    case ZYDIS_MNEMONIC_ROL:
    case ZYDIS_MNEMONIC_ROR: {
        return flag == X86_REF_CF || flag == X86_REF_OF;
    }
    default: {
        return false;
    }
    }
}

static u8* allocateCodeCache(size_t size) {
    void* address = ::mmap(nullptr, max_code_cache_size, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE, -1, 0);
    ASSERT_MSG(address != MAP_FAILED, "Failed to reserve code cache for thread %d?", gettid());
    u8* first_chunk = (u8*)::mmap(address, code_cache_sizes[0], PROT_READ | PROT_WRITE | PROT_EXEC, MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);
    ASSERT(first_chunk == address);
    return first_chunk;
}

static void deallocateCodeCache(void* address) {
    munmap(address, max_code_cache_size);
}

Recompiler::Recompiler() : as(allocateCodeCache(code_cache_sizes[0]), max_code_cache_size) {
    emitNecessaryStuff();

    ZydisMachineMode mode = g_mode32 ? ZYDIS_MACHINE_MODE_LONG_COMPAT_32 : ZYDIS_MACHINE_MODE_LONG_64;
    ZydisStackWidth stack_width = g_mode32 ? ZYDIS_STACK_WIDTH_32 : ZYDIS_STACK_WIDTH_64;

    ZydisDecoderInit(&decoder, mode, stack_width);
    ZydisDecoderEnableMode(&decoder, ZYDIS_DECODER_MODE_AMD_BRANCHES, ZYAN_TRUE);

    if (g_config.always_flags || g_config.paranoid) {
        flag_mode = FlagMode::AlwaysEmit;
    }

    if (g_config.perf_blocks || g_config.perf_global || g_config.perf_libs) {
        u64 end = (u64)as.GetCursorPointer();
        u64 size = end - (u64)enter_dispatcher;
        g_process_globals.perf->addToFile((u64)enter_dispatcher, size, "felix86 dispatcher");
    }

    if (g_config.perf_global) {
        g_process_globals.perf->addToFile((u64)start_of_code_cache, max_code_cache_size - ((u64)start_of_code_cache - (u64)as.GetBufferPointer(0)),
                                          "felix86 code cache");
    }

    if (g_config.address_cache) {
        // Ensure nullptr doesn't match
        address_cache[0].guest = -1ull;
    }
}

Recompiler::~Recompiler() {
    deallocateCodeCache(as.GetBufferPointer(0));
}

void Recompiler::emitNecessaryStuff() {
    emitDispatcher();
    emitInvalidateCallerThunk();

    start_of_code_cache = as.GetCursorPointer();

    flush_icache();
}

void Recompiler::emitDispatcher() {
    enter_dispatcher = (decltype(enter_dispatcher))as.GetCursorPointer();

    // Save the current frame in the stack
    // The size of felix86_frame is bigger than the red zone, so we need to decrement the stack instead of using a temporary
    // This is because a signal could technically thrash values outside the red zone if we don't decrement the stack pointer here
    as.MV(t0, sp);
    as.ADDI(sp, sp, -(int)sizeof(felix86_frame));
    for (size_t i = 0; i < saved_gprs.size(); i++) {
        if (saved_gprs[i] != sp) {
            as.SD(saved_gprs[i], offsetof(felix86_frame, gprs) + i * sizeof(u64), sp);
        } else {
            // Use t0 instead of sp to save our old stack
            as.SD(t0, offsetof(felix86_frame, gprs) + i * sizeof(u64), sp);
        }
    }

    // Save the ThreadState pointer
    as.SD(a0, offsetof(felix86_frame, state), sp);

    // Also save the magic number
    as.LI(t1, felix86_frame::expected_magic);
    as.SD(t1, offsetof(felix86_frame, magic), sp);

    as.MV(threadStatePointer(), a0);

    restoreState();

    compile_next_handler = (u64)as.GetCursorPointer();

    if (g_config.address_cache) {
        biscuit::GPR temp = scratch();
        biscuit::GPR temp2 = scratch();
        biscuit::GPR rip = scratch();
        biscuit::GPR ripreg = allocatedGPR(X86_REF_RIP);
        biscuit::Label not_equal;
        as.LI(temp, (u64)address_cache.data());
        as.SLLI(temp2, ripreg, 64 - address_cache_bits);
        // Multiply by 16, which is size of each address cache entry
        as.SRLI(temp2, temp2, 64 - address_cache_bits - 4);
        as.ADD(temp, temp, temp2);
        as.LD(temp2, 8, temp); // read the AddressCacheEntry::guest field
        as.BNE(temp2, ripreg, &not_equal);

        // Address cache was correct, jump to host address
        as.LD(rip, 0, temp);
        as.MV(t5, x0); // zero out t5, see invalidate_caller_thunk
        as.JR(rip);

        as.Bind(&not_equal);
        popScratch();
        popScratch();
        popScratch();
    }

    writebackState();
    as.MV(a0, threadStatePointer());
    call((u64)Emulator::CompileNext);

    biscuit::GPR retval = scratch();
    as.MV(retval, a0);
    restoreState();
    as.MV(t5, x0); // zero out t5, see invalidate_caller_thunk
    as.JR(retval);
    popScratch();

    exit_dispatcher = (decltype(exit_dispatcher))as.GetCursorPointer();

    // Move stack pointer to current frame (passed as an argument)
    as.MV(sp, a0);

    // Load ThreadState* into t4
    as.LD(t4, offsetof(felix86_frame, state), a0);

    // Load the frame we had before entering the dispatcher
    // First make sure our magic is correct
    Label magic_correct;
    as.LD(t1, offsetof(felix86_frame, magic), sp);
    as.LI(t2, felix86_frame::expected_magic);
    as.BEQ(t1, t2, &magic_correct);

    // Magic is incorrect if we get here
    as.MV(a0, sp);
    as.LI(t0, (u64)incorrect_magic);
    as.JALR(t0);
    as.GetCodeBuffer().Emit32(0);

    as.Bind(&magic_correct);

    for (size_t i = 0; i < saved_gprs.size(); i++) {
        if (saved_gprs[i] != sp) {
            as.LD(saved_gprs[i], offsetof(felix86_frame, gprs) + i * sizeof(u64), sp);
        } else {
            // Load the new stack pointer in t0 and set it later
            as.LD(t0, offsetof(felix86_frame, gprs) + i * sizeof(u64), sp);
        }
    }

    // Sanity check that the stack is just sp + sizeof(felix86_frame)
    Label stack_correct;
    as.ADDI(t1, sp, sizeof(felix86_frame));
    as.BEQ(t1, t0, &stack_correct);

    // Stack pointer is incorrect if we get here
    as.MV(a0, sp);
    as.MV(a1, t0);
    as.LI(t0, (u64)incorrect_stack);
    as.JALR(t0);
    as.GetCodeBuffer().Emit32(0);

    as.Bind(&stack_correct);
    as.MV(sp, t0);

    // Return to wherever the dispatcher was originally entered from using enter_dispatcher
    as.JR(ra);
}

void Recompiler::emitInvalidateCallerThunk() {
    invalidate_caller_thunk = (u64)as.GetCursorPointer();

    // Call the invalidateAt function which will remove the block from the map
    // when it's called and it will go back to the dispatcher, which will trigger a new compilation
    // The JALR that jumps here links to t6. We ASSERT that writebackState doesn't modify t6.
    writebackState();
    as.MV(a0, threadStatePointer());
    as.MV(a1, t6);
    // All block links set the return address in t5, and we collect it here
    // The dispatcher itself sets t5 to 0 to signal that the jump comes from there
    as.MV(a2, t5);
    call((u64)Recompiler::invalidateAt);
    restoreState(); // TODO: instead, make a "backToDispatcherSkipWriteback" function and remove this restore
    backToDispatcher();
}

// Note: This function is important. When we invalidate a block range, our choices are either iterate every block that links
// to this block and unlink it, this is expensive both performance wise and memory wise, or we can instead insert a piece
// of code that invalidates the block and also unlinks the caller. See invalidate_caller_thunk.
void Recompiler::invalidateAt(ThreadState* state, u8* address_of_block, u8* linked_block) {
    // We have an address that is +4 of the start of the block
    // Normally our map is guest->host address but we also have this host_pc_map for signals and for here to do a backwards lookup
    auto it = state->recompiler->host_pc_map.lower_bound((u64)address_of_block);
    ASSERT(it != state->recompiler->host_pc_map.end());

    if (!((u64)address_of_block >= it->second->address && (u64)address_of_block <= it->second->address_end)) {
        // Block was probably already invalidate by some other block that jumped here via a link, don't set address to 0
        // as that would trigger another recompilation for no reason
    } else {
        // Setting it to 0 should be enough, as it will trigger recompilation for this block
        it->second->address = 0;
    }

    // We also need to remove it from the address cache
    if (g_config.address_cache) {
        AddressCacheEntry& entry = state->recompiler->address_cache[it->second->guest_address & ((1 << address_cache_bits) - 1)];
        entry.guest = ~(it->second->guest_address & ((1 << address_cache_bits) - 1));
        entry.host = 0;
    }

    if (linked_block) {
        // This was jumped to by a link. We need to unlink the caller so it doesn't jump here again.
        // The third argument is going to be the instruction after the linked JAL or JALR
        // So we need to subtract 8
        linked_block -= 8;

        auto linked_metadata = get_block_metadata(state, (u64)linked_block);
        ASSERT_MSG(linked_metadata, "Failed to get block metadata for address %lx", linked_block);
        if (linked_metadata->address != 0) {
            u8* cursor = state->recompiler->as.GetCursorPointer();
            ASSERT_MSG(linked_block >= state->recompiler->start_of_code_cache && linked_block < cursor, "%lx <= %lx < %lx",
                       state->recompiler->start_of_code_cache, linked_block, cursor);

            // And here we need to mark the block for linking again. This will either link if the block is already compiled
            // or jump back to dispatcher that will link when the block gets compiled.
            state->recompiler->as.SetCursorPointer(linked_block);
            state->recompiler->jumpAndLink(it->second->guest_address);
            state->recompiler->as.SetCursorPointer(cursor);
            flush_icache();
        }
    } else {
        // The dispatcher makes sure the third argument is set to 0 before we get here
    }
}

void Recompiler::clearCodeCache(ThreadState* state) {
    if (code_cache_size_index < code_cache_sizes_count) {
        // Allocate more of our reserved buffer
        u8* old_mem = as.GetBufferPointer(0);
        u64 old_size = code_cache_sizes[code_cache_size_index];
        u8* past_end = old_mem + old_size;
        ASSERT(!((u64)past_end & 0xFFF));
        code_cache_size_index++;
        u64 new_size = code_cache_sizes[code_cache_size_index];
        u64 size_difference = new_size - old_size;

        void* address = ::mmap(past_end, size_difference, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);
        if (address != past_end) {
            // TODO: investigate
            // Unsure what causes this to happen, but it does. In that case, clear code cache and carry on
            // Perhaps PR_MDWE_REFUSE_EXEC_GAIN
            WARN("Couldn't increment code cache because mmap returned %lx (errno: %s), clearing code cache", address, strerror(errno));
            auto guard = page_map_lock.lock();
            block_metadata.clear();
            host_pc_map.clear();
            page_map.clear();
            std::fill(std::begin(address_cache), std::end(address_cache), AddressCacheEntry{});

            as.RewindBuffer();
            emitNecessaryStuff();
        }
    } else {
        WARN("Clearing cache on thread %u", gettid());
        auto guard = page_map_lock.lock();
        block_metadata.clear();
        host_pc_map.clear();
        page_map.clear();
        std::fill(std::begin(address_cache), std::end(address_cache), AddressCacheEntry{});

        as.RewindBuffer();
        emitNecessaryStuff();
    }
}

u64 Recompiler::compile(ThreadState* state, u64 rip) {
    u64 size = code_cache_sizes[code_cache_size_index];
    size_t remaining_size = size - as.GetCodeBuffer().GetCursorOffset();
    // TODO: restrict max x86 instruction count per block
    if (remaining_size < 200'000) { // less than ~200KB left, clear cache
        clearCodeCache(state);
    }

    // This should never happen, but we have this here to catch it if it does and report it
    if (g_mode32 && (rip & 0xFFFF'FFFF'0000'0000)) {
        if (rip >= 0x1'0000'0000 && rip <= 0x1'FFFF'FFFF) {
            u64 old = rip;
            rip &= 0xFFFF'FFFF;
            WARN("RIP wrapped around... %lx -> %lx", old, rip);
        }
    }

    if (state->unaligned_atomics_counter) {
        WARN_ONCE("This program has unaligned atomics that we can't emulate atomically");
    }

    u64 start = (u64)as.GetCursorPointer();

    // Map it immediately so we can optimize conditional branch to self
    BlockMetadata& block_meta = getBlockMetadata(rip);
    block_meta.address = start;

    // A sequence of code (ie. basic block). This is so that we can also call it recursively later.
    u64 end_rip = compileSequence(rip);

    u64 end = (u64)as.GetCursorPointer();

    ASSERT(end - start >= 8); // At least 2 instructions, so that our unlinking logic works

    host_pc_map[block_meta.address_end - 1] = &block_meta;

    {
        auto guard = page_map_lock.lock();
        u64 start_masked = block_meta.guest_address & ~0xFFFull;
        u64 end_masked = (block_meta.guest_address_end - 1) & ~0xFFFull;
        for (u64 page = start_masked; page <= end_masked; page += 0x1000) {
            page_map[page].push_back(&block_meta);
        }
    }

    if (g_config.address_cache) {
        AddressCacheEntry& entry = address_cache[block_meta.guest_address & ((1 << address_cache_bits) - 1)];
        entry.host = block_meta.address;
        entry.guest = block_meta.guest_address;
    }

    // If other blocks were waiting for this block to be linked, link them now
    expirePendingLinks(rip);

    // Mark the page as read-only to catch self-modifying code
    markPagesAsReadOnly(rip, end_rip);

    if (g_config.perf_blocks || g_config.perf_symbols) {
        std::string symbol;
        size_t size = block_meta.address_end - block_meta.address;
        if (g_config.perf_symbols) {
            if (!has_region(rip)) {
                // Executed region not found, update the symbols
                update_symbols();
            }

            symbol = get_perf_symbol(rip);
        } else {
            symbol = fmt::format("block_0x{:x}", block_meta.guest_address);
        }
        g_process_globals.perf->addToFile(block_meta.address, size, symbol);
    }

    if (g_config.perf_libs) {
        if (!has_region(rip)) {
            // Executed region not found, update the symbols
            update_symbols();
        }

        std::filesystem::path symbol = get_region(rip);
        size_t size = block_meta.address_end - block_meta.address;
        g_process_globals.perf->addToFile(block_meta.address, size, "guest " + symbol.filename().string());
    }

    return start;
}

void Recompiler::markPagesAsReadOnly(u64 start, u64 end) {
    if (!g_config.protect_pages) {
        return;
    }

    u64 start_page = start & ~0xFFFull;
    u64 end_page = (end + 0xFFF) & ~0xFFFull;
    u64 size = end_page - start_page;
    int result = mprotect((void*)start_page, size, PROT_READ);
    if (result != 0) {
        ERROR("Failed to protect pages %016lx-%016lx -- Error: %s", start_page, end_page, strerror(errno));
    }
}

u64 Recompiler::compileSequence(u64 rip) {
    compiling = true;
    u8* bytes = (u8*)rip;
    bool all_zeroes = true;
    if (bytes[0] == 0x00) {
        for (int i = 0; i < 16; i++) {
            if (bytes[i + 1] != 0x00) {
                all_zeroes = false;
                break;
            }
        }
    } else {
        all_zeroes = false;
    }

    if (all_zeroes) {
        ERROR("Jumped to address %lx which has a sequence of zeroes -- probably a bad jump?");
    }

    scanAhead(rip);
    BlockMetadata& block_meta = getBlockMetadata(rip);

    current_block_metadata = &block_meta;
    current_sew = SEW::E1024;
    current_vlen = 0;
    current_grouping = LMUL::M1;
    local_x87_state = x87State::Unknown; // we don't know what ThreadState::x87_state is at runtime
    fsrm_sse = true;                     // dispatcher loads SSE rounding mode as a default

    current_block_metadata->guest_address = rip;

    current_instruction_index = 0;

    while (compiling) {
        auto& [instruction, operands] = instructions[current_instruction_index];

        block_meta.instruction_spans.push_back({rip, (u64)as.GetCursorPointer()});

        bool is_mmx = (operands[0].type == ZYDIS_OPERAND_TYPE_REGISTER && operands[0].reg.value >= ZYDIS_REGISTER_MM0 &&
                       operands[0].reg.value <= ZYDIS_REGISTER_MM7) ||
                      (operands[1].type == ZYDIS_OPERAND_TYPE_REGISTER && operands[1].reg.value >= ZYDIS_REGISTER_MM0 &&
                       operands[1].reg.value <= ZYDIS_REGISTER_MM7);
        bool is_x87 = instruction.meta.isa_ext == ZYDIS_ISA_EXT_X87;
        bool is_sse = (operands[0].type == ZYDIS_OPERAND_TYPE_REGISTER && operands[0].reg.value >= ZYDIS_REGISTER_XMM0 &&
                       operands[0].reg.value <= ZYDIS_REGISTER_XMM15) ||
                      (operands[1].type == ZYDIS_OPERAND_TYPE_REGISTER && operands[1].reg.value >= ZYDIS_REGISTER_XMM0 &&
                       operands[1].reg.value <= ZYDIS_REGISTER_XMM15);
        if (is_mmx) {
            ASSERT_MSG(Extensions::V, "TODO: Implement MMX for no RVV");
            WARN_ONCE("This program makes use of MMX");
            if (local_x87_state != x87State::MMX) {
                switchToMMX();
            }
        }

        if (is_x87) {
            if (local_x87_state == x87State::MMX) {
                WARN("x87 and MMX mixed in a block?");
            }

            if (local_x87_state != x87State::x87) {
                switchToX87();
            }
        }

        if (is_x87 && fsrm_sse) {
            // An x87 instruction, load the x87 rounding mode
            biscuit::GPR rm = scratch();
            as.LBU(rm, offsetof(ThreadState, rmode_x87), threadStatePointer());
            as.FSRM(x0, rm);
            popScratch();
            setFsrmSSE(false);
        } else if (is_sse && !fsrm_sse) {
            biscuit::GPR rm = scratch();
            as.LBU(rm, offsetof(ThreadState, rmode_sse), threadStatePointer());
            as.FSRM(x0, rm);
            popScratch();
            setFsrmSSE(true);
        }

        if (g_breakpoints.find(rip) != g_breakpoints.end()) {
            u64 current_address = (u64)as.GetCursorPointer();
            g_breakpoints[rip].push_back(current_address);
            as.GetCodeBuffer().Emit32(0); // UNIMP instruction
        }

        compileInstruction(instruction, operands, rip);

        if (g_config.inline_syscalls) {
            checkModifiesRax(instruction, operands);
        }

        rip += instruction.length;

        if (g_config.single_step && compiling) {
            resetScratch();
            biscuit::GPR rip_after = allocatedGPR(X86_REF_RIP);
            as.LI(rip_after, rip);
            backToDispatcher();
            stopCompiling();
        }

        current_instruction_index += 1;

        if (skip_next) {
            rip += instructions[current_instruction_index].first.length;
            current_instruction_index += 1;
            skip_next = false;
        }
    }

    resetScratch();

    current_block_metadata->guest_address_end = rip;
    current_block_metadata->address_end = (u64)as.GetCursorPointer();

    if (g_config.gdb) {
        size_t inst_count = current_block_metadata->instruction_spans.size();
        felix86_jit_block_t* gdb_block = GDBJIT::createBlock(inst_count);
        gdb_block->host_start = (u64)as.GetCursorPointer();

        for (size_t i = 0; i < inst_count; i++) {
            u64 guest_address = current_block_metadata->instruction_spans[i].first;
            u64 host_address = current_block_metadata->instruction_spans[i].second;
            ZydisDisassembledInstruction inst;
            ZydisDisassembleIntel(decoder.machine_mode, guest_address, (void*)guest_address, 15, &inst);
            int size = strlen(inst.text);
            inst.text[size] = '\n';
            fwrite(inst.text, size + 1, 1, gdb_block->file);
            gdb_block->lines[i].line = 1 + i;
            gdb_block->lines[i].pc = host_address;
        }

        gdb_block->host_start = current_block_metadata->address;
        gdb_block->host_end = current_block_metadata->address_end;
        gdb_block->guest_address = current_block_metadata->guest_address;
        gdb_block->line_count = inst_count;

        fclose(gdb_block->file);
        g_gdbjit->fire(gdb_block);
    }

    flush_icache();

    current_block_metadata = nullptr;
    local_x87_state = x87State::Unknown; // we don't know what ThreadState::x87_state is at runtime

    return rip;
}

std::pair<ZydisDecodedInstruction*, ZydisDecodedOperand*> Recompiler::getNextInstruction() {
    ASSERT(instructions.size() > current_instruction_index + 1);
    auto& [instruction, operands] = instructions[current_instruction_index + 1];
    return std::make_pair(&instruction, operands);
}

void Recompiler::skipNext() {
    skip_next = true;
}

void Recompiler::compileInstruction(ZydisDecodedInstruction& instruction, ZydisDecodedOperand* operands, u64 rip) {
    current_instruction = &instruction;
    current_operands = operands;
    current_rip = rip;
    resetScratch();

    ZydisMnemonic mnemonic = instruction.mnemonic;

    if (g_config.no_sse2 && (instruction.meta.isa_set == ZYDIS_ISA_SET_SSE2)) {
        ERROR("SSE2 instruction %s at %016lx when FELIX86_NO_SSE2 is enabled", ZydisMnemonicGetString(mnemonic), rip);
    }

    if (g_config.no_sse3 && (instruction.meta.isa_set == ZYDIS_ISA_SET_SSE3)) {
        ERROR("SSE3 instruction %s at %016lx when FELIX86_NO_SSE3 is enabled", ZydisMnemonicGetString(mnemonic), rip);
    }

    if (g_config.no_ssse3 && (instruction.meta.isa_set == ZYDIS_ISA_SET_SSSE3)) {
        ERROR("SSSE3 instruction %s at %016lx when FELIX86_NO_SSSE3 is enabled", ZydisMnemonicGetString(mnemonic), rip);
    }

    if (g_config.no_sse4_1 && (instruction.meta.isa_set == ZYDIS_ISA_SET_SSE4)) {
        ERROR("SSE4.1 instruction %s at %016lx when FELIX86_NO_SSE4_1 is enabled", ZydisMnemonicGetString(mnemonic), rip);
    }

    if (g_config.no_sse4_2 && (instruction.meta.isa_set == ZYDIS_ISA_SET_SSE4)) {
        ERROR("SSE4.2 instruction %s at %016lx when FELIX86_NO_SSE4_2 is enabled", ZydisMnemonicGetString(mnemonic), rip);
    }

    lock_handled = false;
    switch (mnemonic) {
#define X(name)                                                                                                                                      \
    case ZYDIS_MNEMONIC_##name:                                                                                                                      \
        Handlers::ptr_##name(*this, rip, as, instruction, operands);                                                                                 \
        break;
#define SIMD(name) X(name)
#define X87(name) X(name)
#include "felix86/v2/mnemonics.inc"
#undef X
#undef SIMD
#undef X87
    default: {
        ASSERT_MSG(false, "Bad mnemonic? Shouldn't get here");
        break;
    }
    }

    if ((instruction.attributes & ZYDIS_ATTRIB_HAS_LOCK) && !lock_handled) {
        WARN("Didn't properly handle lock instruction: %s", disassemble_one(rip).c_str());
    }
}

biscuit::GPR Recompiler::scratch() {
    ASSERT(scratch_index != (int)scratch_gprs.size());
    return scratch_gprs[scratch_index++];
}

biscuit::Vec Recompiler::scratchVec() {
    ASSERT(vector_scratch_index != (int)scratch_vec.size());
    return scratch_vec[vector_scratch_index++];
}

biscuit::FPR Recompiler::scratchFPR() {
    ASSERT(fpu_scratch_index != (int)scratch_fprs.size());
    return scratch_fprs[fpu_scratch_index++];
}

void Recompiler::popScratchVec() {
    vector_scratch_index--;
    ASSERT(vector_scratch_index >= 0);
}

void Recompiler::popScratch() {
    scratch_index--;
    ASSERT(scratch_index >= 0);
}

void Recompiler::popScratchFPR() {
    fpu_scratch_index--;
    ASSERT(fpu_scratch_index >= 0);
}

void Recompiler::resetScratch() {
    scratch_index = 0;
    vector_scratch_index = 0;
    fpu_scratch_index = 0;
    cached_lea = x0;
    cached_lea_operand = nullptr;
}

x86_ref_e Recompiler::zydisToRef(ZydisRegister reg) {
    x86_ref_e ref;
    switch (reg) {
    case ZYDIS_REGISTER_AL:
    case ZYDIS_REGISTER_AH:
    case ZYDIS_REGISTER_AX:
    case ZYDIS_REGISTER_EAX:
    case ZYDIS_REGISTER_RAX: {
        ref = X86_REF_RAX;
        break;
    }
    case ZYDIS_REGISTER_CL:
    case ZYDIS_REGISTER_CH:
    case ZYDIS_REGISTER_CX:
    case ZYDIS_REGISTER_ECX:
    case ZYDIS_REGISTER_RCX: {
        ref = X86_REF_RCX;
        break;
    }
    case ZYDIS_REGISTER_DL:
    case ZYDIS_REGISTER_DH:
    case ZYDIS_REGISTER_DX:
    case ZYDIS_REGISTER_EDX:
    case ZYDIS_REGISTER_RDX: {
        ref = X86_REF_RDX;
        break;
    }
    case ZYDIS_REGISTER_BL:
    case ZYDIS_REGISTER_BH:
    case ZYDIS_REGISTER_BX:
    case ZYDIS_REGISTER_EBX:
    case ZYDIS_REGISTER_RBX: {
        ref = X86_REF_RBX;
        break;
    }
    case ZYDIS_REGISTER_SPL:
    case ZYDIS_REGISTER_SP:
    case ZYDIS_REGISTER_ESP:
    case ZYDIS_REGISTER_RSP: {
        ref = X86_REF_RSP;
        break;
    }
    case ZYDIS_REGISTER_BPL:
    case ZYDIS_REGISTER_BP:
    case ZYDIS_REGISTER_EBP:
    case ZYDIS_REGISTER_RBP: {
        ref = X86_REF_RBP;
        break;
    }
    case ZYDIS_REGISTER_SIL:
    case ZYDIS_REGISTER_SI:
    case ZYDIS_REGISTER_ESI:
    case ZYDIS_REGISTER_RSI: {
        ref = X86_REF_RSI;
        break;
    }
    case ZYDIS_REGISTER_DIL:
    case ZYDIS_REGISTER_DI:
    case ZYDIS_REGISTER_EDI:
    case ZYDIS_REGISTER_RDI: {
        ref = X86_REF_RDI;
        break;
    }
    case ZYDIS_REGISTER_R8B:
    case ZYDIS_REGISTER_R8W:
    case ZYDIS_REGISTER_R8D:
    case ZYDIS_REGISTER_R8: {
        ref = X86_REF_R8;
        break;
    }
    case ZYDIS_REGISTER_R9B:
    case ZYDIS_REGISTER_R9W:
    case ZYDIS_REGISTER_R9D:
    case ZYDIS_REGISTER_R9: {
        ref = X86_REF_R9;
        break;
    }
    case ZYDIS_REGISTER_R10B:
    case ZYDIS_REGISTER_R10W:
    case ZYDIS_REGISTER_R10D:
    case ZYDIS_REGISTER_R10: {
        ref = X86_REF_R10;
        break;
    }
    case ZYDIS_REGISTER_R11B:
    case ZYDIS_REGISTER_R11W:
    case ZYDIS_REGISTER_R11D:
    case ZYDIS_REGISTER_R11: {
        ref = X86_REF_R11;
        break;
    }
    case ZYDIS_REGISTER_R12B:
    case ZYDIS_REGISTER_R12W:
    case ZYDIS_REGISTER_R12D:
    case ZYDIS_REGISTER_R12: {
        ref = X86_REF_R12;
        break;
    }
    case ZYDIS_REGISTER_R13B:
    case ZYDIS_REGISTER_R13W:
    case ZYDIS_REGISTER_R13D:
    case ZYDIS_REGISTER_R13: {
        ref = X86_REF_R13;
        break;
    }
    case ZYDIS_REGISTER_R14B:
    case ZYDIS_REGISTER_R14W:
    case ZYDIS_REGISTER_R14D:
    case ZYDIS_REGISTER_R14: {
        ref = X86_REF_R14;
        break;
    }
    case ZYDIS_REGISTER_R15B:
    case ZYDIS_REGISTER_R15W:
    case ZYDIS_REGISTER_R15D:
    case ZYDIS_REGISTER_R15: {
        ref = X86_REF_R15;
        break;
    }
    case ZYDIS_REGISTER_XMM0 ... ZYDIS_REGISTER_XMM15: {
        ref = (x86_ref_e)(X86_REF_XMM0 + (reg - ZYDIS_REGISTER_XMM0));
        break;
    }
    case ZYDIS_REGISTER_MM0 ... ZYDIS_REGISTER_MM7: {
        ref = (x86_ref_e)(X86_REF_MM0 + (reg - ZYDIS_REGISTER_MM0));
        break;
    }
    case ZYDIS_REGISTER_RIP: {
        return X86_REF_RIP;
    }
    case ZYDIS_REGISTER_ST0 ... ZYDIS_REGISTER_ST7: {
        return (x86_ref_e)(X86_REF_ST0 + (reg - ZYDIS_REGISTER_ST0));
    }
    case ZYDIS_REGISTER_CS: {
        return X86_REF_CS;
    }
    case ZYDIS_REGISTER_DS: {
        return X86_REF_DS;
    }
    case ZYDIS_REGISTER_SS: {
        return X86_REF_SS;
    }
    case ZYDIS_REGISTER_ES: {
        return X86_REF_ES;
    }
    default: {
        ERROR("Unhandled register %s", ZydisRegisterGetString(reg));
        ref = X86_REF_RAX;
        break;
    }
    }

    return ref;
}

x86_size_e Recompiler::zydisToSize(ZydisRegister reg) {
    switch (reg) {
    case ZYDIS_REGISTER_AL:
    case ZYDIS_REGISTER_CL:
    case ZYDIS_REGISTER_DL:
    case ZYDIS_REGISTER_BL:
    case ZYDIS_REGISTER_SPL:
    case ZYDIS_REGISTER_BPL:
    case ZYDIS_REGISTER_SIL:
    case ZYDIS_REGISTER_DIL:
    case ZYDIS_REGISTER_R8B:
    case ZYDIS_REGISTER_R9B:
    case ZYDIS_REGISTER_R10B:
    case ZYDIS_REGISTER_R11B:
    case ZYDIS_REGISTER_R12B:
    case ZYDIS_REGISTER_R13B:
    case ZYDIS_REGISTER_R14B:
    case ZYDIS_REGISTER_R15B: {
        return X86_SIZE_BYTE;
    }
    case ZYDIS_REGISTER_AH:
    case ZYDIS_REGISTER_CH:
    case ZYDIS_REGISTER_DH:
    case ZYDIS_REGISTER_BH: {
        return X86_SIZE_BYTE_HIGH;
    }
    case ZYDIS_REGISTER_AX ... ZYDIS_REGISTER_R15W: {
        return X86_SIZE_WORD;
    }
    case ZYDIS_REGISTER_EAX ... ZYDIS_REGISTER_R15D: {
        return X86_SIZE_DWORD;
    }
    case ZYDIS_REGISTER_RAX ... ZYDIS_REGISTER_R15: {
        return X86_SIZE_QWORD;
    }
    case ZYDIS_REGISTER_XMM0 ... ZYDIS_REGISTER_XMM15: {
        return X86_SIZE_XMM;
    }
    case ZYDIS_REGISTER_MM0 ... ZYDIS_REGISTER_MM7: {
        return X86_SIZE_QWORD;
    }
    default: {
        UNREACHABLE();
        return X86_SIZE_BYTE;
    }
    }
}

biscuit::GPR Recompiler::getGPR(ZydisRegister reg) {
    x86_ref_e ref = zydisToRef(reg);
    x86_size_e size = zydisToSize(reg);
    return getGPR(ref, size);
}

biscuit::Vec Recompiler::getVec(ZydisRegister reg) {
    ASSERT((reg >= ZYDIS_REGISTER_XMM0 && reg <= ZYDIS_REGISTER_XMM15) || (reg >= ZYDIS_REGISTER_MM0 && reg <= ZYDIS_REGISTER_MM7));
    x86_ref_e ref = zydisToRef(reg);
    return getVec(ref);
}

ZydisMnemonic Recompiler::decode(u64 rip, ZydisDecodedInstruction& instruction, ZydisDecodedOperand* operands) {
    ZyanStatus status = ZydisDecoderDecodeFull(&decoder, (void*)rip, 15, &instruction, operands);
    if (!ZYAN_SUCCESS(status)) {
        ERROR("Failed to decode instruction at 0x%016lx", rip);
    }
    return instruction.mnemonic;
}

x86_size_e Recompiler::getSize(const ZydisDecodedOperand* operand) {
    switch (operand->type) {
    case ZYDIS_OPERAND_TYPE_REGISTER: {
        return zydisToSize(operand->reg.value);
    }
    case ZYDIS_OPERAND_TYPE_MEMORY: {
        switch (operand->size) {
        case 8:
            return X86_SIZE_BYTE;
        case 16:
            return X86_SIZE_WORD;
        case 32:
            return X86_SIZE_DWORD;
        case 64:
            return X86_SIZE_QWORD;
        case 128:
            return X86_SIZE_XMM;
        default:
            UNREACHABLE();
            return X86_SIZE_BYTE;
        }
    }
    case ZYDIS_OPERAND_TYPE_IMMEDIATE: {
        switch (operand->imm.size) {
        case 8:
            return X86_SIZE_BYTE;
        case 16:
            return X86_SIZE_WORD;
        case 32:
            return X86_SIZE_DWORD;
        case 64:
            return X86_SIZE_QWORD;
        default:
            UNREACHABLE();
            return X86_SIZE_BYTE;
        }
    }
    default: {
        UNREACHABLE();
        return X86_SIZE_BYTE;
    }
    }
}

biscuit::GPR Recompiler::getGPR(const ZydisDecodedOperand* operand) {
    switch (operand->type) {
    case ZYDIS_OPERAND_TYPE_REGISTER: {
        biscuit::GPR reg = getGPR(operand->reg.value);
        return reg;
    }
    case ZYDIS_OPERAND_TYPE_MEMORY: {
        biscuit::GPR dest = scratch();
        u64 immediate = operand->mem.disp.value;
        if (IsValidSigned12BitImm(immediate) && !(current_instruction->attributes & ZYDIS_ATTRIB_HAS_SEGMENT) &&
            !g_config.paranoid) { // can't do this with seg+a32
            // Remove the immediate from the operand and use it in the write memory instruction
            // This can turn an ADDI+load into just a load if the LEA is just a register
            ZydisDecodedOperand op = *operand;
            op.mem.disp.value = 0;
            biscuit::GPR address = lea(&op, false);
            readMemory(dest, address, immediate, zydisToSize(operand->size));
        } else {
            biscuit::GPR address = lea(operand, false);
            readMemory(dest, address, 0, zydisToSize(operand->size));
        }
        return dest;
    }
    case ZYDIS_OPERAND_TYPE_IMMEDIATE: {
        u64 value = operand->imm.value.s;
        biscuit::GPR imm = scratch();
        as.LI(imm, value);
        return imm;
    }
    default: {
        UNREACHABLE();
        return x0;
    }
    }
}

biscuit::Vec Recompiler::getVec(const ZydisDecodedOperand* operand) {
    switch (operand->type) {
    case ZYDIS_OPERAND_TYPE_REGISTER: {
        biscuit::Vec reg = getVec(operand->reg.value);
        return reg;
    }
    case ZYDIS_OPERAND_TYPE_MEMORY: {
        biscuit::Vec vec = scratchVec();
        biscuit::GPR address = lea(operand, false);

        readMemory(vec, address, operand->size);

        popScratch(); // pop lea scratch

        return vec;
    }
    default: {
        UNREACHABLE();
        return v0;
    }
    }
}

biscuit::GPR Recompiler::getElementGPR(ZydisDecodedOperand* operand, x86_size_e size, int element, bool sext) {
    biscuit::GPR address;
    int offset;
    if (operand->type == ZYDIS_OPERAND_TYPE_REGISTER) {
        ASSERT(operand->reg.value >= ZYDIS_REGISTER_XMM0 && operand->reg.value <= ZYDIS_REGISTER_XMM15);
        x86_ref_e ref = zydisToRef(operand->reg.value);
        offset = offsetof(ThreadState, xmm) + (ref - X86_REF_XMM0) * sizeof(XmmReg) + (getBitSize(size) / 8) * element;
        address = threadStatePointer();
    } else if (operand->type == ZYDIS_OPERAND_TYPE_MEMORY) {
        address = lea(operand, false);
        offset = (getBitSize(size) / 8) * element;
    } else {
        UNREACHABLE();
    }

    biscuit::GPR temp = scratch();
    switch (size) {
    case X86_SIZE_BYTE: {
        if (!sext) {
            as.LBU(temp, offset, address);
        } else {
            as.LB(temp, offset, address);
        }
        break;
    }
    case X86_SIZE_WORD: {
        if (!sext) {
            as.LHU(temp, offset, address);
        } else {
            as.LH(temp, offset, address);
        }
        break;
    }
    case X86_SIZE_DWORD: {
        if (!sext) {
            as.LWU(temp, offset, address);
        } else {
            as.LW(temp, offset, address);
        }
        break;
    }
    case X86_SIZE_QWORD: {
        as.LD(temp, offset, address);
        break;
    }
    default: {
        UNREACHABLE();
    }
    }
    return temp;
}

biscuit::FPR Recompiler::getElementFPR(ZydisDecodedOperand* operand, x86_size_e size, int element) {
    biscuit::GPR address;
    int offset;
    if (operand->type == ZYDIS_OPERAND_TYPE_REGISTER) {
        ASSERT(operand->reg.value >= ZYDIS_REGISTER_XMM0 && operand->reg.value <= ZYDIS_REGISTER_XMM15);
        x86_ref_e ref = zydisToRef(operand->reg.value);
        offset = offsetof(ThreadState, xmm) + (ref - X86_REF_XMM0) * sizeof(XmmReg) + (getBitSize(size) / 8) * element;
        address = threadStatePointer();
    } else if (operand->type == ZYDIS_OPERAND_TYPE_MEMORY) {
        address = lea(operand, false);
        offset = (getBitSize(size) / 8) * element;
    } else {
        UNREACHABLE();
    }

    biscuit::FPR temp = scratchFPR();
    switch (size) {
    case X86_SIZE_DWORD: {
        as.FLW(temp, offset, address);
        break;
    }
    case X86_SIZE_QWORD: {
        as.FLD(temp, offset, address);
        break;
    }
    default: {
        UNREACHABLE();
    }
    }
    return temp;
}

void Recompiler::setElementGPR(ZydisDecodedOperand* operand, x86_size_e size, int element, biscuit::GPR src) {
    biscuit::GPR address;
    int offset;
    if (operand->type == ZYDIS_OPERAND_TYPE_REGISTER) {
        ASSERT(operand->reg.value >= ZYDIS_REGISTER_XMM0 && operand->reg.value <= ZYDIS_REGISTER_XMM15);
        x86_ref_e ref = zydisToRef(operand->reg.value);
        offset = offsetof(ThreadState, xmm) + (ref - X86_REF_XMM0) * sizeof(XmmReg) + (getBitSize(size) / 8) * element;
        address = threadStatePointer();
    } else if (operand->type == ZYDIS_OPERAND_TYPE_MEMORY) {
        address = lea(operand, false);
        offset = (getBitSize(size) / 8) * element;
    } else {
        UNREACHABLE();
    }

    switch (size) {
    case X86_SIZE_BYTE: {
        as.SB(src, offset, address);
        break;
    }
    case X86_SIZE_WORD: {
        as.SH(src, offset, address);
        break;
    }
    case X86_SIZE_DWORD: {
        as.SW(src, offset, address);
        break;
    }
    case X86_SIZE_QWORD: {
        as.SD(src, offset, address);
        break;
    }
    default: {
        UNREACHABLE();
    }
    }
}

void Recompiler::setElementFPR(ZydisDecodedOperand* operand, x86_size_e size, int element, biscuit::FPR src) {
    biscuit::GPR address;
    int offset;
    if (operand->type == ZYDIS_OPERAND_TYPE_REGISTER) {
        ASSERT(operand->reg.value >= ZYDIS_REGISTER_XMM0 && operand->reg.value <= ZYDIS_REGISTER_XMM15);
        x86_ref_e ref = zydisToRef(operand->reg.value);
        offset = offsetof(ThreadState, xmm) + (ref - X86_REF_XMM0) * sizeof(XmmReg) + (getBitSize(size) / 8) * element;
        address = threadStatePointer();
    } else if (operand->type == ZYDIS_OPERAND_TYPE_MEMORY) {
        address = lea(operand, false);
        offset = (getBitSize(size) / 8) * element;
    } else {
        UNREACHABLE();
    }

    switch (size) {
    case X86_SIZE_DWORD: {
        as.FSW(src, offset, address);
        break;
    }
    case X86_SIZE_QWORD: {
        as.FSD(src, offset, address);
        break;
    }
    default: {
        UNREACHABLE();
    }
    }
}

u64 Recompiler::getImmediate(ZydisDecodedOperand* operand) {
    ASSERT(operand->type == ZYDIS_OPERAND_TYPE_IMMEDIATE);
    return operand->imm.value.u;
}

biscuit::GPR Recompiler::flag(x86_ref_e ref) {
    if (ref == X86_REF_PF) {
        biscuit::GPR reg = scratch();
        as.LBU(reg, offsetof(ThreadState, pf), threadStatePointer());
        return reg;
    } else if (ref == X86_REF_AF) {
        biscuit::GPR reg = scratch();
        as.LBU(reg, offsetof(ThreadState, af), threadStatePointer());
        return reg;
    }

    biscuit::GPR reg = allocatedGPR(ref);
    return reg;
}

biscuit::GPR Recompiler::getGPR(const ZydisDecodedOperand* operand, x86_size_e size) {
    ASSERT(operand->type == ZYDIS_OPERAND_TYPE_REGISTER);
    return getGPR(zydisToRef(operand->reg.value), size);
}

biscuit::GPR Recompiler::getGPR(x86_ref_e ref, x86_size_e size) {
    biscuit::GPR gpr = allocatedGPR(ref);

    switch (size) {
    case X86_SIZE_BYTE: {
        biscuit::GPR gpr8 = scratch();
        zext(gpr8, gpr, X86_SIZE_BYTE);
        return gpr8;
    }
    case X86_SIZE_BYTE_HIGH: {
        biscuit::GPR gpr8 = scratch();
        as.SRLI(gpr8, gpr, 8);
        zext(gpr8, gpr8, X86_SIZE_BYTE);
        return gpr8;
    }
    case X86_SIZE_WORD: {
        biscuit::GPR gpr16 = scratch();
        zext(gpr16, gpr, X86_SIZE_WORD);
        return gpr16;
    }
    case X86_SIZE_DWORD: {
        if (!g_mode32) {
            // Need to zext and store in scratch
            biscuit::GPR gpr32 = scratch();
            zext(gpr32, gpr, X86_SIZE_DWORD);
            return gpr32;
        } else {
            // Already loaded as 32-bit zero-extended register
            return gpr;
        }
    }
    case X86_SIZE_QWORD: {
        return gpr;
    }
    default: {
        UNREACHABLE();
        return x0;
    }
    }
}

bool Recompiler::isGPR(ZydisRegister reg) {
    return zydisToRef(reg) >= X86_REF_RAX && zydisToRef(reg) <= X86_REF_R15;
}

void Recompiler::vsplat(biscuit::Vec vec, u64 imm) {
    if (imm <= 0xF) {
        as.VMV(vec, imm);
    } else {
        biscuit::GPR temp = scratch();
        as.LI(temp, imm);
        as.VMV(vec, temp);
    }
}

biscuit::Vec Recompiler::getVec(x86_ref_e ref) {
    biscuit::Vec vec = allocatedVec(ref);
    return vec;
}

void Recompiler::setGPR(x86_ref_e ref, x86_size_e size, biscuit::GPR reg) {
    switch (size) {
    case X86_SIZE_BYTE: {
        ASSERT(reg != allocatedGPR(ref));
        biscuit::GPR dest = getGPR(ref, X86_SIZE_QWORD);
        biscuit::GPR gpr8 = scratch();
        as.ANDI(gpr8, reg, 0xff);
        as.ANDI(dest, dest, ~0xff);
        as.OR(dest, dest, gpr8);
        popScratch();
        break;
    }
    case X86_SIZE_BYTE_HIGH: {
        ASSERT(reg != allocatedGPR(ref));
        if (!Extensions::B) {
            biscuit::GPR dest = getGPR(ref, X86_SIZE_QWORD);
            biscuit::GPR gpr8 = scratch();
            biscuit::GPR mask = scratch();
            as.LI(mask, 0xff00);
            as.SLLI(gpr8, reg, 8);
            as.AND(gpr8, gpr8, mask);
            as.NOT(mask, mask);
            as.AND(dest, dest, mask);
            as.OR(dest, dest, gpr8);
            popScratch();
            popScratch();
        } else {
            biscuit::GPR dest = getGPR(ref, X86_SIZE_QWORD);
            biscuit::GPR gpr8 = scratch();
            as.ANDI(gpr8, reg, 0xFF);
            as.RORI(dest, dest, 8);
            as.ANDI(dest, dest, ~0xFF);
            as.OR(dest, dest, gpr8);
            as.RORI(dest, dest, 56);
            popScratch();
        }
        break;
    }
    case X86_SIZE_WORD: {
        ASSERT(reg != allocatedGPR(ref));
        biscuit::GPR dest = getGPR(ref, X86_SIZE_QWORD);
        biscuit::GPR gpr16 = scratch();
        if (Extensions::B) {
            as.ZEXTH(gpr16, reg);
        } else {
            as.SLLI(gpr16, reg, 48);
            as.SRLI(gpr16, gpr16, 48);
        }
        as.SRLI(dest, dest, 16);
        as.SLLI(dest, dest, 16);
        as.OR(dest, dest, gpr16);
        popScratch();
        break;
    }
    case X86_SIZE_DWORD: {
        biscuit::GPR dest = allocatedGPR(ref); // don't need to load as the entire register is overwritten
        if (Extensions::B) {                   // TODO: in 32-bit mode we prob don't need to zero extend and we can do the same as 64-bit movs
            as.ZEXTW(dest, reg);
        } else {
            as.SLLI(dest, reg, 32);
            as.SRLI(dest, dest, 32);
        }
        break;
    }
    case X86_SIZE_QWORD: {
        biscuit::GPR dest = allocatedGPR(ref); // don't need to load as the entire register is overwritten
        if (dest != reg)
            as.MV(dest, reg);
        break;
    }
    default: {
        UNREACHABLE();
        break;
    }
    }
}

void Recompiler::setVec(x86_ref_e ref, biscuit::Vec vec) {
    biscuit::Vec dest = allocatedVec(ref);

    if (dest != vec) {
        if (Extensions::VLEN == 128) {
            ASSERT_MSG(isXMMOrMM(ref), "setVec dealing with YMM registers but your VLEN is 128");
            as.VMV1R(dest, vec);
        } else if (Extensions::VLEN >= 256) {
            if (isXMMOrMM(ref)) {
                if (!isCurrentLength128()) {
                    setVectorState(SEW::E8, 16);
                }

                as.VMV(dest, vec);
            } else if (isYMM(ref)) {
                if (Extensions::VLEN == 256) {
                    as.VMV1R(dest, vec); // doesn't have to mess with vector state
                } else {
                    if (!isCurrentLength256()) {
                        setVectorState(SEW::E8, 32);
                    }

                    as.VMV(dest, vec);
                }
            } else {
                UNREACHABLE();
            }
        } else {
            UNREACHABLE();
        }
    }
}

void Recompiler::setGPR(const ZydisDecodedOperand* operand, biscuit::GPR reg) {
    switch (operand->type) {
    case ZYDIS_OPERAND_TYPE_REGISTER: {
        x86_ref_e ref = zydisToRef(operand->reg.value);
        x86_size_e size = zydisToSize(operand->reg.value);
        setGPR(ref, size, reg);
        break;
    }
    case ZYDIS_OPERAND_TYPE_MEMORY: {
        u64 immediate = operand->mem.disp.value;
        if (IsValidSigned12BitImm(immediate) && !(current_instruction->attributes & ZYDIS_ATTRIB_HAS_SEGMENT) &&
            !g_config.paranoid) { // can't do this with seg+a32
            // Remove the immediate from the operand and use it in the write memory instruction
            // This can turn an ADDI+store into just a store if the LEA is just a register
            ZydisDecodedOperand op = *operand;
            op.mem.disp.value = 0;
            biscuit::GPR address = lea(&op, false);
            writeMemory(reg, address, immediate, zydisToSize(operand->size));
        } else {
            biscuit::GPR address = lea(operand, false);
            writeMemory(reg, address, 0, zydisToSize(operand->size));
        }
        break;
    }
    default: {
        UNREACHABLE();
    }
    }
}

void Recompiler::setVec(const ZydisDecodedOperand* operand, biscuit::Vec vec) {
    switch (operand->type) {
    case ZYDIS_OPERAND_TYPE_REGISTER: {
        x86_ref_e ref = zydisToRef(operand->reg.value);
        setVec(ref, vec);
        break;
    }
    case ZYDIS_OPERAND_TYPE_MEMORY: {
        biscuit::GPR address = lea(operand, false);

        switch (operand->size) {
        case 256: {
            setVectorState(SEW::E8, 256 / 8);
            as.VSE8(vec, address);
            break;
        }
        case 128: {
            setVectorState(SEW::E8, 128 / 8);
            as.VSE8(vec, address);
            break;
        }
        case 64: {
            setVectorState(SEW::E8, 64 / 8);
            as.VSE8(vec, address);
            break;
        }
        case 32: {
            setVectorState(SEW::E8, 32 / 8);
            as.VSE8(vec, address);
            break;
        }
        default: {
            UNREACHABLE();
        }
        }
        break;
    }
    default: {
        UNREACHABLE();
    }
    }
}

bool Recompiler::setVectorState(SEW sew, int vlen, LMUL grouping) {
    if (current_sew == sew && current_vlen == vlen && current_grouping == grouping && !g_config.paranoid) {
        return false;
    }

    current_sew = sew;
    current_vlen = vlen;
    current_grouping = grouping;

    // TODO: One day when we have chips that perform better with VTA::Yes, enable it
    if (!Extensions::Xtheadvector) {
        as.VSETIVLI(x0, vlen, sew, grouping, VTA::No, VMA::No);
    } else {
        biscuit::GPR vl = scratch();
        as.LI(vl, vlen);
        as.VSETVLI(x0, vl, sew, grouping);
        popScratch();
    }
    return true;
}

biscuit::GPR Recompiler::lea(const ZydisDecodedOperand* operand, bool use_temp) {
    if (cached_lea_operand == operand) {
        ASSERT(cached_lea_operand->mem.base == operand->mem.base);
        ASSERT(cached_lea_operand->mem.index == operand->mem.index);
        ASSERT(cached_lea_operand->mem.scale == operand->mem.scale);
        ASSERT(cached_lea_operand->mem.disp.value == operand->mem.disp.value);
        ASSERT(cached_lea_operand->mem.segment == operand->mem.segment);
        return cached_lea;
    }

    ASSERT(operand->type == ZYDIS_OPERAND_TYPE_MEMORY);
    biscuit::GPR address = scratch();
    cached_lea = address;
    cached_lea_operand = operand;

    biscuit::GPR base, index;

    if (operand->mem.base == ZYDIS_REGISTER_RIP) {
        ASSERT(!g_mode32);
        as.LI(address, current_rip + current_instruction->length + operand->mem.disp.value);
        return address;
    }

    bool has_base = operand->mem.base != ZYDIS_REGISTER_NONE;
    bool has_index = operand->mem.index != ZYDIS_REGISTER_NONE;
    bool has_segment = current_instruction->attributes & ZYDIS_ATTRIB_HAS_SEGMENT;
    bool has_disp = operand->mem.disp.value != 0;

    // Cover the case of just a segment register
    if (has_segment && !has_base && !has_index && !has_disp) {
        if (operand->mem.segment == ZYDIS_REGISTER_FS) {
            as.LD(address, offsetof(ThreadState, fsbase), threadStatePointer());
        } else if (operand->mem.segment == ZYDIS_REGISTER_GS) {
            as.LD(address, offsetof(ThreadState, gsbase), threadStatePointer());
        } else {
            UNREACHABLE();
        }
        return address;
    }

    if (!use_temp) {
        if (!has_segment && has_base && !has_index && !has_disp) {
            cached_lea_operand = nullptr;
            biscuit::GPR base = getGPR(operand->mem.base);
            return base;
        }

        if (!has_segment && !has_base && has_index && !has_disp && operand->mem.scale == 1) {
            cached_lea_operand = nullptr;
            biscuit::GPR index = getGPR(operand->mem.index);
            return index;
        }
    }

    if (has_disp) {
        // Load the displacement first
        if (has_base && IsValidSigned12BitImm(operand->mem.disp.value)) {
            base = getGPR(operand->mem.base);
            as.ADDI(address, base, operand->mem.disp.value);
        } else {
            as.LI(address, operand->mem.disp.value);
            if (has_base) {
                base = getGPR(operand->mem.base);
                as.ADD(address, address, base);
            }
        }

        if (has_index) {
            index = getGPR(operand->mem.index);
            u8 scale = operand->mem.scale;
            if (scale != 1) {
                if (Extensions::B) {
                    switch (scale) {
                    case 2:
                        as.SH1ADD(address, index, address);
                        break;
                    case 4:
                        as.SH2ADD(address, index, address);
                        break;
                    case 8: {
                        as.SH3ADD(address, index, address);
                        break;
                    }
                    default: {
                        UNREACHABLE();
                        break;
                    }
                    }
                } else {
                    switch (scale) {
                    case 2:
                        scale = 1;
                        break;
                    case 4:
                        scale = 2;
                        break;
                    case 8:
                        scale = 3;
                        break;
                    default:
                        UNREACHABLE();
                        break;
                    }
                    biscuit::GPR scale_reg = scratch();
                    as.SLLI(scale_reg, index, scale);
                    as.ADD(address, address, scale_reg);
                    popScratch();
                }
            } else {
                as.ADD(address, address, index);
            }
        }
    } else {
        if (has_index) {
            index = getGPR(operand->mem.index);
            u8 scale = operand->mem.scale;
            if (!has_base) {
                // No base, shift directly into address
                if (scale == 1) {
                    as.MV(address, index);
                } else {
                    switch (scale) {
                    case 2:
                        scale = 1;
                        break;
                    case 4:
                        scale = 2;
                        break;
                    case 8:
                        scale = 3;
                        break;
                    default:
                        UNREACHABLE();
                        break;
                    }
                    as.SLLI(address, index, scale);
                }
            } else {
                // Add index to the base
                base = getGPR(operand->mem.base);
                if (scale != 1) {
                    if (Extensions::B) {
                        switch (scale) {
                        case 2:
                            as.SH1ADD(address, index, base);
                            break;
                        case 4:
                            as.SH2ADD(address, index, base);
                            break;
                        case 8: {
                            as.SH3ADD(address, index, base);
                            break;
                        }
                        default: {
                            UNREACHABLE();
                            break;
                        }
                        }
                    } else {
                        switch (scale) {
                        case 2:
                            scale = 1;
                            break;
                        case 4:
                            scale = 2;
                            break;
                        case 8:
                            scale = 3;
                            break;
                        default:
                            UNREACHABLE();
                            break;
                        }
                        biscuit::GPR scale_reg = scratch();
                        as.SLLI(scale_reg, index, scale);
                        as.ADD(address, base, scale_reg);
                        popScratch();
                    }
                } else {
                    as.ADD(address, base, index);
                }
            }
        } else if (has_base) {
            base = getGPR(operand->mem.base);
            as.MV(address, base);
        } else {
            // We only get here if there's no base or index or segment and immediate == 0
            ASSERT(operand->mem.disp.value == 0);
            WARN("Compiling null memory access");
            return x0;
        }
    }

    // Address override prefix, this needs to happen before adding the segment override
    if (current_instruction->address_width != 64) {
        zext(address, address, zydisToSize(current_instruction->address_width));
    }

    if (has_segment) {
        int offset;
        switch (operand->mem.segment) {
        case ZYDIS_REGISTER_FS: {
            offset = offsetof(ThreadState, fsbase);
            break;
        }
        case ZYDIS_REGISTER_GS: {
            offset = offsetof(ThreadState, gsbase);
            break;
        }
        case ZYDIS_REGISTER_SS: {
            ASSERT(g_mode32);
            offset = offsetof(ThreadState, ssbase);
            break;
        }
        case ZYDIS_REGISTER_ES: {
            ASSERT(g_mode32);
            offset = offsetof(ThreadState, esbase);
            break;
        }
        case ZYDIS_REGISTER_DS: {
            ASSERT(g_mode32);
            offset = offsetof(ThreadState, dsbase);
            break;
        }
        case ZYDIS_REGISTER_CS: {
            ASSERT(g_mode32);
            offset = offsetof(ThreadState, csbase);
            break;
        }
        default: {
            UNREACHABLE();
        }
        }

        // Whether or not there's a displacement, at this point it's guaranteed that there's something in `address`
        biscuit::GPR seg = scratch();
        as.LD(seg, offset, threadStatePointer());
        as.ADD(address, address, seg);
        popScratch();
    }

    if (g_mode32) {
        // The additions may have overflown the address
        as.ZEXTW(address, address);
    }

    return address;
}

void Recompiler::stopCompiling() {
    // TODO: the stopCompiling should happen when we run out of instructions that were added on scanAhead
    ASSERT(compiling);
    compiling = false;
}

void Recompiler::setExitReason(ExitReason reason) {
    biscuit::GPR reg = scratch();
    as.LI(reg, (int)reason);
    as.SB(reg, offsetof(ThreadState, exit_reason), threadStatePointer());
    popScratch();
}

void Recompiler::writebackState() {
    current_sew = SEW::E1024;
    current_vlen = 0;
    current_grouping = LMUL::M1;

    biscuit::GPR rip = allocatedGPR(X86_REF_RIP);
    as.SD(rip, offsetof(ThreadState, rip), threadStatePointer());

    for (int i = 0; i < 16; i++) {
        x86_ref_e ref = (x86_ref_e)(X86_REF_RAX + i);
        as.SD(allocatedGPR(ref), offsetof(ThreadState, gprs) + i * sizeof(u64), threadStatePointer());
    }

    biscuit::GPR address = scratch();
    ASSERT(address != t6 && address != t5); // reason: see invalidate_caller_thunk

    static_assert(sizeof(XmmReg) == 16); // Change the below if XmmReg length is changed
    setVectorState(SEW::E64, 2);

    // TODO: can we optimize using special stores
    for (int i = 0; i < 16; i++) {
        biscuit::Vec vec = allocatedVec((x86_ref_e)(X86_REF_XMM0 + i));
        as.ADDI(address, threadStatePointer(), offsetof(ThreadState, xmm) + i * sizeof(XmmReg));
        as.VSE64(vec, address);
    }

    biscuit::Label mmx, end;
    as.LBU(address, offsetof(ThreadState, x87_state), threadStatePointer());
    as.XORI(address, address, (int)x87State::x87);
    as.BNEZ(address, &mmx);

    for (int i = 0; i < 8; i++) {
        biscuit::FPR fpr = allocatedFPR((x86_ref_e)(X86_REF_ST0 + i));
        as.FSD(fpr, offsetof(ThreadState, fp) + i * 8, threadStatePointer());
    }
    as.J(&end);

    as.Bind(&mmx);

    setVectorState(SEW::E64, 1);
    for (int i = 0; i < 8; i++) {
        biscuit::Vec vec = allocatedVec((x86_ref_e)(X86_REF_MM0 + i));
        as.ADDI(address, threadStatePointer(), offsetof(ThreadState, fp) + i * sizeof(u64));
        as.VSE64(vec, address);
    }

    as.Bind(&end);

    biscuit::GPR cf = allocatedGPR(X86_REF_CF);
    biscuit::GPR zf = allocatedGPR(X86_REF_ZF);
    biscuit::GPR sf = allocatedGPR(X86_REF_SF);
    biscuit::GPR of = allocatedGPR(X86_REF_OF);

    as.SB(cf, offsetof(ThreadState, cf), threadStatePointer());
    as.SB(zf, offsetof(ThreadState, zf), threadStatePointer());
    as.SB(sf, offsetof(ThreadState, sf), threadStatePointer());
    as.SB(of, offsetof(ThreadState, of), threadStatePointer());

    biscuit::GPR temp = scratch();
    // Now that everything is written to ThreadState, signal handlers can use it
    // instead of reading registers from ucontext
    as.LI(temp, 1);
    as.SB(temp, offsetof(ThreadState, state_is_correct), threadStatePointer());
    popScratch();

    current_sew = SEW::E1024;
    current_vlen = 0;
    current_grouping = LMUL::M1;
    cached_lea_operand = nullptr;
}

void Recompiler::restoreState() {
    current_sew = SEW::E1024;
    current_vlen = 0;
    current_grouping = LMUL::M1;

    biscuit::GPR rip = allocatedGPR(X86_REF_RIP);
    as.LD(rip, offsetof(ThreadState, rip), threadStatePointer());

    for (int i = 0; i < 16; i++) {
        x86_ref_e ref = (x86_ref_e)(X86_REF_RAX + i);
        as.LD(allocatedGPR(ref), offsetof(ThreadState, gprs) + i * sizeof(u64), threadStatePointer());
    }

    biscuit::GPR address = scratch();
    ASSERT(address != t6 && address != t5); // reason: see invalidate_caller_thunk

    static_assert(sizeof(XmmReg) == 16); // Change the below if XmmReg length is changed
    setVectorState(SEW::E64, 2);

    // TODO: can we optimize these using special loads
    for (int i = 0; i < 16; i++) {
        biscuit::Vec vec = allocatedVec((x86_ref_e)(X86_REF_XMM0 + i));
        as.ADDI(address, threadStatePointer(), offsetof(ThreadState, xmm) + sizeof(XmmReg) * i);
        as.VLE64(vec, address);
    }

    biscuit::Label mmx, end;

    as.LBU(address, offsetof(ThreadState, x87_state), threadStatePointer());
    as.XORI(address, address, (int)x87State::x87);
    as.BNEZ(address, &mmx);

    for (int i = 0; i < 8; i++) {
        biscuit::FPR fpr = allocatedFPR((x86_ref_e)(X86_REF_ST0 + i));
        as.FLD(fpr, offsetof(ThreadState, fp) + i * 8, threadStatePointer());
    }
    as.J(&end);

    as.Bind(&mmx);

    setVectorState(SEW::E64, 1);
    for (int i = 0; i < 8; i++) {
        biscuit::Vec vec = allocatedVec((x86_ref_e)(X86_REF_MM0 + i));
        as.ADDI(address, threadStatePointer(), offsetof(ThreadState, fp) + sizeof(u64) * i);
        as.VLE64(vec, address);
    }

    as.Bind(&end);

    popScratch();

    biscuit::GPR cf = allocatedGPR(X86_REF_CF);
    biscuit::GPR zf = allocatedGPR(X86_REF_ZF);
    biscuit::GPR sf = allocatedGPR(X86_REF_SF);
    biscuit::GPR of = allocatedGPR(X86_REF_OF);

    as.LBU(cf, offsetof(ThreadState, cf), threadStatePointer());
    as.LBU(zf, offsetof(ThreadState, zf), threadStatePointer());
    as.LBU(sf, offsetof(ThreadState, sf), threadStatePointer());
    as.LBU(of, offsetof(ThreadState, of), threadStatePointer());

    // Restore the rounding mode
    if (fsrm_sse) {
        biscuit::GPR rm = scratch();
        as.LBU(rm, offsetof(ThreadState, rmode_sse), threadStatePointer());
        as.FSRM(x0, rm);
        popScratch();
    } else {
        biscuit::GPR rm = scratch();
        as.LBU(rm, offsetof(ThreadState, rmode_x87), threadStatePointer());
        as.FSRM(x0, rm);
        popScratch();
    }

    // Mark state as invalid again as we will be modifying the host registers
    as.SB(x0, offsetof(ThreadState, state_is_correct), threadStatePointer());
    as.FENCETSO();

    current_sew = SEW::E1024;
    current_vlen = 0;
    current_grouping = LMUL::M1;
    cached_lea_operand = nullptr;
}

void Recompiler::backToDispatcher() {
    const u64 offset = compile_next_handler - (u64)as.GetCursorPointer();
    ASSERT(IsValid2GBImm(offset));
    const auto hi20 = static_cast<int32_t>(((static_cast<uint32_t>(offset) + 0x800) >> 12) & 0xFFFFF);
    const auto lo12 = static_cast<int32_t>(offset << 20) >> 20;
    ASSERT(isScratch(t6));
    as.AUIPC(t6, hi20);
    as.JR(t6, lo12);
}

void Recompiler::enterDispatcher(ThreadState* state) {
    enter_dispatcher(state);
}

void Recompiler::exitDispatcher(felix86_frame* frame) {
    exit_dispatcher(frame);
    __builtin_unreachable();
}

void Recompiler::scanAhead(u64 rip) {
    for (int i = 0; i < 6; i++) {
        flag_access_cpazso[i].clear();
    }

    instructions.clear();
    while (true) {
        instructions.push_back({});
        auto& [instruction, operands] = instructions.back();
        ZydisMnemonic mnemonic = decode(rip, instruction, operands);
        bool is_jump = instruction.meta.branch_type != ZYDIS_BRANCH_TYPE_NONE;
        bool is_ret = mnemonic == ZYDIS_MNEMONIC_RET || mnemonic == ZYDIS_MNEMONIC_IRETD || mnemonic == ZYDIS_MNEMONIC_IRETQ;
        bool is_call = mnemonic == ZYDIS_MNEMONIC_CALL;
        bool is_illegal = mnemonic == ZYDIS_MNEMONIC_UD2;
        bool is_hlt = mnemonic == ZYDIS_MNEMONIC_HLT;
        bool is_int3 = mnemonic == ZYDIS_MNEMONIC_INT3;

        if (g_config.unsafe_flags && !g_config.paranoid) {
            if (is_call || is_ret) {
                // Pretend that the call/ret changes the flags so that we don't calculate the flags
                // This is most often the case so it's a good optimization.
                flag_access_cpazso[0].push_back({true, rip});
                flag_access_cpazso[1].push_back({true, rip});
                flag_access_cpazso[2].push_back({true, rip});
                flag_access_cpazso[3].push_back({true, rip});
                flag_access_cpazso[4].push_back({true, rip});
                flag_access_cpazso[5].push_back({true, rip});
                break;
            }
        }

        if (instruction.mnemonic == ZYDIS_MNEMONIC_INVLPG) {
            // Don't calculate any flags
            if (!g_config.paranoid) {
                flag_access_cpazso[0].push_back({true, rip});
                flag_access_cpazso[1].push_back({true, rip});
                flag_access_cpazso[2].push_back({true, rip});
                flag_access_cpazso[3].push_back({true, rip});
                flag_access_cpazso[4].push_back({true, rip});
                flag_access_cpazso[5].push_back({true, rip});
            }
            ASSERT(operands[0].type == ZYDIS_OPERAND_TYPE_MEMORY);
            if (operands[0].mem.base == ZYDIS_REGISTER_RAX) {
                // Super hack! After invlpg [rax] comes a string which the recompiler skips and we also need to skip here.
                const char* string = (const char*)(rip + instruction.length);
                size_t size = strlen(string);
                ASSERT(size > 0);
                rip += instruction.length + size + 1; // don't forget null terminator
                continue;
            } else if (operands[0].mem.base == ZYDIS_REGISTER_RCX) {
                // Super hack! After invlpg [rcx] comes an address and a string which the recompiler skips and we also need to skip here.
                const char* string = (const char*)(rip + instruction.length + 8);
                size_t size = strlen(string);
                ASSERT(size > 0);
                rip += instruction.length + 8 + size + 1; // don't forget null terminator
                continue;
            }
        }

        if (instruction.attributes & ZYDIS_ATTRIB_CPUFLAG_ACCESS) {
            u32 changed =
                instruction.cpu_flags->modified | instruction.cpu_flags->set_0 | instruction.cpu_flags->set_1 | instruction.cpu_flags->undefined;
            u32 used = instruction.cpu_flags->tested;

            if (used & ZYDIS_CPUFLAG_CF || flag_passthrough(instruction.mnemonic, X86_REF_CF)) {
                flag_access_cpazso[0].push_back({false, rip});
            } else if (changed & ZYDIS_CPUFLAG_CF) {
                flag_access_cpazso[0].push_back({true, rip});
            }

            if (used & ZYDIS_CPUFLAG_PF || flag_passthrough(instruction.mnemonic, X86_REF_PF)) {
                flag_access_cpazso[1].push_back({false, rip});
            } else if (changed & ZYDIS_CPUFLAG_PF) {
                flag_access_cpazso[1].push_back({true, rip});
            }

            if (used & ZYDIS_CPUFLAG_AF || flag_passthrough(instruction.mnemonic, X86_REF_AF)) {
                flag_access_cpazso[2].push_back({false, rip});
            } else if (changed & ZYDIS_CPUFLAG_AF) {
                flag_access_cpazso[2].push_back({true, rip});
            }

            if (used & ZYDIS_CPUFLAG_ZF || flag_passthrough(instruction.mnemonic, X86_REF_ZF)) {
                flag_access_cpazso[3].push_back({false, rip});
            } else if (changed & ZYDIS_CPUFLAG_ZF) {
                flag_access_cpazso[3].push_back({true, rip});
            }

            if (used & ZYDIS_CPUFLAG_SF || flag_passthrough(instruction.mnemonic, X86_REF_SF)) {
                flag_access_cpazso[4].push_back({false, rip});
            } else if (changed & ZYDIS_CPUFLAG_SF) {
                flag_access_cpazso[4].push_back({true, rip});
            }

            if (used & ZYDIS_CPUFLAG_OF || flag_passthrough(instruction.mnemonic, X86_REF_OF)) {
                flag_access_cpazso[5].push_back({false, rip});
            } else if (changed & ZYDIS_CPUFLAG_OF) {
                flag_access_cpazso[5].push_back({true, rip});
            }
        }

        if (is_jump || is_ret || is_call || is_illegal || is_hlt || is_int3) {
            break;
        }

        rip += instruction.length;
    }
}

bool Recompiler::shouldEmitFlag(u64 rip, x86_ref_e ref) {
    if (flag_mode == FlagMode::AlwaysEmit || g_config.single_step) {
        return true;
    } else if (flag_mode == FlagMode::NeverEmit) {
        return false;
    }

    int index = 0;
    switch (ref) {
    case X86_REF_CF: {
        index = 0;
        break;
    case X86_REF_PF:
        index = 1;
        break;
    case X86_REF_AF:
        index = 2;
        break;
    case X86_REF_ZF:
        index = 3;
        break;
    case X86_REF_SF:
        index = 4;
        break;
    case X86_REF_OF:
        index = 5;
        break;
    default:
        UNREACHABLE();
        break;
    }
    }

    for (auto& [changed, r] : flag_access_cpazso[index]) {
        if (r > rip && !changed) {
            return true;
        }

        if (r > rip && changed) {
            return false;
        }
    }

    return true;
}

void Recompiler::updateOverflowSub(biscuit::GPR lhs, biscuit::GPR rhs, biscuit::GPR result, x86_size_e size_e) {
    int size = getBitSize(size_e);
    biscuit::GPR of = flag(X86_REF_OF);
    biscuit::GPR temp = scratch();
    if (size_e == X86_SIZE_QWORD) {
        as.SGTZ(of, rhs);
        as.SLT(temp, result, lhs);
        as.XOR(of, of, temp);
    } else {
        as.NOT(temp, lhs);
        as.OR(of, temp, rhs);
        as.AND(of, of, result);
        as.AND(temp, temp, rhs);
        as.OR(of, of, temp);
        as.SRLI(temp, of, size - 2);
        as.SRLI(of, of, size - 1);
        as.XOR(of, of, temp);
        as.ANDI(of, of, 1);
    }
    popScratch();
}

void Recompiler::updateOverflowAdd(biscuit::GPR lhs, biscuit::GPR rhs, biscuit::GPR result, x86_size_e size_e) {
    int size = getBitSize(size_e);
    biscuit::GPR of = flag(X86_REF_OF);
    biscuit::GPR temp = scratch();
    if (size_e == X86_SIZE_QWORD) {
        as.SLT(of, result, lhs);
        as.SLTI(temp, rhs, 0);
        as.XOR(of, of, temp);
    } else {
        as.OR(of, lhs, rhs);
        as.NOT(temp, result);
        as.AND(of, temp, of);
        as.AND(temp, lhs, rhs);
        as.OR(of, of, temp);
        as.SRLI(temp, of, size - 2);
        as.SRLI(of, of, size - 1);
        as.XOR(of, of, temp);
        as.ANDI(of, of, 1);
    }
    popScratch();
}

void Recompiler::updateAuxiliaryAdd(biscuit::GPR lhs, biscuit::GPR result) {
    biscuit::GPR af = scratch();
    biscuit::GPR temp = scratch();
    as.ANDI(af, result, 0xF);
    as.ANDI(temp, lhs, 0xF);
    as.SLTU(af, af, temp);
    as.SB(af, offsetof(ThreadState, af), threadStatePointer());
    popScratch();
    popScratch();
}

void Recompiler::updateAuxiliarySub(biscuit::GPR lhs, biscuit::GPR rhs) {
    biscuit::GPR af = scratch();
    biscuit::GPR temp = scratch();
    as.ANDI(af, rhs, 0xF);
    as.ANDI(temp, lhs, 0xF);
    as.SLTU(af, temp, af);
    as.SB(af, offsetof(ThreadState, af), threadStatePointer());
    popScratch();
    popScratch();
}

void Recompiler::updateAuxiliaryAdc(biscuit::GPR lhs, biscuit::GPR result, biscuit::GPR cf, biscuit::GPR result_2) {
    biscuit::GPR af = scratch();
    biscuit::GPR temp = scratch();
    as.ANDI(af, result, 0xF);
    as.ANDI(temp, lhs, 0xF);
    as.SLTU(af, af, temp);
    as.ANDI(temp, result_2, 0xF);
    as.SLTU(temp, temp, cf);
    as.OR(af, af, temp);
    as.SB(af, offsetof(ThreadState, af), threadStatePointer());
    popScratch();
    popScratch();
}

void Recompiler::updateAuxiliarySbb(biscuit::GPR lhs, biscuit::GPR rhs, biscuit::GPR result, biscuit::GPR cf) {
    biscuit::GPR af = scratch();
    biscuit::GPR temp = scratch();
    as.ANDI(af, rhs, 0xF);
    as.ANDI(temp, lhs, 0xF);
    as.SLTU(af, temp, af);
    as.ANDI(temp, result, 0xF);
    as.SLTU(temp, temp, cf);
    as.OR(af, af, temp);
    as.SB(af, offsetof(ThreadState, af), threadStatePointer());
    popScratch();
    popScratch();
}

void Recompiler::updateCarryAdd(biscuit::GPR lhs, biscuit::GPR result, x86_size_e size) {
    biscuit::GPR cf = flag(X86_REF_CF);
    zext(cf, result, size);
    as.SLTU(cf, cf, lhs);
}

void Recompiler::updateCarrySub(biscuit::GPR lhs, biscuit::GPR rhs) {
    biscuit::GPR cf = flag(X86_REF_CF);
    as.SLTU(cf, lhs, rhs);
}

void Recompiler::updateCarryAdc(biscuit::GPR lhs, biscuit::GPR result, biscuit::GPR result_2, x86_size_e size) {
    biscuit::GPR temp = scratch();
    biscuit::GPR temp2 = scratch();
    biscuit::GPR cf = flag(X86_REF_CF);
    zext(temp, result, size);
    zext(temp2, result_2, size);
    as.SLTU(temp, temp, lhs);
    as.SLTU(temp2, temp2, cf);
    as.OR(cf, temp, temp2);
    popScratch();
    popScratch();
}

void Recompiler::clearFlag(x86_ref_e ref) {
    biscuit::GPR f = flag(ref);
    as.LI(f, 0);
}

void Recompiler::setFlag(x86_ref_e ref) {
    biscuit::GPR f = flag(ref);
    as.LI(f, 1);
}

void Recompiler::zext(biscuit::GPR dest, biscuit::GPR src, x86_size_e size) {
    switch (size) {
    case X86_SIZE_BYTE:
    case X86_SIZE_BYTE_HIGH: {
        as.ANDI(dest, src, 0xff);
        break;
    }
    case X86_SIZE_WORD: {
        if (Extensions::B) {
            as.ZEXTH(dest, src);
        } else {
            as.SLLI(dest, src, 48);
            as.SRLI(dest, dest, 48);
        }
        break;
    }
    case X86_SIZE_DWORD: {
        if (Extensions::B) {
            as.ZEXTW(dest, src);
        } else {
            as.SLLI(dest, src, 32);
            as.SRLI(dest, dest, 32);
        }
        break;
    }
    case X86_SIZE_QWORD: {
        if (dest != src)
            as.MV(dest, src);
        break;
    }
    default: {
        UNREACHABLE();
        break;
    }
    }
}

int Recompiler::getBitSize(x86_size_e size) {
    switch (size) {
    case X86_SIZE_BYTE:
    case X86_SIZE_BYTE_HIGH:
        return 8;
    case X86_SIZE_WORD:
        return 16;
    case X86_SIZE_DWORD:
        return 32;
    case X86_SIZE_QWORD:
        return 64;
    case X86_SIZE_XMM:
        return 128;
    default:
        UNREACHABLE();
        return 0;
    }
}

u64 Recompiler::getSignMask(x86_size_e size_e) {
    u16 size = getBitSize(size_e);
    return 1ull << (size - 1);
}

void Recompiler::updateParity(biscuit::GPR result) {
    biscuit::GPR pf = scratch();
    if (Extensions::B) {
        as.ANDI(pf, result, 0xFF);
        as.CPOPW(pf, pf);
    } else {
        biscuit::GPR temp = scratch();
        as.SRLI(pf, result, 4);
        as.XOR(pf, pf, result);
        as.SRLI(temp, pf, 2);
        as.XOR(temp, pf, temp);
        as.SRLI(pf, temp, 1);
        as.XOR(pf, pf, temp);
        popScratch();
    }

    as.ANDI(pf, pf, 1);
    as.XORI(pf, pf, 1);
    as.SB(pf, offsetof(ThreadState, pf), threadStatePointer());
    popScratch();
}

void Recompiler::updateZero(biscuit::GPR result, x86_size_e size) {
    biscuit::GPR zf = flag(X86_REF_ZF);
    if (size != X86_SIZE_QWORD) {
        zext(zf, result, size);
        as.SEQZ(zf, zf);
    } else {
        as.SEQZ(zf, result);
    }
}

void Recompiler::updateSign(biscuit::GPR result, x86_size_e size) {
    biscuit::GPR sf = flag(X86_REF_SF);
    as.SRLI(sf, result, getBitSize(size) - 1);
    as.ANDI(sf, sf, 1);
}

void Recompiler::jumpAndLink(u64 rip) {
    if (!g_config.link) {
        // Just emit jump to dispatcher
        backToDispatcher();
        return;
    }

    u8* start = as.GetCursorPointer();
    if (!blockExists(rip)) {
        u8* link_me = as.GetCursorPointer();
        backToDispatcher();

        getBlockMetadata(rip).pending_links.push_back(link_me);
    } else {
        auto& target_meta = getBlockMetadata(rip);
        u64 target = target_meta.address;

        u64 offset = target - (u64)as.GetCursorPointer();
        if (IsValidJTypeImm(offset - 4)) {
            // TODO: if falling through to block, replace jump with auipc+addi to t5
            as.NOP();
            as.JAL(t5, offset - 4);
        } else {
            // Too far for a regular jump, use AUIPC+JR
            ASSERT(IsValid2GBImm(offset));
            const auto hi20 = static_cast<int32_t>(((static_cast<uint32_t>(offset) + 0x800) >> 12) & 0xFFFFF);
            const auto lo12 = static_cast<int32_t>(offset << 20) >> 20;

            ASSERT(isScratch(t4));
            ASSERT(isScratch(t5));
            as.AUIPC(t4, hi20);
            as.JALR(t5, lo12, t4); // for justification for this link to t5, see invalidate_caller_thunk
        }
    }

    // These jumps are always 2 instructions to keep consistent when backpatching is needed
    ASSERT(as.GetCursorPointer() - start == 2 * 4);
}

void Recompiler::jumpAndLinkConditional(biscuit::GPR condition, u64 rip_true, u64 rip_false) {
    Label true_label;
    as.BNEZ(condition, &true_label);

    biscuit::GPR rip = allocatedGPR(X86_REF_RIP);
    u64 rip_false_offset = rip_false - getCurrentMetadata().guest_address;
    addi(rip, rip, rip_false_offset);
    if (g_mode32) {
        zext(rip, rip, X86_SIZE_DWORD);
        jumpAndLink((u32)rip_false);
    } else {
        jumpAndLink(rip_false);
    }

    as.Bind(&true_label);
    u64 rip_true_offset = rip_true - getCurrentMetadata().guest_address;
    addi(rip, rip, rip_true_offset);
    if (g_mode32) {
        zext(rip, rip, X86_SIZE_DWORD);
        jumpAndLink((u32)rip_true);
    } else {
        jumpAndLink(rip_true);
    }
}

void Recompiler::expirePendingLinks(u64 rip) {
    if (!g_config.link) {
        return;
    }

    if (!blockExists(rip)) {
        return;
    }

    auto& block_meta = getBlockMetadata(rip);
    auto& pending_links = block_meta.pending_links;
    for (u8* link : pending_links) {
        u8* cursor = as.GetCursorPointer();
        as.SetCursorPointer(link);
        jumpAndLink(rip);
        as.SetCursorPointer(cursor);
    }

    flush_icache();

    block_meta.pending_links.clear();
}

u64 Recompiler::sextImmediate(u64 imm, ZyanU8 size) {
    switch (size) {
    case 8: {
        return (i64)(i8)imm;
    }
    case 16: {
        return (i64)(i16)imm;
    }
    case 32: {
        return (i64)(i32)imm;
    }
    case 64: {
        return imm;
    }
    default: {
        UNREACHABLE();
        return 0;
    }
    }
}

void Recompiler::addi(biscuit::GPR dst, biscuit::GPR src, u64 imm) {
    if (imm == 0 && dst == src) {
        return;
    }

    if ((i64)imm >= -2048 && (i64)imm < 2048) {
        as.ADDI(dst, src, imm);
    } else {
        biscuit::GPR reg = scratch();
        as.LI(reg, imm);
        as.ADD(dst, src, reg);
        popScratch();
    }
}

void Recompiler::sextb(biscuit::GPR dest, biscuit::GPR src) {
    if (Extensions::B) {
        as.SEXTB(dest, src);
    } else {
        as.SLLI(dest, src, 56);
        as.SRAI(dest, dest, 56);
    }
}

void Recompiler::sexth(biscuit::GPR dest, biscuit::GPR src) {
    if (Extensions::B) {
        as.SEXTH(dest, src);
    } else {
        as.SLLI(dest, src, 48);
        as.SRAI(dest, dest, 48);
    }
}

biscuit::GPR Recompiler::getCond(int cond) {
    switch (cond & 0xF) {
    case 0:
        return flag(X86_REF_OF);
    case 1: {
        biscuit::GPR of = scratch();
        as.XORI(of, flag(X86_REF_OF), 1);
        return of;
    }
    case 2:
        return flag(X86_REF_CF);
    case 3: {
        biscuit::GPR cf = scratch();
        as.XORI(cf, flag(X86_REF_CF), 1);
        return cf;
    }
    case 4:
        return flag(X86_REF_ZF);
    case 5: {
        biscuit::GPR zf = scratch();
        as.XORI(zf, flag(X86_REF_ZF), 1);
        return zf;
    }
    case 6: {
        biscuit::GPR cond = scratch();
        as.OR(cond, flag(X86_REF_CF), flag(X86_REF_ZF));
        return cond;
    }
    case 7: {
        biscuit::GPR cond = scratch();
        as.OR(cond, flag(X86_REF_CF), flag(X86_REF_ZF));
        as.XORI(cond, cond, 1);
        return cond;
    }
    case 8:
        return flag(X86_REF_SF);
    case 9: {
        biscuit::GPR sf = scratch();
        as.XORI(sf, flag(X86_REF_SF), 1);
        return sf;
    }
    case 10:
        return flag(X86_REF_PF);
    case 11: {
        biscuit::GPR pf = flag(X86_REF_PF);
        as.XORI(pf, pf, 1);
        return pf;
    }
    case 12: {
        biscuit::GPR cond = scratch();
        as.XOR(cond, flag(X86_REF_SF), flag(X86_REF_OF));
        return cond;
    }
    case 13: {
        biscuit::GPR cond = scratch();
        as.XOR(cond, flag(X86_REF_SF), flag(X86_REF_OF));
        as.XORI(cond, cond, 1);
        return cond;
    }
    case 14: {
        biscuit::GPR cond = scratch();
        as.XOR(cond, flag(X86_REF_SF), flag(X86_REF_OF));
        as.OR(cond, cond, flag(X86_REF_ZF));
        return cond;
    }
    case 15: {
        biscuit::GPR cond = scratch();
        as.XOR(cond, flag(X86_REF_SF), flag(X86_REF_OF));
        as.OR(cond, cond, flag(X86_REF_ZF));
        as.XORI(cond, cond, 1);
        return cond;
    }
    }

    UNREACHABLE();
    return x0;
}

void Recompiler::readMemory(biscuit::GPR dest, biscuit::GPR address, i64 offset, x86_size_e size) {
    // Warning: Don't change the LBU->LB etc. here, they must zero extend
    switch (size) {
    case X86_SIZE_BYTE: {
        as.LBU(dest, offset, address);
        break;
    }
    case X86_SIZE_WORD: {
        as.LHU(dest, offset, address);
        break;
    }
    case X86_SIZE_DWORD: {
        as.LWU(dest, offset, address);
        break;
    }
    case X86_SIZE_QWORD: {
        as.LD(dest, offset, address);
        break;
    }
    default: {
        UNREACHABLE();
        break;
    }
    }

    if (g_config.always_tso && !Extensions::TSO) {
        as.FENCE(FenceOrder::R, FenceOrder::RW);
    }
}

void Recompiler::readMemory(biscuit::Vec vec, biscuit::GPR address, int size) {
    switch (size) {
    case 8: {
        setVectorState(SEW::E8, 1);
        as.VLE8(vec, address); // These won't need to be patched as they can't be unaligned
        break;
    }
    case 16: {
        setVectorState(SEW::E8, 2);
        as.VLE8(vec, address);
        break;
    }
    case 32: {
        setVectorState(SEW::E8, 4);
        as.VLE8(vec, address);
        break;
    }
    case 64: {
        setVectorState(SEW::E8, 8);
        as.VLE8(vec, address);
        break;
    }
    case 128: {
        setVectorState(SEW::E8, 16);
        as.VLE8(vec, address);
        break;
    }
    case 256: {
        setVectorState(SEW::E8, 32);
        as.VLE8(vec, address);
        break;
    }
    default: {
        UNREACHABLE();
        break;
    }
    }
}

void Recompiler::writeMemory(biscuit::GPR src, biscuit::GPR address, i64 offset, x86_size_e size) {
    if (g_config.always_tso && !Extensions::TSO) {
        as.FENCE(FenceOrder::RW, FenceOrder::W);
    }

    switch (size) {
    case X86_SIZE_BYTE: {
        as.SB(src, offset, address);
        break;
    }
    case X86_SIZE_WORD: {
        as.SH(src, offset, address);
        break;
    }
    case X86_SIZE_DWORD: {
        as.SW(src, offset, address);
        break;
    }
    case X86_SIZE_QWORD: {
        as.SD(src, offset, address);
        break;
    }
    default: {
        UNREACHABLE();
        break;
    }
    }
}

x86_size_e Recompiler::zydisToSize(ZyanU8 size) {
    switch (size) {
    case 8:
        return X86_SIZE_BYTE;
    case 16:
        return X86_SIZE_WORD;
    case 32:
        return X86_SIZE_DWORD;
    case 64:
        return X86_SIZE_QWORD;
    case 80:
        return X86_SIZE_ST;
    case 128:
        return X86_SIZE_XMM;
    default:
        UNREACHABLE();
        return X86_SIZE_BYTE;
    }
}

void Recompiler::repPrologue(Label* loop_end, biscuit::GPR rcx) {
    // Signal handling would get tricky if we had to account for this looping mess of an instruction
    as.BEQZ(rcx, loop_end);
}

void Recompiler::repEpilogue(Label* loop_body, biscuit::GPR rcx) {
    as.ADDI(rcx, rcx, -1);
    as.BNEZ(rcx, loop_body);
}

void Recompiler::repzEpilogue(Label* loop_body, Label* loop_end, biscuit::GPR rcx, bool is_repz) {
    as.ADDI(rcx, rcx, -1);
    as.BEQZ(rcx, loop_end);

    if (is_repz) {
        biscuit::GPR zf = flag(X86_REF_ZF);
        as.BNEZ(zf, loop_body);
    } else {
        biscuit::GPR zf = flag(X86_REF_ZF);
        as.BEQZ(zf, loop_body);
    }
}

void Recompiler::sext(biscuit::GPR dst, biscuit::GPR src, x86_size_e size) {
    switch (size) {
    case X86_SIZE_BYTE: {
        sextb(dst, src);
        break;
    }
    case X86_SIZE_WORD: {
        sexth(dst, src);
        break;
    }
    case X86_SIZE_DWORD: {
        as.ADDIW(dst, src, 0);
        break;
    }
    case X86_SIZE_QWORD: {
        as.MV(dst, src);
        break;
    }
    default: {
        UNREACHABLE();
        break;
    }
    }
}

bool Recompiler::blockExists(u64 rip) {
    return getBlockMetadata(rip).address != 0;
}

void Recompiler::vrgather(biscuit::Vec dst, biscuit::Vec src, biscuit::Vec iota, VecMask mask) {
    if (dst == src || dst == iota) {
        biscuit::Vec temp = scratchVec();
        as.VRGATHER(temp, src, iota, mask);
        as.VMV(dst, temp);
        popScratch();
    } else {
        as.VRGATHER(dst, src, iota, mask);
    }
}

biscuit::GPR Recompiler::getFlags() {
    biscuit::GPR reg = scratch();
    biscuit::GPR temp = scratch();
    biscuit::GPR cf = flag(X86_REF_CF);
    biscuit::GPR pf = flag(X86_REF_PF);
    biscuit::GPR af = flag(X86_REF_AF);
    biscuit::GPR zf = flag(X86_REF_ZF);
    biscuit::GPR sf = flag(X86_REF_SF);
    biscuit::GPR of = flag(X86_REF_OF);
    as.LBU(temp, offsetof(ThreadState, df), threadStatePointer());
    as.SLLI(temp, temp, 10);
    as.SLLI(reg, of, 11);
    as.OR(reg, reg, temp);
    as.SLLI(temp, sf, 7);
    as.OR(reg, reg, temp);
    as.SLLI(temp, zf, 6);
    as.OR(reg, reg, temp);
    as.SLLI(temp, af, 4);
    as.OR(reg, reg, temp);
    as.SLLI(temp, pf, 2);
    as.OR(reg, reg, temp);
    as.OR(reg, reg, cf);
    as.ORI(reg, reg, 0b10);  // bit 1 always set in flags
    as.ORI(reg, reg, 0x200); // IE bit
    as.LBU(temp, offsetof(ThreadState, cpuid_bit), threadStatePointer());
    as.SLLI(temp, temp, 21);
    as.OR(reg, reg, temp);
    as.LBU(temp, offsetof(ThreadState, ac_bit), threadStatePointer());
    as.SLLI(temp, temp, 18);
    as.OR(reg, reg, temp);
    popScratch();
    return reg;
}

void Recompiler::setFlags(biscuit::GPR flags) {
    biscuit::GPR cf = flag(X86_REF_CF);
    biscuit::GPR zf = flag(X86_REF_ZF);
    biscuit::GPR sf = flag(X86_REF_SF);
    biscuit::GPR of = flag(X86_REF_OF);
    biscuit::GPR temp = scratch();

    as.ANDI(cf, flags, 1);

    as.SRLI(temp, flags, 2);
    as.ANDI(temp, temp, 1);
    as.SB(temp, offsetof(ThreadState, pf), threadStatePointer());

    as.SRLI(zf, flags, 6);
    as.ANDI(zf, zf, 1);

    as.SRLI(temp, flags, 4);
    as.ANDI(temp, temp, 1);
    as.SB(temp, offsetof(ThreadState, af), threadStatePointer());

    as.SRLI(sf, flags, 7);
    as.ANDI(sf, sf, 1);

    as.SRLI(temp, flags, 10);
    as.ANDI(temp, temp, 1);
    as.SB(temp, offsetof(ThreadState, df), threadStatePointer());

    as.SRLI(of, flags, 11);
    as.ANDI(of, of, 1);

    // CPUID bit may have been modified, which we need to emulate because this is how some programs detect CPUID support
    as.SRLI(temp, flags, 21);
    as.ANDI(temp, temp, 1);
    as.SB(temp, offsetof(ThreadState, cpuid_bit), threadStatePointer());

    as.SRLI(temp, flags, 18);
    as.ANDI(temp, temp, 1);
    as.SB(temp, offsetof(ThreadState, ac_bit), threadStatePointer());
}

biscuit::GPR Recompiler::getTOP() {
    biscuit::GPR top = scratch();
    as.LB(top, offsetof(ThreadState, fpu_top), threadStatePointer());
    return top;
}

biscuit::FPR Recompiler::getST(int index) {
    return allocatedFPR((x86_ref_e)(X86_REF_ST0 + index));
}

biscuit::FPR Recompiler::getST(ZydisDecodedOperand* operand) {
    if (operand->type == ZYDIS_OPERAND_TYPE_REGISTER) {
        ASSERT(operand->reg.value >= ZYDIS_REGISTER_ST0 && operand->reg.value <= ZYDIS_REGISTER_ST7);
        return getST(operand->reg.value - ZYDIS_REGISTER_ST0);
    } else if (operand->type == ZYDIS_OPERAND_TYPE_MEMORY) {
        switch (operand->size) {
        case 32: {
            biscuit::FPR st = scratchFPR();
            as.FLW(st, 0, lea(operand, false));
            as.FCVT_D_S(st, st);
            popScratch(); // the gpr address scratch
            return st;
        }
        case 64: {
            biscuit::FPR st = scratchFPR();
            as.FLD(st, 0, lea(operand, false));
            popScratch(); // the gpr address scratch
            return st;
        }
        case 80: {
            UNREACHABLE();
            return ft0;
        }
        default: {
            UNREACHABLE();
            return f0;
        }
        }
    } else {
        UNREACHABLE();
        return f0;
    }
}

void Recompiler::setST(int index, biscuit::FPR st) {
    biscuit::FPR stN = getST(index);
    if (stN != st) {
        as.FMV_D(stN, st);
    }
}

void Recompiler::setST(ZydisDecodedOperand* operand, biscuit::FPR value) {
    if (operand->type == ZYDIS_OPERAND_TYPE_REGISTER) {
        ASSERT(operand->reg.value >= ZYDIS_REGISTER_ST0 && operand->reg.value <= ZYDIS_REGISTER_ST7);
        return setST(operand->reg.value - ZYDIS_REGISTER_ST0, value);
    } else if (operand->type == ZYDIS_OPERAND_TYPE_MEMORY) {
        switch (operand->size) {
        case 32: {
            biscuit::FPR temp = scratchFPR();
            as.FCVT_S_D(temp, value);
            as.FSW(temp, 0, lea(operand, false));
            popScratch(); // the gpr address scratch
            break;
        }
        case 64: {
            as.FSD(value, 0, lea(operand, false));
            popScratch(); // the gpr address scratch
            break;
        }
        case 80: {
            biscuit::GPR address = lea(operand, false);
            writebackState();
            as.MV(a0, address);
            as.FMV_D(fa0, value);
            call((u64)f64_to_80_mem);
            restoreState();
            break;
        }
        default: {
            UNREACHABLE();
        }
        }
    } else {
        UNREACHABLE();
    }
}

void Recompiler::setTOP(biscuit::GPR new_top) {
    as.SB(new_top, offsetof(ThreadState, fpu_top), threadStatePointer());
}

void Recompiler::invalidateBlock(BlockMetadata* block) {
    if (block->address == 0) {
        return;
    }

    u64* address = (u64*)block->address;
    const u64 offset = (u64)invalidate_caller_thunk - (u64)address;
    const auto hi20 = static_cast<int32_t>(((static_cast<uint32_t>(offset) + 0x800) >> 12) & 0xFFFFF);
    const auto lo12 = static_cast<int32_t>(offset << 20) >> 20;
    u64 storage;
    Assembler tas((u8*)&storage, 8);
    ASSERT(isScratch(t4));
    ASSERT(isScratch(t6));
    tas.AUIPC(t4, hi20);
    tas.JALR(t6, lo12, t4);
    __atomic_store(address, &storage, __ATOMIC_SEQ_CST);

    if (g_config.address_cache) {
        AddressCacheEntry& entry = address_cache[block->guest_address & ((1 << address_cache_bits) - 1)];
        entry.guest = ~block->guest_address;
        entry.host = 0;
    }

    block->address = 0;
    std::atomic_thread_fence(std::memory_order_seq_cst);
}

void Recompiler::invalidateRange(u64 start, u64 end) {
    auto guard = page_map_lock.lock();
    auto lower = page_map.lower_bound(start);
    auto upper = page_map.upper_bound(end - 1);

    for (auto it = lower; it != upper; it++) {
        auto& blocks_in_page = it->second;
        for (BlockMetadata* block : blocks_in_page) {
            invalidateBlock(block);
        }
        blocks_in_page.clear();
    }
}

void Recompiler::invalidateRangeGlobal(u64 start, u64 end) {
    // Get all the pages in this range, search all thread states for these pages, invalidate the blocks in those pages
    auto states_guard = g_process_globals.states_lock.lock();
    start &= ~0xFFFull;
    end = (end + 0xFFF) & ~0xFFFull;
    u64 min = UINT64_MAX;
    u64 max = 0;
    for (ThreadState* state : g_process_globals.states) {
        state->recompiler->invalidateRange(start, end);

        u64 cache_start = (u64)state->recompiler->getStartOfCodeCache();
        u64 cache_end = (u64)state->recompiler->getEndOfCodeCache();
        if (cache_start < min) {
            min = cache_start;
        }
        if (cache_end > max) {
            max = cache_end;
        }
    }

    // Flush the entire affected range in one syscall
    flush_icache_global(min, max);
}

bool Recompiler::tryInlineSyscall() {
    if (g_config.paranoid) {
        return false;
    }

    if (g_mode32) {
        // Unimplemented for now
        return false;
    }

    // TODO: currently inlined syscalls are disabled because ecall seems to thrash more regs than we expect
    // and it wouldn't be any more worth to writeback entire state probably
    return false;

    switch (rax_value) {
#define CASE(sysno, argcount)                                                                                                                        \
    case felix86_x86_64_##sysno:                                                                                                                     \
        inlineSyscall(x64_to_riscv(felix86_x86_64_##sysno), argcount);                                                                               \
        return true

        CASE(setuid, 1);
        CASE(write, 3);
        CASE(getgid, 0);
        CASE(fsync, 1);
        CASE(fallocate, 4);
        CASE(clock_getres, 2);
        CASE(setsid, 0);
        CASE(io_setup, 2);
        CASE(semctl, 4);
        CASE(sched_getattr, 4);
        CASE(getpriority, 2);
        CASE(waitid, 5);
        CASE(sendto, 6);
        CASE(getsockopt, 5);
        CASE(sched_setaffinity, 3);
        CASE(getegid, 0);
        CASE(perf_event_open, 5);
        CASE(connect, 3);
        CASE(writev, 3);
        CASE(ppoll, 5);
        CASE(tgkill, 3);
        CASE(geteuid, 0);
        CASE(socket, 3);
        CASE(setsockopt, 5);
        CASE(capget, 2);
        CASE(get_mempolicy, 5);
        CASE(getpgid, 1);
        CASE(setreuid, 2);
        CASE(setfsuid, 1);
        CASE(io_submit, 3);
        CASE(wait4, 4);
        CASE(sched_getaffinity, 3);
        CASE(recvmsg, 3);
        CASE(munlock, 2);
        CASE(accept, 3);
        CASE(io_cancel, 3);
        CASE(io_destroy, 1);
        CASE(sched_rr_get_interval, 2);
        CASE(pipe2, 2);
        CASE(getppid, 0);
        CASE(pread64, 4);
        CASE(getsid, 1);
        CASE(socketpair, 4);
        CASE(bind, 3);
        CASE(nanosleep, 2);
        CASE(prctl, 5);
        CASE(getdents64, 3);
        CASE(clock_gettime, 2);
        CASE(semtimedop, 4);
        CASE(recvfrom, 6);
        CASE(setpgid, 2);
        CASE(clock_nanosleep, 4);
        CASE(mincore, 3);
        CASE(readv, 3);
        CASE(mlock, 2);
        CASE(pselect6, 6);
        CASE(set_robust_list, 2);
        CASE(listen, 2);
        CASE(mprotect, 3);
        CASE(sched_yield, 0);
        CASE(sched_setattr, 3);
        CASE(read, 3);
        CASE(pwrite64, 4);
        CASE(madvise, 3);
        CASE(inotify_init1, 1);
        CASE(ptrace, 4);
        CASE(gettid, 0);
        CASE(getresuid, 3);
        CASE(getrandom, 3);
        CASE(getpeername, 3);
        CASE(eventfd2, 2);
        CASE(setgid, 1);
        CASE(setregid, 2);
        CASE(getuid, 0);
        CASE(lookup_dcookie, 3);
        CASE(setfsgid, 1);
        CASE(exit_group, 1);
        CASE(getresgid, 3);
        CASE(sched_getscheduler, 1);
        CASE(flock, 2);
        CASE(mbind, 6);
        CASE(getsockname, 3);
        CASE(futex, 6);
        CASE(sendmsg, 3);
        CASE(kill, 2);
        CASE(dup3, 3);
        CASE(accept4, 4);
        CASE(getpid, 0);
        CASE(prlimit64, 4);
        CASE(gettimeofday, 2);

#undef CASE
    default: {
        return false;
    }
    }
}

void Recompiler::inlineSyscall(int sysno, int argcount) {
    // TODO: this doesn't work, I guess kernel thrashes some regs?
    // biscuit::GPR old_a0 = scratch();
    // biscuit::GPR old_a1 = scratch();
    // biscuit::GPR old_a7 = scratch();
    // as.MV(old_a0, a0);
    // as.MV(old_a1, a1);
    // as.MV(old_a7, a7);
    // as.LI(a7, sysno);
    // as.ECALL();
    // setGPR(X86_REF_RAX, X86_SIZE_QWORD, a0);
    // as.MV(a0, old_a0);
    // as.MV(a1, old_a1);
    // as.MV(a7, old_a7);
    UNIMPLEMENTED();
}

void Recompiler::checkModifiesRax(ZydisDecodedInstruction& instruction, ZydisDecodedOperand* operands) {
    u8 opcount = instruction.operand_count;

    if (instruction.mnemonic == ZYDIS_MNEMONIC_MOV) {
        bool is_rax = operands[0].type == ZYDIS_OPERAND_TYPE_REGISTER &&
                      (operands[0].reg.value == ZYDIS_REGISTER_RAX || operands[0].reg.value == ZYDIS_REGISTER_EAX);
        bool is_imm = operands[1].type == ZYDIS_OPERAND_TYPE_IMMEDIATE;
        if (is_rax && is_imm) {
            // We don't care to zero extend this in the EAX case as any negative number wouldn't be a valid syscall anyway
            // Compilers won't emit a write to ax/al/ah because that would keep the upper bits
            rax_value = operands[1].imm.value.s;
            return;
        }
    }

    // If any of the operands modifies RAX/EAX/AX/AL/AH we discard its old value by setting it to -1, which will not
    // inline to any syscall
    for (int i = 0; i < opcount; i++) {
        if (operands[i].type == ZYDIS_OPERAND_TYPE_REGISTER) {
            bool is_rax = false;
            switch (operands[i].reg.value) {
            case ZYDIS_REGISTER_AL:
            case ZYDIS_REGISTER_AH:
            case ZYDIS_REGISTER_AX:
            case ZYDIS_REGISTER_EAX:
            case ZYDIS_REGISTER_RAX:
                is_rax = true;
                break;
            default: {
                break;
            }
            }
            bool modified = operands[i].actions & ZYDIS_OPERAND_ACTION_MASK_WRITE;
            if (is_rax && modified) {
                rax_value = -1;
                return;
            }
        }
    }
}

// We statically allocate the x87 stack for speed
// Because it's a stack model we need to rotate our allocated registers on push/pop
void Recompiler::decrementTOP() {
    biscuit::FPR st0 = allocatedFPR(X86_REF_ST0);
    biscuit::FPR st1 = allocatedFPR(X86_REF_ST1);
    biscuit::FPR st2 = allocatedFPR(X86_REF_ST2);
    biscuit::FPR st3 = allocatedFPR(X86_REF_ST3);
    biscuit::FPR st4 = allocatedFPR(X86_REF_ST4);
    biscuit::FPR st5 = allocatedFPR(X86_REF_ST5);
    biscuit::FPR st6 = allocatedFPR(X86_REF_ST6);
    biscuit::FPR st7 = allocatedFPR(X86_REF_ST7);
    biscuit::FPR temp = scratchFPR();

    as.FMV_D(temp, st0);
    as.FMV_D(st0, st7);
    as.FMV_D(st7, st6);
    as.FMV_D(st6, st5);
    as.FMV_D(st5, st4);
    as.FMV_D(st4, st3);
    as.FMV_D(st3, st2);
    as.FMV_D(st2, st1);
    as.FMV_D(st1, temp);

    popScratchFPR();

    biscuit::GPR top = getTOP();
    as.ADDI(top, top, -1);
    as.ANDI(top, top, 0b111);
    setTOP(top);
    popScratch();
}

void Recompiler::pushX87(biscuit::FPR val) {
    biscuit::FPR st0 = allocatedFPR(X86_REF_ST0);
    biscuit::FPR st1 = allocatedFPR(X86_REF_ST1);
    biscuit::FPR st2 = allocatedFPR(X86_REF_ST2);
    biscuit::FPR st3 = allocatedFPR(X86_REF_ST3);
    biscuit::FPR st4 = allocatedFPR(X86_REF_ST4);
    biscuit::FPR st5 = allocatedFPR(X86_REF_ST5);
    biscuit::FPR st6 = allocatedFPR(X86_REF_ST6);
    biscuit::FPR st7 = allocatedFPR(X86_REF_ST7);

    as.FMV_D(st7, st6);
    as.FMV_D(st6, st5);
    as.FMV_D(st5, st4);
    as.FMV_D(st4, st3);
    as.FMV_D(st3, st2);
    as.FMV_D(st2, st1);
    as.FMV_D(st1, st0);
    as.FMV_D(st0, val);

    biscuit::GPR top = getTOP();
    as.ADDI(top, top, -1);
    as.ANDI(top, top, 0b111);
    setTOP(top);
    popScratch();
}

void Recompiler::popX87() {
    biscuit::FPR st0 = allocatedFPR(X86_REF_ST0);
    biscuit::FPR st1 = allocatedFPR(X86_REF_ST1);
    biscuit::FPR st2 = allocatedFPR(X86_REF_ST2);
    biscuit::FPR st3 = allocatedFPR(X86_REF_ST3);
    biscuit::FPR st4 = allocatedFPR(X86_REF_ST4);
    biscuit::FPR st5 = allocatedFPR(X86_REF_ST5);
    biscuit::FPR st6 = allocatedFPR(X86_REF_ST6);
    biscuit::FPR st7 = allocatedFPR(X86_REF_ST7);
    biscuit::FPR temp = scratchFPR();

    as.FMV_D(temp, st0);
    as.FMV_D(st0, st1);
    as.FMV_D(st1, st2);
    as.FMV_D(st2, st3);
    as.FMV_D(st3, st4);
    as.FMV_D(st4, st5);
    as.FMV_D(st5, st6);
    as.FMV_D(st6, st7);
    as.FMV_D(st7, temp);

    popScratchFPR();

    biscuit::GPR top = getTOP();
    as.ADDI(top, top, 1);
    as.ANDI(top, top, 0b111);
    setTOP(top);
    popScratch();
}

// Move from x87 registers to MMX registers and switch the x87_state flag
void Recompiler::switchToMMX() {
    biscuit::Label after;
    biscuit::GPR val = scratch();
    as.LBU(val, offsetof(ThreadState, x87_state), threadStatePointer());
    as.XORI(val, val, (int)x87State::MMX);
    as.BEQZ(val, &after);

    // Per the manual, MMX instructions set the TOP to 0
    setTOP(x0);

    setVectorState(SEW::E64, 1);

    // TODO: In the case that x87 TOP was not 0 this transition could be wrong?
    // IDK whats supposed to happen if there's x87 code, say TOP is 5, and then an MMX instruction
    // Does MM0 get ST5 (aka ST[0] when top is 5) or does it get ST0? Investigate
    for (int i = 0; i < 8; i++) {
        biscuit::Vec mm = allocatedVec((x86_ref_e)(X86_REF_MM0 + i));
        biscuit::FPR st = allocatedFPR((x86_ref_e)(X86_REF_ST0 + i));
        as.VFMV_SF(mm, st);
    }

    as.LI(val, (int)x87State::MMX);
    as.SB(val, offsetof(ThreadState, x87_state), threadStatePointer());

    popScratch();
    as.Bind(&after);
    local_x87_state = x87State::MMX;
}

void Recompiler::switchToX87() {
    biscuit::Label after;
    biscuit::GPR val = scratch();
    as.LBU(val, offsetof(ThreadState, x87_state), threadStatePointer());
    as.XORI(val, val, (int)x87State::x87);
    as.BEQZ(val, &after);

    setVectorState(SEW::E64, 1);
    for (int i = 0; i < 8; i++) {
        biscuit::Vec mm = allocatedVec((x86_ref_e)(X86_REF_MM0 + i));
        biscuit::FPR st = allocatedFPR((x86_ref_e)(X86_REF_ST0 + i));
        as.VFMV_FS(st, mm);
    }

    as.LI(val, (int)x87State::x87);
    as.SB(val, offsetof(ThreadState, x87_state), threadStatePointer());

    popScratch();
    as.Bind(&after);
    local_x87_state = x87State::x87;
}
