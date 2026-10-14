%ifdef CONFIG
{
  "RegData": {
    "RBX": "0x100",
    "RCX": "0x100",
    "RDX": "0x100"
  }
}
%endif
bits 64

finit

fld1
fldz
fcompp
fnstsw ax
and eax, 0x4500
mov ebx, eax

fld1
fldz
fucompp
fnstsw ax
and eax, 0x4500
mov ecx, eax

fld1
fldz
fucomp st1
fnstsw ax
and eax, 0x4500
mov edx, eax

hlt
