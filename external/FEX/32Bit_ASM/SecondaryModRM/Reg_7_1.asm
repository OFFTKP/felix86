%ifdef CONFIG
{
  "Mode": "32BIT",
  "HostFeatures": ["Linux"]
}
%endif
org 10000h
bits 32

; We can't really check the results of this
rdtscp

hlt
