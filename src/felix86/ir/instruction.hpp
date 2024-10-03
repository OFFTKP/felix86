#pragma once

#include <string>
#include <variant>
#include <vector>
#include "felix86/common/log.hpp"
#include "felix86/common/utility.hpp"
#include "felix86/frontend/instruction.hpp"

enum class IROpcode : u8 {
    Null,

    Phi,
    Comment,
    TupleExtract,

    Mov,
    Immediate,
    Popcount,
    Sext8,
    Sext16,
    Sext32,
    Syscall,
    Cpuid,
    Rdtsc,

    GetGuest, // placeholder instruction that indicates a use of a register, replaced by the ssa pass
    SetGuest, // placeholder instruction that indicates a def of a register, replaced by the ssa pass
    LoadGuestFromMemory,
    StoreGuestToMemory,

    Add,
    Sub,
    IMul64,
    IDiv8,
    IDiv16,
    IDiv32,
    IDiv64,
    UDiv8,
    UDiv16,
    UDiv32,
    UDiv64,
    Clz,
    Ctz,
    ShiftLeft,
    ShiftRight,
    ShiftRightArithmetic,
    LeftRotate8,
    LeftRotate16,
    LeftRotate32,
    LeftRotate64,
    Select,
    And,
    Or,
    Xor,
    Not,
    Lea,
    Equal,
    NotEqual,
    IGreaterThan,
    ILessThan,
    UGreaterThan,
    ULessThan,

    ReadByte,
    ReadWord,
    ReadDWord,
    ReadQWord,
    ReadXmmWord,
    WriteByte,
    WriteWord,
    WriteDWord,
    WriteQWord,
    WriteXmmWord,

    CastIntegerToVector,
    CastVectorToInteger,

    VInsertInteger,
    VExtractInteger,
    VUnpackByteLow,
    VUnpackWordLow,
    VUnpackDWordLow,
    VUnpackQWordLow,
    VAnd,
    VOr,
    VXor,
    VShr,
    VShl,
    VPackedSubByte,
    VPackedAddQWord,
    VPackedEqualByte,
    VPackedEqualWord,
    VPackedEqualDWord,
    VPackedShuffleDWord,
    VMoveByteMask,
    VPackedMinByte,
    VZext64, // zero extend the bottom 64-bits of a vector
};

enum class IRType : u8 {
    Void,
    Integer64,
    Vector128,
    Float80, // :(
    TupleTwoInteger64,
    TupleFourInteger64,

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

struct PhiNode {
    IRBlock* block = nullptr;
    IRInstruction* value = nullptr;
};

struct Phi {
    Phi() = default;
    Phi(const Phi& other) = delete;
    Phi(Phi&& other) = default;
    Phi& operator=(const Phi& other) = delete;
    Phi& operator=(Phi&& other) = default;

    x86_ref_e ref = X86_REF_COUNT;
    std::vector<PhiNode> nodes = {};
};

struct TupleAccess {
    IRInstruction* tuple = nullptr;
    u8 index = 0;
};

struct Comment {
    std::string comment = {};
};

using Expression = std::variant<Operands, Immediate, GetGuest, SetGuest, Phi, Comment, TupleAccess>;

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
    }

    IRInstruction(u64 immediate) : opcode(IROpcode::Immediate), return_type{IRType::Integer64} {
        Immediate imm;
        imm.immediate = immediate;
        expression = imm;
    }

    IRInstruction(IROpcode opcode, x86_ref_e ref) : opcode(opcode), return_type{IRInstruction::getTypeFromOpcode(opcode, ref)} {
        GetGuest get;
        get.ref = ref;
        expression = get;
    }

    IRInstruction(IROpcode opcode, x86_ref_e ref, IRInstruction* source)
        : opcode(opcode), return_type{IRInstruction::getTypeFromOpcode(opcode, ref)} {
        SetGuest set;
        set.ref = ref;
        set.source = source;
        expression = set;

        source->AddUse();
    }

    IRInstruction(Phi phi) : opcode(IROpcode::Phi), return_type{IRInstruction::getTypeFromOpcode(opcode, phi.ref)} {
        expression = std::move(phi);

        for (auto& node : phi.nodes) {
            node.value->AddUse();
        }
    }

    IRInstruction(const std::string& comment) : opcode(IROpcode::Comment), return_type{IRInstruction::getTypeFromOpcode(opcode)} {
        Comment c;
        c.comment = comment;
        expression = c;
    }

    IRInstruction(IRInstruction* tuple, u8 index)
        : opcode(IROpcode::TupleExtract), return_type(IRInstruction::getTypeFromTuple(tuple->return_type, index)) {
        TupleAccess tg;
        tg.tuple = tuple;
        tg.index = index;
        expression = tg;

        tuple->AddUse();
    }

    IRInstruction(IRInstruction* mov) : opcode(IROpcode::Mov), return_type{mov->return_type} {
        Operands op;
        op.operands.push_back(mov);
        expression = op;

        mov->AddUse();
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

    const Comment& AsComment() const {
        return std::get<Comment>(expression);
    }

    const TupleAccess& AsTupleAccess() const {
        return std::get<TupleAccess>(expression);
    }

    Operands& AsOperands() {
        return std::get<Operands>(expression);
    }

    Phi& AsPhi() {
        return std::get<Phi>(expression);
    }

    u32 GetName() const {
        return name;
    }

    void SetName(u32 name) {
        this->name = name;
    }

    std::string GetNameString() const;

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

    void ReplaceWith(IRInstruction&& other) {
        Invalidate();
        u16 uses = this->uses;
        *this = std::move(other);
        this->uses = uses;
    }

    u64 GetExtraData() const {
        return AsOperands().extra_data;
    }

    void SetExtraData(u64 extra_data) {
        AsOperands().extra_data = extra_data;
    }

    Expression& GetExpression() {
        return expression;
    }

    std::string Print() const;

    void Lock() {
        locked = true;
    }

private:
    static IRType getTypeFromOpcode(IROpcode opcode, x86_ref_e ref = X86_REF_COUNT);
    static IRType getTypeFromTuple(IRType type, u8 index);
    static void checkValidity(IROpcode opcode, const Operands& operands);

    Expression expression;
    u32 name = 0;
    u16 uses = 0;
    IROpcode opcode;
    IRType return_type;
    bool locked = false; // must not be removed by optimizations, even when used by nothing
};
