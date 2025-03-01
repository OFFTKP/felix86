#include <cstring>
#include "felix86/common/log.hpp"
#include "felix86/common/script.hpp"

Script::PeekResult Script::Peek(const std::filesystem::path& path) {
    FILE* file = fopen(path.c_str(), "r");

    if (!file) {
        ERROR("Failed to open file %s", path.c_str());
        return PeekResult::NotScript;
    }

    size_t size;
    fseek(file, 0, SEEK_END);
    size = ftell(file);
    fseek(file, 0, SEEK_SET);

    u8 data[PATH_MAX];
    size = std::min((size_t)PATH_MAX, size);
    size_t size_read = fread(data, 1, size, file);
    fclose(file);

    if (size_read != size) {
        ERROR("Failed to read file %s", path.c_str());
        return PeekResult::NotScript;
    }

    if (size < 2) {
        return PeekResult::NotScript;
    }

    if (data[0] != '#' || data[1] != '!') {
        return PeekResult::NotScript;
    }

    return PeekResult::Script;
}

Script::Script(const std::filesystem::path& script) {
    FILE* file = fopen(script.c_str(), "r");

    if (!file) {
        ERROR("Failed to open file %s", script.c_str());
    }

    // This constructor assumes we are already a script file so it doesn't do checks
    size_t size;
    fseek(file, 0, SEEK_END);
    size = ftell(file);
    fseek(file, 0, SEEK_SET);

    u8 data[PATH_MAX];
    fseek(file, 2, SEEK_SET); // skip #!
    size = std::min((size_t)PATH_MAX, size - 2);
    size_t size_read = fread(data, 1, size, file);
    fclose(file);

    if (size_read != size) {
        ERROR("Failed to read file %s", script.c_str());
    }

    bool found = false;
    for (size_t i = 0; i < size; i++) {
        if (data[i] == '\n') {
            found = true;
            data[i] = 0; // set to terminator to copy
            break;
        }
    }

    if (!found) {
        ERROR("Could not parse interpreter for script file");
    }

    char interpreter[PATH_MAX];
    memset(interpreter, 0, PATH_MAX);
    strncpy(interpreter, (char*)data, PATH_MAX);

    this->interpreter = interpreter;

    ASSERT(std::filesystem::exists(interpreter));
    ASSERT(std::filesystem::is_regular_file(interpreter));
}