#pragma once

#include "felix86/common/log.hpp"
#include "felix86/common/x86.hpp"
#include "felix86/frontend/instruction.hpp"
#include "felix86/hle/filesystem.hpp"
#include "felix86/ir/function_cache.hpp"

struct Config {
    std::filesystem::path rootfs_path;
    std::filesystem::path executable_path;
    bool testing = false;
    bool optimize = false;
    bool print_blocks = false;
    bool use_interpreter = false;
    u64 base_address = 0;
    u64 brk_base_address = 0;
    std::vector<std::string> argv;
    std::vector<std::string> envp;
};

struct Emulator {
    Emulator(const Config& config) : fs(config.rootfs_path), config(config) {
        if (!fs.Good()) {
            ERROR("Failed to initialize filesystem");
        }
    }

    ~Emulator() = default;

    ThreadState& GetThreadState() {
        return state;
    }

    Filesystem& GetFilesystem() {
        return fs;
    }

    Config& GetConfig() {
        return config;
    }

    u64 GetGpr(x86_ref_e ref) const {
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

    bool GetFlag(x86_ref_e flag) const {
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

    FpReg GetFpReg(x86_ref_e ref) const {
        if (ref < X86_REF_ST0 || ref > X86_REF_ST7) {
            ERROR("Invalid FP register reference: %d", ref);
            return {};
        }

        return state.fp[ref - X86_REF_ST0];
    }

    void SetFpReg(x86_ref_e ref, const FpReg& value) {
        if (ref < X86_REF_ST0 || ref > X86_REF_ST7) {
            ERROR("Invalid FP register reference: %d", ref);
            return;
        }

        state.fp[ref - X86_REF_ST0] = value;
    }

    XmmReg GetXmmReg(x86_ref_e ref) const {
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

    u64 GetRip() const {
        return state.rip;
    }

    void SetRip(u64 value) {
        state.rip = value;
    }

    u64 GetGSBase() const {
        return state.gsbase;
    }

    void SetGSBase(u64 value) {
        state.gsbase = value;
    }

    u64 GetFSBase() const {
        return state.fsbase;
    }

    void SetFSBase(u64 value) {
        state.fsbase = value;
    }

    void Run();

private:
    FunctionCache cache;
    ThreadState state;
    Filesystem fs;
    Config config;
};
