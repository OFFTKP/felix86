#pragma once

#include <array>
#include <string>
#include <variant>
#include <vector>
#include "felix86/common/utility.hpp"
#include "felix86/frontend/instruction.hpp"

enum class IROpcode : u8 { // TODO: match naming scheme of IRType
    IR_NULL,

    IR_START_OF_BLOCK,
    IR_PHI,
    IR_COMMENT,
    IR_TUPLE_GET,

    IR_MOV,
    IR_IMMEDIATE,
    IR_POPCOUNT,
    IR_SEXT8,
    IR_SEXT16,
    IR_SEXT32,
    IR_SYSCALL,
    IR_CPUID,
    IR_RDTSC,

    IR_GET_GUEST, // placeholder instruction that indicates a use of a register, replaced by the ssa
                  // pass
    IR_SET_GUEST, // placeholder instruction that indicates a def of a register, replaced by the ssa
                  // pass
    IR_LOAD_GUEST_FROM_MEMORY,
    IR_STORE_GUEST_TO_MEMORY,

    IR_ADD,
    IR_SUB,
    IR_IMUL64,
    IR_IDIV8,
    IR_IDIV16,
    IR_IDIV32,
    IR_IDIV64,
    IR_UDIV8,
    IR_UDIV16,
    IR_UDIV32,
    IR_UDIV64,
    IR_CLZ,
    IR_CTZ,
    IR_SHIFT_LEFT,
    IR_SHIFT_RIGHT,
    IR_SHIFT_RIGHT_ARITHMETIC,
    IR_LEFT_ROTATE8,
    IR_LEFT_ROTATE16,
    IR_LEFT_ROTATE32,
    IR_LEFT_ROTATE64,
    IR_SELECT,
    IR_AND,
    IR_OR,
    IR_XOR,
    IR_NOT,
    IR_LEA,
    IR_EQUAL,
    IR_NOT_EQUAL,
    IR_GREATER_THAN_SIGNED,
    IR_LESS_THAN_SIGNED,
    IR_GREATER_THAN_UNSIGNED,
    IR_LESS_THAN_UNSIGNED,

    IR_READ_BYTE,
    IR_READ_WORD,
    IR_READ_DWORD,
    IR_READ_QWORD,
    IR_READ_XMMWORD,
    IR_WRITE_BYTE,
    IR_WRITE_WORD,
    IR_WRITE_DWORD,
    IR_WRITE_QWORD,
    IR_WRITE_XMMWORD,

    IR_INSERT_INTEGER_TO_VECTOR,
    IR_EXTRACT_INTEGER_FROM_VECTOR,
    IR_VECTOR_FROM_INTEGER,
    IR_INTEGER_FROM_VECTOR,
    IR_VECTOR_UNPACK_BYTE_LOW,
    IR_VECTOR_UNPACK_WORD_LOW,
    IR_VECTOR_UNPACK_DWORD_LOW,
    IR_VECTOR_UNPACK_QWORD_LOW,
    IR_VECTOR_PACKED_AND,
    IR_VECTOR_PACKED_OR,
    IR_VECTOR_PACKED_XOR,
    IR_VECTOR_PACKED_SHIFT_RIGHT,
    IR_VECTOR_PACKED_SHIFT_LEFT,
    IR_VECTOR_PACKED_SUB_BYTE,
    IR_VECTOR_PACKED_ADD_QWORD,
    IR_VECTOR_PACKED_COMPARE_EQ_BYTE,
    IR_VECTOR_PACKED_COMPARE_EQ_WORD,
    IR_VECTOR_PACKED_COMPARE_EQ_DWORD,
    IR_VECTOR_PACKED_SHUFFLE_DWORD,
    IR_VECTOR_PACKED_MOVE_BYTE_MASK,
    IR_VECTOR_PACKED_MIN_BYTE,
    IR_VECTOR_PACKED_COMPARE_IMPLICIT_STRING_INDEX,
    IR_VECTOR_ZEXT64, // zero extend the bottom 64-bits of a vector
};

enum class IRType : u8 {
    Void,
    Integer64,
    Vector128,
    TupleTwoInteger64,
    TupleFourInteger64,
};

struct IRInstruction;
struct IRBlock;

struct Operands {
    std::vector<IRInstruction*> operands = {};
    u8 control_byte = 0; // for some sse instructions
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
    Phi(const Phi& other) = delete;
    Phi(Phi&& other) = default;
    Phi& operator=(const Phi& other) = delete;
    Phi& operator=(Phi&& other) = default;

    x86_ref_e ref = X86_REF_COUNT;
    std::vector<PhiNode> nodes = {};
};

struct TupleGet {
    IRInstruction* tuple = nullptr;
    u8 index = 0;
};

struct Comment {
    std::string comment = {};
};

using Expression = std::variant<Operands, Immediate, GetGuest, SetGuest, Phi, Comment, TupleGet>;

IRType GetTypeFromOpcode(IROpcode opcode);
IRType GetTypeFromTuple(IRType type, u8 index);
IRType GetTypeFromPhi(const Phi& phi);

struct IRInstruction {
    IRInstruction(IROpcode opcode, std::initializer_list<IRInstruction*> operands) : opcode(opcode), returnType{GetTypeFromOpcode(opcode)} {
        Operands op;
        op.operands = operands;
        expression = op;

        for (auto& operand : operands) {
            operand->uses++;
        }
    }

    IRInstruction(u64 immediate) : opcode(IROpcode::IR_IMMEDIATE), returnType{GetTypeFromOpcode(opcode)} {
        Immediate imm;
        imm.immediate = immediate;
        expression = imm;
    }

    IRInstruction(x86_ref_e ref) : opcode(IROpcode::IR_GET_GUEST), returnType{GetTypeFromOpcode(opcode)} {
        GetGuest get;
        get.ref = ref;
        expression = get;
    }

    IRInstruction(x86_ref_e ref, IRInstruction* source) : opcode(IROpcode::IR_SET_GUEST), returnType{GetTypeFromOpcode(opcode)} {
        SetGuest set;
        set.ref = ref;
        set.source = source;
        expression = set;

        source->uses++;
    }

    IRInstruction(Phi phi) : opcode(IROpcode::IR_PHI), returnType{GetTypeFromPhi(phi)} {
        expression = std::move(phi);

        for (auto& node : phi.nodes) {
            node.value->uses++;
        }
    }

    IRInstruction(const std::string& comment) : opcode(IROpcode::IR_COMMENT), returnType{GetTypeFromOpcode(opcode)} {
        Comment c;
        c.comment = comment;
        expression = c;
    }

    IRInstruction(IRInstruction* tuple, u8 index) : opcode(IROpcode::IR_TUPLE_GET), returnType(GetTypeFromTuple(tuple->returnType, index)) {
        TupleGet tg;
        tg.tuple = tuple;
        tg.index = index;
        expression = tg;

        tuple->uses++;
    }

    IRInstruction(IRInstruction* mov) : opcode(IROpcode::IR_MOV), returnType{mov->returnType} {
        Operands op;
        op.operands.push_back(mov);
        expression = op;

        mov->uses++;
    }

    IRInstruction(const IRInstruction& other) = delete;
    IRInstruction& operator=(const IRInstruction& other) = delete;
    IRInstruction(IRInstruction&& other) = default;
    IRInstruction& operator=(IRInstruction&& other) = default;

    bool IsSameExpression(const IRInstruction& other) const;
    IRType GetType() const { return returnType; }
    void UndoUses();

private:
    Expression expression;
    u16 name = 0;
    u16 uses = 0;
    IROpcode opcode;
    IRType returnType;
};
