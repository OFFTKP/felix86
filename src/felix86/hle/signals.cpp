#include <array>
#include "felix86/emulator.hpp"
#include "felix86/hle/filesystem.hpp"
#include "felix86/hle/signals.hpp"

std::array<RegisteredSignal, 32> handlers{};

bool is_in_jit_code(uintptr_t ptr) {
    uintptr_t start = g_emulator->GetAssembler().GetCodeBuffer().GetOffsetAddress(0);
    uintptr_t end = g_emulator->GetAssembler().GetCodeBuffer().GetCursorAddress();
    return ptr >= start && ptr < end;
}

#ifndef REG_PC
#define REG_PC 0
#endif

#ifdef __x86_64__
void signal_handler(int sig, siginfo_t* info, void* ctx) {
    UNREACHABLE();
}
#elif defined(__riscv)
void signal_handler(int sig, siginfo_t* info, void* ctx) {
    ucontext_t* context = (ucontext_t*)ctx;
    uintptr_t pc = context->uc_mcontext.__gregs[REG_PC];

    ASSERT(FastRecompiler::threadStatePointer() == x27);
    FastRecompiler& recompiler = g_emulator->GetRecompiler();

    switch (sig) {
    case SIGBUS: {
        if (is_in_jit_code(pc)) {
            switch (info->si_code) {
            case BUS_ADRALN: {
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
            }
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
}

void Signals::registerSignalHandler(int sig, void* handler, sigset_t mask, int flags) {
    ASSERT(sig > 0 && sig < 32);
    handlers[sig] = {handler, mask, flags};
    WARN("Registering signal handler for signal %d", sig);
}

RegisteredSignal Signals::getSignalHandler(int sig) {
    ASSERT(sig > 0 && sig < 32);
    return handlers[sig];
}