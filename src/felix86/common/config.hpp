#pragma once

#include <deque>
#include <filesystem>
#include <string>
#include <vector>

struct Config {
    std::filesystem::path executable_path;
    std::deque<std::string> argv;
    std::vector<std::string> envp;
};
