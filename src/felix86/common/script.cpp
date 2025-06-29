#include <climits>
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

    char data[PATH_MAX];
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

    int arg_start = -1;
    bool slash_found = false;
    for (int i = 0; i < PATH_MAX; i++) {
        if (data[i] == 0) {
            break;
        } else if (data[i] == '/') {
            slash_found = true;
        } else if (data[i] == ' ') {
            // skip spaces before the first slash, for example there could be this:
            // #! /bin/bash args here
            //   ^--- gotta make sure to skip this space
            if (!slash_found)
                continue;

            data[i] = 0;
            arg_start = i + 1;
            if (data[arg_start] == 0) {
                arg_start = -1;
            }
            break;
        }
    }

    std::string interpreter = data;
    if (arg_start != -1) {
        args = &data[arg_start];
    }

    auto first_slash = interpreter.find_first_of('/');
    ASSERT(first_slash != std::string::npos);
    ASSERT(interpreter.size() > first_slash + 1);

    this->interpreter = &interpreter[first_slash];

    LOG("Running a script file: %s, interpreter: %s, args: %s", script.c_str(), this->interpreter.c_str(), args.c_str());
    ASSERT(std::filesystem::exists(this->interpreter));
    ASSERT(std::filesystem::is_regular_file(this->interpreter));
}