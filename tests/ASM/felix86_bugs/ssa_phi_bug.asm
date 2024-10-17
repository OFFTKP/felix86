%ifdef CONFIG
{
    "RegData": {
    }
}
%endif
bits 64

mov rcx, 10

loop:
cmp rcx, 5
jz past
past:
dec rcx
cmp rcx, 0
jnz loop

hlt
