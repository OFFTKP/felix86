#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "felix86/common/utility.h"

typedef struct {
    void* user_data;
    void* (*fopen)(const char* path, void* user_data);
    bool (*fread)(void* handle, void* buffer, u64 offset, u64 size, void* user_data);
    void (*fclose)(void* handle, void* user_data);
    u64 (*get_size)(void* handle, void* user_data);
} file_reading_callbacks_t;

#ifdef __cplusplus
}
#endif