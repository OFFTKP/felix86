#pragma once

#include <string>
#include "felix86/common/utility.hpp"

#define IR_OPCODES                                                                                                                                   \
    X(Null)                                                                                                                                          \
    X(GetThreadStatePointer)                                                                                                                         \
    X(SetExitReason)                                                                                                                                 \
    X(Phi)                                                                                                                                           \
    X(Comment)                                                                                                                                       \
    X(Mov)                                                                                                                                           \
    X(Immediate)                                                                                                                                     \
    X(Parity)                                                                                                                                        \
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
    X(Ctzh)                                                                                                                                          \
    X(Ctzw)                                                                                                                                          \
    X(Ctz)                                                                                                                                           \
    X(ShiftLeft)                                                                                                                                     \
    X(ShiftRight)                                                                                                                                    \
    X(ShiftRightArithmetic)                                                                                                                          \
    X(LeftRotate8)                                                                                                                                   \
    X(LeftRotate16)                                                                                                                                  \
    X(LeftRotate32)                                                                                                                                  \
    X(LeftRotate64)                                                                                                                                  \
    X(Select)                                                                                                                                        \
    X(Addi)                                                                                                                                          \
    X(And)                                                                                                                                           \
    X(Or)                                                                                                                                            \
    X(Xor)                                                                                                                                           \
    X(Not)                                                                                                                                           \
    X(Neg)                                                                                                                                           \
    X(Equal)                                                                                                                                         \
    X(NotEqual)                                                                                                                                      \
    X(AmoAdd8)                                                                                                                                       \
    X(AmoAdd16)                                                                                                                                      \
    X(AmoAdd32)                                                                                                                                      \
    X(AmoAdd64)                                                                                                                                      \
    X(AmoAnd8)                                                                                                                                       \
    X(AmoAnd16)                                                                                                                                      \
    X(AmoAnd32)                                                                                                                                      \
    X(AmoAnd64)                                                                                                                                      \
    X(AmoOr8)                                                                                                                                        \
    X(AmoOr16)                                                                                                                                       \
    X(AmoOr32)                                                                                                                                       \
    X(AmoOr64)                                                                                                                                       \
    X(AmoXor8)                                                                                                                                       \
    X(AmoXor16)                                                                                                                                      \
    X(AmoXor32)                                                                                                                                      \
    X(AmoXor64)                                                                                                                                      \
    X(AmoSwap8)                                                                                                                                      \
    X(AmoSwap16)                                                                                                                                     \
    X(AmoSwap32)                                                                                                                                     \
    X(AmoSwap64)                                                                                                                                     \
    X(AmoCAS8)                                                                                                                                       \
    X(AmoCAS16)                                                                                                                                      \
    X(AmoCAS32)                                                                                                                                      \
    X(AmoCAS64)                                                                                                                                      \
    X(AmoCAS128)                                                                                                                                     \
    X(SetLessThanSigned)                                                                                                                             \
    X(SetLessThanUnsigned)                                                                                                                           \
    X(ReadByte)                                                                                                                                      \
    X(ReadWord)                                                                                                                                      \
    X(ReadDWord)                                                                                                                                     \
    X(ReadQWord)                                                                                                                                     \
    X(ReadXmmWord)                                                                                                                                   \
    X(WriteByte)                                                                                                                                     \
    X(WriteWord)                                                                                                                                     \
    X(WriteDWord)                                                                                                                                    \
    X(WriteQWord)                                                                                                                                    \
    X(ReadByteRelative)                                                                                                                              \
    X(ReadQWordRelative)                                                                                                                             \
    X(ReadXmmWordRelative)                                                                                                                           \
    X(WriteByteRelative)                                                                                                                             \
    X(WriteQWordRelative)                                                                                                                            \
    X(WriteXmmWordRelative)                                                                                                                          \
    X(WriteXmmWord)                                                                                                                                  \
    X(CastVectorFromInteger)                                                                                                                         \
    X(CastIntegerFromVector)                                                                                                                         \
    X(VInsertInteger)                                                                                                                                \
    X(VExtractInteger)                                                                                                                               \
    X(VUnpackByteLow)                                                                                                                                \
    X(VUnpackWordLow)                                                                                                                                \
    X(VUnpackDWordLow)                                                                                                                               \
    X(VUnpackQWordLow)                                                                                                                               \
    X(VAnd)                                                                                                                                          \
    X(VOr)                                                                                                                                           \
    X(VXor)                                                                                                                                          \
    X(VShiftRight)                                                                                                                                   \
    X(VShiftLeft)                                                                                                                                    \
    X(VPackedSubByte)                                                                                                                                \
    X(VPackedAddQWord)                                                                                                                               \
    X(VPackedEqualByte)                                                                                                                              \
    X(VPackedEqualWord)                                                                                                                              \
    X(VPackedEqualDWord)                                                                                                                             \
    X(VPackedShuffleDWord)                                                                                                                           \
    X(VMoveByteMask)                                                                                                                                 \
    X(VPackedMinByte)                                                                                                                                \
    X(VZext64) /* zero extend the bottom 64-bits of a vector */                                                                                      \
    X(Count)

enum class IROpcode : u8 {
#define X(stuff) stuff,
    IR_OPCODES
#undef X
};

namespace Opcode {
std::string GetOpcodeString(IROpcode opcode);
bool IsAuxiliary(IROpcode opcode);
} // namespace Opcode