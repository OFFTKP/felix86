%ifdef CONFIG
{
  "RegData": {
    "RAX": "0x1200",
    "RDI": "0xE0000007",
    "RSI": "0xDFFFFFFF"
  },
  "MemoryRegions": {
    "0x100000000": "4096"
  }
}
%endif
bits 64

mov rdx, 0xe0000000

mov rax, 0x61
mov [rdx + 8 * 0], rax
mov rax, 0x47
mov [rdx + 8 * 1], rax

lea rdi, [rdx + 8 * 1]
lea rsi, [rdx + 8 * 0]

std
cmpsb ; rdi cmp rsi
; cmp = 0x47 - 0x61 = 0xE6
; 0: CF - 00000001
; 1:    - 00000010
; 2: PF - 00000000
; 3:  0 - 00000000
; 4: AF - 00000000
; 5:  0 - 00000000
; 6: ZF - 00000000
; 7: SF - 10000000
; ================
;         10000011
; OF: LAHF doesn't load - 0

mov rax, 0
lahf

hlt
