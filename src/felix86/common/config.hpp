// Configures the state of the emulator
// Read from a file usually in home directory, and environment variables
// Environment variables can override the file for quick configuration
// Update config.inc to add more stuff
// Config can be accessed via g_config.config_name (ie. g_config.verbose)
#pragma once

#include <filesystem>
#include "felix86/common/types.hpp"

struct Config {
#define X(group, type, name, value, ...) type name = value;
#include "config.inc"
#undef X

    static bool initialize(bool ignore_envs = false);
    static const char* getDescription(const char* name);
    const char* getEnvironment() {
        return __environment.c_str();
    }

    std::filesystem::path path() const {
        return config_path;
    }

    [[nodiscard]] static Config load(const std::filesystem::path& path, bool ignore_envs = false);
    static void save(const std::filesystem::path& path, const Config& config);

    static bool addTrustedPath(const std::filesystem::path& path);

    static std::filesystem::path getConfigDir();

private:
    std::string __environment;
    std::filesystem::path config_path;

    friend void addToEnvironment(Config& config, const char* env_name, const char* env);
};

extern Config g_config;
