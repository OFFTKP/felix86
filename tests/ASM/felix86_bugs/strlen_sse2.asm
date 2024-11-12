%ifdef CONFIG
{
  "RegData": {
    "RAX": "0x000000000000000D"
  }
}
%endif
bits 64

lea rdi, [rel test_str]
call strlen_sse2
hlt

strlen_sse2:
    endbr64
    pxor    xmm0, xmm0
    pxor    xmm1, xmm1
    pxor    xmm2, xmm2
    pxor    xmm3, xmm3
    mov     rax, rdi
    mov     rcx, rdi
    and     rcx, 0xfff
    cmp     rcx, 0xfcf
    ja      .check_aligned

    movdqu  xmm4, [rax]
    pcmpeqb xmm4, xmm0
    pmovmskb edx, xmm4
    test    edx, edx
    jz      .scan_next
    bsf     eax, edx
    ret

.scan_next:
    and     rax, -16
    pcmpeqb xmm1, [rax + 16]
    pcmpeqb xmm2, [rax + 32]
    pcmpeqb xmm3, [rax + 48]
    pmovmskb edx, xmm1
    pmovmskb r8d, xmm2
    pmovmskb ecx, xmm3
    shl     rdx, 16
    shl     rcx, 16
    or      rcx, r8
    shl     rcx, 32
    or      rdx, rcx
    mov     rcx, rdi
    xor     rcx, rax
    and     rax, -64
    sar     rdx, cl
    test    rdx, rdx
    jz      .scan_large_blocks
    bsf     rax, rdx
    ret

.check_aligned:
    and     rax, -64
    pcmpeqb xmm0, [rax]
    pcmpeqb xmm1, [rax + 16]
    pcmpeqb xmm2, [rax + 32]
    pcmpeqb xmm3, [rax + 48]
    pmovmskb esi, xmm0
    pmovmskb edx, xmm1
    pmovmskb r8d, xmm2
    pmovmskb ecx, xmm3
    shl     rdx, 16
    shl     rcx, 16
    or      rdx, rsi
    or      rcx, r8
    shl     rcx, 32
    or      rdx, rcx
    mov     rcx, rdi
    xor     rcx, rax
    and     rax, -64
    sar     rdx, cl
    test    rdx, rdx
    jz      .init_large_scan
    bsf     rax, rdx
    ret

.init_large_scan:
    pxor    xmm1, xmm1
    pxor    xmm2, xmm2
    pxor    xmm3, xmm3

.scan_large_blocks:
    movdqa  xmm0, [rax + 64]
    pminub  xmm0, [rax + 80]
    pminub  xmm0, [rax + 96]
    pminub  xmm0, [rax + 112]
    pcmpeqb xmm0, xmm3
    pmovmskb edx, xmm0
    test    edx, edx
    jnz     .found_in_second_half

    sub     rax, -128
    movdqa  xmm0, [rax]
    pminub  xmm0, [rax + 16]
    pminub  xmm0, [rax + 32]
    pminub  xmm0, [rax + 48]
    pcmpeqb xmm0, xmm3
    pmovmskb edx, xmm0
    test    edx, edx
    jnz     .find_exact
    jmp     .scan_large_blocks

.found_in_second_half:
    add     rax, 64

.find_exact:
    pxor    xmm0, xmm0
    pcmpeqb xmm0, [rax]
    pcmpeqb xmm1, [rax + 16]
    pcmpeqb xmm2, [rax + 32]
    pcmpeqb xmm3, [rax + 48]
    pmovmskb esi, xmm0
    pmovmskb edx, xmm1
    pmovmskb r8d, xmm2
    pmovmskb ecx, xmm3
    shl     rdx, 16
    shl     rcx, 16
    or      rdx, rsi
    or      rcx, r8
    shl     rcx, 32
    or      rdx, rcx
    bsf     rdx, rdx
    add     rax, rdx
    sub     rax, rdi
    ret

align 16
test_str db "Hello, World!", 0