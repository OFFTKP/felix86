#pragma once
#include <string>

bool detect_binfmt_misc();
void binfmt_misc(bool is_register);
bool unregister_binfmt_misc(const std::string& path);
