#pragma once

#include <functional>
#include <span>
#include <string>
#include <variant>
#include <vector>
#include "felix86/common/log.hpp"
#include "felix86/common/riscv.hpp"
#include "felix86/common/utility.hpp"
#include "felix86/frontend/instruction.hpp"
#include "felix86/ir/opcode.hpp"

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
    std::array<IRInstruction*, 4> operands;
    u64 immediate_data = 0; // for some sse instructions
    u8 operand_count = 0;
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
    GetGuest,
    SetGuest,
    Phi,
    Comment,
    PushHost,
    PopHost,

    Count,
};

using Expression = std::variant<Operands, GetGuest, SetGuest, Phi, Comment, PushHost, PopHost>;
static_assert(std::variant_size_v<Expression> == (u8)ExpressionType::Count);
static_assert(std::is_same_v<Operands, std::variant_alternative_t<(u8)ExpressionType::Operands, Expression>>);
static_assert(std::is_same_v<GetGuest, std::variant_alternative_t<(u8)ExpressionType::GetGuest, Expression>>);
static_assert(std::is_same_v<SetGuest, std::variant_alternative_t<(u8)ExpressionType::SetGuest, Expression>>);
static_assert(std::is_same_v<Phi, std::variant_alternative_t<(u8)ExpressionType::Phi, Expression>>);
static_assert(std::is_same_v<Comment, std::variant_alternative_t<(u8)ExpressionType::Comment, Expression>>);
static_assert(std::is_same_v<PushHost, std::variant_alternative_t<(u8)ExpressionType::PushHost, Expression>>);
static_assert(std::is_same_v<PopHost, std::variant_alternative_t<(u8)ExpressionType::PopHost, Expression>>);

// Reduced instruction, after the SSA destruction it gets transformed to this form
// for easier register allocation and so that instruction operands are names and not
// pointers since we remove phis
struct RIRInstruction {
    RIRInstruction() = default;
    RIRInstruction(const RIRInstruction& other) = delete;
    RIRInstruction& operator=(const RIRInstruction& other) = delete;
    RIRInstruction(RIRInstruction&& other) = default;
    RIRInstruction& operator=(RIRInstruction&& other) = default;

    std::array<u32, 4> operands;
    u64 immediate_data;
    u32 name;
    IROpcode opcode;
    u8 operand_count;
};

struct IRInstruction {
    IRInstruction(IROpcode opcode, std::initializer_list<IRInstruction*> operands)
        : opcode(opcode), return_type{IRInstruction::getTypeFromOpcode(opcode)} {
        Operands op;
        for (size_t i = 0; i < operands.size(); i++) {
            if (i >= op.operands.size()) {
                ERROR("Too many operands");
            }

            IRInstruction* inst = *(operands.begin() + i);
            op.operands[i] = inst;
        }
        expression = op;

        for (auto& operand : operands) {
            operand->AddUse();
        }

        checkValidity(opcode, op);
        expression_type = ExpressionType::Operands;
    }

    IRInstruction(u64 immediate) : opcode(IROpcode::Immediate), return_type{IRType::Integer64} {
        Operands op;
        op.immediate_data = immediate;
        op.operand_count = 0;
        expression = op;

        expression_type = ExpressionType::Operands;
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

    IRInstruction(const std::string& comment)
        : opcode(IROpcode::Comment), return_type{IRInstruction::getTypeFromOpcode(opcode)} {
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
        return opcode == IROpcode::Immediate;
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

    void SetName(u32 name) {
        this->name = name;
    }

    u32 GetName() const {
        return name;
    }

    std::string GetNameString() const;

    std::string GetTypeString() const;

    const IRInstruction* GetOperand(u8 index) const {
        return AsOperands().operands[index];
    }

    IRInstruction* GetOperand(u8 index) {
        return AsOperands().operands[index];
    }

    std::string GetOperandNameString(u8 index) const {
        return AsOperands().operands[index]->GetNameString();
    }

    std::span<IRInstruction*> GetUsedInstructions();

    void ReplaceExpressionWithMov(IRInstruction* mov) {
        Invalidate();
        Operands op;
        op.operands[0] = mov;
        op.operand_count = 1;

        Expression swap = {op};
        expression.swap(swap);
        expression_type = ExpressionType::Operands;
        opcode = IROpcode::Mov;
        return_type = mov->return_type;

        mov->AddUse();
    }

    u64 GetImmediateData() const {
        return AsOperands().immediate_data;
    }

    void SetImmediateData(u64 immediate_data) {
        AsOperands().immediate_data = immediate_data;
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

    bool IsCallerSaved() const;

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

    bool IsVoid() const;

    bool ExitsVM() const;

    void PropagateMovs();

    RIRInstruction AsReducedInstruction() const {
        if (!IsOperands()) {
            ERROR("Tried to reduce non-operands instruction");
        }

        RIRInstruction rir_inst;
        rir_inst.name = GetName();
        rir_inst.opcode = GetOpcode();
        rir_inst.operand_count = AsOperands().operand_count;
        rir_inst.immediate_data = GetImmediateData();
        for (u8 i = 0; i < rir_inst.operand_count; i++) {
            rir_inst.operands[i] = GetOperand(i)->GetName();
        }
        return rir_inst;
    }

private:
    static IRType getTypeFromOpcode(IROpcode opcode, x86_ref_e ref = X86_REF_COUNT);
    static void checkValidity(IROpcode opcode, const Operands& operands);

    Expression expression;
    u32 name = 0;
    u16 uses = 0;
    ExpressionType expression_type;
    IROpcode opcode;
    IRType return_type;
    bool locked = false; // must not be removed by optimizations, even when used by nothing
};
