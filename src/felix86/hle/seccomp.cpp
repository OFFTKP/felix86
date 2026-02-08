#include <csignal>
#include <cstddef>
#include <linux/audit.h>
#include <linux/bpf_common.h>
#include <linux/seccomp.h>
#include <unistd.h>
#include "biscuit/assembler.hpp"
#include "felix86/common/config.hpp"
#include "felix86/common/global.hpp"
#include "felix86/common/log.hpp"
#include "felix86/common/state.hpp"
#include "felix86/common/utility.hpp"
#include "felix86/hle/seccomp.hpp"
#include "felix86/hle/syscall.hpp"
#include "felix86/v2/recompiler.hpp"

static u64 g_filter_index = 0;
static std::vector<u8> g_filter_instructions{};

struct x64_sock_filter {
    u16 code;
    u8 jt;
    u8 jf;
    u32 k;
};

struct x64_sock_fprog {
    u16 len;
    const x64_sock_filter* array;
};

struct x64_seccomp_data {
    u32 nr;
    u32 arch;
    u64 rip;
    u64 args[6];
};

struct BPFJit {
    explicit BPFJit(Assembler& as, u64 rip) : as(as), rip(rip) {}
    ~BPFJit() = default;

    void compileProgram(const x64_sock_fprog* program) {
        ASSERT(!g_mode32);
        if (program->len == 0) {
            WARN("seccomp program len == 0?");
            return;
        }
        labels.resize(program->len);
        prologue();
        for (int i = 0; i < program->len; i++) {
            if (g_config.dump_seccomp) {
                printInstruction(program->array[i]);
            }
            compileInstruction(program->array[i], i);
        }
        // Code did not return and led here
        as.LI(x1, (u64)felix86_crash_and_burn);
        as.JALR(x1);
        as.C_UNDEF();
        as.C_UNDEF();
        as.Bind(&end_of_program);
        epilogue();
    }

private:
    BPFJit(const BPFJit&) = delete;
    BPFJit& operator=(const BPFJit&) = delete;
    BPFJit(BPFJit&&) = delete;
    BPFJit& operator=(BPFJit&&) = delete;
    void prologue();
    void compileInstruction(const x64_sock_filter& instruction, int i);
    void printInstruction(const x64_sock_filter& instruction);
    void epilogue();

    Assembler& as;
    u64 rip;
    std::vector<Label> labels;
    Label end_of_program;
};

// Uses a specific convention to match the scratch registers in the felix86 recompiler:
// X -> x28
// A -> x29
// temporary -> x30
// pointer to data -> x31
constexpr inline auto X = x28;
constexpr inline auto A = x29;
constexpr inline auto temp = x30;
constexpr inline auto pointer = x31;
static_assert(Recompiler::isScratch(x28));
static_assert(Recompiler::isScratch(x29));
static_assert(Recompiler::isScratch(x30));
static_assert(Recompiler::isScratch(x31));

void BPFJit::compileInstruction(const x64_sock_filter& instruction, int index) {
    Label& label = labels[index];
    as.Bind(&label);
    u16 code = instruction.code;
#define SRC() (BPF_SRC(code) == BPF_K ? temp : X)
    switch (BPF_CLASS(code)) {
    case BPF_LD: {
        ASSERT(BPF_SIZE(code) == BPF_W);
        switch (BPF_MODE(code)) {
        case BPF_ABS: {
            u32 offset = instruction.k;
            ASSERT(offset < sizeof(x64_seccomp_data));
            as.LWU(A, offset, pointer);
            break;
        }
        case BPF_IMM:
        case BPF_LEN:
        case BPF_MEM:
        default: {
            ERROR("Bad BPF mode: %x during BPF_LD", BPF_MODE(code));
            break;
        }
        }
        break;
    }
    case BPF_ALU: {
        if (BPF_SRC(code) == BPF_K) {
            as.LI(temp, instruction.k);
        }
        switch (BPF_OP(code)) {
        case BPF_ADD: {
            as.ADDW(A, A, SRC());
            break;
        }
        case BPF_SUB: {
            as.SUBW(A, A, SRC());
            break;
        }
        case BPF_MUL: {
            as.MULW(A, A, SRC());
            break;
        }
        case BPF_DIV: {
            biscuit::Label is_zero, end;
            as.BEQZ(SRC(), &is_zero);
            as.DIVW(A, A, SRC());
            as.J(&end);
            as.Bind(&is_zero);
            as.MV(A, x0);
            as.Bind(&end);
            break;
        }
        case BPF_OR: {
            as.OR(A, A, SRC());
            break;
        }
        case BPF_AND: {
            as.AND(A, A, SRC());
            break;
        }
        case BPF_LSH: {
            as.SLLW(A, A, SRC());
            break;
        }
        case BPF_RSH: {
            as.SRLW(A, A, SRC());
            break;
        }
        case BPF_NEG: {
            as.NEGW(A, A);
            break;
        }
        case BPF_XOR: {
            as.XOR(A, A, SRC());
            break;
        }
        default: {
            ERROR("Bad BPF operation: %x", BPF_OP(code));
            break;
        }
        }
        break;
    }
    case BPF_JMP: {
        Label& jump_true = labels[index + 1 + instruction.jt];
        Label& jump_false = labels[index + 1 + instruction.jf];
        if (BPF_SRC(code) == BPF_K) {
            as.LI(temp, instruction.k);
        }
        switch (BPF_OP(code)) {
        case BPF_JA: {
            as.J(&jump_true);
            break;
        }
        case BPF_JEQ: {
            as.BEQ(A, SRC(), &jump_true);
            if (instruction.jf != 0) {
                as.J(&jump_false);
            }
            break;
        }
        case BPF_JGT: {
            as.BGT(A, SRC(), &jump_true);
            if (instruction.jf != 0) {
                as.J(&jump_false);
            }
            break;
        }
        case BPF_JGE: {
            as.BGE(A, SRC(), &jump_true);
            if (instruction.jf != 0) {
                as.J(&jump_false);
            }
            break;
        }
        case BPF_JSET: {
            as.AND(temp, A, SRC());
            as.BNEZ(temp, &jump_true);
            if (instruction.jf != 0) {
                as.J(&jump_false);
            }
            break;
        }
        default: {
            ERROR("Bad BPF jump op: %x", BPF_OP(code));
        }
        }
        break;
    }
    // TODO: handle ret cases when we need them
    // TODO: properly handle multiple filters/ret cases
    // TODO: a filter with tgkill and a filter with kill right after won't work correctly
    case BPF_RET: {
        if (BPF_SRC(code) == BPF_K) {
            switch (instruction.k) {
            case SECCOMP_RET_KILL_PROCESS: {
                as.LI(a7, felix86_riscv64_kill);
                as.LI(a0, getpid());
                as.LI(a1, SIGKILL);
                as.ECALL();
                as.LI(x1, (u64)felix86_crash_and_burn);
                as.JALR(x1);
                as.C_UNDEF();
                as.C_UNDEF();
                break;
            }
            case SECCOMP_RET_KILL_THREAD: {
                as.LI(a7, felix86_riscv64_tgkill);
                as.LI(a0, getpid());
                as.LI(a1, gettid());
                as.LI(a2, SIGKILL);
                as.ECALL();
                as.LI(x1, (u64)felix86_crash_and_burn);
                as.JALR(x1);
                as.C_UNDEF();
                as.C_UNDEF();
                break;
            }
            case SECCOMP_RET_LOG: {
                WARN("SECCOMP_RET_LOG, treating as SECCOMP_RET_ALLOW");
                [[fallthrough]];
            }
            case SECCOMP_RET_ALLOW: {
                as.J(&end_of_program);
                break;
            }
            default: {
                WARN("Unknown RET value: %x", instruction.k);
                as.C_UNDEF();
                as.C_UNDEF();
                break;
            }
            }
        } else {
            ERROR("Ret with SRC=X is unsupported");
        }
        break;
    }
    default: {
        ERROR("Bad BPF class: %x", BPF_CLASS(code));
        break;
    }
    }
#undef SRC
}

void BPFJit::printInstruction(const x64_sock_filter& instruction) {
    u16 code = instruction.code;
    std::string cl, size, mode, op, opj, src;
    switch (BPF_CLASS(code)) {
    case BPF_LD:
        cl = "BPF_LD";
        break;
    case BPF_LDX:
        cl = "BPF_LDX";
        break;
    case BPF_ST:
        cl = "BPF_ST";
        break;
    case BPF_STX:
        cl = "BPF_STX";
        break;
    case BPF_ALU:
        cl = "BPF_ALU";
        break;
    case BPF_JMP:
        cl = "BPF_JMP";
        break;
    case BPF_RET:
        cl = "BPF_RET";
        break;
    case BPF_MISC:
        cl = "BPF_MISC";
        break;
    default:
        UNREACHABLE();
    }
    switch (BPF_SIZE(code)) {
    case BPF_B: {
        size = "BPF_B";
        break;
    }
    case BPF_H: {
        size = "BPF_H";
        break;
    }
    case BPF_W: {
        size = "BPF_W";
        break;
    }
    default:
        UNREACHABLE();
    }
    switch (BPF_MODE(code)) {
    case BPF_IMM:
        mode = "BPF_IMM";
        break;
    case BPF_ABS:
        mode = "BPF_ABS";
        break;
    case BPF_IND:
        mode = "BPF_IND";
        break;
    case BPF_MEM:
        mode = "BPF_MEM";
        break;
    case BPF_LEN:
        mode = "BPF_LEN";
        break;
    case BPF_MSH:
        mode = "BPF_MSH";
        break;
    default:
        UNREACHABLE();
    }
    switch (BPF_OP(code)) {
    case BPF_ADD:
        op = "BPF_ADD";
        break;
    case BPF_SUB:
        op = "BPF_SUB";
        break;
    case BPF_MUL:
        op = "BPF_MUL";
        break;
    case BPF_DIV:
        op = "BPF_DIV";
        break;
    case BPF_OR:
        op = "BPF_OR";
        break;
    case BPF_AND:
        op = "BPF_AND";
        break;
    case BPF_LSH:
        op = "BPF_LSH";
        break;
    case BPF_RSH:
        op = "BPF_RSH";
        break;
    case BPF_NEG:
        op = "BPF_NEG";
        break;
    case BPF_MOD:
        op = "BPF_MOD";
        break;
    case BPF_XOR:
        op = "BPF_XOR";
        break;
    default:
        UNREACHABLE();
    }
    switch (BPF_OP(code)) {
    case BPF_JA:
        opj = "BPF_JA";
        break;
    case BPF_JEQ:
        opj = "BPF_JEQ";
        break;
    case BPF_JGT:
        opj = "BPF_JGT";
        break;
    case BPF_JGE:
        opj = "BPF_JGE";
        break;
    case BPF_JSET:
        opj = "BPF_JSET";
        break;
    default: {
        opj = "Bad OPJ?";
        break;
    }
    }
    switch (BPF_SRC(code)) {
    case BPF_K:
        src = "BPF_K";
        break;
    case BPF_X:
        src = "BPF_X";
        break;
    default:
        UNREACHABLE();
    }

    switch (BPF_CLASS(code)) {
    case BPF_LD:
    case BPF_ST:
    case BPF_LDX:
    case BPF_STX:
        PLAIN("%s | %s | %s (k: %x)", cl.c_str(), size.c_str(), mode.c_str(), instruction.k);
        break;
    case BPF_ALU:
        if (BPF_SRC(code) == BPF_K) {
            PLAIN("%s | %s | %s | %s (k: %x)", cl.c_str(), size.c_str(), op.c_str(), src.c_str(), instruction.k);
        } else {
            PLAIN("%s | %s | %s | %s", cl.c_str(), size.c_str(), op.c_str(), src.c_str());
        }
        break;
    case BPF_JMP:
        if (BPF_SRC(code) == BPF_K) {
            PLAIN("%s | %s | %s (k: %x, jt: %x, jf: %x)", cl.c_str(), opj.c_str(), src.c_str(), instruction.k, instruction.jt, instruction.jf);
        } else {
            PLAIN("%s | %s | %s (jt: %x, jf: %x)", cl.c_str(), opj.c_str(), src.c_str(), instruction.jt, instruction.jf);
        }
        break;
    case BPF_RET:
        if (BPF_SRC(code) == BPF_K) {
            PLAIN("%s | %s (k: %x)", cl.c_str(), src.c_str(), instruction.k);
        } else {
            PLAIN("%s | %s", cl.c_str(), src.c_str());
        }
        break;
    case BPF_MISC:
        PLAIN("BPF_MISC...?");
        break;
    default:
        UNREACHABLE();
    }
}

void BPFJit::prologue() {
    as.ADDI(sp, sp, (i32)(-sizeof(x64_seccomp_data)));
    as.MV(pointer, sp);

    as.SW(Recompiler::allocatedGPR(X86_REF_RAX), offsetof(x64_seccomp_data, nr), pointer);

    as.LI(temp, AUDIT_ARCH_X86_64);
    as.SW(temp, offsetof(x64_seccomp_data, arch), pointer);

    as.LI(temp, rip);
    as.SD(temp, offsetof(x64_seccomp_data, rip), pointer);

    int i = 0;
    auto refs = {X86_REF_RDI, X86_REF_RSI, X86_REF_RDX, X86_REF_R10, X86_REF_R8, X86_REF_R9};
    for (auto ref : refs) {
        as.SD(Recompiler::allocatedGPR(ref), offsetof(x64_seccomp_data, args) + i++ * sizeof(u64), pointer);
    }
}

void BPFJit::epilogue() {
    as.ADDI(sp, sp, sizeof(x64_seccomp_data));
}

bool Seccomp::setFilter(u32 flags, void* args, u64 rip) {
    if (flags & ~(0)) {
        WARN("Unsupported seccomp flags: %x", flags);
    }

    if (args == nullptr) {
        WARN("args is null during seccomp");
        return false;
    }

    if (((x64_sock_fprog*)args)->array == nullptr) {
        WARN("args->array is null during seccomp");
        return false;
    }

    if (g_filter_instructions.size() - g_filter_index < 4096) {
        g_filter_instructions.resize(g_filter_instructions.size() + 4096);
    }

    u8* pointer = g_filter_instructions.data() + g_filter_index;
    Assembler as(pointer, g_filter_instructions.size() - g_filter_index);
    u64 start = (u64)as.GetCursorPointer();
    BPFJit jit(as, rip);
    jit.compileProgram((const x64_sock_fprog*)args);
    u64 here = (u64)as.GetCursorPointer();
    u64 size = here - start;
    ASSERT(size % 4 == 0);
    g_filter_index += size;
    WARN("Seccomp filter installed");
    return true;
}

void Seccomp::emitFilters(biscuit::Assembler& as) {
    for (u64 i = 0; i < g_filter_index; i += 4) {
        as.GetCodeBuffer().Emit32(*(u32*)(g_filter_instructions.data() + i));
    }
}

bool Seccomp::hasFilters() {
    return g_filter_index != 0;
}