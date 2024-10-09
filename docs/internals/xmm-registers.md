# XMM registers

Before reading this, see `setguest-getguest.md`

In x86-64 the xmm registers can be both used for floating point operations such as with `addss`, `mulss` etc., and also for
SIMD operations (such as `pand`). However in RISC-V you have your FPRs f0-f31 and your Vector regs v0-v31.

So you need a way to identify when a register is being used for vector operations and when it's used for each purpose.

For this reason, operations that use vector or float registers wrap them in the `VEC` or `FPR` macro.

This macro basically emits a cast to vector or cast to float.

So for example:
```
mulss xmm0, ...
mulss xmm1, ...
addss xmm1, xmm0
```
Something like this would be emitted
```
%1 <- GetGuest xmm0
%2 <- cast_to_float(%1)
%3 <- fmul(%2, ...)
%4 <- SetGuest xmm0, %3

%5 <- GetGuest xmm1
%6 <- cast_to_float(%5)
%7 <- fmul(%6, ...)
%8 <- SetGuest xmm1, %7

%9 <- GetGuest xmm0
%10 <- cast_to_float(%9)
%11 <- GetGuest xmm1
%12 <- cast_to_float(%11)
%13 <- fadd(%10, %12)
%14 <- SetGuest xmm0, %13
```

Then after SetGuest/GetGuest elimination, each individual GetGuest would be forwarded whatever the last SetGuest had.
But we don't know what the last SetGuest had. So we add a cast right after the GetGuest.

Seems hacky? Maybe.

Now after SetGuest/GetGuest removal and optimizations, we can do a pass to remove unneeded casts.

If a cast to vector happens on a value that has type vector, it can be removed and propagated. If a cast to float happens on a value that already is a float, it can also be removed and propagated.
