%ifdef CONFIG
{
  "HostFeatures": ["AVX"],
  "RegData": {
    "RAX": "0x0000000000000001",
    "RBX": "0x0000000000000002",
    "RCX": "0x0000000000000003",
    "RDX": "0x0000000000000004",
    "RBP": "0x00000000FFFFFFFE",
    "RSI": "0xFFFFFFFFFFFFFFFC",
    "RDI": "0x0000000080000000",
    "R8":  "0x0000000080000000"
  }
}
%endif
bits 64

lea r15, [rel .data]

vmovapd xmm0, [r15 + 8 * 0]
vmovapd xmm1, [r15 + 8 * 2]

mov rax, -1
mov rbx, -1
mov rcx, -1
mov rdx, -1
mov rbp, -1
mov rsi, -1
mov rdi, -1
mov r8, -1

vcvttsd2si eax, xmm0
vcvttsd2si rbx, xmm1

vcvttsd2si ebp, [r15 + 8 * 8]
vcvttsd2si rsi, [r15 + 8 * 10]

vcvttsd2si ecx, [r15 + 8 * 4]
vcvttsd2si rdx, [r15 + 8 * 6]

vcvttsd2si edi, [r15 + 8 * 12]
vcvttsd2si r8, [r15 + 8 * 14]

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

dq 0x41E0000000000000
dq 0x5152535455565758

dq 0xC1E0000000100000
dq 0x5152535455565758
