#include "felix86/aot/aot.hpp"

#include "felix86/common/log.hpp"

enum class InstructionWrapperType : u8 {
    NoOperands,
    OneReg,
    TwoReg,
    R12ToR15,
    R12ToR15_Reg,
    RegImmediate,
};

struct R12ToR15 {};
struct R12ToR15_Reg {};
struct SomeImmediate {};

struct InstructionWrapper {
    InstructionWrapper(ZydisMnemonic mnemonic) : mnemonic(mnemonic), type(InstructionWrapperType::NoOperands) {}

    InstructionWrapper(ZydisMnemonic mnemonic, ZydisRegister reg, SomeImmediate) : mnemonic(mnemonic), type(InstructionWrapperType::RegImmediate) {
        regs[0] = reg;
    }

    InstructionWrapper(ZydisMnemonic mnemonic, R12ToR15) : mnemonic(mnemonic), type(InstructionWrapperType::R12ToR15) {}

    InstructionWrapper(ZydisMnemonic mnemonic, R12ToR15_Reg) : mnemonic(mnemonic), type(InstructionWrapperType::R12ToR15_Reg) {}

    InstructionWrapper(ZydisMnemonic mnemonic, ZydisRegister reg) : mnemonic(mnemonic), type(InstructionWrapperType::OneReg) {
        regs[0] = reg;
    }

    InstructionWrapper(ZydisMnemonic mnemonic, ZydisRegister first_reg, ZydisRegister second_reg)
        : mnemonic(mnemonic), type(InstructionWrapperType::TwoReg) {
        regs[0] = first_reg;
        regs[1] = second_reg;
    }

    ZydisMnemonic mnemonic = ZYDIS_MNEMONIC_NOP;

    ZydisRegister regs[2] = {ZYDIS_REGISTER_NONE, ZYDIS_REGISTER_NONE};
    InstructionWrapperType type;

    bool compareWith(const ZydisDecodedInstruction& inst, const ZydisDecodedOperand* operands) const {
        if (inst.mnemonic != mnemonic) {
            return false;
        }

        switch (type) {
        case InstructionWrapperType::OneReg: {
            if (operands[0].type != ZYDIS_OPERAND_TYPE_REGISTER) {
                return false;
            }

            if (operands[1].type != ZYDIS_OPERAND_TYPE_UNUSED) {
                return false;
            }

            if (regs[0] != operands[0].reg.value) {
                return false;
            }
            break;
        }

        case InstructionWrapperType::TwoReg: {
            if (operands[0].type != ZYDIS_OPERAND_TYPE_REGISTER || operands[1].type != ZYDIS_OPERAND_TYPE_REGISTER) {
                return false;
            }

            if (regs[0] != operands[0].reg.value || regs[1] != operands[1].reg.value) {
                return false;
            }
            break;
        }

        case InstructionWrapperType::RegImmediate: {
            if (operands[0].type != ZYDIS_OPERAND_TYPE_REGISTER || operands[1].type != ZYDIS_OPERAND_TYPE_IMMEDIATE) {
                return false;
            }

            if (regs[0] != operands[0].reg.value) {
                return false;
            }
            break;
        }

        case InstructionWrapperType::R12ToR15: {
            if (operands[0].type != ZYDIS_OPERAND_TYPE_REGISTER) {
                return false;
            }

            if (operands[0].reg.value < ZYDIS_REGISTER_R12 || operands[0].reg.value > ZYDIS_REGISTER_R15) {
                return false;
            }
            break;
        }

        case InstructionWrapperType::R12ToR15_Reg: {
            if (operands[0].type != ZYDIS_OPERAND_TYPE_REGISTER || operands[1].type != ZYDIS_OPERAND_TYPE_REGISTER) {
                return false;
            }

            if (operands[0].reg.value < ZYDIS_REGISTER_R12 || operands[0].reg.value > ZYDIS_REGISTER_R15) {
                return false;
            }

            if (operands[1].reg.value < ZYDIS_REGISTER_RAX || operands[1].reg.value > ZYDIS_REGISTER_R15) {
                return false;
            }
            break;
        }

        case InstructionWrapperType::NoOperands: {
            break;
        }
        }

        return true;
    }
};

#define _None_(mnemonic) {(InstructionWrapper(ZYDIS_MNEMONIC_##mnemonic))}
#define _Regs_(mnemonic, first_reg, second_reg)                                                                                                      \
    {(InstructionWrapper(ZYDIS_MNEMONIC_##mnemonic, ZYDIS_REGISTER_##first_reg, ZYDIS_REGISTER_##second_reg))}
#define _Reg_(mnemonic, first_reg) {(InstructionWrapper(ZYDIS_MNEMONIC_##mnemonic, ZYDIS_REGISTER_##first_reg))}
#define _RegImm_(mnemonic, first_reg) {(InstructionWrapper(ZYDIS_MNEMONIC_##mnemonic, ZYDIS_REGISTER_##first_reg, SomeImmediate{}))}
#define _R12R15_(mnemonic) {(InstructionWrapper(ZYDIS_MNEMONIC_##mnemonic, R12ToR15{}))}
#define _R12R15Reg_(mnemonic) {(InstructionWrapper(ZYDIS_MNEMONIC_##mnemonic, R12ToR15_Reg{}))}

// Based largely on:
// https://github.com/NationalSecurityAgency/ghidra/blob/master/Ghidra/Processors/x86/data/patterns/x86-64gcc_patterns.xml

// These patterns are what is expected before the start of a function, usually they are what a function ends on.
// Most functions end in nops for alignment purposes
static const std::vector<InstructionWrapper> prepatterns[] = {
    {{_None_(NOP)}},                          // any sized nop
    {{_None_(LEAVE)}, {_None_(RET)}},         // leave + ret
    {{_Regs_(XOR, EAX, EAX)}, {_None_(RET)}}, // xor eax, eax + ret
    {{_Reg_(POP, RBX), {_None_(RET)}}},       // pop rbx + ret
    {{_Reg_(POP, RBP), {_None_(RET)}}},       // pop rbp + ret
    {{_Reg_(POP, R12), {_None_(RET)}}},       // pop r12 + ret
    {{_Reg_(POP, R13), {_None_(RET)}}},       // pop r13 + ret
    {{_Reg_(POP, R14), {_None_(RET)}}},       // pop r14 + ret
    {{_Reg_(POP, R15), {_None_(RET)}}},       // pop r15 + ret
};

static const std::vector<InstructionWrapper> postpatterns[] = {
    {_None_(ENDBR64)},                                                        // endbr64, libc functions start with this for rop prevention reasons
    {_Reg_(PUSH, R12), _Reg_(PUSH, RBP), _R12R15Reg_(MOV)},                   // push r12 + push rbp + mov r12-15, reg
    {_Reg_(PUSH, R12), _Reg_(PUSH, RBP), _Reg_(PUSH, RBX), _R12R15Reg_(MOV)}, // push r12 + push rbp + push rbx + mov r12-15, reg
    {_Reg_(PUSH, R15), _Reg_(PUSH, R14), _Reg_(PUSH, R13)},                   // push r15 + push r14 + push r13
    {_Reg_(PUSH, R14), _Reg_(PUSH, R13)},                                     // push r14 + push r13
    {_Reg_(PUSH, R13), _Reg_(PUSH, R12)},                                     // push r13 + push r12
    {_R12R15_(PUSH), _R12R15Reg_(MOV), _Reg_(PUSH, RBP)},                     // push r12-15 + mov r12-15, reg + push rbp
    {_R12R15_(PUSH), _R12R15_(PUSH), _R12R15Reg_(MOV)},                       // push r12-15 + push r12-15 + mov r12-r15, reg
    {_Reg_(PUSH, RBP), _Regs_(MOV, RBP, RSP), _RegImm_(SUB, RSP)},            // push rbp + mov rbp, rsp + sub rsp, some_imm
    {_Reg_(PUSH, RBP), _Regs_(MOV, RBP, RSP), _Reg_(PUSH, RBX)},              // push rbp + mov rbp, rsp + push rbx
    {_Reg_(PUSH, RBP), _Regs_(MOV, RBP, RDI), _Reg_(PUSH, RBX)},              // push rbp + mov rbp, rdi + push rbx
    {_Reg_(PUSH, RBX), _Regs_(MOV, RBX, RDI)},                                // push rbx + mov rbx, rdi
    {_Reg_(PUSH, RBX), _RegImm_(SUB, RSP)},                                   // push rbx + sub rsp, some_imm
};

enum State {
    Scanning,            // just scanning instructions
    Prepattern,          // at least one byte of a prepattern was found, we need to scan the rest
    ScanningPostpattern, // a full prepattern was scanned, check if postpattern follows
    Postpattern,         // scan the rest of the postpattern bytes
};

#define RESET_STATE()                                                                                                                                \
    prepattern = nullptr;                                                                                                                            \
    postpattern = nullptr;                                                                                                                           \
    postpattern_index = 0;                                                                                                                           \
    prepattern_index = 0;                                                                                                                            \
    state = Scanning

void AOT::FunctionStartFinder() {
    u64 start_address = 0;
    const std::vector<InstructionWrapper>* prepattern = nullptr;
    size_t prepattern_index = 0;
    const std::vector<InstructionWrapper>* postpattern = nullptr;
    size_t postpattern_index = 0;
    State state = Scanning;
    for (auto [code, size] : elf.executable_segments) {
        u8* code_final = code + size;
        while (code < code_final) {
            ZydisDecodedInstruction inst = {};
            ZydisDecodedOperand operands[10] = {};

            ZyanStatus result = decodeInstruction(inst, operands, code, code_final - code);
            if (!ZYAN_SUCCESS(result)) {
                code++;
                // If we were in some pattern, since we hit a bad instruction let's go back
                // to scanning
                RESET_STATE();
                continue;
            }

            switch (state) {
            case Scanning: {
                RESET_STATE();
                for (auto& pattern : prepatterns) {
                    if (pattern[0].compareWith(inst, operands)) {
                        // This instruction is the start of this pattern
                        prepattern_index = 1;
                        start_address = (u64)code;

                        if (pattern.size() == prepattern_index) {
                            // Pattern is just one instruction, go straight to scanning
                            // for the postpattern
                            state = ScanningPostpattern;
                        } else {
                            // We need to scan for the rest of the prepattern
                            prepattern = &pattern;
                            state = Prepattern;
                        }
                        break;
                    }
                }
                break;
            }
            case Prepattern: {
                if (prepattern->operator[](prepattern_index).compareWith(inst, operands)) {
                    prepattern_index += 1;

                    if (prepattern->size() == prepattern_index) {
                        state = ScanningPostpattern;
                    }
                } else {
                    RESET_STATE();
                }
                break;
            }
            case ScanningPostpattern: {
                RESET_STATE();
                for (auto& pattern : postpatterns) {
                    if (pattern[0].compareWith(inst, operands)) {
                        // This instruction is the start of this pattern
                        postpattern_index = 1;

                        if (pattern.size() == postpattern_index) {
                            // Pattern is just one instruction, we can just add it
                            addresses.insert(start_address);
                        } else {
                            // We need to scan for the rest of the prepattern
                            postpattern = &pattern;
                            state = Postpattern;
                        }
                        break;
                    }
                }
                break;
            }
            case Postpattern: {
                if (postpattern->operator[](postpattern_index).compareWith(inst, operands)) {
                    postpattern_index += 1;

                    if (postpattern->size() == postpattern_index) {
                        addresses.insert(start_address);
                        RESET_STATE();
                    }
                } else {
                    RESET_STATE();
                }
                break;
            }
            default: {
                ERROR("Unreachable");
            }
            }

            code += inst.length;
        }
    }
}