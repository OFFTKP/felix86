#pragma once

#include "felix86/backend/backend.hpp"
#include "felix86/common/elf.hpp"
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
    std::vector<std::string> argv;
    std::vector<std::string> envp;
};

struct Emulator {
    Emulator(const Config& config) : thread_state(), backend(thread_state), config(config) {
        fs.LoadRootFS(config.rootfs_path);
        fs.LoadExecutable(config.executable_path);
        setupStack();
        SetRip((u64)fs.GetEntrypoint());
    }

    ~Emulator() = default;

    ThreadState& GetThreadState() {
        return thread_state;
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

        return thread_state.gprs[ref - X86_REF_RAX];
    }

    void SetGpr(x86_ref_e ref, u64 value) {
        if (ref < X86_REF_RAX || ref > X86_REF_R15) {
            ERROR("Invalid GPR reference: %d", ref);
        }

        thread_state.gprs[ref - X86_REF_RAX] = value;
    }

    bool GetFlag(x86_ref_e flag) const {
        switch (flag) {
        case X86_REF_CF:
            return thread_state.cf;
        case X86_REF_PF:
            return thread_state.pf;
        case X86_REF_AF:
            return thread_state.af;
        case X86_REF_ZF:
            return thread_state.zf;
        case X86_REF_SF:
            return thread_state.sf;
        case X86_REF_OF:
            return thread_state.of;
        default:
            ERROR("Invalid flag reference: %d", flag);
            return false;
        }
    }

    void SetFlag(x86_ref_e flag, bool value) {
        switch (flag) {
        case X86_REF_CF:
            thread_state.cf = value;
            break;
        case X86_REF_PF:
            thread_state.pf = value;
            break;
        case X86_REF_AF:
            thread_state.af = value;
            break;
        case X86_REF_ZF:
            thread_state.zf = value;
            break;
        case X86_REF_SF:
            thread_state.sf = value;
            break;
        case X86_REF_OF:
            thread_state.of = value;
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

        return thread_state.fp[ref - X86_REF_ST0];
    }

    void SetFpReg(x86_ref_e ref, const FpReg& value) {
        if (ref < X86_REF_ST0 || ref > X86_REF_ST7) {
            ERROR("Invalid FP register reference: %d", ref);
            return;
        }

        thread_state.fp[ref - X86_REF_ST0] = value;
    }

    XmmReg GetXmmReg(x86_ref_e ref) const {
        if (ref < X86_REF_XMM0 || ref > X86_REF_XMM15) {
            ERROR("Invalid XMM register reference: %d", ref);
            return {};
        }

        return thread_state.xmm[ref - X86_REF_XMM0];
    }

    void SetXmmReg(x86_ref_e ref, const XmmReg& value) {
        if (ref < X86_REF_XMM0 || ref > X86_REF_XMM15) {
            ERROR("Invalid XMM register reference: %d", ref);
            return;
        }

        thread_state.xmm[ref - X86_REF_XMM0] = value;
    }

    u64 GetRip() const {
        return thread_state.rip;
    }

    void SetRip(u64 value) {
        thread_state.rip = value;
    }

    u64 GetGSBase() const {
        return thread_state.gsbase;
    }

    void SetGSBase(u64 value) {
        thread_state.gsbase = value;
    }

    u64 GetFSBase() const {
        return thread_state.fsbase;
    }

    void SetFSBase(u64 value) {
        thread_state.fsbase = value;
    }

    void Run();

private:
    void setupStack();

    ThreadState thread_state;
    Backend backend;
    FunctionCache function_cache;
    Filesystem fs;
    Config config;
};
