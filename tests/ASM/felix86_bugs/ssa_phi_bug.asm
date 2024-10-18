%ifdef CONFIG
{
    "RegData": {
        "RCX": "0x0000000000000000"
    }
}
%endif
bits 64

mov rcx, 10

loop:
cmp rcx, 5
jz past
past: ; stupid conditional jump that points to the same location for both branches, would cause ssa problems
dec rcx
cmp rcx, 0
jne loop

hlt
