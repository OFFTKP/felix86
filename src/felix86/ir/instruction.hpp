#pragma once

#include <functional>
#include <list>
#include <span>
#include <string>
#include <variant>
#include <vector>
#include "felix86/backend/registers.hpp"
#include "felix86/common/log.hpp"
#include "felix86/common/riscv.hpp"
#include "felix86/common/utility.hpp"
#include "felix86/frontend/instruction.hpp"

#define IR_OPCODES                                                                                                                                   \
    X(Null)                                                                                                                                          \
                                                                                                                                                     \
    X(Phi)                                                                                                                                           \
    X(Comment)                                                                                                                                       \
                                                                                                                                                     \
    X(Mov)                                                                                                                                           \
    X(Immediate)                                                                                                                                     \
    X(Popcount)                                                                                                                                      \
    X(Sext8)                                                                                                                                         \
    X(Sext16)                                                                                                                                        \
    X(Sext32)                                                                                                                                        \
    X(Syscall)                                                                                                                                       \
    X(Cpuid)                                                                                                                                         \
    X(Rdtsc)                                                                                                                                         \
    X(GetGuest)            /* placeholder instruction that indicates a use of a register, replaced by the ssa pass */                                \
    X(SetGuest)            /* placeholder instruction that indicates a def of a register, replaced by the ssa pass */                                \
    X(LoadGuestFromMemory) /* to load or store to the thread_state struct which contains x86 register info */                                        \
    X(StoreGuestToMemory)                                                                                                                            \
    X(PushHost) /* to load or store to the vm_state struct which contains risc-v reg info. for when we need to exit vm */                            \
    X(PopHost)  /* and not screw up our allocated registers */                                                                                       \
    X(Add)                                                                                                                                           \
    X(Sub)                                                                                                                                           \
    X(Divu)                                                                                                                                          \
    X(Div)                                                                                                                                           \
    X(Remu)                                                                                                                                          \
    X(Rem)                                                                                                                                           \
    X(Divuw)                                                                                                                                         \
    X(Divw)                                                                                                                                          \
    X(Remuw)                                                                                                                                         \
    X(Remw)                                                                                                                                          \
    X(Div128)                                                                                                                                        \
    X(Divu128)                                                                                                                                       \
    X(Mul)                                                                                                                                           \
    X(Mulh)                                                                                                                                          \
    X(Mulhu)                                                                                                                                         \
    X(Clz)                                                                                                                                           \
    X(Ctz)                                                                                                                                           \
    X(ShiftLeft)                                                                                                                                     \
    X(ShiftRight)                                                                                                                                    \
    X(ShiftRightArithmetic)                                                                                                                          \
    X(LeftRotate8)                                                                                                                                   \
    X(LeftRotate16)                                                                                                                                  \
    X(LeftRotate32)                                                                                                                                  \
    X(LeftRotate64)                                                                                                                                  \
    X(Select)                                                                                                                                        \
    X(And)                                                                                                                                           \
    X(Or)                                                                                                                                            \
    X(Xor)                                                                                                                                           \
    X(Not)                                                                                                                                           \
    X(Equal)                                                                                                                                         \
    X(NotEqual)                                                                                                                                      \
    X(IGreaterThan)                                                                                                                                  \
    X(ILessThan)                                                                                                                                     \
    X(UGreaterThan)                                                                                                                                  \
    X(ULessThan)                                                                                                                                     \
    X(ReadByte)                                                                                                                                      \
    X(ReadWord)                                                                                                                                      \
    X(ReadDWord)                                                                                                                                     \
    X(ReadQWord)                                                                                                                                     \
    X(ReadXmmWord)                                                                                                                                   \
    X(WriteByte)                                                                                                                                     \
    X(WriteWord)                                                                                                                                     \
    X(WriteDWord)                                                                                                                                    \
    X(WriteQWord)                                                                                                                                    \
    X(WriteXmmWord)                                                                                                                                  \
    X(CastIntegerToVector)                                                                                                                           \
    X(CastVectorToInteger)                                                                                                                           \
    X(VInsertInteger)                                                                                                                                \
    X(VExtractInteger)                                                                                                                               \
    X(VUnpackByteLow)                                                                                                                                \
    X(VUnpackWordLow)                                                                                                                                \
    X(VUnpackDWordLow)                                                                                                                               \
    X(VUnpackQWordLow)                                                                                                                               \
    X(VAnd)                                                                                                                                          \
    X(VOr)                                                                                                                                           \
    X(VXor)                                                                                                                                          \
    X(VShr)                                                                                                                                          \
    X(VShl)                                                                                                                                          \
    X(VPackedSubByte)                                                                                                                                \
    X(VPackedAddQWord)                                                                                                                               \
    X(VPackedEqualByte)                                                                                                                              \
    X(VPackedEqualWord)                                                                                                                              \
    X(VPackedEqualDWord)                                                                                                                             \
    X(VPackedShuffleDWord)                                                                                                                           \
    X(VMoveByteMask)                                                                                                                                 \
    X(VPackedMinByte)                                                                                                                                \
    X(VZext64) /* zero extend the bottom 64-bits of a vector */                                                                                      \
                                                                                                                                                     \
    X(Count)

enum class IROpcode : u8 {
#define X(stuff) stuff,
    IR_OPCODES
#undef X
};

enum class IRType : u8 {
    Void,
    Integer64,
    Vector128,
    Float64,
    Float80, // :(

    Count,
};

struct IRInstruction;
struct IRBlock;

struct Operands {
    std::vector<IRInstruction*> operands = {};
    u64 extra_data = 0; // for some sse instructions
};

struct Immediate {
    u64 immediate = 0;
};

struct GetGuest {
    x86_ref_e ref = X86_REF_COUNT;
};

struct SetGuest {
    x86_ref_e ref = X86_REF_COUNT;
    IRInstruction* source = nullptr;
};

struct Phi {
    Phi() = default;
    Phi(const Phi& other) = delete;
    Phi(Phi&& other) = default;
    Phi& operator=(const Phi& other) = delete;
    Phi& operator=(Phi&& other) = default;

    x86_ref_e ref = X86_REF_COUNT;
    std::vector<IRBlock*> blocks = {};
    std::vector<IRInstruction*> values = {};
};

struct Comment {
    std::string comment = {};
};

struct PushHost {
    riscv_ref_e ref = RISCV_REF_COUNT;
};

struct PopHost {
    riscv_ref_e ref = RISCV_REF_COUNT;
};

enum class ExpressionType : u8 {
    Operands,
    Immediate,
    GetGuest,
    SetGuest,
    Phi,
    Comment,
    PushHost,
    PopHost,

    Count,
};

enum class AllocationType : u8 {
    Null,
    GPR,
    FPR,
    Vec,
    Spill,
};

// Don't change their order and make sure to properly update stuff if you add to the end
using Allocation = std::variant<std::monostate, biscuit::GPR, biscuit::FPR, biscuit::Vec, u32>;
static_assert(std::variant_size_v<Allocation> == 5);
static_assert(std::is_same_v<std::monostate, std::variant_alternative_t<(u8)AllocationType::Null, Allocation>>);
static_assert(std::is_same_v<biscuit::GPR, std::variant_alternative_t<(u8)AllocationType::GPR, Allocation>>);
static_assert(std::is_same_v<biscuit::FPR, std::variant_alternative_t<(u8)AllocationType::FPR, Allocation>>);
static_assert(std::is_same_v<biscuit::Vec, std::variant_alternative_t<(u8)AllocationType::Vec, Allocation>>);
static_assert(std::is_same_v<u32, std::variant_alternative_t<(u8)AllocationType::Spill, Allocation>>);

using Expression = std::variant<Operands, Immediate, GetGuest, SetGuest, Phi, Comment, PushHost, PopHost>;
static_assert(std::variant_size_v<Expression> == (u8)ExpressionType::Count);
static_assert(std::is_same_v<Operands, std::variant_alternative_t<(u8)ExpressionType::Operands, Expression>>);
static_assert(std::is_same_v<Immediate, std::variant_alternative_t<(u8)ExpressionType::Immediate, Expression>>);
static_assert(std::is_same_v<GetGuest, std::variant_alternative_t<(u8)ExpressionType::GetGuest, Expression>>);
static_assert(std::is_same_v<SetGuest, std::variant_alternative_t<(u8)ExpressionType::SetGuest, Expression>>);
static_assert(std::is_same_v<Phi, std::variant_alternative_t<(u8)ExpressionType::Phi, Expression>>);
static_assert(std::is_same_v<Comment, std::variant_alternative_t<(u8)ExpressionType::Comment, Expression>>);
static_assert(std::is_same_v<PushHost, std::variant_alternative_t<(u8)ExpressionType::PushHost, Expression>>);
static_assert(std::is_same_v<PopHost, std::variant_alternative_t<(u8)ExpressionType::PopHost, Expression>>);

struct IRInstruction {
    IRInstruction(IROpcode opcode, std::initializer_list<IRInstruction*> operands)
        : opcode(opcode), return_type{IRInstruction::getTypeFromOpcode(opcode)} {
        Operands op;
        op.operands = operands;
        expression = op;

        for (auto& operand : operands) {
            operand->AddUse();
        }

        checkValidity(opcode, op);
        expression_type = ExpressionType::Operands;
    }

    IRInstruction(u64 immediate) : opcode(IROpcode::Immediate), return_type{IRType::Integer64} {
        Immediate imm;
        imm.immediate = immediate;
        expression = imm;

        expression_type = ExpressionType::Immediate;

        // If it's zero we can just give it x0 which is hardwired to 0
        if (immediate == 0) {
            Allocate(Registers::Zero());
        }
    }

    IRInstruction(IROpcode opcode, x86_ref_e ref) : opcode(opcode), return_type{IRInstruction::getTypeFromOpcode(opcode, ref)} {
        GetGuest get;
        get.ref = ref;
        expression = get;

        expression_type = ExpressionType::GetGuest;
    }

    IRInstruction(IROpcode opcode, x86_ref_e ref, IRInstruction* source)
        : opcode(opcode), return_type{IRInstruction::getTypeFromOpcode(opcode, ref)} {
        SetGuest set;
        set.ref = ref;
        set.source = source;
        expression = set;

        source->AddUse();
        expression_type = ExpressionType::SetGuest;
    }

    IRInstruction(Phi phi) : opcode(IROpcode::Phi), return_type{IRInstruction::getTypeFromOpcode(opcode, phi.ref)} {
        expression = std::move(phi);

        for (auto& value : phi.values) {
            value->AddUse();
        }

        expression_type = ExpressionType::Phi;
    }

    IRInstruction(const std::string& comment) : opcode(IROpcode::Comment), return_type{IRInstruction::getTypeFromOpcode(opcode)} {
        Comment c;
        c.comment = comment;
        expression = c;

        expression_type = ExpressionType::Comment;

        Lock();
    }

    IRInstruction(riscv_ref_e ref, bool push) {
        if (push) {
            opcode = IROpcode::PushHost;
        } else {
            opcode = IROpcode::PopHost;
        }

        if (push) {
            expression = PushHost{ref};
        } else {
            expression = PopHost{ref};
        }
        expression_type = push ? ExpressionType::PushHost : ExpressionType::PopHost;
        return_type = IRType::Void;
    }

    IRInstruction(const IRInstruction& other) = delete;
    IRInstruction& operator=(const IRInstruction& other) = delete;
    IRInstruction(IRInstruction&& other) = default;
    IRInstruction& operator=(IRInstruction&& other) = default;

    bool IsSameExpression(const IRInstruction& other) const;

    IRType GetType() const {
        return return_type;
    }

    IROpcode GetOpcode() const {
        return opcode;
    }

    u32 GetUseCount() const {
        return uses;
    }

    void AddUse() {
        uses++;
    }

    void RemoveUse() {
        uses--;
    }

    void Invalidate();

    const Operands& AsOperands() const {
        return std::get<Operands>(expression);
    }

    const GetGuest& AsGetGuest() const {
        return std::get<GetGuest>(expression);
    }

    const SetGuest& AsSetGuest() const {
        return std::get<SetGuest>(expression);
    }

    const Immediate& AsImmediate() const {
        return std::get<Immediate>(expression);
    }

    const Phi& AsPhi() const {
        return std::get<Phi>(expression);
    }

    const PushHost& AsPushHost() const {
        return std::get<PushHost>(expression);
    }

    const PopHost& AsPopHost() const {
        return std::get<PopHost>(expression);
    }

    const Comment& AsComment() const {
        return std::get<Comment>(expression);
    }

    Operands& AsOperands() {
        return std::get<Operands>(expression);
    }

    GetGuest& AsGetGuest() {
        return std::get<GetGuest>(expression);
    }

    SetGuest& AsSetGuest() {
        return std::get<SetGuest>(expression);
    }

    Immediate& AsImmediate() {
        return std::get<Immediate>(expression);
    }

    Phi& AsPhi() {
        return std::get<Phi>(expression);
    }

    Comment& AsComment() {
        return std::get<Comment>(expression);
    }

    ExpressionType GetExpressionType() const {
        return expression_type;
    }

    bool IsOperands() const {
        return expression_type == ExpressionType::Operands;
    }

    bool IsImmediate() const {
        return expression_type == ExpressionType::Immediate;
    }

    bool IsGetGuest() const {
        return expression_type == ExpressionType::GetGuest;
    }

    bool IsSetGuest() const {
        return expression_type == ExpressionType::SetGuest;
    }

    bool IsPhi() const {
        return expression_type == ExpressionType::Phi;
    }

    bool IsComment() const {
        return expression_type == ExpressionType::Comment;
    }

    u32 GetName() const {
        return name;
    }

    void SetName(u32 name) {
        this->name = name;
    }

    std::string GetNameString() const;

    std::string GetTypeString() const;

    const IRInstruction* GetOperand(u8 index) const {
        return AsOperands().operands[index];
    }

    IRInstruction* GetOperand(u8 index) {
        return AsOperands().operands[index];
    }

    u32 GetOperandName(u8 index) const {
        return AsOperands().operands[index]->GetName();
    }

    std::string GetOperandNameString(u8 index) const {
        return AsOperands().operands[index]->GetNameString();
    }

    std::span<IRInstruction*> GetUsedInstructions();

    void ReplaceExpressionWithMov(IRInstruction* mov) {
        Invalidate();
        Operands op;
        op.operands.push_back(mov);

        Expression swap = {op};
        expression.swap(swap);
        expression_type = ExpressionType::Operands;
        opcode = IROpcode::Mov;
        return_type = mov->return_type;

        mov->AddUse();
    }

    u64 GetExtraData() const {
        return AsOperands().extra_data;
    }

    void SetExtraData(u64 extra_data) {
        AsOperands().extra_data = extra_data;
    }

    std::string Print(const std::function<std::string(const IRInstruction*)>& callback) const;

    void Unlock() {
        locked = false;
    }

    void Lock() {
        locked = true;
    }

    bool IsLocked() const {
        return locked;
    }

    AllocationType GetAllocationType() const {
        return (AllocationType)allocation.index();
    }

    bool IsSpilled() const {
        if (GetAllocationType() == AllocationType::Null) {
            ERROR("Uninitialized allocation");
        }

        if (GetAllocationType() == AllocationType::Spill) {
            return true;
        }

        return false;
    }

    bool IsCallerSaved() const;

    Allocation& GetAllocation() {
        return allocation;
    }

    bool IsGPR() const {
        if (return_type == IRType::Integer64) {
            return true;
        }

        return false;
    }

    bool IsFPR() const {
        if (return_type == IRType::Float64) {
            return true;
        }

        return false;
    }

    bool IsVec() const {
        if (return_type == IRType::Vector128) {
            return true;
        }

        return false;
    }

    biscuit::GPR GetGPR() const {
        return std::get<biscuit::GPR>(allocation);
    }

    biscuit::FPR GetFPR() const {
        return std::get<biscuit::FPR>(allocation);
    }

    biscuit::Vec GetVec() const {
        return std::get<biscuit::Vec>(allocation);
    }

    u32 GetSpillLocation() const {
        return std::get<u32>(allocation);
    }

    bool IsVoid() const;

    bool NeedsAllocation() const;

    bool ExitsVM() const;

    void Allocate(Allocation&& alloc) {
        allocation = std::move(alloc);
    }

private:
    static IRType getTypeFromOpcode(IROpcode opcode, x86_ref_e ref = X86_REF_COUNT);
    static void checkValidity(IROpcode opcode, const Operands& operands);

    Expression expression;
    u32 name = 0; // TODO: merge with allocated name?
    Allocation allocation;
    u16 uses = 0;
    ExpressionType expression_type;
    IROpcode opcode;
    IRType return_type;
    bool locked = false; // must not be removed by optimizations, even when used by nothing
};
