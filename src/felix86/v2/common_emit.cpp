#include "felix86/common/config.hpp"
#include "felix86/common/global.hpp"
#include "felix86/common/log.hpp"
#include "felix86/common/state.hpp"
#include "felix86/common/utility.hpp"
#include "felix86/v2/allocations.hpp"
#include "felix86/v2/common_emit.hpp"

void CommonEmit::writebackGuestState(biscuit::Assembler& as, ScratchAllocator& scratches) {
    biscuit::GPR rip = Allocations::RIP();
    as.SD(rip, offsetof(ThreadState, ctx.rip), Allocations::threadStatePointer());

    for (int i = 0; i < 16; i++) {
        x86_ref_e ref = (x86_ref_e)(X86_REF_RAX + i);
        as.SD(Allocations::allocatedGPR(ref), offsetof(ThreadState, ctx.gprs) + i * sizeof(u64), Allocations::threadStatePointer());
    }

    biscuit::GPR address = scratches.scratch();

    if (Extensions::VLEN == 256 && g_config.group_loadstore) {
        static_assert(sizeof(XmmReg) == 256 / 8);
        as.VSETVLI(address, x0, SEW::E64, LMUL::M8);
        biscuit::Vec xmm0 = Allocations::allocatedXMM(X86_REF_XMM0);
        biscuit::Vec xmm8 = Allocations::allocatedXMM(X86_REF_XMM8);
        as.ADDI(address, Allocations::threadStatePointer(), offsetof(ThreadState, ctx.xmm) + 0 * sizeof(XmmReg));
        as.VSE64(xmm0, address);
        as.ADDI(address, Allocations::threadStatePointer(), offsetof(ThreadState, ctx.xmm) + 8 * sizeof(XmmReg));
        as.VSE64(xmm8, address);
    } else {
        int vlen;
        if (Extensions::VLEN >= 256) {
            vlen = 4;
        } else {
            ASSERT_MSG(g_config.no_avx && g_config.no_avx2, "AVX not disabled on VLEN=128 machine?");
            vlen = 2;
        }
        as.VSETIVLI(x0, vlen, SEW::E64, LMUL::M1, VTA::No, VMA::No);
        for (int i = 0; i < 16; i++) {
            biscuit::Vec vec = Allocations::allocatedXMM((x86_ref_e)(X86_REF_XMM0 + i));
            as.ADDI(address, Allocations::threadStatePointer(), offsetof(ThreadState, ctx.xmm) + i * sizeof(XmmReg));
            as.VSE64(vec, address);
        }
    }

    scratches.popScratch();

    as.SB(Allocations::CF(), offsetof(ThreadState, ctx.cf), Allocations::threadStatePointer());
    as.SB(Allocations::ZF(), offsetof(ThreadState, ctx.zf), Allocations::threadStatePointer());
    as.SB(Allocations::SF(), offsetof(ThreadState, ctx.sf), Allocations::threadStatePointer());
    as.SB(Allocations::OF(), offsetof(ThreadState, ctx.of), Allocations::threadStatePointer());
}

void CommonEmit::restoreGuestState(biscuit::Assembler& as, ScratchAllocator& scratches, bool sse_rounding_mode) {
    biscuit::GPR rip = Allocations::RIP();
    as.LD(rip, offsetof(ThreadState, ctx.rip), Allocations::threadStatePointer());

    for (int i = 0; i < 16; i++) {
        x86_ref_e ref = (x86_ref_e)(X86_REF_RAX + i);
        as.LD(Allocations::allocatedGPR(ref), offsetof(ThreadState, ctx.gprs) + i * sizeof(u64), Allocations::threadStatePointer());
    }

    biscuit::GPR address = scratches.scratch();

    if (Extensions::VLEN == 256 && g_config.group_loadstore) {
        static_assert(sizeof(XmmReg) == 256 / 8);
        as.VSETVLI(address, x0, SEW::E64, LMUL::M8);
        biscuit::Vec xmm0 = Allocations::allocatedXMM(X86_REF_XMM0);
        biscuit::Vec xmm8 = Allocations::allocatedXMM(X86_REF_XMM8);
        as.ADDI(address, Allocations::threadStatePointer(), offsetof(ThreadState, ctx.xmm) + 0 * sizeof(XmmReg));
        as.VLE64(xmm0, address);
        as.ADDI(address, Allocations::threadStatePointer(), offsetof(ThreadState, ctx.xmm) + 8 * sizeof(XmmReg));
        as.VLE64(xmm8, address);
    } else {
        as.VSETIVLI(x0, Extensions::VLEN >= 256 ? 4 : 2, SEW::E64, LMUL::M1, VTA::No, VMA::No);
        for (int i = 0; i < 16; i++) {
            biscuit::Vec vec = Allocations::allocatedXMM((x86_ref_e)(X86_REF_XMM0 + i));
            as.ADDI(address, Allocations::threadStatePointer(), offsetof(ThreadState, ctx.xmm) + sizeof(XmmReg) * i);
            as.VLE64(vec, address);
        }
    }

    scratches.popScratch();

    as.LBU(Allocations::CF(), offsetof(ThreadState, ctx.cf), Allocations::threadStatePointer());
    as.LBU(Allocations::ZF(), offsetof(ThreadState, ctx.zf), Allocations::threadStatePointer());
    as.LBU(Allocations::SF(), offsetof(ThreadState, ctx.sf), Allocations::threadStatePointer());
    as.LBU(Allocations::OF(), offsetof(ThreadState, ctx.of), Allocations::threadStatePointer());

    // Restore the rounding mode
    loadRoundingMode(as, scratches, sse_rounding_mode);

    as.FENCETSO();
}

void CommonEmit::loadRoundingMode(biscuit::Assembler& as, ScratchAllocator& scratches, bool sse) {
    biscuit::GPR rm = scratches.scratch();
    as.LBU(rm, sse ? offsetof(ThreadState, ctx.rmode_sse) : offsetof(ThreadState, ctx.rmode_x87), Allocations::threadStatePointer());
    as.FSRM(x0, rm);
    scratches.popScratch();
}

void CommonEmit::addressCacheLookup(biscuit::Assembler& as, ScratchAllocator& scratches, biscuit::GPR guest_address,
                                    void on_hit(biscuit::Assembler&, biscuit::GPR), bool mode32, bool use_ra) {
    if (!use_ra) {
        // Waste the ra scratch
        (void)scratches.scratch();
    }
    biscuit::GPR host_address = use_ra ? x1 : scratches.scratch();
    biscuit::GPR temp = scratches.scratch();
    biscuit::GPR temp2 = scratches.scratch();
    biscuit::GPR temp3;
    biscuit::GPR tagged_address = guest_address;
    if (mode32) {
        temp3 = scratches.scratch();
        as.NOT(temp3, guest_address);
        tagged_address = temp3;
    }
    biscuit::Label not_equal;
    i32 lo12 = 0;
    u64 offset = -address_cache_offset;
    const auto hi20 = static_cast<int32_t>(((static_cast<uint32_t>(offset) + 0x800) >> 12) & 0xFFFFF);
    lo12 = static_cast<int32_t>(offset << 20) >> 20;
    const bool lo12overflow = lo12 + 8 > 2047;
    as.LUI(temp, hi20);
    if (lo12overflow) {
        as.ADDI(temp, temp, lo12);
        lo12 = 0;
    }
    as.ADD(temp, temp, Allocations::threadStatePointer());
    const i32 offset_guest = lo12 + 8;
    const i32 offset_host = lo12;
    as.SLLI(temp2, guest_address, 64 - address_cache_bits);
    // Multiply by 16, which is size of each address cache entry
    as.SRLI(temp2, temp2, 64 - address_cache_bits - 4);
    as.ADD(temp, temp, temp2);
    // Load even if branch fails is slightly better for fusion
    as.LD(host_address, offset_host, temp);
    as.LD(temp2, offset_guest, temp);
    as.BNE(temp2, tagged_address, &not_equal);
    as.MV(t5, x0); // zero out t5, see invalidate_caller_thunk
    on_hit(as, host_address);
    as.Bind(&not_equal);
    scratches.popScratch();
    scratches.popScratch();
    if (!use_ra) {
        scratches.popScratch();
        scratches.popScratch();
    }
    if (mode32) {
        scratches.popScratch();
    }
}

void CommonEmit::call(biscuit::Assembler& as, u64 target, bool relocatable) {
    static_assert(ScratchAllocator::isScratch(t4));
    i64 offset = target - (u64)as.GetCursorPointer();
    if (IsValidJTypeImm(offset) && !relocatable) {
        as.JAL(offset);
    } else if (IsValid2GBImm(offset) && !relocatable) {
        const auto hi20 = static_cast<int32_t>(((static_cast<uint32_t>(offset) + 0x800) >> 12) & 0xFFFFF);
        const auto lo12 = static_cast<int32_t>(offset << 20) >> 20;
        as.AUIPC(t4, hi20);
        as.JALR(ra, lo12, t4);
    } else {
        as.LI(t4, target);
        as.JALR(t4);
    }
}
