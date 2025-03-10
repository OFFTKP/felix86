#pragma once

#include <array>
#include <mutex>
#include <unordered_map>
#include <Zydis/Utils.h>
#include "Zydis/Decoder.h"
#include "biscuit/assembler.hpp"
#include "felix86/common/state.hpp"
#include "felix86/common/utility.hpp"

// 16 gprs, 5 flags, 16 xmm registers
constexpr u64 allocated_reg_count = 16 + 5 + 16;

constexpr int block_cache_bits = 16;

struct HandlerMetadata {
    HostAddress rip{};
    HostAddress block_start{};
};

struct BlockCacheEntry {
    HostAddress host{}, guest{};
};

// This struct is for indicating within a block at which points a register contains a value of a guest register,
// and when it is just undefined. For example within a block, the register that represents RAX is not valid until it's loaded
// for the first time, and then when it's written back it becomes invalid again because it may change due to a syscall or something.
struct RegisterAccess {
    HostAddress address; // address where the load or writeback happened
    bool valid;          // true if loaded and potentially modified, false if written back to memory and allocated register holds garbage
};

struct BlockMetadata {
    HostAddress address{};
    HostAddress address_end{};
    HostAddress guest_address{};
    HostAddress guest_address_end{};
    std::vector<u8*> pending_links{};
    std::vector<u8*> links{}; // where this block was linked to, used for unlinking it
    std::vector<std::pair<GuestAddress, HostAddress>> instruction_spans{};
};

struct Recompiler {
    explicit Recompiler();
    ~Recompiler();
    Recompiler(const Recompiler&) = delete;
    Recompiler& operator=(const Recompiler&) = delete;
    Recompiler(Recompiler&&) = delete;
    Recompiler& operator=(Recompiler&&) = delete;

    HostAddress compile(ThreadState* state, HostAddress rip);

    inline Assembler& getAssembler() {
        return as;
    }

    biscuit::GPR scratch();

    biscuit::Vec scratchVec();

    biscuit::FPR scratchFPR();

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

    void setRefVec(x86_ref_e ref, biscuit::Vec vec);

    biscuit::GPR lea(ZydisDecodedOperand* operand);

    biscuit::GPR leaAddBase(ZydisDecodedOperand* operand);

    void stopCompiling();

    void setExitReason(ExitReason reason);

    void writebackDirtyState();

    void restoreRoundingMode();

    void backToDispatcher(bool use_rsb = false);

    void enterDispatcher(ThreadState* state);

    [[noreturn]] void exitDispatcher(ThreadState* state);

    void* getCompileNext();

    void disableSignals();

    void enableSignals();

    bool shouldEmitFlag(HostAddress current_rip, x86_ref_e ref);

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

    void jumpAndLink(HostAddress rip, bool use_rsb = false);

    void jumpAndLinkConditional(biscuit::GPR condition, biscuit::GPR gpr_true, biscuit::GPR gpr_false, HostAddress rip_true, HostAddress rip_false);

    void invalidateBlock(BlockMetadata* block);

    constexpr static biscuit::GPR threadStatePointer() {
        return x27; // saved register so that when we exit VM we don't have to save it
    }

    void setFlagUndefined(x86_ref_e ref);

    // TODO: move these elsewhere
    static x86_ref_e zydisToRef(ZydisRegister reg);

    static x86_size_e zydisToSize(ZydisRegister reg);

    static x86_size_e zydisToSize(ZyanU8 size);

    std::lock_guard<std::mutex> lock() {
        return std::lock_guard{block_map_mutex};
    }

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
            return biscuit::x11; // a1
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
        default: {
            UNREACHABLE();
            return v0;
        }
        }
    }

    bool setVectorState(SEW sew, int elem_count, LMUL grouping = LMUL::M1);

    static constexpr u16 maxVlen() {
        return 128;
    }

    void sextb(biscuit::GPR dest, biscuit::GPR src);

    void sexth(biscuit::GPR dest, biscuit::GPR src);

    void sext(biscuit::GPR dest, biscuit::GPR src, x86_size_e size);

    biscuit::GPR getCond(int cond);

    void readMemory(biscuit::GPR dest, biscuit::GPR address, i64 offset, x86_size_e size);

    void readMemoryVectorNoBase(biscuit::Vec dest, biscuit::GPR address, int size);

    void writeMemory(biscuit::GPR src, biscuit::GPR address, i64 offset, x86_size_e size);

    void readMemoryNoBase(biscuit::GPR dest, biscuit::GPR address, i64 offset, x86_size_e size);

    void writeMemoryNoBase(biscuit::GPR src, biscuit::GPR address, i64 offset, x86_size_e size);

    void repPrologue(Label* loop_end, biscuit::GPR rcx);

    void repEpilogue(Label* loop_body, biscuit::GPR rcx);

    void repzEpilogue(Label* loop_body, Label* loop_end, biscuit::GPR rcx, bool is_repz);

    bool isGPR(ZydisRegister reg);

    BlockMetadata& getBlockMetadata(HostAddress rip) {
        return block_metadata[rip.raw()];
    }

    void vrgather(biscuit::Vec dst, biscuit::Vec src, biscuit::Vec iota, VecMask mask = VecMask::No);

    bool blockExists(HostAddress rip);

    biscuit::GPR getFlags();

    u64 getImmediate(ZydisDecodedOperand* operand);

    HostAddress emitSigreturnThunk();

    HostAddress emitUnlinkIndirectThunk();

    auto& getBlockMap() {
        return block_metadata;
    }

    auto& getHostPcMap() {
        return host_pc_map;
    }

    HostAddress getCompiledBlock(ThreadState* state, HostAddress rip);

    void pushCalltrace();

    void popCalltrace();

    void unlinkBlock(ThreadState* state, HostAddress rip);

    bool tryInlineSyscall();

    void checkModifiesRax(ZydisDecodedInstruction& instruction, ZydisDecodedOperand* operands);

    u8 stackPointerSize() {
        return g_mode32 ? 4 : 8;
    }

    x86_size_e addressWidth() {
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

    void zeroFlag(x86_ref_e flag);

    void setFlag(x86_ref_e flag);

    void trace(u64 address);

    void printTrace();

    void linkIndirect();

    u8* getUnlinkIndirectThunk() {
        return unlink_indirect_thunk;
    }

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

    u8* getStartOfCodeCache() {
        return (u8*)start_of_code_cache;
    }

private:
    struct RegisterMetadata {
        x86_ref_e reg;
        bool dirty = false;  // whether an instruction modified this value, so we know to store it to memory before exiting execution
        bool loaded = false; // whether a previous instruction loaded this value from memory, so we don't load it again
                             // if a syscall happens for example, this would be set to false so we load it again
    };

    struct FlagAccess {
        bool modification; // true if modified, false if used
        HostAddress position;
    };

    HostAddress compileSequence(HostAddress rip);

    // Get the register and load the value into it if needed
    biscuit::GPR gpr(ZydisRegister reg);

    biscuit::Vec vec(ZydisRegister reg);

    void resetScratch();

    void emitDispatcher();

    void loadGPR(x86_ref_e reg, biscuit::GPR gpr);

    void loadVec(x86_ref_e reg, biscuit::Vec vec);

    RegisterMetadata& getMetadata(x86_ref_e reg);

    void scanFlagUsageAhead(HostAddress rip);

    ZydisMnemonic decode(HostAddress rip, ZydisDecodedInstruction& instruction, ZydisDecodedOperand* operands);

    void expirePendingLinks(HostAddress rip);

    void emitNecessaryStuff();

    void markPagesAsReadOnly(HostAddress start, HostAddress end);

    void inlineSyscall(int sysno, int argcount);

    void unlinkAt(u8* address_of_jump);

    u8* code_cache{};
    biscuit::Assembler as{};
    ZydisDecoder decoder{};

    ZydisDecodedInstruction instruction{};
    ZydisDecodedOperand operands[10]{};

    void (*enter_dispatcher)(ThreadState*){};

    void (*exit_dispatcher)(ThreadState*){};

    void* compile_next_handler{};

    void* start_of_code_cache{};

    u8* unlink_indirect_thunk{};

    // 16 GPRS followed by 4 flags (CF,OF,ZF,SF) then 16 XMMs
    std::array<RegisterMetadata, 16 + 4 + 16> metadata{};

    // This may be locked by a different thread on a signal handler to unlink a block
    std::mutex block_map_mutex{};

    std::unordered_map<u64, BlockMetadata> block_metadata{};

    // For fast host pc -> block metadata lookup (binary search vs looking up one by one)
    // on signal handlers
    std::map<u64, BlockMetadata*> host_pc_map{};

    bool compiling{};

    int scratch_index = 0;

    int vector_scratch_index = 0;

    int fpu_scratch_index = 0;

    int rax_value = -1;

    // TODO: replace this method of flag detection with 5 (4? remove AF) assemblers that emit flag calculations seperately.
    // TODO: Then when a flag is used copy the instructions over from the equivalent assembler, reset it when overwritten
    std::array<std::vector<FlagAccess>, 6> flag_access_cpazso{};

    BlockMetadata* current_block_metadata{};
    HandlerMetadata* current_meta{};
    SEW current_sew = SEW::E1024;
    u8 current_vlen = 0;
    LMUL current_grouping = LMUL::M1;
    bool rounding_mode_set = false;
    int perf_fd = -1;

    std::vector<u64> block_trace;
    size_t block_trace_index = 0;

    std::array<bool, 16> zexted_gprs; // gprs that have been set in 32-bit form, to avoid future zexts

    std::array<BlockCacheEntry, 1 << block_cache_bits> block_cache{};
};