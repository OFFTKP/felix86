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

cvttsd2si eax, xmm0
cvttsd2si rbx, xmm1

cvttsd2si ebp, [r15 + 8 * 8]
cvttsd2si rsi, [r15 + 8 * 10]

cvttsd2si ecx, [r15 + 8 * 4]
cvttsd2si rdx, [r15 + 8 * 6]

cvttsd2si edi, [r15 + 8 * 12]
cvttsd2si r8, [r15 + 8 * 14]

hlt

align 32
.data:
dq 0x3FF0000000000000
dq 0x5152535455565758

dq 0x4000000000000000
dq 0x5152535455565758

dq 0x4008000000000000
dq 0x5152535455565758

dq 0x4010000000000000
dq 0x5152535455565758

dq 0xC000000000000000
dq 0x5152535455565758

dq 0xC010000000000000
dq 0x5152535455565758

dq 0x43E0000000000000
dq 0x5152535455565758

dq 0xC3E0000000000000
dq 0x5152535455565758
