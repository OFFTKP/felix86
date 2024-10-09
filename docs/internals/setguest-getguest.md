# SetGuest/GetGuest instructions

A quite early decision in the IR design was to use these SetGuest/GetGuest instructions as defines/uses of the
individual x86-64 registers for easy identification during the SSA pass.
There's several different ways to deal with representing definitions and uses but this is how we do it.

The way IR is emitted before any pass is like so:

Say you have a few instructions like this:
```
add rax, rbx
sub rax, rcx
mov rdx, rax
```
Something like this will be emitted:
```
%1 <- GetGuest rax
%2 <- GetGuest rbx
%3 <- %1 + %2
%4 <- SetGuest rax, %3

%5 <- GetGuest rax
%6 <- GetGuest rcx
%7 <- %5 - %6
%8 <- SetGuest rax, %7

%9 <- GetGuest rax
%10 <- SetGuest rdx, %9
```
Then at the SSA pass we can call `SetGuest` a definition of a variable, and `GetGuest` a use of a variable.
During the SSA pass each SetGuest will be propagated to a GetGuest if possible. If a GetGuest is at a block
that has multiple predecessors with multiple SetGuests a phi will be inserted.
The algorithm employed for inserting phis is Cytron et al. and can be found in ssa_pass.cpp

This way the SSA pass gets rid of all the GetGuest instructions. Afterwards SetGuest instructions can also be
replaced by a simple move, which will then be copy propagated and eliminated by dead code elimination.

You might rightfully have a question:

What if a GetGuest exists with no prior SetGuest?

We make sure every GetGuest is dominated by a SetGuest. This is done by inserting a full load of VM state in the Entry block,
and a full VM writeback in the Exit block.

The entry block looks something like this:
```
%1 <- <load rax from vm state>
%2 <- SetGuest rax, %1

repeat for every single register/flag
```
The exit block, like this:
```
%1 <- GetGuest rax
<store %1 to rax in vm state>

repeat for every single register/flag
```
If loading the entire state seems wasteful, it is, but the Extraneous Writeback pass gets rid of any writebacks to VM state that
store the original value from the Entry block unchanged. This is easily detectable after SetGuest/GetGuest have been eliminated.

The final code from the initial example might look like this:
```
%3 <- %... + %... (these variables have whatever name they were given in the Entry block)
%7 <- %3 - %... (%3 holds the rax value, rcx is taken from the Entry block)
```
And assuming no other modifications happen to rax, the exit block will then do something like this:
```
<store %7 to rax in vm state>
<store %7 to rdx in vm state>
```
The `mov rdx, rax` got removed during copy propagation and the register for rax is used directly in the Exit block.
