#pragma once

#include "felix86/common/log.hpp"
#include "felix86/common/x86.hpp"
#include "felix86/frontend/instruction.hpp"
#include "felix86/ir/block.hpp"
#include "felix86/ir/function_cache.hpp"

struct Config {
	bool testing;
	bool optimize;
	bool print_blocks;
	bool use_interpreter;
	u64 base_address;
	bool verify;
	u64 brk_base_address;
};

struct Emulator {
    u64 GetGpr(x86_ref_e ref) {
        if (ref < X86_REF_RAX || ref > X86_REF_R15) {
            ERROR("Invalid GPR reference: %d", ref);
            return 0;
        }

        return state.gprs[ref - X86_REF_RAX];
    }

    void SetGpr(x86_ref_e ref, u64 value) {
        if (ref < X86_REF_RAX || ref > X86_REF_R15) {
            ERROR("Invalid GPR reference: %d", ref);
        }

        state.gprs[ref - X86_REF_RAX] = value;
    }

    bool GetFlag(x86_ref_e flag) {
        switch (flag) {
            case X86_REF_CF:
                return state.cf;
            case X86_REF_PF:
                return state.pf;
            case X86_REF_AF:
                return state.af;
            case X86_REF_ZF:
                return state.zf;
            case X86_REF_SF:
                return state.sf;
            case X86_REF_OF:
                return state.of;
            default:
                ERROR("Invalid flag reference: %d", flag);
                return false;
        }
    }

    void SetFlag(x86_ref_e flag, bool value) {
        switch (flag) {
            case X86_REF_CF:
                state.cf = value;
                break;
            case X86_REF_PF:
                state.pf = value;
                break;
            case X86_REF_AF:
                state.af = value;
                break;
            case X86_REF_ZF:
                state.zf = value;
                break;
            case X86_REF_SF:
                state.sf = value;
                break;
            case X86_REF_OF:
                state.of = value;
                break;
            default:
                ERROR("Invalid flag reference: %d", flag);
        }
    }

    FpReg GetFpReg(x86_ref_e ref) {
        if (ref < X86_REF_MM0 || ref > X86_REF_MM7) {
            ERROR("Invalid FP register reference: %d", ref);
            return {};
        }

        return state.fp[ref - X86_REF_MM0];
    }

    void SetFpReg(x86_ref_e ref, const FpReg& value) {
        if (ref < X86_REF_MM0 || ref > X86_REF_MM7) {
            ERROR("Invalid FP register reference: %d", ref);
            return;
        }

        state.fp[ref - X86_REF_MM0] = value;
    }

    XmmReg GetXmmReg(x86_ref_e ref) {
        if (ref < X86_REF_XMM0 || ref > X86_REF_XMM15) {
            ERROR("Invalid XMM register reference: %d", ref);
            return {};
        }

        return state.xmm[ref - X86_REF_XMM0];
    }

    void SetXmmReg(x86_ref_e ref, const XmmReg& value) {
        if (ref < X86_REF_XMM0 || ref > X86_REF_XMM15) {
            ERROR("Invalid XMM register reference: %d", ref);
            return;
        }

        state.xmm[ref - X86_REF_XMM0] = value;
    }

    u64 GetRip() {
        return state.rip;
    }

    void SetRip(u64 value) {
        state.rip = value;
    }

    u64 GetGSBase() {
        return state.gsbase;
    }

    void SetGSBase(u64 value) {
        state.gsbase = value;
    }

    u64 GetFSBase() {
        return state.fsbase;
    }

    void SetFSBase(u64 value) {
        state.fsbase = value;
    }

    void Run();

private:
    FunctionCache cache;
    ThreadState state;
    Config config;
};
