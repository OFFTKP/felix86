%ifdef CONFIG
{
  "RegData": {
    "R14":"0x2000",
    "R13":"0x10",
    "R12":"0x4000",
    "R11":"0xC000",
    "R15":"0x6"
  }
}
%endif
bits 64

%macro cfmerge 0

; Get CF
lahf
shr rax, 8
and rax, 1

; Merge in to results
shl r15, 1
or r15, rax

%endmacro

xor r15, r15

mov rdi, 0xe0000000
mov rax, 0x10
mov [rdi], rax
mov rax, 0
mov [rdi+8], rax
mov [rdi+16], rax
mov [rdi+32], rax
mov rax, 0x4000
mov [rdi+24], rax

clc
xor rcx, rcx
bts rcx, 13
mov r14, rcx
cfmerge

clc
bts qword[rdi], 64 * 3 + 4 ; this gets masked and it's actually rdi bit 4
mov r13, [rdi]
cfmerge

clc
mov rax, 192 + 14
bts qword[rdi], rax ; this indexes the 206th bit
mov r12, [rdi + 24]
cfmerge

clc
mov rax, 192 + 15
bts qword[rdi], rax ; this indexes the 207th bit, not set
mov r11, [rdi + 24]
cfmerge


hlt