#include "felix86/aot/aot.hpp"

ZydisDecoder AOT::decoder{};

ZyanStatus AOT::decodeInstruction(ZydisDecodedInstruction& inst, ZydisDecodedOperand* operands, u8* data, u64 size) {
    return ZydisDecoderDecodeFull(&decoder, data, size, &inst, operands);
}