%ifdef CONFIG
{
  "RegData": {
    "R14": "1",
    "R13": "1",
    "R12": "1",
    "R11": "1",
    "R10": "0",
    "R9": "0",
    "R8": "0",
    "RSP": "0"
  }
}
%endif
bits 64

mov r15, 1

; Should all set OF
;8bit
mov rax, 0x41

mov cl, 1
ror al, cl

mov r14, 0
cmovo r14, r15

; 16bit
mov rax, 0x4001

mov cl, 1
ror ax, cl

mov r13, 0
cmovo r13, r15

; 32bit
mov rax, 0x40000001

mov cl, 1
ror eax, cl

mov r12, 0
cmovo r12, r15

; 64bit
mov rax, 0x4000000000000001

mov cl, 1
ror rax, cl

mov r11, 0
cmovo r11, r15

; Let's clear OF really quick
mov rax, 0
ror rax, 1

; Shouldn't set OF

;8bit
clc
mov rax, 0x80

mov cl, 0
ror al, cl

mov r10, 0
cmovo r10, r15

; 16bit
clc
mov rax, 0x8000

mov cl, 0
ror ax, cl

mov r9, 0
cmovo r9, r15

; 32bit
clc
mov rax, 0x80000000

mov cl, 0
ror eax, cl

mov r8, 0
cmovo r8, r15

; 64bit
clc
mov rax, 0x8000000000000000

mov cl, 0
ror rax, cl

mov rsp, 0
cmovo rsp, r15

hlt
