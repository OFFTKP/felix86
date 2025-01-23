%ifdef CONFIG
{
  "RegData": {
    "R15": "0xFFFFFFFFFFFF000F",
    "R14": "0x1F",
    "R13": "0x3F",
    "R12": "0xFFFFFFFFFFFF000C",
    "R11": "0x1C",
    "R10": "0x3C",
    "R9":  "0xFFFFFFFFFFFFFFFF",
    "R8":  "0xFFFFFFFFFFFFFFFF",
    "RSI": "0xFFFFFFFFFFFFFFFF"
  }
}
%endif
bits 64

mov ecx, 0

mov rsi, -1

bsr rsi, rcx

hlt