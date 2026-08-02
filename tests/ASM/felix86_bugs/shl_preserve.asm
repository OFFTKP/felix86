%ifdef CONFIG
{
  "RegData": {
    "R8": "0x0000000000000001",
    "R9": "0x0000000000000080",
    "R10": "0x0000000000000885",
    "R11": "0x0000000000000000"
  }
}
%endif
bits 64

mov rax, 0xabcdef1234567890
mov rbx, 0x800000abcdef1234

add rax, rbx
shl rax, 3
pushfq
pop r8
and r8, 0x8d5

or rax, rbx
shl rax, 0
pushfq
pop r9
and r9, 0x8d5

mov rcx, 5
xor rax, rbx
shl rax, cl
pushfq
pop r10
and r10, 0x8d5

xor ecx, ecx
xor rax, rbx
shl rax, cl
pushfq
pop r11
and r11, 0x8d5

hlt