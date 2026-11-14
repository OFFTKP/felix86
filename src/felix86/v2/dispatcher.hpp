#pragma once

#include "felix86/common/types.hpp"

struct CodeCachePointers;

struct Dispatcher {
    static u8* emitDispatcher(u8* buffer, u64 size, CodeCachePointers& pointers, bool relocatable);
};
