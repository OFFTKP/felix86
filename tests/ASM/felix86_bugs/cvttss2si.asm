%ifdef CONFIG
{
  "HostFeatures": [],
  "RegData": {
    "RAX": "0x0000000000000001",
    "RBX": "0x0000000000000002",
    "RCX": "0x0000000000000003",
    "RDX": "0x0000000000000004",
    "RBP": "0x00000000FFFFFFFE",
    "RSI": "0xFFFFFFFFFFFFFFFC",
    "RDI": "0x0000000080000000",
    "R8":  "0x8000000000000000"
  }
}
%endif
bits 64

lea r15, [rel .data]

movapd xmm0, [r15 + 8 * 0]
movapd xmm1, [r15 + 8 * 2]

mov rax, -1
mov rbx, -1
mov rcx, -1
mov rdx, -1
mov rbp, -1
mov rsi, -1
mov rdi, -1
mov r8, -1

cvttss2si eax, xmm0
cvttss2si rbx, xmm1

cvttss2si ebp, [r15 + 8 * 8]
cvttss2si rsi, [r15 + 8 * 10]

cvttss2si ecx, [r15 + 8 * 4]
cvttss2si rdx, [r15 + 8 * 6]

cvttss2si edi, [r15 + 8 * 12]
cvttss2si r8, [r15 + 8 * 14]

hlt

align 32
.data:
dq 0x414243443F800000
dq 0x5152535455565758

dq 0x4142434440000000
dq 0x5152535455565758

dq 0x4142434440400000
dq 0x5152535455565758

dq 0x4142434440800000
dq 0x5152535455565758

dq 0x41424344C0000000
dq 0x5152535455565758

dq 0x41424344C0800000
dq 0x5152535455565758

dd 0x414243444F000000
dq 0x5152535455565758

dd 0x41424344CF000001
dq 0x5152535455565758
