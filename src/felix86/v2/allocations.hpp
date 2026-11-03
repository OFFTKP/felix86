#pragma once

#include "biscuit/registers.hpp"
#include "felix86/common/log.hpp"
#include "felix86/common/x86_ref.hpp"

// !!! --- WARN: If any allocations are changed here, change them in the docs as well
struct Allocations {
    static constexpr biscuit::GPR RIP() {
        return biscuit::s11;
    }
    static constexpr biscuit::GPR RAX() {
        return biscuit::x5;
    }
    static constexpr biscuit::GPR RCX() {
        return biscuit::x26;
    }
    static constexpr biscuit::GPR RDX() {
        return biscuit::x12;
    }
    static constexpr biscuit::GPR RBX() {
        return biscuit::x8;
    }
    static constexpr biscuit::GPR RSP() {
        return biscuit::x9;
    }
    static constexpr biscuit::GPR RBP() {
        return biscuit::x18;
    }
    static constexpr biscuit::GPR RSI() {
        return biscuit::x11;
    }
    static constexpr biscuit::GPR RDI() {
        return biscuit::x10;
    }
    static constexpr biscuit::GPR R8() {
        return biscuit::x14;
    }
    static constexpr biscuit::GPR R9() {
        return biscuit::x15;
    }
    static constexpr biscuit::GPR R10() {
        return biscuit::x13;
    }
    static constexpr biscuit::GPR R11() {
        return biscuit::x16;
    }
    static constexpr biscuit::GPR R12() {
        return biscuit::x17;
    }
    static constexpr biscuit::GPR R13() {
        return biscuit::x22;
    }
    static constexpr biscuit::GPR R14() {
        return biscuit::x19;
    }
    static constexpr biscuit::GPR R15() {
        return biscuit::x20;
    }
    static constexpr biscuit::GPR CF() {
        return biscuit::x21;
    }
    static constexpr biscuit::GPR ZF() {
        return biscuit::x23;
    }
    static constexpr biscuit::GPR SF() {
        return biscuit::x24;
    }
    static constexpr biscuit::GPR OF() {
        return biscuit::x25;
    }
    static constexpr biscuit::Vec XMM0() {
        // Important to start on a vector register divisible by eight so maximum vector grouping works when we save/restore the entire state,
        // but also not use v0 because that's used for the mask register
        return biscuit::v16;
    }
    static constexpr biscuit::Vec XMM1() {
        return biscuit::v17;
    }
    static constexpr biscuit::Vec XMM2() {
        return biscuit::v18;
    }
    static constexpr biscuit::Vec XMM3() {
        return biscuit::v19;
    }
    static constexpr biscuit::Vec XMM4() {
        return biscuit::v20;
    }
    static constexpr biscuit::Vec XMM5() {
        return biscuit::v21;
    }
    static constexpr biscuit::Vec XMM6() {
        return biscuit::v22;
    }
    static constexpr biscuit::Vec XMM7() {
        return biscuit::v23;
    }
    static constexpr biscuit::Vec XMM8() {
        return biscuit::v24;
    }
    static constexpr biscuit::Vec XMM9() {
        return biscuit::v25;
    }
    static constexpr biscuit::Vec XMM10() {
        return biscuit::v26;
    }
    static constexpr biscuit::Vec XMM11() {
        return biscuit::v27;
    }
    static constexpr biscuit::Vec XMM12() {
        return biscuit::v28;
    }
    static constexpr biscuit::Vec XMM13() {
        return biscuit::v29;
    }
    static constexpr biscuit::Vec XMM14() {
        return biscuit::v30;
    }
    static constexpr biscuit::Vec XMM15() {
        return biscuit::v31;
    }

    static constexpr biscuit::GPR allocatedGPR(x86_ref_e reg) {
        switch (reg) {
        case X86_REF_RIP: {
            return RIP();
        }
        case X86_REF_RAX: {
            return RAX();
        }
        case X86_REF_RCX: {
            return RCX();
        }
        case X86_REF_RDX: {
            return RDX();
        }
        case X86_REF_RBX: {
            return RBX();
        }
        case X86_REF_RSP: {
            return RSP();
        }
        case X86_REF_RBP: {
            return RBP();
        }
        case X86_REF_RSI: {
            return RSI();
        }
        case X86_REF_RDI: {
            return RDI();
        }
        case X86_REF_R8: {
            return R8();
        }
        case X86_REF_R9: {
            return R9();
        }
        case X86_REF_R10: {
            return R10();
        }
        case X86_REF_R11: {
            return R11();
        }
        case X86_REF_R12: {
            return R12();
        }
        case X86_REF_R13: {
            return R13();
        }
        case X86_REF_R14: {
            return R14();
        }
        case X86_REF_R15: {
            return R15();
        }
        case X86_REF_CF: {
            return CF();
        }
        case X86_REF_ZF: {
            return ZF();
        }
        case X86_REF_SF: {
            return SF();
        }
        case X86_REF_OF: {
            return OF();
        }
        default: {
            UNREACHABLE();
        }
        }
    }

    static constexpr biscuit::Vec allocatedXMM(x86_ref_e reg) {
        switch (reg) {
        case X86_REF_XMM0: {
            return XMM0();
        }
        case X86_REF_XMM1: {
            return XMM1();
        }
        case X86_REF_XMM2: {
            return XMM2();
        }
        case X86_REF_XMM3: {
            return XMM3();
        }
        case X86_REF_XMM4: {
            return XMM4();
        }
        case X86_REF_XMM5: {
            return XMM5();
        }
        case X86_REF_XMM6: {
            return XMM6();
        }
        case X86_REF_XMM7: {
            return XMM7();
        }
        case X86_REF_XMM8: {
            return XMM8();
        }
        case X86_REF_XMM9: {
            return XMM9();
        }
        case X86_REF_XMM10: {
            return XMM10();
        }
        case X86_REF_XMM11: {
            return XMM11();
        }
        case X86_REF_XMM12: {
            return XMM12();
        }
        case X86_REF_XMM13: {
            return XMM13();
        }
        case X86_REF_XMM14: {
            return XMM14();
        }
        case X86_REF_XMM15: {
            return XMM15();
        }
        default: {
            UNREACHABLE();
        }
        }
    }
};
