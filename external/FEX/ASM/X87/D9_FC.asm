%ifdef CONFIG
{
  "RegData": {
    "MM7": ["0x8000000000000000", "0x3fff"]
  },
  "MemoryRegions": {
    "0x100000000": "4096"
  }
}
%endif
bits 64

mov rdx, 0xe0000000

mov eax, 0x3f834241 ; 1.02546
mov [rdx + 8 * 0], eax

fld dword [rdx + 8 * 0]

frndint

hlt
