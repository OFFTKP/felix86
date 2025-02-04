#pragma once

#include <array>
#include <unordered_map>
#include <Zydis/Utils.h>
#include "Zydis/Decoder.h"
#include "biscuit/assembler.hpp"
#include "felix86/common/state.hpp"
#include "felix86/common/utility.hpp"

// 16 gprs, 5 flags, 16 xmm registers
constexpr u64 allocated_reg_count = 16 + 5 + 16;

struct HandlerMetadata {
    u64 rip;
    u64 block_start;
};

// This struct is for indicating within a block at which points a register contains a value of a guest register,
// and when it is just undefined. For example within a block, the register that represents RAX is not valid until it's loaded
// for the first time, and then when it's written back it becomes invalid again because it may change due to a syscall or something.
struct RegisterAccess {
    u64 address; // address where the load or writeback happened
    bool valid;  // true if loaded and potentially modified, false if written back to memory and allocated register holds garbage
};

struct BlockMetadata {
    void* address{};
    void* address_end{};
    std::vector<u64> pending_links{};
    std::vector<std::pair<u64, u64>> instruction_spans{}; // {guest, host}
    std::array<std::vector<RegisterAccess>, allocated_reg_count> register_accesses;
};

struct Recompiler {
    Recompiler();
    ~Recompiler();
    Recompiler(const Recompiler&) = delete;
    Recompiler& operator=(const Recompiler&) = delete;
    Recompiler(Recompiler&&) = delete;
    Recompiler& operator=(Recompiler&&) = delete;

    void* compile(u64 rip);

    inline Assembler& getAssembler() {
        return as;
    }

    biscuit::GPR scratch();

    biscuit::Vec scratchVec();

    void popScratch();

    void popScratchVec();

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

    void stopCompiling();

    void setExitReason(ExitReason reason);

    void writebackDirtyState();

    void backToDispatcher();

    void enterDispatcher(ThreadState* state);

    void exitDispatcher(ThreadState* state);

    void* getCompileNext();

    void disableSignals();

    void enableSignals();

    bool shouldEmitFlag(u64 current_rip, x86_ref_e ref);

    void zext(biscuit::GPR dest, biscuit::GPR src, x86_size_e size);

    u64 sextImmediate(u64 imm, ZyanU8 size);

    void addi(biscuit::GPR dest, biscuit::GPR src, u64 imm);

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

    void jumpAndLinkConditional(biscuit::GPR condition, biscuit::GPR gpr_true, biscuit::GPR gpr_false, u64 rip_true, u64 rip_false);

    constexpr static biscuit::GPR threadStatePointer() {
        return x27; // saved register so that when we exit VM we don't have to save it
    }

    void setFlagUndefined(x86_ref_e ref);

    x86_ref_e zydisToRef(ZydisRegister reg);

    x86_size_e zydisToSize(ZydisRegister reg);

    x86_size_e zydisToSize(ZyanU8 size);

    // Get the allocated register for the given register reference
    static biscuit::GPR allocatedGPR(x86_ref_e reg);

    static biscuit::Vec allocatedVec(x86_ref_e reg);

    bool setVectorState(SEW sew, int elem_count, LMUL grouping = LMUL::M1);

    u16 maxVlen() {
        return max_vlen;
    }

    void sextb(biscuit::GPR dest, biscuit::GPR src);

    void sexth(biscuit::GPR dest, biscuit::GPR src);

    void sext(biscuit::GPR dest, biscuit::GPR src, x86_size_e size);

    biscuit::GPR getCond(int cond);

    void readMemory(biscuit::GPR dest, biscuit::GPR address, i64 offset, x86_size_e size);

    void writeMemory(biscuit::GPR src, biscuit::GPR address, i64 offset, x86_size_e size);

    void repPrologue(Label* loop_end);

    void repEpilogue(Label* loop_body);

    void repzEpilogue(Label* loop_body, bool is_repz);

    bool isGPR(ZydisRegister reg);

    BlockMetadata& getBlockMetadata(u64 rip);

    void vrgather(biscuit::Vec dst, biscuit::Vec src, biscuit::Vec iota, VecMask mask = VecMask::No);

    bool blockExists(u64 rip);

    biscuit::GPR getFlags();

    u64 getImmediate(ZydisDecodedOperand* operand);

    void* emitSigreturnThunk();

    auto& getBlockMap() {
        return block_metadata;
    }

    void* getCompiledBlock(u64 rip);

    void pushCalltrace();

    void popCalltrace();

    void tryFastReturn(biscuit::GPR rip);

    void readBitstring(biscuit::GPR dest, ZydisDecodedOperand* operand, biscuit::GPR shift);

private:
    struct RegisterMetadata {
        x86_ref_e reg;
        bool dirty = false;  // whether an instruction modified this value, so we know to store it to memory before exiting execution
        bool loaded = false; // whether a previous instruction loaded this value from memory, so we don't load it again
                             // if a syscall happens for example, this would be set to false so we load it again
    };

    struct FlagAccess {
        bool modification; // true if modified, false if used
        u64 position;
    };

    void compileSequence(u64 rip);

    // Get the register and load the value into it if needed
    biscuit::GPR gpr(ZydisRegister reg);

    biscuit::Vec vec(ZydisRegister reg);

    void resetScratch();

    void emitDispatcher();

    void loadGPR(x86_ref_e reg, biscuit::GPR gpr);

    void loadVec(x86_ref_e reg, biscuit::Vec vec);

    RegisterMetadata& getMetadata(x86_ref_e reg);

    void scanFlagUsageAhead(u64 rip);

    ZydisMnemonic decode(u64 rip, ZydisDecodedInstruction& instruction, ZydisDecodedOperand* operands);

    void expirePendingLinks(u64 rip);

    void addRegisterAccess(x86_ref_e ref, bool is_load);

    u8* code_cache{};
    biscuit::Assembler as{};
    ZydisDecoder decoder{};

    ZydisDecodedInstruction instruction{};
    ZydisDecodedOperand operands[10]{};

    void (*enter_dispatcher)(ThreadState*){};

    void (*exit_dispatcher)(ThreadState*){};

    void* compile_next_handler{};

    std::array<RegisterMetadata, 16 + 5 + 16> metadata{};

    std::unordered_map<u64, BlockMetadata> block_metadata{};

    bool compiling{};

    int scratch_index = 0;

    int vector_scratch_index = 0;

    std::array<std::vector<FlagAccess>, 6> flag_access_cpazso{};

    BlockMetadata* current_block_metadata{};
    HandlerMetadata* current_meta{};
    SEW current_sew = SEW::E1024;
    u8 current_vlen = 0;
    LMUL current_grouping = LMUL::M1;
    u16 max_vlen = 128;
};