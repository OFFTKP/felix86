#include "felix86/common/decoder.hpp"

ZydisDecoder Decoder::decoder{};

ZyanStatus Decoder::decodeInstruction(ZydisDecodedInstruction& inst, ZydisDecodedOperand* operands, u8* data, u64 size) {
    return ZydisDecoderDecodeFull(&decoder, data, size, &inst, operands);
}