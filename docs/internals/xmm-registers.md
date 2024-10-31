# XMM registers

XMM registers are allocated on the v0-v31 registers, regardless of whether or not they are doing scalar operations
like `mulss`, or SIMD operations like `pand`. Moving them back and forth from FPR regs was considered but
it would be both expensive (have to writeback vector registers when switching to FPR regs, because scalar operations
preserve upper bits) and annoying to get right.

Same goes for mmx/x87 stack, for the same reasons.