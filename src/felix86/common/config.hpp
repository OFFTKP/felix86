#pragma once

#include <filesystem>
#include <string>
#include <vector>

struct Config {
    std::filesystem::path rootfs_path;
    std::filesystem::path executable_path;
    std::vector<std::string> argv;
    std::vector<std::string> envp;
};
