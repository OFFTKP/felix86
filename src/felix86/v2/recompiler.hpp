#pragma once

#include <algorithm>
#include <array>
#include <unordered_map>
#include <Zydis/Utils.h>
#include "Zydis/Decoder.h"
#include "biscuit/assembler.hpp"
#include "felix86/common/frame.hpp"
#include "felix86/common/state.hpp"
#include "felix86/common/types.hpp"
#include "felix86/common/utility.hpp"

constexpr int address_cache_bits = 20;

constexpr static u64 jit_stack_size = 1024 * 1024;

struct AddressCacheEntry {
    u64 host{}, guest{};
};

enum class FlagMode {
    Default,
    AlwaysEmit,
    NeverEmit,
};

// This struct is for indicating within a block at which points a register contains a value of a guest register,
// and when it is just undefined. For example within a block, the register that represents RAX is not valid until it's loaded
// for the first time, and then when it's written back it becomes invalid again because it may change due to a syscall or something.
struct RegisterAccess {
    u64 address; // address where the load or writeback happened
    bool valid;  // true if loaded and potentially modified, false if written back to memory and allocated register holds garbage
};

struct BlockMetadata {
    u64 address{};
    u64 address_end{};
    u64 guest_address{};
    u64 guest_address_end{};
    std::vector<u8*> pending_links{};
    std::vector<std::pair<u64, u64>> instruction_spans{};
};

// WARN: don't allocate this struct on the stack as it's quite big due to address_cache and can lead to stack overflow
struct Recompiler {
    explicit Recompiler();
    ~Recompiler();
    Recompiler(const Recompiler&) = delete;
    Recompiler& operator=(const Recompiler&) = delete;
    Recompiler(Recompiler&&) = delete;
    Recompiler& operator=(Recompiler&&) = delete;

    u64 compile(ThreadState* state, u64 rip);

    inline Assembler& getAssembler() {
        return as;
    }

    biscuit::GPR scratch();

    biscuit::Vec scratchVec();

    biscuit::FPR scratchFPR();

    ZydisMnemonic decode(u64 rip, ZydisDecodedInstruction& instruction, ZydisDecodedOperand* operands);

    constexpr static bool isScratch(biscuit::GPR reg) {
        if (std::find(scratch_gprs.begin(), scratch_gprs.end(), reg) != scratch_gprs.end()) {
            return true;
        }

        return false;
    }

    void popScratch();

    void popScratchVec();

    void popScratchFPR();

    void resetScratch();

    biscuit::GPR getTOP();

    void setTOP(biscuit::GPR top);

    biscuit::FPR getST(int index);

    biscuit::FPR getST(ZydisDecodedOperand* operand);

    void setST(int index, biscuit::FPR value);

    void setST(ZydisDecodedOperand* operand, biscuit::FPR value);

    x86_size_e getSize(const ZydisDecodedOperand* operand);

    biscuit::GPR getGPR(const ZydisDecodedOperand* operand);

    biscuit::GPR getGPR(const ZydisDecodedOperand* operand, x86_size_e size);

    biscuit::GPR getGPR(x86_ref_e ref, x86_size_e size);

    biscuit::GPR getGPR(ZydisRegister reg);

    biscuit::Vec getVec(const ZydisDecodedOperand* operand);

    biscuit::Vec getVec(ZydisRegister reg);

    biscuit::Vec getVec(x86_ref_e ref);

    biscuit::GPR getElementGPR(ZydisDecodedOperand* operand, x86_size_e size, int element, bool sext = false);

    biscuit::FPR getElementFPR(ZydisDecodedOperand* operand, x86_size_e size, int element);

    void setElementGPR(ZydisDecodedOperand* operand, x86_size_e size, int element, biscuit::GPR src);

    void setElementFPR(ZydisDecodedOperand* operand, x86_size_e size, int element, biscuit::FPR src);

    void setGPR(const ZydisDecodedOperand* operand, biscuit::GPR reg);

    void setVec(const ZydisDecodedOperand* operand, biscuit::Vec reg);

    void setGPR(x86_ref_e ref, x86_size_e size, biscuit::GPR reg);

    void setGPR(ZydisRegister ref, x86_size_e size, biscuit::GPR reg) {
        return setGPR(zydisToRef(ref), size, reg);
    }

    void setVec(x86_ref_e ref, biscuit::Vec vec);

    biscuit::GPR lea(const ZydisDecodedOperand* operand, bool use_temp = true);

    void stopCompiling();

    void setExitReason(ExitReason reason);

    void backToDispatcher();

    void writebackState();

    void restoreState();

    void enterDispatcher(ThreadState* state);

    [[noreturn]] void exitDispatcher(felix86_frame* state);

    bool shouldEmitFlag(u64 current_rip, x86_ref_e ref);

    void zext(biscuit::GPR dest, biscuit::GPR src, x86_size_e size);

    u64 sextImmediate(u64 imm, ZyanU8 size);

    void addi(biscuit::GPR dest, biscuit::GPR src, u64 imm);

    biscuit::GPR flag(x86_ref_e ref);

    void updateParity(biscuit::GPR result);

    void updateZero(biscuit::GPR result, x86_size_e size);

    void updateSign(biscuit::GPR result, x86_size_e size);

    int getBitSize(x86_size_e size);

    u64 getSignMask(x86_size_e size_e);

    void jumpAndLink(u64 rip);

    void jumpAndLinkConditional(biscuit::GPR condition, u64 rip_true, u64 rip_false);

    void invalidateBlock(BlockMetadata* block);

    static void invalidateRangeGlobal(u64 start, u64 end);

    void invalidateRange(u64 start, u64 end);

    constexpr static biscuit::GPR threadStatePointer() {
        return x27; // saved register so that when we exit VM we don't have to save it
    }

    // TODO: move these elsewhere
    static x86_ref_e zydisToRef(ZydisRegister reg);

    static x86_size_e zydisToSize(ZydisRegister reg);

    static x86_size_e zydisToSize(ZyanU8 size);

    // Get the allocated register for the given register reference
    static constexpr biscuit::GPR allocatedGPR(x86_ref_e reg) {
        // RDI, RSI, RDX, R10, R8, R9 are allocated to a0, a1, a2, a3, a4, a5 to match the syscall abi and save some swapping instructions
        switch (reg) {
        case X86_REF_RIP: {
            return biscuit::gp; // we set --no-relax flag so that we can allocate gp
        }
        case X86_REF_RAX: {
            return biscuit::x5;
        }
        case X86_REF_RCX: {
            return biscuit::x26;
        }
        case X86_REF_RDX: {
            return biscuit::x12; // a2
        }
        case X86_REF_RBX: {
            return biscuit::x8;
        }
        case X86_REF_RSP: {
            return biscuit::x9;
        }
        case X86_REF_RBP: {
            return biscuit::x18;
        }
        case X86_REF_RSI: {
            return biscuit::x11; // a1 -- TODO: one day match abi for 32-bit version also
        }
        case X86_REF_RDI: {
            return biscuit::x10; // a0
        }
        case X86_REF_R8: {
            return biscuit::x14; // a5
        }
        case X86_REF_R9: {
            return biscuit::x15; // a4
        }
        case X86_REF_R10: {
            return biscuit::x13; // a3
        }
        case X86_REF_R11: {
            return biscuit::x16;
        }
        case X86_REF_R12: {
            return biscuit::x17;
        }
        case X86_REF_R13: {
            return biscuit::x22;
        }
        case X86_REF_R14: {
            return biscuit::x19;
        }
        case X86_REF_R15: {
            return biscuit::x20;
        }
        case X86_REF_CF: {
            return biscuit::x21;
        }
        case X86_REF_ZF: {
            return biscuit::x23;
        }
        case X86_REF_SF: {
            return biscuit::x24;
        }
        case X86_REF_OF: {
            return biscuit::x25;
        }
        default: {
            UNREACHABLE();
            return x0;
        }
        }
    }

    static constexpr biscuit::FPR allocatedFPR(x86_ref_e reg) {
        switch (reg) {
        case X86_REF_ST0 ... X86_REF_ST7: {
            return biscuit::FPR(ft0.Index() + (reg - X86_REF_ST0));
        }
        default: {
            UNREACHABLE();
            return f0;
        }
        }
    }

    static constexpr biscuit::Vec allocatedVec(x86_ref_e reg) {
        switch (reg) {
        case X86_REF_XMM0: {
            // Important to start on an even vector register so vector grouping works when we save/restore the entire state,
            // but also not use v0 because that's used for the mask register
            return biscuit::v2;
        }
        case X86_REF_XMM1: {
            return biscuit::v3;
        }
        case X86_REF_XMM2: {
            return biscuit::v4;
        }
        case X86_REF_XMM3: {
            return biscuit::v5;
        }
        case X86_REF_XMM4: {
            return biscuit::v6;
        }
        case X86_REF_XMM5: {
            return biscuit::v7;
        }
        case X86_REF_XMM6: {
            return biscuit::v8;
        }
        case X86_REF_XMM7: {
            return biscuit::v9;
        }
        case X86_REF_XMM8: {
            return biscuit::v10;
        }
        case X86_REF_XMM9: {
            return biscuit::v11;
        }
        case X86_REF_XMM10: {
            return biscuit::v12;
        }
        case X86_REF_XMM11: {
            return biscuit::v13;
        }
        case X86_REF_XMM12: {
            return biscuit::v14;
        }
        case X86_REF_XMM13: {
            return biscuit::v15;
        }
        case X86_REF_XMM14: {
            return biscuit::v16;
        }
        case X86_REF_XMM15: {
            return biscuit::v17;
        }
        case X86_REF_MM0: {
            return biscuit::v18;
        }
        case X86_REF_MM1: {
            return biscuit::v19;
        }
        case X86_REF_MM2: {
            return biscuit::v20;
        }
        case X86_REF_MM3: {
            return biscuit::v21;
        }
        case X86_REF_MM4: {
            return biscuit::v22;
        }
        case X86_REF_MM5: {
            return biscuit::v23;
        }
        case X86_REF_MM6: {
            return biscuit::v24;
        }
        case X86_REF_MM7: {
            return biscuit::v25;
        }
        default: {
            UNREACHABLE();
            return v0;
        }
        }
    }

    bool setVectorState(SEW sew, int elem_count, LMUL grouping = LMUL::M1);

    void sextb(biscuit::GPR dest, biscuit::GPR src);

    void sexth(biscuit::GPR dest, biscuit::GPR src);

    void sext(biscuit::GPR dest, biscuit::GPR src, x86_size_e size);

    biscuit::GPR getCond(int cond);

    void readMemory(biscuit::GPR dest, biscuit::GPR address, i64 offset, x86_size_e size);

    void readMemory(biscuit::Vec dest, biscuit::GPR address, int size);

    void writeMemory(biscuit::GPR src, biscuit::GPR address, i64 offset, x86_size_e size);

    void repPrologue(Label* loop_end, biscuit::GPR rcx);

    void repEpilogue(Label* loop_body, biscuit::GPR rcx);

    void repzEpilogue(Label* loop_body, Label* loop_end, biscuit::GPR rcx, bool is_repz);

    bool isGPR(ZydisRegister reg);

    void vsplat(biscuit::Vec vec, u64 imm);

    void setLockHandled() {
        lock_handled = true;
    }

    BlockMetadata& getBlockMetadata(u64 rip) {
        return block_metadata[rip];
    }

    void vrgather(biscuit::Vec dst, biscuit::Vec src, biscuit::Vec iota, VecMask mask = VecMask::No);

    bool blockExists(u64 rip);

    biscuit::GPR getFlags();

    void setFlags(biscuit::GPR flags);

    u64 getImmediate(ZydisDecodedOperand* operand);

    auto& getBlockMap() {
        return block_metadata;
    }

    auto& getHostPcMap() {
        return host_pc_map;
    }

    u64 getCompileNext() {
        return compile_next_handler;
    }

    u64 getCompiledBlock(ThreadState* state, u64 rip) {
        if (g_config.address_cache) {
            AddressCacheEntry& entry = address_cache[rip & ((1 << address_cache_bits) - 1)];
            if (entry.guest == rip) {
                return entry.host;
            } else if (blockExists(rip)) {
                u64 host = getBlockMetadata(rip).address;
                entry.guest = rip;
                entry.host = host;
                return host;
            } else {
                return compile(state, rip);
            }
        } else {
            if (blockExists(rip)) {
                return getBlockMetadata(rip).address;
            } else {
                return compile(state, rip);
            }
        }

        UNREACHABLE();
        return {};
    }

    bool tryInlineSyscall();

    void checkModifiesRax(ZydisDecodedInstruction& instruction, ZydisDecodedOperand* operands);

    u8 stackPointerSize() {
        return g_mode32 ? 4 : 8;
    }

    x86_size_e stackWidth() {
        return g_mode32 ? X86_SIZE_DWORD : X86_SIZE_QWORD;
    }

    void updateOverflowAdd(biscuit::GPR lhs, biscuit::GPR rhs, biscuit::GPR result, x86_size_e size);

    void updateOverflowSub(biscuit::GPR lhs, biscuit::GPR rhs, biscuit::GPR result, x86_size_e size);

    void updateCarryAdd(biscuit::GPR lhs, biscuit::GPR result, x86_size_e size);

    void updateCarrySub(biscuit::GPR lhs, biscuit::GPR rhs);

    void updateAuxiliaryAdd(biscuit::GPR lhs, biscuit::GPR result);

    void updateAuxiliarySub(biscuit::GPR lhs, biscuit::GPR rhs);

    void updateAuxiliaryAdc(biscuit::GPR lhs, biscuit::GPR result, biscuit::GPR cf, biscuit::GPR result_2);

    void updateAuxiliarySbb(biscuit::GPR lhs, biscuit::GPR rhs, biscuit::GPR result, biscuit::GPR cf);

    void updateCarryAdc(biscuit::GPR dst, biscuit::GPR result, biscuit::GPR result_2, x86_size_e size);

    void clearFlag(x86_ref_e flag);

    void setFlag(x86_ref_e flag);

    void clearCodeCache(ThreadState* state);

    void call(u64 target) {
        current_sew = SEW::E1024; // set state to garbage as a call may overwrite it
        call(as, target);
    }

    void callPointer(u64 offset) {
        ASSERT(isScratch(t4));
        as.LD(t4, offset, threadStatePointer());
        as.JALR(t4);
    }

    static void call(Assembler& as, u64 target) {
        ASSERT(isScratch(t4));
        i64 offset = target - (u64)as.GetCursorPointer();
        if (IsValidJTypeImm(offset)) {
            as.JAL(offset);
        } else if (IsValid2GBImm(offset)) {
            const auto hi20 = static_cast<int32_t>(((static_cast<uint32_t>(offset) + 0x800) >> 12) & 0xFFFFF);
            const auto lo12 = static_cast<int32_t>(offset << 20) >> 20;
            as.AUIPC(t4, hi20);
            as.JALR(ra, lo12, t4);
        } else {
            as.LI(t4, target);
            as.JALR(t4);
        }
    }

    void pushCalltrace() {
        if (g_config.calltrace) {
            ASSERT(isScratch(t4));
            writebackState();
            as.LI(t4, (u64)push_calltrace);
            as.MV(a0, threadStatePointer());
            as.LI(a1, current_rip);
            as.JALR(t4);
            restoreState();
        }
    }

    void popCalltrace() {
        if (g_config.calltrace) {
            ASSERT(isScratch(t4));
            writebackState();
            as.LI(t4, (u64)pop_calltrace);
            as.MV(a0, threadStatePointer());
            as.JALR(t4);
            restoreState();
        }
    }

    u8* getStartOfCodeCache() const {
        return (u8*)start_of_code_cache;
    }

    u8* getEndOfCodeCache() const {
        return (u8*)as.GetCursorPointer();
    }

    static bool isXMMOrMM(x86_ref_e ref) {
        return (ref >= X86_REF_XMM0 && ref <= X86_REF_XMM15) || (ref >= X86_REF_MM0 && ref <= X86_REF_MM7);
    }

    static bool isYMM(x86_ref_e ref) {
        return ref >= X86_REF_YMM0 && ref <= X86_REF_YMM15;
    }

    // return true if SEW and VL add up to 128 bits
    bool isCurrentLength128() {
        if (current_grouping != LMUL::M1) {
            return false;
        }

        switch (current_sew) {
        case biscuit::SEW::E64: {
            return current_vlen == 2;
        }
        case biscuit::SEW::E32: {
            return current_vlen == 4;
        }
        case biscuit::SEW::E16: {
            return current_vlen == 8;
        }
        case biscuit::SEW::E8: {
            return current_vlen == 16;
        }
        default: {
            break;
        }
        }

        return false;
    }

    // Return true if SEW and VL add up to 256 bits
    bool isCurrentLength256() {
        if (current_grouping != LMUL::M1) {
            return false;
        }

        switch (current_sew) {
        case biscuit::SEW::E64: {
            return current_vlen == 4;
        }
        case biscuit::SEW::E32: {
            return current_vlen == 8;
        }
        case biscuit::SEW::E16: {
            return current_vlen == 16;
        }
        case biscuit::SEW::E8: {
            return current_vlen == 32;
        }
        default: {
            break;
        }
        }

        return false;
    }

    u64 compileSequence(u64 rip);

    void compileInstruction(ZydisDecodedInstruction& instruction, ZydisDecodedOperand* operands, u64 rip);

    void setFlagMode(FlagMode mode) {
        flag_mode = mode;
    }

    std::deque<u64>& getCalltrace() {
        return calltrace;
    }

    void pushX87(biscuit::FPR val);

    void popX87();

    void switchToMMX();

    void switchToX87();

    BlockMetadata& getCurrentMetadata() {
        ASSERT(current_block_metadata);
        return *current_block_metadata;
    }

    void setFsrmSSE(bool is_sse) {
        fsrm_sse = is_sse;
    }

    void skipNext();

    std::pair<ZydisDecodedInstruction*, ZydisDecodedOperand*> getNextInstruction();

private:
    struct FlagAccess {
        bool modification; // true if modified, false if used
        u64 position;
    };

    void emitNecessaryStuff();

    void emitDispatcher();

    void emitInvalidateCallerThunk();

    void scanAhead(u64 rip);

    void expirePendingLinks(u64 rip);

    void markPagesAsReadOnly(u64 start, u64 end);

    void inlineSyscall(int sysno, int argcount);

    static void invalidateAt(ThreadState* state, u8* address_of_block, u8* linked_block);

    biscuit::Assembler as{};
    ZydisDecoder decoder{};

    using Operands = ZydisDecodedOperand[ZYDIS_MAX_OPERAND_COUNT];
    std::vector<std::pair<ZydisDecodedInstruction, Operands>> instructions;

    ZydisDecodedInstruction* current_instruction;
    ZydisDecodedOperand* current_operands;
    u64 current_rip;
    u64 current_instruction_index = 0;

    void (*enter_dispatcher)(ThreadState*){};

    void (*exit_dispatcher)(felix86_frame*){};

    u64 compile_next_handler{};

    u64 invalidate_caller_thunk{};

    void* start_of_code_cache{};

    std::unordered_map<u64, BlockMetadata> block_metadata{};

    Semaphore page_map_lock;
    std::map<u64, std::vector<BlockMetadata*>> page_map{};

    // For fast host pc -> block metadata lookup (binary search vs looking up one by one)
    // on signal handlers
    std::map<u64, BlockMetadata*> host_pc_map{};

    bool compiling{};

    // Whether we already set ThreadState::x87_state to a value or not
    x87State local_x87_state = x87State::Unknown;

    int scratch_index = 0;

    int vector_scratch_index = 0;

    int fpu_scratch_index = 0;

    int rax_value = -1;

    std::array<std::vector<FlagAccess>, 6> flag_access_cpazso{};

    BlockMetadata* current_block_metadata{};
    SEW current_sew = SEW::E1024;
    u8 current_vlen = 0;
    LMUL current_grouping = LMUL::M1;

    biscuit::GPR cached_lea = x0;
    const ZydisDecodedOperand* cached_lea_operand;

    bool fsrm_sse = true;

    bool lock_handled = false;

    bool skip_next = false;

    std::array<AddressCacheEntry, 1 << address_cache_bits> address_cache{};

    std::deque<u64> calltrace{};

    FlagMode flag_mode = FlagMode::Default;

    u64 code_cache_size_index = 0;

    int optimization_guard_counter = 0; // see OptimizationGuard

    constexpr static std::array scratch_gprs = {
        x1, x6, x28, x29, x7, x30, x31,
    };

    // TODO: For better or for worst (definitely for worst) we rely on the fact that we start with an even
    // register and go sequentially like this
    // This has to do with the fact we want even registers sometimes so widening operations can use
    // the register group. In the future with a proper allocator we can make it so the order here doesn't
    // matter and the order picks an available group.
    constexpr static std::array scratch_vec = {v26, v27, v28, v29, v30, v31, v1}; // If changed, also change hardcoded in punpckh

    constexpr static std::array scratch_fprs = {ft8, ft9, ft10, ft11};
};