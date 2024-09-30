#include <stdio.h>
#include "file.hpp"

void* easy_fopen(const char* path, void* user_data) {
    return (void*)(intptr_t)fopen(path, "rb");
}

bool easy_fread(void* handle, void* buffer, u64 offset, u64 size, void* user_data) {
    FILE* file = (FILE*)(intptr_t)handle;
    fseek(file, offset, SEEK_SET);
    size_t res = fread(buffer, 1, size, file);
    return res == size;
}

void easy_fclose(void* handle, void* user_data) {
    FILE* file = (FILE*)(intptr_t)handle;
    fclose(file);
}

u64 easy_get_size(void* handle, void* user_data) {
    FILE* file = (FILE*)(intptr_t)handle;
    fseek(file, 0, SEEK_END);
    return ftell(file);
}