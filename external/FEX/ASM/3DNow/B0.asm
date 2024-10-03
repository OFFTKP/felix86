%ifdef CONFIG
{
  "RegData": {
    "MM0":  ["0x0", "0x0"],
    "MM1":  ["0xFFFFFFFF00000000", "0x0"],
    "MM2":  ["0xFFFFFFFFFFFFFFFF", "0x0"],
    "MM3":  ["0x00000000FFFFFFFF", "0x0"]
  },
  "HostFeatures": ["3DNOW"]
}
%endif

movq mm0, [rel data5]
movq mm1, [rel data6]
movq mm2, [rel data7]
movq mm3, [rel data8]

; False, False
; 0.0 == 1.0
; 0.0 == 1.0
pfcmpeq mm0, [rel data1]

; False, True
; 0.0 == 1.0
; 1.0 == 1.0
pfcmpeq mm1, [rel data2]

; True, True
; -2.0 == -2.0
; -1.0 == -1.0

pfcmpeq mm2, [rel data3]

; True, False
; 0.0 == 0.0
; 0.0 == 1.0
pfcmpeq mm3, [rel data4]

hlt

align 8
data1:
dd 1.0
dd 1.0

data2:
dd 1.0
dd 1.0

data3:
dd -2.0
dd -1.0

data4:
dd 0.0
dd 1.0

data5:
dd 0.0
dd 0.0

data6:
dd 0.0
dd 1.0

data7:
dd -2.0
dd -1.0

data8:
dd 0.0
dd 0.0
