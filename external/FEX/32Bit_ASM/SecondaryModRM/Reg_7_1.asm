%ifdef CONFIG
{
  "Mode": "32BIT",
  "HostFeatures": ["Linux"]
}
%endif
bits 32

; We can't really check the results of this
rdtscp

hlt
