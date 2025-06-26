#pragma once

#include <span>
#include "felix86/common/types.hpp"

struct VDSO {
    static std::span<u8> getObject64();

private:
    VDSO() = delete;
};