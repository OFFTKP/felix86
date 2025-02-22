%ifdef CONFIG
{
  "HostFeatures": ["AVX"],
  "RegData": {
    "XMM0":  ["0xA76C4F06A12BFCE0", "0x9B80767F1E6A060F", "0x0C7FCC33573D4A81", "0xB2B1594B0900051F"],
    "XMM1":  ["0x6868C3F3AAED56E0", "0xF0FCE9E294E6E6DE", "0x48CACFD5667F2042", "0xC3BF1B89A0DFEE04"],
    "XMM2":  ["0x6C8BABD754A8356E", "0x277EA625CA925F77", "0x77D4FD3ED900079E", "0xA454D66F18BE061B"],
    "XMM3":  ["0x6A6FD695EC73CDC7", "0xDDA1B927BBF2AEBB", "0x22E096464DD75EF1", "0xF8DD0BC501EC1573"],
    "XMM4":  ["0x88312CD5C7D14D73", "0x7F091E1EFDDBE7FE", "0x8952DF26784EFD5F", "0x06BE3C607E0C7DC7"],
    "XMM5":  ["0xF29AE6EF954EFA14", "0x8273A8A49A6242A0", "0x0DCA8E436C33CE72", "0xD237159B6EF41772"],
    "XMM6":  ["0x3212073882160F0E", "0xB3780763C1923507", "0xE482F34CE3FE3EFC", "0xC3F2D5A8975969F8"],
    "XMM7":  ["0x462A372B571946CB", "0xA38DCD3D790E041F", "0x879EF228FB9D8A41", "0xCA0E4DAE9D595C1A"],
    "XMM8":  ["0x01D700F201DD018B", "0x021B012D00EC015B", "0x00AB018100EF0139", "0x015401BB020C0160"],
    "XMM9":  ["0x021B01EA0147019C", "0x017900FB00D801D9", "0x014400CB01160185", "0x016D00CE01A1014C"],
    "XMM10": ["0x010500E801000153", "0x011A015F01530171", "0x01AD00CD027F0105", "0x00F3018301A60197"],
    "XMM11": ["0x019C0124018F014D", "0x011F0100011E0116", "0x01580145016B0106", "0x01E301CD013A0119"],
    "XMM12": ["0x0136007E009D01E0", "0x02A802C80245019D", "0x0149015101AF016A", "0x010600E400E30120"],
    "XMM13": ["0x009F0115017B0132", "0x013C01AF01F90179", "0x015601040159025A", "0x017D01B40202017D"],
    "XMM14": ["0x0077012B011900E8", "0x00BC016E019E0146", "0x01D900F8011201BE", "0x00E5012000B60130"],
    "XMM15": ["0x0100011C010300D5", "0x00F3014A016700CD", "0x011900A400F60156", "0x0063010A019B0185"]
  }
}
%endif
bits 64

lea rdx, [rel .data]

vmovaps ymm0, [rdx + 32 * 0]
vmovaps ymm1, [rdx + 32 * 1]
vmovaps ymm2, [rdx + 32 * 2]
vmovaps ymm3, [rdx + 32 * 3]
vmovaps ymm4, [rdx + 32 * 4]
vmovaps ymm5, [rdx + 32 * 5]
vmovaps ymm6, [rdx + 32 * 6]
vmovaps ymm7, [rdx + 32 * 7]

vmpsadbw ymm8,  ymm0, [rdx + 32 * 8],  000000b
vmpsadbw ymm9,  ymm1, [rdx + 32 * 9],  001001b
vmpsadbw ymm10, ymm2, [rdx + 32 * 10], 010010b
vmpsadbw ymm11, ymm3, [rdx + 32 * 11], 011011b
vmpsadbw ymm12, ymm4, [rdx + 32 * 12], 100100b
vmpsadbw ymm13, ymm5, [rdx + 32 * 13], 101101b
vmpsadbw ymm14, ymm6, [rdx + 32 * 14], 110110b
vmpsadbw ymm15, ymm7, [rdx + 32 * 15], 111111b

hlt

align 32
.data:
dq 0xA76C4F06A12BFCE0, 0x9B80767F1E6A060F, 0x0C7FCC33573D4A81, 0xB2B1594B0900051F
dq 0x6868C3F3AAED56E0, 0xF0FCE9E294E6E6DE, 0x48CACFD5667F2042, 0xC3BF1B89A0DFEE04
dq 0x6C8BABD754A8356E, 0x277EA625CA925F77, 0x77D4FD3ED900079E, 0xA454D66F18BE061B
dq 0x6A6FD695EC73CDC7, 0xDDA1B927BBF2AEBB, 0x22E096464DD75EF1, 0xF8DD0BC501EC1573
dq 0x88312CD5C7D14D73, 0x7F091E1EFDDBE7FE, 0x8952DF26784EFD5F, 0x06BE3C607E0C7DC7
dq 0xF29AE6EF954EFA14, 0x8273A8A49A6242A0, 0x0DCA8E436C33CE72, 0xD237159B6EF41772
dq 0x3212073882160F0E, 0xB3780763C1923507, 0xE482F34CE3FE3EFC, 0xC3F2D5A8975969F8
dq 0x462A372B571946CB, 0xA38DCD3D790E041F, 0x879EF228FB9D8A41, 0xCA0E4DAE9D595C1A
dq 0x3057BAAB2F86F32B, 0xEF3F4F46F02CD62E, 0x94C77DFE4CE24002, 0xF21AA894D8B40A7B
dq 0xDE3C4B3485BBD1EF, 0x9DE3718DB9A3489E, 0xEB916DE33FC4D6C4, 0xD0514FFFD3EFFCE5
dq 0x9D50328ADEFB7209, 0xEEF7EB52F6F19869, 0xABC6D5DBC52734DA, 0xED34B0EAE12FB881
dq 0xCE021C30FFC299D6, 0xA60E9C56F1B20570, 0xCF0CECBC8DF25E5E, 0xABE3B9B0215B088A
dq 0x30763886E2C46218, 0xEB535D0EA7E4A12F, 0xAA418BA42D1E3354, 0x1701761E8F4456D0
dq 0x6802E8E1B7E04514, 0x46EBF28FC18EFE1A, 0xC42510C384410A30, 0xB029D9C4A89A6C74
dq 0x032E9746236A5D7F, 0xAC5976548F321298, 0xF537B9098166726E, 0x97C312089BF23896
dq 0xB6D30C71C85F76C8, 0x881D2CA6ABEA19C5, 0xF3F32FC9BBDA1589, 0x2732CF8F4E17D917
