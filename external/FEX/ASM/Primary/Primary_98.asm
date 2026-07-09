%ifdef CONFIG
{
  "RegData": {
    "RAX": "0xFFFFFFFFFFFFFFF0"
  }
}
%endif
bits 64

mov al, 0xF0
cbw
cwde
cdqe

hlt
