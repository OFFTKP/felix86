#include <pwd.h>
#include <sys/types.h>
#include <toml.hpp>
#include "felix86/common/config.hpp"
#include "felix86/common/log.hpp"

Config g_config{};

namespace toml {
template <>
struct from<std::filesystem::path> {
    static std::filesystem::path from_toml(const toml::value& v) {
        return std::filesystem::path(toml::get<std::string>(v));
    }
};

// Specialization to convert from std::filesystem::path to TOML
template <>
struct into<toml::value> {
    static toml::value into_toml(const std::filesystem::path& p) {
        return toml::value(p.string());
    }
};
} // namespace toml

bool Config::initialize() {
    const char* homedir;
    if ((homedir = getenv("HOME")) == NULL) {
        homedir = getpwuid(getuid())->pw_dir;
    }

    std::filesystem::path config_path = homedir;
    config_path /= ".config";
    if (!std::filesystem::exists(config_path)) {
        bool ok = std::filesystem::create_directories(config_path);
        if (!ok) {
            return false;
        }
    } else if (!std::filesystem::is_directory(config_path)) {
        return false;
    }

    config_path /= "felix86";
    if (!std::filesystem::exists(config_path)) {
        bool ok = std::filesystem::create_directory(config_path);
        if (!ok) {
            return false;
        }
    } else if (!std::filesystem::is_directory(config_path)) {
        return false;
    }

    config_path /= "config.toml";
    if (!std::filesystem::exists(config_path)) {
        LOG("Created configuration file: %s", config_path.c_str());
        save(config_path, g_config);
    }

    g_config = load(config_path);

    return true;
}

bool is_truthy(const char* str) {
    if (!str) {
        return false;
    }

    std::string lower = str;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    return lower == "true" || lower == "1" || lower == "yes" || lower == "on" || lower == "y" || lower == "enable";
}

u64 get_int(const char* str) {
    int len = strlen(str);
    if (len > 2) {
        // Check if hex
        if (str[0] == '0' && str[1] == 'x') {
            return std::stoull(str, nullptr, 16);
        } else {
            return std::stoull(str);
        }
    } else {
        return std::stoull(str);
    }
}

template <typename Type>
bool loadFromToml(const toml::value& toml, const char* group, const char* name, Type& value) {
    if (toml.contains(group)) {
        const toml::value& group_toml = toml.at(group);
        if (group_toml.contains(name)) {
            const toml::value& value_toml = group_toml.at(name);
            if constexpr (std::is_same_v<Type, bool>) {
                value = value_toml.as_boolean();
                return true;
            } else if constexpr (std::is_same_v<Type, u64>) {
                value = value_toml.as_integer();
                return true;
            } else if constexpr (std::is_same_v<Type, std::filesystem::path>) {
                value = value_toml.as_string();
                return true;
            } else {
                static_assert(false);
            }
        }
    }
    return false;
}

void addToEnvironment(Config& config, const char* env_name, const char* env) {
    config.__environment += "\n";
    config.__environment += env_name;
    config.__environment += "=";
    config.__environment += env;
}

template <typename Type>
bool loadFromEnv(Config& config, Type& value, const char* env_name, const char* env) {
    addToEnvironment(config, env_name, env);

    if constexpr (std::is_same_v<Type, bool>) {
        value = is_truthy(env);
        return true;
    } else if constexpr (std::is_same_v<Type, u64>) {
        value = get_int(env);
        return true;
    } else if constexpr (std::is_same_v<Type, std::filesystem::path>) {
        value = env;
        return true;
    }

    return false;
}

Config Config::load(const std::filesystem::path& path) {
    Config config = {};

    auto attempt = toml::try_parse(path);
    if (attempt.is_err()) {
        return config;
    }

    auto toml = attempt.unwrap();

#define X(group, type, name, default_value, env_name, description, required)                                                                         \
    {                                                                                                                                                \
        bool loaded = false;                                                                                                                         \
        const char* env = getenv(#env_name);                                                                                                         \
        if (env) {                                                                                                                                   \
            loaded = loadFromEnv<type>(config, config.name, #env_name, env);                                                                         \
        } else {                                                                                                                                     \
            loaded = loadFromToml<type>(toml, #group, #name, config.name);                                                                           \
        }                                                                                                                                            \
        if (!loaded && required) {                                                                                                                   \
            ERROR("A value for %s is required but was not set. Please set it using the %s environment variable or in the configuration file %s in "  \
                  "group [\"%s\"]",                                                                                                                  \
                  #name, #env_name, path.c_str(), #group);                                                                                           \
        }                                                                                                                                            \
    }
#include "config.inc"
#undef X

    return config;
}

void Config::save(const std::filesystem::path& path, const Config& config) {
    toml::ordered_table toml;

#define X(group, type, name, default_value, env_name, description, required)                                                                         \
    {                                                                                                                                                \
        if (!toml.contains(#group)) {                                                                                                                \
            toml[#group] = toml::ordered_table{};                                                                                                    \
        }                                                                                                                                            \
        auto& value = toml[#group][#name];                                                                                                           \
        value = config.name;                                                                                                                         \
        value.comments().push_back("# " #name " (" #type ")");                                                                                       \
        value.comments().push_back("# Description: " description);                                                                                   \
        value.comments().push_back("# Environment variable: " #env_name);                                                                            \
    }
#include "config.inc"
#undef X

    std::ofstream ofs(path);
    ofs << "# Autogenerated TOML configuration file for felix86\n";
    ofs << "# You may change any values here, or their respective environment variable\n";
    ofs << "# The environment variables override the values here\n";
    ofs << toml::ordered_value{toml};
}