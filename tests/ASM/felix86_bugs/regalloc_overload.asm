%ifdef CONFIG
{
    "RegData": {
        "RAX": "0x0000000000000001",
        "RCX": "0x0000000000000000",
        "RDX": "0x0000000000000003",
        "RBX": "0x0000000000000004",
        "RSP": "0x0000000000000005",
        "RBP": "0x0000000000000006",
        "RSI": "0x0000000000000007",
        "RDI": "0x0000000000000008",
        "R8": "0x0000000000000009",
        "R9": "0x000000000000000A",
        "R10": "0x000000000000000B",
        "R11": "0x000000000000000C",
        "R12": "0x000000000000000D",
        "R13": "0x000000000000000E"
    }
}
%endif
bits 64

; try it with fewer allocatable registers to induce spilling
; there must be enough to allow for loading from spill though
mov rcx, 10

loop:
mov rax, [rel my_data]
mov rdx, [rel my_data + 16]
mov rbx, [rel my_data + 24]
mov rsp, [rel my_data + 32]
mov rbp, [rel my_data + 40]
mov rsi, [rel my_data + 48]
mov rdi, [rel my_data + 56]
cmp rcx, 5
jz past
mov r8, [rel my_data + 64]
mov r9, [rel my_data + 72]
mov r10, [rel my_data + 80]
mov r11, [rel my_data + 88]
mov r12, [rel my_data + 96]
past:
mov r13, [rel my_data + 104]
dec rcx
jnz loop

hlt


dq 100, 200, 300, 400, 500, 600, 700, 800, 900, 1000, 1100, 1200, 1300, 1400, 1500, 1600
my_data:
dq 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16
dq 100, 200, 300, 400, 500, 600, 700, 800, 900, 1000, 1100, 1200, 1300, 1400, 1500, 1600