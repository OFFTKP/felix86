%ifdef CONFIG
{
  "RegData": {
    "XMM0": ["0x7f8080807f808080", "0x7f8080807f808080"]
  }
}
%endif

bits 64

; 0x7fff800080008000, 0x7fff800080008000
; 0x7f8080807f808080, 0x7f8080807f808080
; 0xff808080ff800000, 0xff808080ff808080
mov rax, 0x7fff800080008000
mov rdi, 0xe0000000
mov [rdi], rax
mov [rdi + 8], rax

movaps xmm3, [rdi]
movaps xmm0, [rdi]

packsswb xmm0, xmm3

hlt