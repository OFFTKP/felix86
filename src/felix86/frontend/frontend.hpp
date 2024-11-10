#pragma once

#include "felix86/ir/function.hpp"

struct IREmitter;

void frontend_compile_block(IRFunction& function, IRBlock* block);
void frontend_compile_function(IRFunction& function);
