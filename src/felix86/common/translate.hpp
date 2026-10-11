#pragma once
#include <string>

__attribute__((format_arg(1))) const char* translate(const char* en);

__attribute__((format(printf, 1, 2))) std::string translate_format(const char* format, ...);

bool translation_is_english();

#define TR(en) translate(en)

#define TRF(en, ...) translate_format(TR(en), ##__VA_ARGS__).c_str()
