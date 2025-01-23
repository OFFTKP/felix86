%ifdef CONFIG
{
  "RegData": {
    "R15": "0xFFFFFFFFFFFF000F",
    "R14": "0x1F",
    "R13": "0x3F",
    "R12": "0xFFFFFFFFFFFF000C",
    "R11": "0x1C",
    "R10": "0x3C",
    "R9":  "0xFFFFFFFFFFFFFFFF",
    "R8":  "0xFFFFFFFFFFFFFFFF",
    "RSI": "0xFFFFFFFFFFFFFFFF"
  }
}
%endif
bits 64

mov rax, 0xFFFFFFFFFFFFFFFF
mov rbx, 0x1010101010101010
mov ecx, 0

mov r15, -1
mov r14, -1
mov r13, -1
mov r12, -1
mov r11, -1
mov r10, -1
mov r9,  -1
mov r8,  -1
mov rsi, -1

bsr r15w, ax
bsr r14d, eax
bsr r13,  rax

bsr r12w, bx
bsr r11d, ebx
bsr r10,  rbx

bsr r9w, cx
bsr r8d, ecx
bsr rsi, rcx

hlt