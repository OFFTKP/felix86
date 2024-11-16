#pragma once

#include "Zycore/Status.h"
#include "Zydis/Decoder.h"
#include "Zydis/DecoderTypes.h"
#include "felix86/common/utility.hpp"

struct Decoder {
    static ZydisDecoder decoder;
    static ZyanStatus decodeInstruction(ZydisDecodedInstruction& inst, ZydisDecodedOperand* operands, u8* data, u64 size = 20);
};