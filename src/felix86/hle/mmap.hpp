#pragma once

#include "felix86/common/utility.hpp"

void* felix86_mmap(void* addr, u64 size, int prot, int flags, int fd, u64 offset);
