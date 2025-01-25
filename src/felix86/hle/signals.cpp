#include <array>
#include "felix86/emulator.hpp"
#include "felix86/hle/filesystem.hpp"
#include "felix86/hle/signals.hpp"

std::array<RegisteredSignal, 64> handlers{};

bool is_in_jit_code(uintptr_t ptr) {
    uintptr_t start = g_emulator->GetAssembler().GetCodeBuffer().GetOffsetAddress(0);
    uintptr_t end = g_emulator->GetAssembler().GetCodeBuffer().GetCursorAddress();
    return ptr >= start && ptr < end;
}

#ifndef REG_PC
#define REG_PC 0
#endif

#if defined(__x86_64__)
void signal_handler(int sig, siginfo_t* info, void* ctx) {
    UNREACHABLE();
}
#elif defined(__riscv)
void signal_handler(int sig, siginfo_t* info, void* ctx) {
    ucontext_t* context = (ucontext_t*)ctx;
    uintptr_t pc = context->uc_mcontext.__gregs[REG_PC];

    ASSERT(Recompiler::threadStatePointer() == x27);
    Recompiler& recompiler = g_emulator->GetRecompiler();

    switch (sig) {
    case SIGBUS: {
        switch (info->si_code) {
        case BUS_ADRALN: {
            ASSERT(is_in_jit_code(pc));
            // Go back one instruction, we are going to overwrite it with vsetivli.
            // It's guaranteed to be either a vsetivli or a nop.
            context->uc_mcontext.__gregs[REG_PC] = pc - 4;

            Assembler& as = recompiler.getAssembler();
            VectorMemoryAccess vma = recompiler.getVectorMemoryAccess(pc - 4);

            ptrdiff_t cursor = as.GetCodeBuffer().GetCursorOffset();
            as.RewindBuffer(pc - as.GetCodeBuffer().GetOffsetAddress(0) - 4); // go to vsetivli
            switch (vma.sew) {
            case SEW::E64: {
                as.VSETIVLI(x0, vma.len * 8, SEW::E8);
                if (vma.load) {
                    as.VLE8(vma.dest, vma.address);
                } else {
                    as.VSE8(vma.dest, vma.address);
                }
                as.VSETIVLI(x0, vma.len, vma.sew);
                break;
            }
            case SEW::E32: {
                as.VSETIVLI(x0, vma.len * 4, SEW::E8);
                if (vma.load) {
                    as.VLE8(vma.dest, vma.address);
                } else {
                    as.VSE8(vma.dest, vma.address);
                }
                as.VSETIVLI(x0, vma.len, vma.sew);
                break;
            }
            default: {
                UNREACHABLE();
                break;
            }
            }
            as.AdvanceBuffer(cursor);
            flush_icache();
            break;
        }
        default: {
            ERROR("Unhandled SIGBUS code: %d", info->si_code);
            break;
        }
        }
        break;
    }
    case SIGILL: {
        bool found = false;
        if (is_in_jit_code(pc)) {
            // Search to see if it is our breakpoint
            // Note the we don't use EBREAK as gdb refuses to continue when it hits that if it doesn't have a breakpoint,
            // and also refuses to call our signal handler.
            // So we use illegal instructions to emulate breakpoints.
            for (auto& bp : g_breakpoints) {
                for (u64 location : bp.second) {
                    if (location == pc) {
                        // Skip the breakpoint and continue
                        printf("Guest breakpoint %016lx hit at %016lx\n", bp.first, pc);
                        context->uc_mcontext.__gregs[REG_PC] = pc + 4;
                        found = true;
                        break;
                    }
                }

                if (found) {
                    break;
                }
            }
        }

        if (!found) {
            ERROR("Unhandled SIGILL (%d) at PC: %016lx", info->si_code, pc);
        }
        break;
    }
    default: {
        if (handlers[sig - 1].handler) {
            // There's a guest signal handler for this signal.
            // If the signal happened inside the JIT code, we need to do some sort of state reconstruction at the end
            // of the guest signal handler.
            // Otherwise I think we're good to recompile and run it :cluegi:
            ERROR("implme");
        } else {
            ERROR("Unhandled signal %d", sig);
        }
        break;
    }
    }
}
#endif

void Signals::initialize() {
    struct sigaction sa;
    sa.sa_sigaction = signal_handler;
    sa.sa_flags = SA_SIGINFO;
    sigemptyset(&sa.sa_mask);

    sigaction(SIGBUS, &sa, nullptr);
    sigaction(SIGILL, &sa, nullptr);
}

void Signals::registerSignalHandler(int sig, void* handler, sigset_t mask, int flags) {
    ASSERT(sig > 0 && sig < 64);
    handlers[sig - 1] = {handler, mask, flags};
    WARN("Registering signal handler for signal %d", sig);

    struct sigaction sa;
    sa.sa_sigaction = signal_handler;
    sa.sa_flags = SA_SIGINFO;
    sigemptyset(&sa.sa_mask);
    sigaction(sig, &sa, nullptr);
}

RegisteredSignal Signals::getSignalHandler(int sig) {
    u8 sig_index = sig - 1;
    if (sig_index >= 0 && sig_index <= 63) {
        return handlers[sig_index];
    } else {
        WARN("Trying to get signal %d, but it is out of bounds", sig);
        return {nullptr, {}, 0};
    }
}