#pragma once

#include <string>
#include "felix86/common/utility.hpp"

#define IR_OPCODES                                                                                                                                   \
    X(Null)                                                                                                                                          \
    X(GetThreadStatePointer)                                                                                                                         \
    X(SetVectorStateFloat) /* these set the state of vector operations, to operate on float/doubles or packed data */                                \
    X(SetVectorStateDouble)                                                                                                                          \
    X(SetVectorStatePackedByte) /* we do an optimization pass to remove unnecessary SetVectorState* (really we just unlock them and they get removed \
                                   by dce) */                                                                                                        \
    X(SetVectorStatePackedWord)                                                                                                                      \
    X(SetVectorStatePackedDWord)                                                                                                                     \
    X(SetVectorStatePackedQWord)                                                                                                                     \
    X(SetExitReason)                                                                                                                                 \
    X(CallHostFunction)                                                                                                                              \
    X(Phi)                                                                                                                                           \
    X(Comment)                                                                                                                                       \
    X(Mov)                                                                                                                                           \
    X(Immediate)                                                                                                                                     \
    X(Parity)                                                                                                                                        \
    X(Sext8)                                                                                                                                         \
    X(Sext16)                                                                                                                                        \
    X(Sext32)                                                                                                                                        \
    X(Zext8)                                                                                                                                         \
    X(Zext16)                                                                                                                                        \
    X(Zext32)                                                                                                                                        \
    X(Syscall)                                                                                                                                       \
    X(Cpuid)                                                                                                                                         \
    X(Rdtsc)                                                                                                                                         \
    X(GetGuest)            /* placeholder instruction that indicates a use of a register, replaced by the ssa pass */                                \
    X(SetGuest)            /* placeholder instruction that indicates a def of a register, replaced by the ssa pass */                                \
    X(LoadGuestFromMemory) /* to load or store to the thread_state struct which contains x86 register info */                                        \
    X(StoreGuestToMemory)                                                                                                                            \
    X(LoadSpill)                                                                                                                                     \
    X(StoreSpill)                                                                                                                                    \
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
    X(Shl)                                                                                                                                           \
    X(Shli)                                                                                                                                          \
    X(Shr)                                                                                                                                           \
    X(Shri)                                                                                                                                          \
    X(Sar)                                                                                                                                           \
    X(Sari)                                                                                                                                          \
    X(Rol8)                                                                                                                                          \
    X(Rol16)                                                                                                                                         \
    X(Rol32)                                                                                                                                         \
    X(Rol64)                                                                                                                                         \
    X(Ror8)                                                                                                                                          \
    X(Ror16)                                                                                                                                         \
    X(Ror32)                                                                                                                                         \
    X(Ror64)                                                                                                                                         \
    X(Select)                                                                                                                                        \
    X(Addi)                                                                                                                                          \
    X(And)                                                                                                                                           \
    X(Andi)                                                                                                                                          \
    X(Or)                                                                                                                                            \
    X(Ori)                                                                                                                                           \
    X(Xor)                                                                                                                                           \
    X(Xori)                                                                                                                                          \
    X(Not)                                                                                                                                           \
    X(Neg)                                                                                                                                           \
    X(Seqz)                                                                                                                                          \
    X(Snez)                                                                                                                                          \
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
    X(ReadWordRelative)                                                                                                                              \
    X(ReadDWordRelative)                                                                                                                             \
    X(ReadQWordRelative)                                                                                                                             \
    X(ReadXmmWordRelative)                                                                                                                           \
    X(WriteByteRelative)                                                                                                                             \
    X(WriteWordRelative)                                                                                                                             \
    X(WriteDWordRelative)                                                                                                                            \
    X(WriteQWordRelative)                                                                                                                            \
    X(WriteXmmWordRelative)                                                                                                                          \
    X(WriteXmmWord)                                                                                                                                  \
    X(IToV)                                                                                                                                          \
    X(VToI)                                                                                                                                          \
    X(VInsertInteger)                                                                                                                                \
    X(VExtractInteger)                                                                                                                               \
    X(SetVMask)                                                                                                                                      \
    X(VAnd)                                                                                                                                          \
    X(VOr)                                                                                                                                           \
    X(VXor)                                                                                                                                          \
    X(VSub)                                                                                                                                          \
    X(VAdd)                                                                                                                                          \
    X(VEqual)                                                                                                                                        \
    X(VIota)                                                                                                                                         \
    X(VGather)                                                                                                                                       \
    X(VSplat)                                                                                                                                        \
    X(VSplati)                                                                                                                                       \
    X(VMergei)                                                                                                                                       \
    X(VSlli)                                                                                                                                         \
    X(VSrai)                                                                                                                                         \
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