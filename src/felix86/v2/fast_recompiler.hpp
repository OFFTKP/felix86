#pragma once

#include <array>
#include <unordered_map>
#include <Zydis/Utils.h>
#include "Zydis/Decoder.h"
#include "biscuit/registers.hpp"
#include "felix86/common/riscv.hpp"
#include "felix86/common/utility.hpp"
#include "felix86/common/x86.hpp"

struct HandlerMetadata {
    u64 rip;
    u64 block_start;
};

struct BlockMetadata {
    void* address = nullptr;
    std::vector<u64> pending_links{};
};

struct VectorMemoryAccess {
    u64 rip;
    biscuit::Vec dest;
    biscuit::GPR address;
    u16 len;
    SEW sew;
    bool load;
};

struct FastRecompiler {
    FastRecompiler(Emulator& emulator);
    ~FastRecompiler();
    FastRecompiler(const FastRecompiler&) = delete;
    FastRecompiler& operator=(const FastRecompiler&) = delete;
    FastRecompiler(FastRecompiler&&) = delete;
    FastRecompiler& operator=(FastRecompiler&&) = delete;

    void* compile(u64 rip);

    inline Assembler& getAssembler() {
        return as;
    }

    biscuit::GPR scratch();

    biscuit::Vec scratchVec();

    void popScratch();

    void popScratchVec();

    biscuit::GPR getOperandGPR(ZydisDecodedOperand* operand);

    biscuit::GPR getOperandGPRDontZext(ZydisDecodedOperand* operand);

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

    void* getCompileNext();

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
    biscuit::GPR allocatedGPR(x86_ref_e reg);

    biscuit::Vec allocatedVec(x86_ref_e reg);

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

    void registerVLE(u64 rip, SEW sew, u16 len, biscuit::Vec dst, biscuit::GPR address);

    void registerVSE(u64 rip, SEW sew, u16 len, biscuit::Vec dst, biscuit::GPR address);

    VectorMemoryAccess getVectorMemoryAccess(u64 rip);

    void vrgather(biscuit::Vec dst, biscuit::Vec src, biscuit::Vec iota, VecMask mask = VecMask::No);

    bool blockExists(u64 rip);

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

    Emulator& emulator;

    u8* code_cache{};
    biscuit::Assembler as{};
    ZydisDecoder decoder{};

    ZydisDecodedInstruction instruction{};
    ZydisDecodedOperand operands[10]{};

    void (*enter_dispatcher)(ThreadState*){};

    void* compile_next_handler{};

    // 16 gprs, 6 flags, 16 xmm registers
    std::array<RegisterMetadata, 16 + 5 + 16> metadata{};

    std::unordered_map<u64, BlockMetadata> block_metadata{};

    bool compiling{};

    int scratch_index = 0;

    int vector_scratch_index = 0;

    std::array<std::vector<FlagAccess>, 6> flag_access_cpazso{};

    std::unordered_map<u64, VectorMemoryAccess> vector_memory_access{};

    HandlerMetadata* current_meta{};
    SEW current_sew = SEW::E1024;
    u8 current_vlen = 0;
    LMUL current_grouping = LMUL::M1;
    u16 max_vlen = 128;
};