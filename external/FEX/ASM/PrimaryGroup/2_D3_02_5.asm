%ifdef CONFIG
{
  "RegData": {
    "RBX": "0x00000001",
    "RDI": "0x00000001",
    "RDX": "0x40000000",
    "RSI": "0x40000000",
    "R8":  "0x1",
    "R9":  "0x0",
    "R10": "0x1",
    "R11": "0x0"
  }
}
%endif
bits 64

mov rbx, 0x00000001
mov rdi, 0x00000001
mov rdx, 0x40000000
mov rsi, 0x40000000
mov rcx, 32 ; Test wraparound with zero shift

stc
rcl ebx, cl
lahf
mov r8w, ax
shr r8, 8
and r8, 1 ; We only care about carry flag here

clc
rcl edi, cl
lahf
mov r9w, ax
shr r9, 8
and r9, 1 ; We only care about carry flag here

stc
rcl edx, cl
lahf
mov r10w, ax
shr r10, 8
and r10, 1 ; We only care about carry flag here

clc
rcl esi, cl
lahf
mov r11w, ax
shr r11, 8
and r11, 1 ; We only care about carry flag here

hlt
