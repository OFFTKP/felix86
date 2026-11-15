#pragma once

#include "felix86/common/types.hpp"

struct CodeCachePointers;

struct Dispatcher {
    static void emitDispatcher(u8* buffer, u64 size, CodeCachePointers& pointers, bool relocatable);
};
