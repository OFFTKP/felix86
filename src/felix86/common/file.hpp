#pragma once

#include "felix86/common/utility.hpp"

// TODO: get rid of these

void* easy_fopen(const char* path, void* user_data);
bool easy_fread(void* handle, void* buffer, u64 offset, u64 size, void* user_data);
void easy_fclose(void* handle, void* user_data);
u64 easy_get_size(void* handle, void* user_data);
