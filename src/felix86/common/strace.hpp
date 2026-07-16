#pragma once

#include <string>
#include "felix86/common/types.hpp"

[[maybe_unused]] [[nodiscard]] std::string trace32(int syscall_no, u64 arg1, u64 arg2, u64 arg3, u64 arg4, u64 arg5, u64 arg6);
[[maybe_unused]] [[nodiscard]] std::string trace64(int syscall_no, u64 arg1, u64 arg2, u64 arg3, u64 arg4, u64 arg5, u64 arg6);
