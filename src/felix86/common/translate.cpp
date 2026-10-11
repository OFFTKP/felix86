#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <iterator>
#include <string_view>
#include "felix86/common/translate.hpp"

static constexpr const char* strings_en[] = {
#define X(en, zh) en,
#include "felix86/common/strings.inc"
#undef X
};

static constexpr const char* strings_zh[] = {
#define X(en, zh) zh,
#include "felix86/common/strings.inc"
#undef X
};

static_assert(std::size(strings_en) == std::size(strings_zh));

struct TranslationLanguage {
    std::string_view locale_prefix;
    const char* const* strings;
};

static constexpr TranslationLanguage languages[] = {
    {"zh", strings_zh},
};

static const char* const* translation_detect() {
    for (const char* var : {"LC_ALL", "LC_MESSAGES", "LANG"}) {
        const char* value = getenv(var);
        if (value && *value) {
            std::string_view locale = value;
            for (const TranslationLanguage& language : languages) {
                if (locale.starts_with(language.locale_prefix)) {
                    return language.strings;
                }
            }
            return strings_en;
        }
    }
    return strings_en;
}

static const char* const* translation_table() {
    static const char* const* table = translation_detect();
    return table;
}

bool translation_is_english() {
    return translation_table() == strings_en;
}

const char* translate(const char* en) {
    const char* const* table = translation_table();
    if (table == strings_en) {
        return en;
    }

    std::string_view needle = en;
    for (size_t i = 0; i < std::size(strings_en); i++) {
        if (needle == strings_en[i]) {
            return table[i];
        }
    }
    return en;
}

std::string translate_format(const char* format, ...) {
    va_list args;
    va_start(args, format);
    va_list args_copy;
    va_copy(args_copy, args);
    int size = vsnprintf(nullptr, 0, format, args_copy);
    va_end(args_copy);

    std::string result;
    if (size > 0) {
        result.resize(size);
        vsnprintf(result.data(), size + 1, format, args);
    }
    va_end(args);
    return result;
}
