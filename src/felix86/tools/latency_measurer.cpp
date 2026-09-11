#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <random>
#include <vector>
#include <linux/perf_event.h>
#include <nlohmann/json.hpp>
#include <sys/syscall.h>
#include <unistd.h>
#include "felix86/common/global.hpp"
#include "felix86/common/state.hpp"
#include "felix86/v2/handlers.hpp"
#include "felix86/v2/recompiler.hpp"
#define XBYAK_NO_EXCEPTION
#define XBYAK64
#define XBYAK64_GCC
#include <sys/cachectl.h>
#include "xbyak/xbyak.h"

constexpr biscuit::GPR tmp = x31;
constexpr biscuit::GPR tmp2 = x30;
static_assert(Recompiler::isScratch(tmp));
static_assert(Recompiler::isScratch(tmp2));
constexpr int loops = 500;
constexpr int iterations = 100;
constexpr int runs = 31;
constexpr int warmup_runs = 3;

constexpr std::array callee_saved = {s0, s1, s2, s3, s4, s5, s6, s7, s8, s9, s10, s11};

struct alignas(16) Frame {
    u64 ra;
    u64 saved[callee_saved.size()];
    u64 counter;
    u64 cycles;
};
static_assert(sizeof(Frame) % 16 == 0);

bool g_flags = false;
bool g_mode32 = false;
Recompiler* g_rec = nullptr;

void reset_recompiler() {
    g_rec->setVectorState(SEW::E1024, 0);
    g_rec->setFlagMode(g_flags ? FlagMode::AlwaysEmit : FlagMode::NeverEmit);
    g_rec->resetX87();
    g_rec->v0Modified();
}

void compile_sequence(u64 rip) {
    reset_recompiler();
    g_rec->compileSequence(g_mode32, rip);
    BlockMetadata& metadata = g_rec->getBlockMetadata(rip);
    u64 start = metadata.host_address;
    u64 end = start;
    for (auto instr : metadata.translation_sizes) {
        end += instr.riscv_instructions_size;
    }
    u64 spans_end = end + 4;
    u64 code_end = (u64)g_rec->getEndOfCodeCache();
    if (spans_end == code_end) {
        // Remove compiled UNDEF instructions off the end, if any
        u16* fin = (u16*)(end - 2);
        while (*fin == 0) {
            end -= 2;
            fin = (u16*)(end - 2);
        }

        // Also remove a store to zero and a hint used to emulate ud2
        end -= 8;
    }
    g_rec->getAssembler().SetCursorPointer((u8*)end);
}

enum WidthFlag : u32 {
    W64 = 1 << 0,
    W32 = 1 << 1,
    W16 = 1 << 2,
    W8 = 1 << 3,
    W8HI = 1 << 4,
    W8MIX = 1 << 5,
    XMM = 1 << 6,
    YMM = 1 << 7,
    ALL = W64 | W32 | W16 | W8 | W8HI,
    VEC = XMM | YMM,
    RR = ALL | W8MIX,
};

struct Width {
    const char* name;
    u32 flag;
    int bits;
    std::vector<const Xbyak::Reg*> dsts;
    std::vector<const Xbyak::Reg*> srcs;
};

const std::vector<Width>& widths() {
    using namespace Xbyak::util;
    static const std::vector<Width> list = {
        {"r64", W64, 64, {&rax, &rbx, &rdx, &rbp, &rsi, &rdi, &r8, &r9, &r10}, {&rcx, &r11, &r12, &r13, &r14, &r15}},
        {"r32", W32, 32, {&eax, &ebx, &edx, &ebp, &esi, &edi, &r8d, &r9d, &r10d}, {&ecx, &r11d, &r12d, &r13d, &r14d, &r15d}},
        {"r16", W16, 16, {&ax, &bx, &dx, &bp, &si, &di, &r8w, &r9w, &r10w}, {&cx, &r11w, &r12w, &r13w, &r14w, &r15w}},
        {"r8 lo, lo", W8, 8, {&al, &bl, &dl, &bpl, &sil, &dil, &r8b, &r9b, &r10b}, {&cl, &r11b, &r12b, &r13b, &r14b, &r15b}},
        {"r8 hi, lo", W8HI, 8, {&ah, &bh, &dh}, {&cl}},
        {"r8 lo, hi", W8MIX, 8, {&al, &bl, &dl}, {&ch}},
        {"r8 hi, hi", W8MIX, 8, {&ah, &bh, &dh}, {&ch}},
        {"xmm", XMM, 128, {&xmm1, &xmm2, &xmm3, &xmm4, &xmm5, &xmm6, &xmm7}, {&xmm8, &xmm9, &xmm10, &xmm11, &xmm12, &xmm13, &xmm14, &xmm15}},
        {"ymm", YMM, 256, {&ymm1, &ymm2, &ymm3, &ymm4, &ymm5, &ymm6, &ymm7}, {&ymm8, &ymm9, &ymm10, &ymm11, &ymm12, &ymm13, &ymm14, &ymm15}},
    };
    return list;
}

const Width& width_for(int bits) {
    for (const Width& w : widths()) {
        if (w.bits == bits && !(w.flag & (W8HI | W8MIX))) {
            return w;
        }
    }
    UNREACHABLE();
    return widths()[0];
}

struct Ctx {
    Xbyak::CodeGenerator& x;
    const Width& width;
    size_t cur = 0;
    std::mt19937 rng{0x86};

    const Xbyak::Reg& dst() {
        return *width.dsts[cur];
    }

    const Xbyak::Reg& dst(int bits) {
        return bits == width.bits ? dst() : *width_for(bits).dsts[cur];
    }

    const Xbyak::Reg& src() {
        return pick(width.srcs);
    }

    const Xbyak::Reg& src(int bits) {
        return pick(width_for(bits).srcs);
    }

    const Xbyak::Reg32e& dst32e() {
        return static_cast<const Xbyak::Reg32e&>(dst());
    }

    const Xbyak::Reg32e& src32e() {
        return static_cast<const Xbyak::Reg32e&>(src());
    }

    const Xbyak::Reg64& dst64() {
        return static_cast<const Xbyak::Reg64&>(dst(64));
    }

    const Xbyak::Reg64& src64() {
        return static_cast<const Xbyak::Reg64&>(src(64));
    }

    const Xbyak::Reg32& src32() {
        return static_cast<const Xbyak::Reg32&>(src(32));
    }

    const Xbyak::Xmm& xdst() {
        return static_cast<const Xbyak::Xmm&>(dst());
    }

    const Xbyak::Xmm& xsrc() {
        return static_cast<const Xbyak::Xmm&>(src());
    }

    const Xbyak::Ymm& ydst() {
        return static_cast<const Xbyak::Ymm&>(dst());
    }

    const Xbyak::Ymm& ysrc() {
        return static_cast<const Xbyak::Ymm&>(src());
    }

    Xbyak::Xmm xdst128() {
        return Xbyak::Xmm(dst().getIdx());
    }

    Xbyak::Xmm xsrc128() {
        return Xbyak::Xmm(src().getIdx());
    }

    u32 imm(int bits) {
        return rng() & ((1ull << bits) - 1);
    }

    u8 shift() {
        return 1 + rng() % (width.bits - 1);
    }

private:
    const Xbyak::Reg& pick(const std::vector<const Xbyak::Reg*>& pool) {
        return *pool[rng() % pool.size()];
    }
};

struct Test {
    const char* name;
    u32 widths;
    void (*emit)(Ctx&);
    bool no_throughput = false;
};

const std::vector<Test>& tests() {
    using namespace Xbyak::util;
    static const std::vector<Test> list = {
        {"add r, r", RR, [](Ctx& c) { c.x.add(c.dst(), c.src()); }},
        {"add r, imm", ALL, [](Ctx& c) { c.x.add(c.dst(), c.imm(8)); }},
        {"add r, imm32", W64 | W32, [](Ctx& c) { c.x.add(c.dst(), c.imm(31)); }},
        {"sub r, r", RR, [](Ctx& c) { c.x.sub(c.dst(), c.src()); }},
        {"sub r, imm", ALL, [](Ctx& c) { c.x.sub(c.dst(), c.imm(8)); }},
        {"sub r, imm32", W64 | W32, [](Ctx& c) { c.x.sub(c.dst(), c.imm(31)); }},
        {"adc r, r", RR, [](Ctx& c) { c.x.adc(c.dst(), c.src()); }},
        {"adc r, imm", ALL, [](Ctx& c) { c.x.adc(c.dst(), c.imm(8)); }},
        {"sbb r, r", RR, [](Ctx& c) { c.x.sbb(c.dst(), c.src()); }},
        {"sbb r, imm", ALL, [](Ctx& c) { c.x.sbb(c.dst(), c.imm(8)); }},
        {"or r, r", RR, [](Ctx& c) { c.x.or_(c.dst(), c.src()); }},
        {"or r, imm", ALL, [](Ctx& c) { c.x.or_(c.dst(), c.imm(8)); }},
        {"or r, imm32", W64 | W32, [](Ctx& c) { c.x.or_(c.dst(), c.imm(31)); }},
        {"and r, r", RR, [](Ctx& c) { c.x.and_(c.dst(), c.src()); }},
        {"and r, imm", ALL, [](Ctx& c) { c.x.and_(c.dst(), c.imm(8)); }},
        {"and r, imm32", W64 | W32, [](Ctx& c) { c.x.and_(c.dst(), c.imm(31)); }},
        {"xor r, r", RR, [](Ctx& c) { c.x.xor_(c.dst(), c.src()); }},
        {"xor r, imm", ALL, [](Ctx& c) { c.x.xor_(c.dst(), c.imm(8)); }},
        {"xor r, imm32", W64 | W32, [](Ctx& c) { c.x.xor_(c.dst(), c.imm(31)); }},
        {"inc r", ALL, [](Ctx& c) { c.x.inc(c.dst()); }},
        {"dec r", ALL, [](Ctx& c) { c.x.dec(c.dst()); }},
        {"neg r", ALL, [](Ctx& c) { c.x.neg(c.dst()); }},
        {"not r", ALL, [](Ctx& c) { c.x.not_(c.dst()); }},
        {"shl r, imm", ALL, [](Ctx& c) { c.x.shl(c.dst(), c.shift()); }},
        {"shl r, cl", ALL, [](Ctx& c) { c.x.shl(c.dst(), cl); }},
        {"shr r, imm", ALL, [](Ctx& c) { c.x.shr(c.dst(), c.shift()); }},
        {"shr r, cl", ALL, [](Ctx& c) { c.x.shr(c.dst(), cl); }},
        {"sar r, imm", ALL, [](Ctx& c) { c.x.sar(c.dst(), c.shift()); }},
        {"sar r, cl", ALL, [](Ctx& c) { c.x.sar(c.dst(), cl); }},
        {"rol r, imm", ALL, [](Ctx& c) { c.x.rol(c.dst(), c.shift()); }},
        {"rol r, cl", ALL, [](Ctx& c) { c.x.rol(c.dst(), cl); }},
        {"ror r, imm", ALL, [](Ctx& c) { c.x.ror(c.dst(), c.shift()); }},
        {"ror r, cl", ALL, [](Ctx& c) { c.x.ror(c.dst(), cl); }},
        {"rcl r, imm", ALL, [](Ctx& c) { c.x.rcl(c.dst(), c.shift()); }},
        {"rcl r, cl", ALL, [](Ctx& c) { c.x.rcl(c.dst(), cl); }},
        {"rcr r, imm", ALL, [](Ctx& c) { c.x.rcr(c.dst(), c.shift()); }},
        {"rcr r, cl", ALL, [](Ctx& c) { c.x.rcr(c.dst(), cl); }},
        {"shld r, r, imm", W64 | W32 | W16, [](Ctx& c) { c.x.shld(c.dst(), c.src(), c.shift()); }},
        {"shld r, r, cl", W64 | W32 | W16, [](Ctx& c) { c.x.shld(c.dst(), c.src(), cl); }},
        {"shrd r, r, imm", W64 | W32 | W16, [](Ctx& c) { c.x.shrd(c.dst(), c.src(), c.shift()); }},
        {"shrd r, r, cl", W64 | W32 | W16, [](Ctx& c) { c.x.shrd(c.dst(), c.src(), cl); }},
        {"mul r", W64 | W32 | W16 | W8, [](Ctx& c) { c.x.mul(c.src()); }, true},
        {"imul r", W64 | W32 | W16 | W8, [](Ctx& c) { c.x.imul(c.src()); }, true},
        {"imul r, r", W64 | W32 | W16, [](Ctx& c) { c.x.imul(c.dst(), c.src()); }},
        {"imul r, r, imm", W64 | W32 | W16, [](Ctx& c) { c.x.imul(c.dst(), c.dst(), c.imm(8)); }},
        {"imul r, r, imm32", W64 | W32, [](Ctx& c) { c.x.imul(c.dst(), c.dst(), c.imm(31)); }},
        {"div r", W8, [](Ctx& c) { c.x.div(c.src()); }, true},
        {"xor edx, edx; div r", W64 | W32 | W16,
         [](Ctx& c) {
             c.x.xor_(edx, edx);
             c.x.div(c.src());
         },
         true},
        {"cbw; idiv r", W8,
         [](Ctx& c) {
             c.x.cbw();
             c.x.idiv(c.src());
         },
         true},
        {"cwd; idiv r", W16,
         [](Ctx& c) {
             c.x.cwd();
             c.x.idiv(c.src());
         },
         true},
        {"cdq; idiv r", W32,
         [](Ctx& c) {
             c.x.cdq();
             c.x.idiv(c.src());
         },
         true},
        {"cqo; idiv r", W64,
         [](Ctx& c) {
             c.x.cqo();
             c.x.idiv(c.src());
         },
         true},
        {"mov r, r", RR, [](Ctx& c) { c.x.mov(c.dst(), c.src()); }},
        {"mov r, imm", ALL, [](Ctx& c) { c.x.mov(c.dst(), c.imm(8)); }},
        {"mov r, imm32", W64 | W32, [](Ctx& c) { c.x.mov(c.dst(), c.imm(31)); }},
        {"mov r, imm64", W64, [](Ctx& c) { c.x.mov(c.dst(), 0x8000000000000000ull | c.rng()); }},
        {"movzx r, r8", W64 | W32 | W16, [](Ctx& c) { c.x.movzx(c.dst(), c.dst(8)); }},
        {"movzx r, r16", W64 | W32, [](Ctx& c) { c.x.movzx(c.dst(), c.dst(16)); }},
        {"movsx r, r8", W64 | W32 | W16, [](Ctx& c) { c.x.movsx(c.dst(), c.dst(8)); }},
        {"movsx r, r16", W64 | W32, [](Ctx& c) { c.x.movsx(c.dst(), c.dst(16)); }},
        {"movsxd r, r32", W64, [](Ctx& c) { c.x.movsxd(static_cast<const Xbyak::Reg64&>(c.dst()), c.dst(32)); }},
        {"cbw", W16, [](Ctx& c) { c.x.cbw(); }, true},
        {"cwde", W32, [](Ctx& c) { c.x.cwde(); }, true},
        {"cdqe", W64, [](Ctx& c) { c.x.cdqe(); }, true},
        {"lea r, [r]", W64 | W32 | W16, [](Ctx& c) { c.x.lea(c.dst(), ptr[c.dst64()]); }},
        {"lea r, [r + disp8]", W64 | W32 | W16, [](Ctx& c) { c.x.lea(c.dst(), ptr[c.dst64() + 0x10]); }},
        {"lea r, [r + disp32]", W64 | W32 | W16, [](Ctx& c) { c.x.lea(c.dst(), ptr[c.dst64() + 0x123456]); }},
        {"lea r, [r + r]", W64 | W32 | W16, [](Ctx& c) { c.x.lea(c.dst(), ptr[c.dst64() + c.src64()]); }},
        {"lea r, [r + r*2]", W64 | W32 | W16, [](Ctx& c) { c.x.lea(c.dst(), ptr[c.dst64() + c.src64() * 2]); }},
        {"lea r, [r + r*4]", W64 | W32 | W16, [](Ctx& c) { c.x.lea(c.dst(), ptr[c.dst64() + c.src64() * 4]); }},
        {"lea r, [r + r*8]", W64 | W32 | W16, [](Ctx& c) { c.x.lea(c.dst(), ptr[c.dst64() + c.src64() * 8]); }},
        {"lea r, [r*4 + disp]", W64 | W32 | W16, [](Ctx& c) { c.x.lea(c.dst(), ptr[c.dst64() * 4 + 0x10]); }},
        {"lea r, [r + r + disp8]", W64 | W32 | W16, [](Ctx& c) { c.x.lea(c.dst(), ptr[c.dst64() + c.src64() + 0x10]); }},
        {"lea r, [r + r*4 + disp8]", W64 | W32 | W16, [](Ctx& c) { c.x.lea(c.dst(), ptr[c.dst64() + c.src64() * 4 + 0x10]); }},
        {"lea r, [r + r*4 + disp32]", W64 | W32 | W16, [](Ctx& c) { c.x.lea(c.dst(), ptr[c.dst64() + c.src64() * 4 + 0x123456]); }},
        {"lea r, [r32 + r32*4 + disp8]", W32, [](Ctx& c) { c.x.lea(c.dst(), ptr[c.dst32e() + c.src32e() * 4 + 0x10]); }},
        {"cmovz r, r", W64 | W32 | W16, [](Ctx& c) { c.x.cmovz(c.dst(), c.src()); }},
        {"cmovb r, r", W64 | W32 | W16, [](Ctx& c) { c.x.cmovb(c.dst(), c.src()); }},
        {"cmovl r, r", W64 | W32 | W16, [](Ctx& c) { c.x.cmovl(c.dst(), c.src()); }},
        {"setz r8", W8 | W8HI, [](Ctx& c) { c.x.setz(c.dst()); }},
        {"bswap r", W64 | W32, [](Ctx& c) { c.x.bswap(c.dst32e()); }},
        {"or r, r; bsf r, r", W64 | W32 | W16, [](Ctx& c) { c.x.or_(c.dst(), c.src()); c.x.bsf(c.dst(), c.dst()); }},
        {"or r, r; bsr r, r", W64 | W32 | W16, [](Ctx& c) { c.x.or_(c.dst(), c.src()); c.x.bsr(c.dst(), c.dst()); }},
        {"popcnt r, r", W64 | W32 | W16, [](Ctx& c) { c.x.popcnt(c.dst(), c.dst()); }},
        {"lzcnt r, r", W64 | W32 | W16, [](Ctx& c) { c.x.lzcnt(c.dst(), c.dst()); }},
        {"tzcnt r, r", W64 | W32 | W16, [](Ctx& c) { c.x.tzcnt(c.dst(), c.dst()); }},
        {"crc32 r, r8", W64 | W32, [](Ctx& c) { c.x.crc32(c.dst32e(), c.dst(8)); }},
        {"crc32 r, r", W64 | W32, [](Ctx& c) { c.x.crc32(c.dst32e(), c.dst()); }},
        {"andn r, r, r", W64 | W32, [](Ctx& c) { c.x.andn(c.dst32e(), c.dst32e(), c.src()); }},
        {"blsi r, r", W64 | W32, [](Ctx& c) { c.x.blsi(c.dst32e(), c.dst()); }},
        {"blsmsk r, r", W64 | W32, [](Ctx& c) { c.x.blsmsk(c.dst32e(), c.dst()); }},
        {"blsr r, r", W64 | W32, [](Ctx& c) { c.x.blsr(c.dst32e(), c.dst()); }},
        {"bextr r, r, r", W64 | W32, [](Ctx& c) { c.x.bextr(c.dst32e(), c.dst(), c.src32e()); }},
        {"bzhi r, r, r", W64 | W32, [](Ctx& c) { c.x.bzhi(c.dst32e(), c.dst(), c.src32e()); }},
        // {"pdep r, r, r", W64 | W32, [](Ctx& c) { c.x.pdep(c.dst32e(), c.dst32e(), c.src()); }},
        {"pext r, r, r", W64 | W32, [](Ctx& c) { c.x.pext(c.dst32e(), c.dst32e(), c.src()); }},
        {"rorx r, r, imm", W64 | W32, [](Ctx& c) { c.x.rorx(c.dst32e(), c.dst(), c.shift()); }},
        {"sarx r, r, r", W64 | W32, [](Ctx& c) { c.x.sarx(c.dst32e(), c.dst(), c.src32e()); }},
        {"shlx r, r, r", W64 | W32, [](Ctx& c) { c.x.shlx(c.dst32e(), c.dst(), c.src32e()); }},
        {"shrx r, r, r", W64 | W32, [](Ctx& c) { c.x.shrx(c.dst32e(), c.dst(), c.src32e()); }},

#define X2(name)                                                                                                                                     \
    {                                                                                                                                                \
        #name " x, x", XMM, [](Ctx& c) { c.x.name(c.xdst(), c.xsrc()); }                                                                             \
    }
#define X2I(name, bits)                                                                                                                              \
    {                                                                                                                                                \
        #name " x, x, imm", XMM, [](Ctx& c) { c.x.name(c.xdst(), c.xsrc(), c.imm(bits)); }                                                           \
    }
#define XU(name)                                                                                                                                     \
    {                                                                                                                                                \
        #name " x, x", XMM, [](Ctx& c) { c.x.name(c.xdst(), c.xdst()); }                                                                             \
    }
#define XUI(name, bits)                                                                                                                              \
    {                                                                                                                                                \
        #name " x, x, imm", XMM, [](Ctx& c) { c.x.name(c.xdst(), c.xdst(), c.imm(bits)); }                                                           \
    }
#define XSHIFT(name)                                                                                                                                 \
    {#name " x, imm", XMM, [](Ctx& c) { c.x.name(c.xdst(), c.imm(4)); }}, {                                                                          \
        #name " x, x", XMM, [](Ctx& c) { c.x.name(c.xdst(), c.xsrc()); }                                                                             \
    }
#define V3(name, w)                                                                                                                                  \
    {                                                                                                                                                \
        #name " v, v, v", w, [](Ctx& c) { c.x.name(c.xdst(), c.xdst(), c.xsrc()); }                                                                  \
    }
#define V3I(name, w, bits)                                                                                                                           \
    {                                                                                                                                                \
        #name " v, v, v, imm", w, [](Ctx& c) { c.x.name(c.xdst(), c.xdst(), c.xsrc(), c.imm(bits)); }                                                \
    }
#define V2(name, w)                                                                                                                                  \
    {                                                                                                                                                \
        #name " v, v", w, [](Ctx& c) { c.x.name(c.xdst(), c.xdst()); }                                                                               \
    }
#define V2I(name, w, bits)                                                                                                                           \
    {                                                                                                                                                \
        #name " v, v, imm", w, [](Ctx& c) { c.x.name(c.xdst(), c.xdst(), c.imm(bits)); }                                                             \
    }
#define V4(name, w)                                                                                                                                  \
    {                                                                                                                                                \
        #name " v, v, v, v", w, [](Ctx& c) { c.x.name(c.xdst(), c.xdst(), c.xsrc(), c.xsrc()); }                                                     \
    }
#define VSHIFT(name)                                                                                                                                 \
    {#name " v, v, imm", VEC, [](Ctx& c) { c.x.name(c.xdst(), c.xdst(), c.imm(4)); }}, {                                                             \
        #name " v, v, x", VEC, [](Ctx& c) { c.x.name(c.xdst(), c.xdst(), c.xsrc128()); }                                                             \
    }
#define VFROMX(name)                                                                                                                                 \
    {                                                                                                                                                \
        #name " v, x", VEC, [](Ctx& c) { c.x.name(c.xdst(), c.xdst128()); }                                                                          \
    }
#define VTOX(name)                                                                                                                                   \
    {                                                                                                                                                \
        #name " x, v", VEC, [](Ctx& c) { c.x.name(c.xdst128(), c.xdst()); }                                                                          \
    }
#define Y3(name)                                                                                                                                     \
    {                                                                                                                                                \
        #name " y, y, y", YMM, [](Ctx& c) { c.x.name(c.ydst(), c.ydst(), c.ysrc()); }                                                                \
    }
#define Y3I(name, bits)                                                                                                                              \
    {                                                                                                                                                \
        #name " y, y, y, imm", YMM, [](Ctx& c) { c.x.name(c.ydst(), c.ydst(), c.ysrc(), c.imm(bits)); }                                              \
    }
#define Y2I(name, bits)                                                                                                                              \
    {                                                                                                                                                \
        #name " y, y, imm", YMM, [](Ctx& c) { c.x.name(c.ydst(), c.ydst(), c.imm(bits)); }                                                           \
    }

        X2(movaps), X2(movapd), X2(movups), X2(movupd), X2(movdqa), X2(movdqu), X2(movss), X2(movsd),
        X2(movhlps), X2(movlhps), XU(movsldup), XU(movshdup), XU(movddup),
        X2(addps), X2(addpd), X2(subps), X2(subpd), X2(mulps), X2(mulpd), X2(divps), X2(divpd),
        X2(addss), X2(addsd), X2(subss), X2(subsd), X2(mulss), X2(mulsd), X2(divss), X2(divsd),
        XU(sqrtps), XU(sqrtpd), XU(sqrtss), XU(sqrtsd), XU(rcpps), XU(rsqrtps), XU(rcpss), XU(rsqrtss),
        X2(minps), X2(minpd), X2(maxps), X2(maxpd), X2(minss), X2(minsd), X2(maxss), X2(maxsd),
        X2(addsubps), X2(addsubpd), X2(haddps), X2(haddpd), X2(hsubps), X2(hsubpd),
        X2(andps), X2(andpd), X2(andnps), X2(andnpd), X2(orps), X2(orpd), X2(xorps), X2(xorpd),
        X2I(cmpps, 3), X2I(cmppd, 3), X2I(cmpss, 3), X2I(cmpsd, 3),
        X2I(shufps, 8), X2I(shufpd, 2), X2(unpcklps), X2(unpckhps), X2(unpcklpd), X2(unpckhpd),
        X2I(blendps, 4), X2I(blendpd, 2), X2(blendvps), X2(blendvpd), X2I(dpps, 8), X2I(dppd, 8),
        XUI(roundps, 4), XUI(roundpd, 4), X2I(roundss, 4), X2I(roundsd, 4), X2I(insertps, 8),
        XU(cvtdq2ps), XU(cvtps2dq), XU(cvttps2dq), XU(cvtdq2pd), XU(cvtpd2dq), XU(cvttpd2dq),
        XU(cvtps2pd), XU(cvtpd2ps), X2(cvtss2sd), X2(cvtsd2ss),
        {"cvtsi2ss x, r32", XMM, [](Ctx& c) { c.x.cvtsi2ss(c.xdst(), c.src32()); }},
        {"cvtsi2sd x, r64", XMM, [](Ctx& c) { c.x.cvtsi2sd(c.xdst(), c.src64()); }},
        {"cvttss2si r, x; cvtsi2ss x, r", XMM, [](Ctx& c) { c.x.cvttss2si(c.src32(), c.xdst()); c.x.cvtsi2ss(c.xdst(), c.src32()); }},
        {"cvttsd2si r, x; cvtsi2sd x, r", XMM, [](Ctx& c) { c.x.cvttsd2si(c.src64(), c.xdst()); c.x.cvtsi2sd(c.xdst(), c.src64()); }},
        {"cvtsd2si r, x; cvtsi2sd x, r", XMM, [](Ctx& c) { c.x.cvtsd2si(c.src64(), c.xdst()); c.x.cvtsi2sd(c.xdst(), c.src64()); }},
        {"movd x, r32", XMM, [](Ctx& c) { c.x.movd(c.xdst(), c.src32()); }},
        {"movq x, r64", XMM, [](Ctx& c) { c.x.movq(c.xdst(), c.src64()); }},
        {"movd r32, x; movd x, r32", XMM, [](Ctx& c) { c.x.movd(c.src32(), c.xdst()); c.x.movd(c.xdst(), c.src32()); }},
        {"movq r64, x; movq x, r64", XMM, [](Ctx& c) { c.x.movq(c.src64(), c.xdst()); c.x.movq(c.xdst(), c.src64()); }},
        X2(paddb), X2(paddw), X2(paddd), X2(paddq), X2(psubb), X2(psubw), X2(psubd), X2(psubq),
        X2(paddsb), X2(paddsw), X2(psubsb), X2(psubsw), X2(paddusb), X2(paddusw), X2(psubusb), X2(psubusw),
        X2(pmullw), X2(pmulld), X2(pmulhw), X2(pmulhuw), X2(pmulhrsw), X2(pmuludq), X2(pmuldq), X2(pmaddwd), X2(pmaddubsw),
        X2(phaddw), X2(phaddd), X2(phaddsw), X2(phsubw), X2(phsubd), X2(phsubsw),
        X2(pand), X2(pandn), X2(por), X2(pxor),
        X2(pcmpeqb), X2(pcmpeqw), X2(pcmpeqd), X2(pcmpeqq), X2(pcmpgtb), X2(pcmpgtw), X2(pcmpgtd), X2(pcmpgtq),
        X2(pminsb), X2(pminsw), X2(pminsd), X2(pmaxsb), X2(pmaxsw), X2(pmaxsd),
        X2(pminub), X2(pminuw), X2(pminud), X2(pmaxub), X2(pmaxuw), X2(pmaxud),
        X2(pavgb), X2(pavgw), X2(psadbw), X2I(mpsadbw, 3), XU(phminposuw),
        XU(pabsb), XU(pabsw), XU(pabsd), X2(psignb), X2(psignw), X2(psignd),
        X2(pshufb), XUI(pshufd, 8), XUI(pshuflw, 8), XUI(pshufhw, 8), X2I(palignr, 5),
        X2(punpcklbw), X2(punpcklwd), X2(punpckldq), X2(punpcklqdq), X2(punpckhbw), X2(punpckhwd), X2(punpckhdq), X2(punpckhqdq),
        X2(packsswb), X2(packssdw), X2(packuswb), X2(packusdw),
        X2I(pblendw, 8), X2(pblendvb),
        XSHIFT(psllw), XSHIFT(pslld), XSHIFT(psllq), XSHIFT(psrlw), XSHIFT(psrld), XSHIFT(psrlq), XSHIFT(psraw), XSHIFT(psrad),
        {"pslldq x, imm", XMM, [](Ctx& c) { c.x.pslldq(c.xdst(), c.imm(4)); }},
        {"psrldq x, imm", XMM, [](Ctx& c) { c.x.psrldq(c.xdst(), c.imm(4)); }},
        XU(pmovzxbw), XU(pmovzxbd), XU(pmovzxbq), XU(pmovzxwd), XU(pmovzxwq), XU(pmovzxdq),
        XU(pmovsxbw), XU(pmovsxbd), XU(pmovsxbq), XU(pmovsxwd), XU(pmovsxwq), XU(pmovsxdq),
        {"pinsrb x, r32, imm", XMM, [](Ctx& c) { c.x.pinsrb(c.xdst(), c.src32(), c.imm(4)); }},
        {"pinsrw x, r32, imm", XMM, [](Ctx& c) { c.x.pinsrw(c.xdst(), c.src32(), c.imm(3)); }},
        {"pinsrd x, r32, imm", XMM, [](Ctx& c) { c.x.pinsrd(c.xdst(), c.src32(), c.imm(2)); }},
        {"pinsrq x, r64, imm", XMM, [](Ctx& c) { c.x.pinsrq(c.xdst(), c.src64(), c.imm(1)); }},
        {"pextrw r32, x, imm; pinsrw x, r32, imm", XMM, [](Ctx& c) { c.x.pextrw(c.src32(), c.xdst(), c.imm(3)); c.x.pinsrw(c.xdst(), c.src32(), c.imm(3)); }},
        {"pextrd r32, x, imm; pinsrd x, r32, imm", XMM, [](Ctx& c) { c.x.pextrd(c.src32(), c.xdst(), c.imm(2)); c.x.pinsrd(c.xdst(), c.src32(), c.imm(2)); }},
        {"pextrq r64, x, imm; pinsrq x, r64, imm", XMM, [](Ctx& c) { c.x.pextrq(c.src64(), c.xdst(), c.imm(1)); c.x.pinsrq(c.xdst(), c.src64(), c.imm(1)); }},
        X2(aesenc), X2(aesenclast), X2(aesdec), X2(aesdeclast), XU(aesimc), XUI(aeskeygenassist, 8), X2I(pclmulqdq, 8),

        V3(vmovss, XMM), V3(vmovsd, XMM), V2(vmovaps, VEC), V2(vmovapd, VEC), V2(vmovups, VEC), V2(vmovupd, VEC), V2(vmovdqa, VEC), V2(vmovdqu, VEC),
        V3(vmovhlps, XMM), V3(vmovlhps, XMM), V2(vmovsldup, VEC), V2(vmovshdup, VEC), V2(vmovddup, VEC),
        {"vmovd x, r32", XMM, [](Ctx& c) { c.x.vmovd(c.xdst(), c.src32()); }},
        {"vmovq x, r64", XMM, [](Ctx& c) { c.x.vmovq(c.xdst(), c.src64()); }},
        {"vmovd r32, x; vmovd x, r32", XMM, [](Ctx& c) { c.x.vmovd(c.src32(), c.xdst()); c.x.vmovd(c.xdst(), c.src32()); }},
        {"vmovq r64, x; vmovq x, r64", XMM, [](Ctx& c) { c.x.vmovq(c.src64(), c.xdst()); c.x.vmovq(c.xdst(), c.src64()); }},
        V3(vaddps, VEC), V3(vaddpd, VEC), V3(vsubps, VEC), V3(vsubpd, VEC), V3(vmulps, VEC), V3(vmulpd, VEC), V3(vdivps, VEC), V3(vdivpd, VEC),
        V3(vaddss, XMM), V3(vaddsd, XMM), V3(vsubss, XMM), V3(vsubsd, XMM), V3(vmulss, XMM), V3(vmulsd, XMM), V3(vdivss, XMM), V3(vdivsd, XMM),
        V2(vsqrtps, VEC), V2(vsqrtpd, VEC), V3(vsqrtss, XMM), V3(vsqrtsd, XMM), V2(vrcpps, VEC), V2(vrsqrtps, VEC), V3(vrcpss, XMM), V3(vrsqrtss, XMM),
        V3(vminps, VEC), V3(vminpd, VEC), V3(vmaxps, VEC), V3(vmaxpd, VEC), V3(vminss, XMM), V3(vminsd, XMM), V3(vmaxss, XMM), V3(vmaxsd, XMM),
        V3(vaddsubps, VEC), V3(vaddsubpd, VEC), V3(vhaddps, VEC), V3(vhaddpd, VEC), V3(vhsubps, VEC), V3(vhsubpd, VEC),
        V3(vandps, VEC), V3(vandpd, VEC), V3(vandnps, VEC), V3(vandnpd, VEC), V3(vorps, VEC), V3(vorpd, VEC), V3(vxorps, VEC), V3(vxorpd, VEC),
        V3I(vcmpps, VEC, 5), V3I(vcmppd, VEC, 5), V3I(vcmpss, XMM, 5), V3I(vcmpsd, XMM, 5),
        V3I(vshufps, VEC, 8), V3I(vshufpd, VEC, 4), V3(vunpcklps, VEC), V3(vunpckhps, VEC), V3(vunpcklpd, VEC), V3(vunpckhpd, VEC),
        V3I(vblendps, VEC, 8), V3I(vblendpd, VEC, 4), V4(vblendvps, VEC), V4(vblendvpd, VEC), V3I(vdpps, VEC, 8), V3I(vdppd, XMM, 8),
        V2I(vroundps, VEC, 4), V2I(vroundpd, VEC, 4), V3I(vroundss, XMM, 4), V3I(vroundsd, XMM, 4), V3I(vinsertps, XMM, 8),
        V2I(vpermilps, VEC, 8), V2I(vpermilpd, VEC, 4), V3(vpermilps, VEC), V3(vpermilpd, VEC),
        Y3I(vperm2f128, 8), Y3I(vperm2i128, 8), Y3(vpermd), Y3(vpermps), Y2I(vpermq, 8), Y2I(vpermpd, 8),
        {"vinsertf128 y, y, x, imm", YMM, [](Ctx& c) { c.x.vinsertf128(c.ydst(), c.ydst(), c.xsrc128(), c.imm(1)); }},
        {"vinserti128 y, y, x, imm", YMM, [](Ctx& c) { c.x.vinserti128(c.ydst(), c.ydst(), c.xsrc128(), c.imm(1)); }},
        {"vextractf128 x, y, imm", YMM, [](Ctx& c) { c.x.vextractf128(c.xdst128(), c.ydst(), c.imm(1)); }},
        {"vextracti128 x, y, imm", YMM, [](Ctx& c) { c.x.vextracti128(c.xdst128(), c.ydst(), c.imm(1)); }},
        VFROMX(vbroadcastss), {"vbroadcastsd y, x", YMM, [](Ctx& c) { c.x.vbroadcastsd(c.ydst(), c.xdst128()); }}, VFROMX(vpbroadcastb), VFROMX(vpbroadcastw), VFROMX(vpbroadcastd), VFROMX(vpbroadcastq),
        V2(vcvtdq2ps, VEC), V2(vcvtps2dq, VEC), V2(vcvttps2dq, VEC), VFROMX(vcvtdq2pd), VFROMX(vcvtps2pd), VTOX(vcvtpd2dq), VTOX(vcvttpd2dq), VTOX(vcvtpd2ps),
        VFROMX(vcvtph2ps), {"vcvtps2ph x, v, imm", VEC, [](Ctx& c) { c.x.vcvtps2ph(c.xdst128(), c.xdst(), c.imm(4)); }},
        V3(vcvtss2sd, XMM), V3(vcvtsd2ss, XMM),
        {"vcvtsi2ss x, x, r32", XMM, [](Ctx& c) { c.x.vcvtsi2ss(c.xdst(), c.xdst(), c.src32()); }},
        {"vcvtsi2sd x, x, r64", XMM, [](Ctx& c) { c.x.vcvtsi2sd(c.xdst(), c.xdst(), c.src64()); }},
        {"vcvttss2si r, x; vcvtsi2ss x, x, r", XMM, [](Ctx& c) { c.x.vcvttss2si(c.src32(), c.xdst()); c.x.vcvtsi2ss(c.xdst(), c.xdst(), c.src32()); }},
        {"vcvttsd2si r, x; vcvtsi2sd x, x, r", XMM, [](Ctx& c) { c.x.vcvttsd2si(c.src64(), c.xdst()); c.x.vcvtsi2sd(c.xdst(), c.xdst(), c.src64()); }},
        V3(vpaddb, VEC), V3(vpaddw, VEC), V3(vpaddd, VEC), V3(vpaddq, VEC), V3(vpsubb, VEC), V3(vpsubw, VEC), V3(vpsubd, VEC), V3(vpsubq, VEC),
        V3(vpaddsb, VEC), V3(vpaddsw, VEC), V3(vpsubsb, VEC), V3(vpsubsw, VEC), V3(vpaddusb, VEC), V3(vpaddusw, VEC), V3(vpsubusb, VEC), V3(vpsubusw, VEC),
        V3(vpmullw, VEC), V3(vpmulld, VEC), V3(vpmulhw, VEC), V3(vpmulhuw, VEC), V3(vpmulhrsw, VEC), V3(vpmuludq, VEC), V3(vpmuldq, VEC), V3(vpmaddwd, VEC), V3(vpmaddubsw, VEC),
        V3(vphaddw, VEC), V3(vphaddd, VEC), V3(vphaddsw, VEC), V3(vphsubw, VEC), V3(vphsubd, VEC), V3(vphsubsw, VEC), V2(vphminposuw, XMM),
        V3(vpand, VEC), V3(vpandn, VEC), V3(vpor, VEC), V3(vpxor, VEC),
        V3(vpcmpeqb, VEC), V3(vpcmpeqw, VEC), V3(vpcmpeqd, VEC), V3(vpcmpeqq, VEC), V3(vpcmpgtb, VEC), V3(vpcmpgtw, VEC), V3(vpcmpgtd, VEC), V3(vpcmpgtq, VEC),
        V3(vpminsb, VEC), V3(vpminsw, VEC), V3(vpminsd, VEC), V3(vpmaxsb, VEC), V3(vpmaxsw, VEC), V3(vpmaxsd, VEC),
        V3(vpminub, VEC), V3(vpminuw, VEC), V3(vpminud, VEC), V3(vpmaxub, VEC), V3(vpmaxuw, VEC), V3(vpmaxud, VEC),
        V3(vpavgb, VEC), V3(vpavgw, VEC), V3(vpsadbw, VEC), V3I(vmpsadbw, VEC, 3),
        V2(vpabsb, VEC), V2(vpabsw, VEC), V2(vpabsd, VEC), V3(vpsignb, VEC), V3(vpsignw, VEC), V3(vpsignd, VEC),
        V3(vpshufb, VEC), V2I(vpshufd, VEC, 8), V2I(vpshuflw, VEC, 8), V2I(vpshufhw, VEC, 8), V3I(vpalignr, VEC, 5),
        V3(vpunpcklbw, VEC), V3(vpunpcklwd, VEC), V3(vpunpckldq, VEC), V3(vpunpcklqdq, VEC), V3(vpunpckhbw, VEC), V3(vpunpckhwd, VEC), V3(vpunpckhdq, VEC), V3(vpunpckhqdq, VEC),
        V3(vpacksswb, VEC), V3(vpackssdw, VEC), V3(vpackuswb, VEC), V3(vpackusdw, VEC),
        V3I(vpblendw, VEC, 8), V3I(vpblendd, VEC, 8), V4(vpblendvb, VEC),
        VSHIFT(vpsllw), VSHIFT(vpslld), VSHIFT(vpsllq), VSHIFT(vpsrlw), VSHIFT(vpsrld), VSHIFT(vpsrlq), VSHIFT(vpsraw), VSHIFT(vpsrad),
        V2I(vpslldq, VEC, 4), V2I(vpsrldq, VEC, 4),
        V3(vpsllvd, VEC), V3(vpsllvq, VEC), V3(vpsrlvd, VEC), V3(vpsrlvq, VEC), V3(vpsravd, VEC),
        VFROMX(vpmovzxbw), VFROMX(vpmovzxbd), VFROMX(vpmovzxbq), VFROMX(vpmovzxwd), VFROMX(vpmovzxwq), VFROMX(vpmovzxdq),
        VFROMX(vpmovsxbw), VFROMX(vpmovsxbd), VFROMX(vpmovsxbq), VFROMX(vpmovsxwd), VFROMX(vpmovsxwq), VFROMX(vpmovsxdq),
        {"vpinsrb x, x, r32, imm", XMM, [](Ctx& c) { c.x.vpinsrb(c.xdst(), c.xdst(), c.src32(), c.imm(4)); }},
        {"vpinsrw x, x, r32, imm", XMM, [](Ctx& c) { c.x.vpinsrw(c.xdst(), c.xdst(), c.src32(), c.imm(3)); }},
        {"vpinsrd x, x, r32, imm", XMM, [](Ctx& c) { c.x.vpinsrd(c.xdst(), c.xdst(), c.src32(), c.imm(2)); }},
        {"vpinsrq x, x, r64, imm", XMM, [](Ctx& c) { c.x.vpinsrq(c.xdst(), c.xdst(), c.src64(), c.imm(1)); }},
        {"vpextrd r32, x, imm; vpinsrd x, x, r32, imm", XMM, [](Ctx& c) { c.x.vpextrd(c.src32(), c.xdst(), c.imm(2)); c.x.vpinsrd(c.xdst(), c.xdst(), c.src32(), c.imm(2)); }},
        {"vpextrq r64, x, imm; vpinsrq x, x, r64, imm", XMM, [](Ctx& c) { c.x.vpextrq(c.src64(), c.xdst(), c.imm(1)); c.x.vpinsrq(c.xdst(), c.xdst(), c.src64(), c.imm(1)); }},
        V3(vaesenc, XMM), V3(vaesenclast, XMM), V3(vaesdec, XMM), V3(vaesdeclast, XMM), V2(vaesimc, XMM), V2I(vaeskeygenassist, XMM, 8), V3I(vpclmulqdq, XMM, 8),
        V3(vfmadd132ps, VEC), V3(vfmadd213ps, VEC), V3(vfmadd231ps, VEC), V3(vfmadd132pd, VEC), V3(vfmadd213pd, VEC), V3(vfmadd231pd, VEC),
        V3(vfmadd132ss, XMM), V3(vfmadd213ss, XMM), V3(vfmadd231ss, XMM), V3(vfmadd132sd, XMM), V3(vfmadd213sd, XMM), V3(vfmadd231sd, XMM),
        V3(vfmsub132ps, VEC), V3(vfmsub213ps, VEC), V3(vfmsub231ps, VEC), V3(vfmsub132pd, VEC), V3(vfmsub213pd, VEC), V3(vfmsub231pd, VEC),
        V3(vfmsub132ss, XMM), V3(vfmsub213ss, XMM), V3(vfmsub231ss, XMM), V3(vfmsub132sd, XMM), V3(vfmsub213sd, XMM), V3(vfmsub231sd, XMM),
        V3(vfnmadd132ps, VEC), V3(vfnmadd213ps, VEC), V3(vfnmadd231ps, VEC), V3(vfnmadd132pd, VEC), V3(vfnmadd213pd, VEC), V3(vfnmadd231pd, VEC),
        V3(vfnmadd132ss, XMM), V3(vfnmadd213ss, XMM), V3(vfnmadd231ss, XMM), V3(vfnmadd132sd, XMM), V3(vfnmadd213sd, XMM), V3(vfnmadd231sd, XMM),
        V3(vfnmsub132ps, VEC), V3(vfnmsub213ps, VEC), V3(vfnmsub231ps, VEC), V3(vfnmsub132pd, VEC), V3(vfnmsub213pd, VEC), V3(vfnmsub231pd, VEC),
        V3(vfnmsub132ss, XMM), V3(vfnmsub213ss, XMM), V3(vfnmsub231ss, XMM), V3(vfnmsub132sd, XMM), V3(vfnmsub213sd, XMM), V3(vfnmsub231sd, XMM),
        V3(vfmaddsub132ps, VEC), V3(vfmaddsub213ps, VEC), V3(vfmaddsub231ps, VEC), V3(vfmaddsub132pd, VEC), V3(vfmaddsub213pd, VEC), V3(vfmaddsub231pd, VEC),
        V3(vfmsubadd132ps, VEC), V3(vfmsubadd213ps, VEC), V3(vfmsubadd231ps, VEC), V3(vfmsubadd132pd, VEC), V3(vfmsubadd213pd, VEC), V3(vfmsubadd231pd, VEC),
#undef X2
#undef X2I
#undef XU
#undef XUI
#undef XSHIFT
#undef V3
#undef V3I
#undef V2
#undef V2I
#undef V4
#undef VSHIFT
#undef VFROMX
#undef VTOX
#undef Y3
#undef Y3I
#undef Y2I
    };
    return list;
}

double measure(const Test& test, const Width& width, bool throughput) {
    void* data = malloc(1024 * 1024);
    ASSERT(data);
    Xbyak::CodeGenerator x(1024 * 1024, data);
    Ctx ctx{x, width};
    Xbyak::ClearError();
    for (int i = 0; i < iterations; i++) {
        ctx.cur = throughput ? i % width.dsts.size() : 0;
        test.emit(ctx);
    }
    ASSERT_MSG(Xbyak::GetError() == 0, "xbyak failed to encode %s %s: %s", test.name, width.name, Xbyak::ConvertErrorToString(Xbyak::GetError()));

    u8* end = x.getCurr<u8*>();
    end[0] = 0x0F;
    end[1] = 0x0B;

    Assembler& as = g_rec->getAssembler();
    Label loop;
    Label done;
    double (*func)() = (decltype(func))as.GetCursorPointer();
    as.ADDI(biscuit::sp, biscuit::sp, -(int)sizeof(Frame));
    as.SD(ra, offsetof(Frame, ra), biscuit::sp);
    for (size_t i = 0; i < callee_saved.size(); i++) {
        as.SD(callee_saved[i], offsetof(Frame, saved) + i * sizeof(u64), biscuit::sp);
    }
    for (int ref = X86_REF_RAX; ref <= X86_REF_R15; ref++) {
        u64 value = ref == X86_REF_RAX ? 0x3f3f3f3f3f3f3f3full : ref == X86_REF_RDX ? 0 : 0x8181818181818181ull;
        as.LI(Recompiler::allocatedGPR((x86_ref_e)ref), value);
    }
    for (x86_ref_e ref : {X86_REF_CF, X86_REF_ZF, X86_REF_SF, X86_REF_OF}) {
        as.MV(Recompiler::allocatedGPR(ref), x0);
    }
    as.VSETVLI(tmp, x0, SEW::E64, LMUL::M1);
    as.LI(tmp, 0x3FC000003FC00000ull);
    for (int ref = X86_REF_XMM0; ref <= X86_REF_XMM15; ref++) {
        as.VMV(Recompiler::allocatedXMM((x86_ref_e)ref), tmp);
    }
    as.LI(tmp, loops);
    as.SD(tmp, offsetof(Frame, counter), biscuit::sp);
    as.RDCYCLE(tmp);
    as.SD(tmp, offsetof(Frame, cycles), biscuit::sp);
    while ((u64)as.GetCursorPointer() & 63) {
        as.NOP();
    }
    as.Bind(&loop);
    compile_sequence((u64)data);
    as.LD(tmp, offsetof(Frame, counter), biscuit::sp);
    as.ADDI(tmp, tmp, -1);
    as.SD(tmp, offsetof(Frame, counter), biscuit::sp);
    as.BEQZ(tmp, &done);
    as.J(&loop);
    as.Bind(&done);
    as.RDCYCLE(tmp);
    as.LD(tmp2, offsetof(Frame, cycles), biscuit::sp);
    as.SUB(tmp, tmp, tmp2);
    as.LI(tmp2, iterations * loops);
    as.FCVT_D_L(fa0, tmp);
    as.FCVT_D_L(fa1, tmp2);
    as.FDIV_D(fa0, fa0, fa1);
    as.LD(ra, offsetof(Frame, ra), biscuit::sp);
    for (size_t i = 0; i < callee_saved.size(); i++) {
        as.LD(callee_saved[i], offsetof(Frame, saved) + i * sizeof(u64), biscuit::sp);
    }
    as.ADDI(biscuit::sp, biscuit::sp, (int)sizeof(Frame));
    as.RET();
    __riscv_flush_icache((void*)func, as.GetCursorPointer(), 0);
    for (int i = 0; i < warmup_runs; i++) {
        func();
    }
    std::vector<double> samples(runs);
    for (double& sample : samples) {
        sample = func();
    }
    std::sort(samples.begin(), samples.end());
    int trim = runs / 5;
    double cycles = 0;
    for (int i = trim; i < runs - trim; i++) {
        cycles += samples[i];
    }
    cycles /= runs - 2 * trim;
    as.SetCursorPointer((u8*)func);
    free(data);
    return cycles;
}

double rounded(double value) {
    return std::round(value * 1000.0) / 1000.0;
}

std::string cpu_model() {
    std::ifstream cpuinfo("/proc/cpuinfo");
    std::string line;
    while (std::getline(cpuinfo, line)) {
        size_t colon = line.find(':');
        if (line.starts_with("model name") && colon != std::string::npos) {
            return line.substr(line.find_first_not_of(" \t", colon + 1));
        }
    }
    return "unknown";
}

std::string model_filename() {
    std::string name;
    for (char c : cpu_model()) {
        if (isalnum((unsigned char)c)) {
            name += tolower(c);
        } else if (!name.empty() && name.back() != '_') {
            name += '_';
        }
    }
    while (!name.empty() && name.back() == '_') {
        name.pop_back();
    }
    return name + ".json";
}

nlohmann::ordered_json run_all() {
    nlohmann::ordered_json results;
    for (const Test& test : tests()) {
        for (const Width& width : widths()) {
            if (!(test.widths & width.flag)) {
                continue;
            }
            nlohmann::ordered_json& entry = results[test.name][width.name];
            entry["latency"] = rounded(measure(test, width, false));
            if (!test.no_throughput) {
                entry["throughput"] = rounded(measure(test, width, true));
            }
        }
    }

    nlohmann::ordered_json json;
    json["schema"] = 1;
    json["commit"] = g_git_hash;
    json["model"] = cpu_model();
    json["results"] = results;
    return json;
}

void start_cycle_counter() {
    perf_event_attr attr{};
    attr.type = PERF_TYPE_HARDWARE;
    attr.size = sizeof(attr);
    attr.config = PERF_COUNT_HW_CPU_CYCLES;
    attr.exclude_kernel = 1;
    attr.exclude_hv = 1;
    int fd = syscall(SYS_perf_event_open, &attr, 0, -1, -1, 0);
    if (fd < 0) {
        ERROR("perf_event_open failed: %s", strerror(errno));
    }
}

bool report_changes(const nlohmann::json& old, const nlohmann::json& fresh) {
    bool changed = false;
    for (auto& [test, widths] : old.items()) {
        for (auto& [width, entry] : widths.items()) {
            if (!fresh.contains(test) || !fresh[test].contains(width)) {
                printf("removed      %-30s %s\n", test.c_str(), width.c_str());
                changed = true;
                continue;
            }
            for (const char* metric : {"latency", "throughput"}) {
                if (!entry.contains(metric) || !fresh[test][width].contains(metric)) {
                    continue;
                }
                double before = entry[metric];
                double after = fresh[test][width][metric];
                double delta = std::abs(after - before);
                if (delta < 0.05 || delta < 0.05 * before) {
                    continue;
                }
                printf("%-12s %-30s %-10s %-10s %8.2f -> %8.2f  %+.1f%%\n", after > before ? "regression" : "improvement", test.c_str(),
                       width.c_str(), metric, before, after, (after - before) / before * 100.0);
                changed = true;
            }
        }
    }
    for (auto& [test, widths] : fresh.items()) {
        for (auto& [width, entry] : widths.items()) {
            if (!old.contains(test) || !old[test].contains(width)) {
                printf("new          %-30s %s\n", test.c_str(), width.c_str());
                changed = true;
            }
        }
    }
    return changed;
}

int main(int argc, char** argv) {
    start_cycle_counter();
    Config::initialize();
    initialize_globals();
    g_process_globals.initialize();
    g_config.inline_syscalls = false;
    g_config.scan_ahead_multi = false;
    g_config.protect_pages = false;
    g_config.max_block_size = -1ull;
    g_rec = ThreadState::Create()->recompiler;

    nlohmann::ordered_json json = run_all();
    std::filesystem::create_directory("latencies");
    std::string path = "latencies/" + model_filename();
    std::string baseline = argc > 1 ? argv[1] : path;
    if (std::filesystem::exists(baseline)) {
        std::ifstream old(baseline);
        if (!report_changes(nlohmann::json::parse(old)["results"], json["results"])) {
            printf("Results are the same\n");
            return 0;
        }
    }
    std::ofstream out(path);
    out << json.dump(4) << '\n';
}
