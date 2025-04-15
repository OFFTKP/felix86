#pragma once

#include <array>
#include <mutex>
#include <unordered_map>
#include <Zydis/Utils.h>
#include "Zydis/Decoder.h"
#include "biscuit/assembler.hpp"
#include "felix86/common/state.hpp"
#include "felix86/common/utility.hpp"

constexpr int address_cache_bits = 16;

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

    bool isScratch(biscuit::GPR reg);

    void popScratch();

    void popScratchVec();

    void popScratchFPR();

    biscuit::GPR getTOP();

    void pushST(biscuit::GPR top, biscuit::FPR st);

    void popST(biscuit::GPR top);

    void setTOP(biscuit::GPR top);

    biscuit::FPR getST(biscuit::GPR top, int index);

    biscuit::FPR getST(biscuit::GPR top, ZydisDecodedOperand* operand);

    void setST(biscuit::GPR top, int index, biscuit::FPR value);

    void setST(biscuit::GPR top, ZydisDecodedOperand* operand, biscuit::FPR value);

    biscuit::GPR getOperandGPR(ZydisDecodedOperand* operand);

    x86_size_e getOperandSize(ZydisDecodedOperand* operand);

    biscuit::Vec getOperandVec(ZydisDecodedOperand* operand);

    biscuit::GPR getRefGPR(x86_ref_e ref, x86_size_e size);

    biscuit::Vec getRefVec(x86_ref_e ref);

    void setOperandGPR(ZydisDecodedOperand* operand, biscuit::GPR reg);

    void setOperandVec(ZydisDecodedOperand* operand, biscuit::Vec reg);

    void setRefGPR(x86_ref_e ref, x86_size_e size, biscuit::GPR reg);

    void setRefGPR(ZydisRegister ref, x86_size_e size, biscuit::GPR reg) {
        return setRefGPR(zydisToRef(ref), size, reg);
    }

    void setRefVec(x86_ref_e ref, biscuit::Vec vec);

    biscuit::GPR lea(ZydisDecodedOperand* operand, bool use_temp = true);

    void stopCompiling();

    void setExitReason(ExitReason reason);

    void writebackDirtyState();

    void writebackMMXState();

    void restoreRoundingMode();

    void backToDispatcher();

    void enterDispatcher(ThreadState* state);

    [[noreturn]] void exitDispatcher(ThreadState* state);

    void* getCompileNext();

    void disableSignals();

    void enableSignals();

    bool shouldEmitFlag(u64 current_rip, x86_ref_e ref);

    void zext(biscuit::GPR dest, biscuit::GPR src, x86_size_e size);

    u64 sextImmediate(u64 imm, ZyanU8 size);

    void addi(biscuit::GPR dest, biscuit::GPR src, u64 imm);

    void invalidStateUntilJump();

    biscuit::GPR flag(x86_ref_e ref);

    biscuit::GPR flagW(x86_ref_e ref);

    biscuit::GPR flagWR(x86_ref_e ref);

    void updateParity(biscuit::GPR result);

    void updateZero(biscuit::GPR result, x86_size_e size);

    void updateSign(biscuit::GPR result, x86_size_e size);

    int getBitSize(x86_size_e size);

    u64 getSignMask(x86_size_e size_e);

    void setRip(biscuit::GPR reg);

    biscuit::GPR getRip();

    void jumpAndLink(u64 rip);

    void jumpAndLinkConditional(biscuit::GPR condition, u64 rip_true, u64 rip_false);

    void invalidateBlock(BlockMetadata* block);

    static void invalidateRangeGlobal(u64 start, u64 end);

    void invalidateRange(u64 start, u64 end);

    constexpr static biscuit::GPR threadStatePointer() {
        return x27; // saved register so that when we exit VM we don't have to save it
    }

    void setFlagUndefined(x86_ref_e ref);

    // TODO: move these elsewhere
    static x86_ref_e zydisToRef(ZydisRegister reg);

    static x86_size_e zydisToSize(ZydisRegister reg);

    static x86_size_e zydisToSize(ZyanU8 size);

    // Get the allocated register for the given register reference
    static constexpr biscuit::GPR allocatedGPR(x86_ref_e reg) {
        // RDI, RSI, RDX, R10, R8, R9 are allocated to a0, a1, a2, a3, a4, a5 to match the syscall abi and save some swapping instructions
        switch (reg) {
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

    static constexpr biscuit::Vec allocatedVec(x86_ref_e reg) {
        switch (reg) {
        case X86_REF_XMM0: {
            return biscuit::v1;
        }
        case X86_REF_XMM1: {
            return biscuit::v2;
        }
        case X86_REF_XMM2: {
            return biscuit::v3;
        }
        case X86_REF_XMM3: {
            return biscuit::v4;
        }
        case X86_REF_XMM4: {
            return biscuit::v5;
        }
        case X86_REF_XMM5: {
            return biscuit::v6;
        }
        case X86_REF_XMM6: {
            return biscuit::v7;
        }
        case X86_REF_XMM7: {
            return biscuit::v8;
        }
        case X86_REF_XMM8: {
            return biscuit::v9;
        }
        case X86_REF_XMM9: {
            return biscuit::v10;
        }
        case X86_REF_XMM10: {
            return biscuit::v11;
        }
        case X86_REF_XMM11: {
            return biscuit::v12;
        }
        case X86_REF_XMM12: {
            return biscuit::v13;
        }
        case X86_REF_XMM13: {
            return biscuit::v14;
        }
        case X86_REF_XMM14: {
            return biscuit::v15;
        }
        case X86_REF_XMM15: {
            return biscuit::v16;
        }
        case X86_REF_MM0: {
            return biscuit::v17;
        }
        case X86_REF_MM1: {
            return biscuit::v18;
        }
        case X86_REF_MM2: {
            return biscuit::v19;
        }
        case X86_REF_MM3: {
            return biscuit::v20;
        }
        case X86_REF_MM4: {
            return biscuit::v21;
        }
        case X86_REF_MM5: {
            return biscuit::v22;
        }
        case X86_REF_MM6: {
            return biscuit::v23;
        }
        case X86_REF_MM7: {
            return biscuit::v24;
        }
        default: {
            UNREACHABLE();
            return v0;
        }
        }
    }

    bool setVectorState(SEW sew, int elem_count, LMUL grouping = LMUL::M1);

    static constexpr u16 maxVlen() {
        return sizeof(XmmReg) * 8;
    }

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

    BlockMetadata& getBlockMetadata(u64 rip) {
        return block_metadata[rip];
    }

    void vrgather(biscuit::Vec dst, biscuit::Vec src, biscuit::Vec iota, VecMask mask = VecMask::No);

    bool blockExists(u64 rip);

    biscuit::GPR getFlags();

    void setFlags(biscuit::GPR flags);

    u64 getImmediate(ZydisDecodedOperand* operand);

    void emitSigreturnThunk();

    auto& getBlockMap() {
        return block_metadata;
    }

    auto& getHostPcMap() {
        return host_pc_map;
    }

    u64 getCompiledBlock(ThreadState* state, u64 rip);

    void pushCalltrace();

    void popCalltrace();

    void unlinkBlock(ThreadState* state, u64 rip);

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
        call(as, target);
    }

    static void call(Assembler& as, u64 target) {
        i64 offset = target - (u64)as.GetCursorPointer();
        if (IsValidJTypeImm(offset)) {
            as.JAL(offset);
        } else if (IsValid2GBImm(offset)) {
            const auto hi20 = static_cast<int32_t>(((static_cast<uint32_t>(offset) + 0x800) >> 12) & 0xFFFFF);
            const auto lo12 = static_cast<int32_t>(offset << 20) >> 20;
            as.AUIPC(t0, hi20);
            as.JALR(ra, lo12, t0);
        } else {
            as.LI(t0, target);
            as.JALR(t0);
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

    void assumeLoaded();

    void markDirty(x86_ref_e ref) {
        auto& metadata = getMetadata(ref);
        metadata.dirty = true;
        metadata.loaded = true; // since the value is fresh it's as if we read it from memory
    }

    void setFlagMode(FlagMode mode) {
        flag_mode = mode;
    }

private:
    struct RegisterMetadata {
        bool dirty = false;  // whether an instruction modified this value, so we know to store it to memory before exiting execution
        bool loaded = false; // whether a previous instruction loaded this value from memory, so we don't load it again
                             // if a syscall happens for example, this would be set to false so we load it again
    };

    struct FlagAccess {
        bool modification; // true if modified, false if used
        u64 position;
    };

    // Get the register and load the value into it if needed
    biscuit::GPR gpr(ZydisRegister reg);

    biscuit::Vec vec(ZydisRegister reg);

    void resetScratch();

    void emitDispatcher();

    void emitInvalidateCallerThunk();

    void loadGPR(x86_ref_e reg, biscuit::GPR gpr);

    void loadVec(x86_ref_e reg, biscuit::Vec vec);

    RegisterMetadata& getMetadata(x86_ref_e reg);

    void scanAhead(u64 rip);

    void expirePendingLinks(u64 rip);

    void emitNecessaryStuff();

    void markPagesAsReadOnly(u64 start, u64 end);

    void inlineSyscall(int sysno, int argcount);

    void unlinkAt(u8* address_of_jump);

    static void invalidateAt(ThreadState* state, u8* address_of_block);

    u8* code_cache{};
    biscuit::Assembler as{};
    ZydisDecoder decoder{};

    using Operands = ZydisDecodedOperand[ZYDIS_MAX_OPERAND_COUNT];
    std::vector<std::pair<ZydisDecodedInstruction, Operands>> instructions;

    ZydisDecodedInstruction* current_instruction;
    ZydisDecodedOperand* current_operands;
    u64 current_rip;

    void (*enter_dispatcher)(ThreadState*){};

    void (*exit_dispatcher)(ThreadState*){};

    void* compile_next_handler{};

    u64 invalidate_caller_thunk{};

    void* start_of_code_cache{};

    std::array<RegisterMetadata, 16> gpr_metadata{};
    std::array<RegisterMetadata, 16> xmm_metadata{};
    std::array<RegisterMetadata, 8> mm_metadata{};
    std::array<RegisterMetadata, 4> flag_metadata{};

    std::unordered_map<u64, BlockMetadata> block_metadata{};

    Semaphore page_map_lock;
    std::map<u64, std::vector<BlockMetadata*>> page_map{};

    // For fast host pc -> block metadata lookup (binary search vs looking up one by one)
    // on signal handlers
    std::map<u64, BlockMetadata*> host_pc_map{};

    bool compiling{};

    int scratch_index = 0;

    int vector_scratch_index = 0;

    int fpu_scratch_index = 0;

    int rax_value = -1;

    std::array<std::vector<FlagAccess>, 6> flag_access_cpazso{};

    BlockMetadata* current_block_metadata{};
    SEW current_sew = SEW::E1024;
    u8 current_vlen = 0;
    LMUL current_grouping = LMUL::M1;
    bool rounding_mode_set = false;
    int perf_fd = -1;

    biscuit::GPR cached_lea = x0;
    ZydisDecodedOperand* cached_lea_operand;

    std::array<AddressCacheEntry, 1 << address_cache_bits> address_cache{};

    FlagMode flag_mode = FlagMode::Default;

    constexpr static std::array scratch_gprs = {x1, x6, x28, x29, x30, x31, x7};

    // TODO: For better or for worst (definitely for worst) we rely on the fact that we start with an even
    // register and go sequentially like this
    // This has to do with the fact we want even registers sometimes so widening operations can use
    // the register group. In the future with a proper allocator we can make it so the order here doesn't
    // matter and the order picks an available group.
    constexpr static std::array scratch_vec = {v26, v27, v28, v29, v30, v31, v25}; // If changed, also change hardcoded in punpckh
};