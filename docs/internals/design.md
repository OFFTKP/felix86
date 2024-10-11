# Design

x86-64 code is compiled into chunks of basic blocks, called an "IRFunction". The frontend continues adding instructions
to a basic block until it hits a jump. If we know the jump target we add it to the function and compile it as a new block.
If we don't know the target we stop compiling on that path and jump to the Exit block. This creates a control flow graph of many blocks that start at an
Entry block and end in an Exit block, which loads and stores the VM state for this thread.

If a syscall/cpuid/... operation needs to happen, the currently allocated registers at that point are pushed to the
stack, the call is made and then they are popped. Such VM exits are wrapped in GetGuest+StoreToMemory/LoadFromMemory+SetGuest (see `setguest-getguest.md`) instructions so the relevant registers are written to state. For example, for CPUID the relevant registers are RAX and RCX, so only those need to be written to memory and only those are.

When an IRFunction is ready, it's not yet ready to be transformed into RISC-V. It goes through an SSA pass which uses information
from the SetGuest/GetGuest to find the definitions of each variable (a variable being any sort of x86 state such as the RAX register) and add phis wherever necessary. This pass removes any GetGuest instructions. Another pass converts the remaining SetGuest instructions into copy operations which are then copy propagated later.

After SSA construction, optimizations can be applied on the IR, followed by SSA destruction.
During SSA destruction the IR is converted into a "backend" mode, where operands are references to by integers instead of
pointers, because now two instructions can define the same temporary (due to phi destruction). The RA allocates registers/spill locations on this form.

Finally the backend passes this form to the emitter and emits RISC-V.

# Register allocation

Felix86 does not do "static register allocation", meaning, giving some GPRs a specific purposes such as r1 is for rax, r2 is for rcx etc. I have not tested on whether or not that would be preferable but I consider it subpar as it would be hogging some registers when they don't need to be hogged.

# Multithreaded code

Emitted code can run in parallel but only 1 function may compile at once. This is to reduce possible duplicate functions in the backend. A mutex is locked when we need to compile.
Currently there's no plans to allow the compilation process to run on multiple threads, we'll see in the future.

# TSO

As of writing this there's no TSO support. It is planned to make it a configuration to enable/disable and implement using memory barriers or similar. There's also Ztso extension but nothing seems to ship it.