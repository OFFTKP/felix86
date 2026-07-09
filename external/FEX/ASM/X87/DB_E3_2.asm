%ifdef CONFIG
{
  "RegData": {
    "RAX": "0"
  }
}
%endif
bits 64

; Tests that fninit clears the status word (which includes the IE flag)
fninit
fldz
fldz
fdiv ; sets IE flag

fninit
fnstsw ax

hlt
