%ifdef CONFIG
{
  "RegData": {
    "MM0":  ["0x00000000c0000000", "0x0"],
    "MM1":  ["0x00000000c3800000", "0x0"]
  },
  "HostFeatures": ["3DNOW"]
}
%endif

movq mm0, [rel data1]
movq mm1, [rel data2]

pfpnacc mm0, [rel data3]
pfpnacc mm1, [rel data4]

hlt

align 8
data1:
dd -1.0
dd 1.0

data2:
dd -128.0
dd 128.0

data3:
dd 256.0
dd -256.0

data4:
dd 512.0
dd -512.0
