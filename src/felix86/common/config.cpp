#include <algorithm>
#include <filesystem>
#include <optional>
#include <system_error>
#include <pwd.h>
#include <sys/types.h>
#include "felix86/common/config.hpp"
#include "felix86/common/log.hpp"
#include "felix86/common/types.hpp"
#include "felix86/common/utility.hpp"
#include "fmt/format.h"
#include "tomlc17.h"

Config g_config{};
Config g_initial_config{};

std::filesystem::path Config::getConfigDir() {
    std::string homedir;

    // If SUDO_HOME is defined, use that as the home directory
    // `sudo` sets SUDO_HOME to the original HOME, and HOME to /root
    // We want felix86 instances running under sudo to use the original HOME so they can find the config file
    if (getenv("SUDO_HOME")) {
        homedir = getenv("SUDO_HOME");
    } else if (getenv("SUDO_USER")) {
        homedir = "/home/" + std::string(getenv("SUDO_USER"));
    } else if (getenv("HOME")) {
        homedir = getenv("HOME");
    }

    if (homedir.empty()) {
        return {};
    }

    if (homedir == "/root") {
        WARN("Home dir is /root, couldn't find user home");
    }

    std::error_code ec;
    std::filesystem::path config_path = homedir;
    config_path /= ".config";
    if (!std::filesystem::exists(config_path, ec)) {
        bool ok = std::filesystem::create_directories(config_path, ec);
        if (!ok) {
            return {};
        }
    } else if (!std::filesystem::is_directory(config_path, ec)) {
        return {};
    }

    config_path /= "felix86";
    if (!std::filesystem::exists(config_path, ec)) {
        bool ok = std::filesystem::create_directory(config_path, ec);
        if (!ok) {
            return {};
        }
    } else if (!std::filesystem::is_directory(config_path, ec)) {
        return {};
    }

    return config_path;
}

void addToEnvironment(Config& config, const char* env_name, const char* env) {
    config.__environment += "\n";
    config.__environment += env_name;
    config.__environment += "=";
    config.__environment += env;
}

template <typename T>
std::string namify(const T& val);

template <>
std::string namify(const bool& val) {
    return val ? "true" : "false";
}

template <>
std::string namify(const u64& val) {
    return fmt::format("{:x}", val);
}

template <>
std::string namify(const std::filesystem::path& val) {
    return val;
}

template <>
std::string namify(const std::string& val) {
    return val;
}

bool is_truthy(const char* str) {
    if (!str) {
        return false;
    }

    std::string lower = str;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    return lower == "true" || lower == "1" || lower == "yes" || lower == "on" || lower == "y" || lower == "enable";
}

bool Config::initialize(bool ignore_envs) {
    std::filesystem::path config_path;
    std::filesystem::path profiles_path;
    bool no_config_file = is_truthy(getenv("FELIX86_NO_CONFIG_FILE"));
    if (!no_config_file) {
        const std::filesystem::path config_dir = getConfigDir();
        if (config_dir.empty()) {
            return false;
        }

        config_path = config_dir / "config.toml";
        if (!std::filesystem::exists(config_path)) {
            LOG("Created configuration file: %s", config_path.c_str());
            save(config_path, g_config);

            if (getuid() == 0) {
                // Config file created while running as sudo
                // This can happen if for example the first instance of felix86 happens to be
                // running `sudo --preserve-env=HOME felix86 -b` to register to binfmt_misc
                // See if running through sudo and change permissions
                const char* uid = getenv("SUDO_UID");
                const char* gid = getenv("SUDO_GID");
                bool owner_changed = false;
                if (uid && gid) {
                    long nuid = std::atol(uid);
                    long ngid = std::atol(gid);
                    if (nuid && ngid) {
                        int result = chown(config_path.c_str(), nuid, ngid);
                        if (result == 0) {
                            result = chown(config_dir.c_str(), nuid, ngid);
                            if (result == 0) {
                                owner_changed = true;
                            }
                        }
                    }
                }

                if (!owner_changed) {
                    WARN("The created configuration file %s may be owned by root, which may not be intended", config_path.c_str());
                    WARN("You can change them manually by doing `sudo chown $USER:$USER %s`", config_path.c_str());
                }
            }
        }

        std::error_code ec;
        profiles_path = config_dir / "profiles";
        std::filesystem::create_directories(profiles_path, ec);

        if (!std::filesystem::exists(profiles_path / "extreme.toml", ec)) {
            // Enable all optimizations, even ones that may break programs
            Config extreme_config{};
            extreme_config.link = true;
            extreme_config.address_cache = true;
            extreme_config.unsafe_flags = true;
            extreme_config.opcode_fusing = true;
            extreme_config.inline_syscalls = true;
            extreme_config.inaccurate_minmax = true;
            extreme_config.always_tso = false;
            extreme_config.protect_pages = true; // this one is too breaking to disable
            extreme_config.noflag_opts = true;
            extreme_config.auto_compress = false;
            extreme_config.scan_ahead_multi = true;
            extreme_config.no_address_overflow = true;
            Config::save(profiles_path / "extreme.toml", extreme_config, true);
        }

        if (!std::filesystem::exists(profiles_path / "safe.toml", ec)) {
            // Disable most optimizations
            Config safe_config{};
            safe_config.link = true;
            safe_config.address_cache = true;
            safe_config.unsafe_flags = false;
            safe_config.opcode_fusing = false;
            safe_config.inline_syscalls = false;
            safe_config.inaccurate_minmax = false;
            safe_config.always_tso = true;
            safe_config.protect_pages = true;
            safe_config.noflag_opts = true;
            safe_config.auto_compress = false;
            safe_config.scan_ahead_multi = false;
            safe_config.no_address_overflow = false;
            Config::save(profiles_path / "safe.toml", safe_config, true);
        }

        if (!std::filesystem::exists(profiles_path / "paranoid.toml", ec)) {
            // Disable all optimizations except block linking and enable some safety checks
            Config paranoid_config{};
            paranoid_config.paranoid = true;
            paranoid_config.alignment_check = true;
            paranoid_config.always_flags = true;
            paranoid_config.link = true;
            paranoid_config.address_cache = false;
            paranoid_config.unsafe_flags = false;
            paranoid_config.opcode_fusing = false;
            paranoid_config.inline_syscalls = false;
            paranoid_config.inaccurate_minmax = false;
            paranoid_config.always_tso = true;
            paranoid_config.protect_pages = true;
            paranoid_config.noflag_opts = false;
            paranoid_config.auto_compress = false;
            paranoid_config.scan_ahead_multi = false;
            paranoid_config.no_address_overflow = false;
            Config::save(profiles_path / "paranoid.toml", paranoid_config, true);
        }

        if (!std::filesystem::exists(profiles_path / "zink.toml", ec)) {
            // Enables Vulkan/Wayland thunking and sets environment variables to enable Zink
            Config zink_config{};
            zink_config.enabled_thunks = "vk,wl";
            zink_config.environment = "LIBGL_KOPPER_DRI2=1;MESA_LOADER_DRIVER_OVERRIDE=zink";
            // Set in host environment too for thunks
            zink_config.host_environment = "LIBGL_KOPPER_DRI2=1;MESA_LOADER_DRIVER_OVERRIDE=zink";
            Config::save(profiles_path / "zink.toml", zink_config, true);
        }
    }

    g_config = load(config_path, ignore_envs);
    g_config.config_path = config_path;

    const char* profile = getenv("FELIX86_PROFILE");
    if (profile) {
        std::filesystem::path path;

        // Sets either the absolute profile path or a name of a profile in $HOME/.config/felix86/profiles
        if (profile[0] != '/') {
            std::string sprofile = profile;
            std::transform(sprofile.begin(), sprofile.end(), sprofile.begin(), [](unsigned char c) { return std::tolower(c); });
            path = profiles_path / (sprofile + ".toml");
        } else {
            path = profile;
        }

        std::error_code ec;
        if (std::filesystem::exists(path, ec)) {
            Config::loadProfile(g_config, path);
        } else {
            if (ec) {
                WARN("Error while trying to access profile %s at %s", profile, path.c_str());
            } else {
                WARN("Profile %s doesn't exist at %s", profile, path.c_str());
            }
        }
    }

    // g_config can be changed, g_initial_config won't be changed
    g_initial_config = g_config;

#define X(group, type, name, default_value, env_name, description)                                                                                   \
    if (g_config.name != type{default_value}) {                                                                                                      \
        addToEnvironment(g_config, #env_name, namify(g_config.name).c_str());                                                                        \
    }
#include "config.inc"
#undef X

    return true;
}

template <typename Type>
void addValue(std::string& str, Type& value) {
    if constexpr (std::is_same_v<Type, bool>) {
        str += value ? "1" : "0";
    } else if constexpr (std::is_same_v<Type, u64>) {
        str += std::to_string(value);
    } else if constexpr (std::is_same_v<Type, std::filesystem::path>) {
        str += value.string();
    } else if constexpr (std::is_same_v<Type, std::string>) {
        str += value;
    } else {
        static_assert(false);
    }
}

std::string Config::getConfigHex() {
    std::string str;
#define X(group, type, name, default_value, env_name, description)                                                                                   \
    {                                                                                                                                                \
        str += #env_name;                                                                                                                            \
        str += "=";                                                                                                                                  \
        addValue(str, g_initial_config.name);                                                                                                        \
        str += "\n";                                                                                                                                 \
    }
#include "config.inc"
#undef X

    if (str.back() == '\n') {
        str.pop_back();
    }

    std::string hex_string = string_to_hex(str);
    return hex_string;
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
bool loadFromToml(const toml_result_t& toml, const char* group, const char* name, Type& value) {
    toml_datum_t table = toml_get(toml.toptab, group);
    if (table.type != TOML_UNKNOWN) {
        if (table.type != TOML_TABLE) {
            WARN("Expected %s to be table when opening toml file", group);
            return false;
        }
        toml_datum_t member = toml_get(table, name);
        if (member.type != TOML_UNKNOWN) {
            if constexpr (std::is_same_v<Type, bool>) {
                if (member.type != TOML_BOOLEAN) {
                    WARN("Expected %s to be boolean but it has type of %d", name, member.type);
                    return false;
                }
                value = member.u.boolean;
                return true;
            } else if constexpr (std::is_same_v<Type, u64>) {
                if (member.type != TOML_INT64) {
                    WARN("Expected %s to be int64 but it has type of %d", name, member.type);
                    return false;
                }
                value = member.u.int64;
                return true;
            } else if constexpr (std::is_same_v<Type, std::filesystem::path>) {
                if (member.type != TOML_STRING) {
                    WARN("Expected %s to be string but it has type of %d", name, member.type);
                    return false;
                }
                value = std::string(member.u.str.ptr, member.u.str.len);
                return true;
            } else if constexpr (std::is_same_v<Type, std::string>) {
                if (member.type != TOML_STRING) {
                    WARN("Expected %s to be string but it has type of %d", name, member.type);
                    return false;
                }
                value = std::string(member.u.str.ptr, member.u.str.len);
                return true;
            } else {
                static_assert(false);
            }
        }
    }
    return false;
}

template <typename Type>
bool setFromString(Config& config, Type& value, const char* str) {
    if constexpr (std::is_same_v<Type, bool>) {
        value = is_truthy(str);
        return true;
    } else if constexpr (std::is_same_v<Type, u64>) {
        value = get_int(str);
        return true;
    } else if constexpr (std::is_same_v<Type, std::filesystem::path>) {
        value = str;
        return true;
    } else if constexpr (std::is_same_v<Type, std::string>) {
        value = str;
        return true;
    } else {
        static_assert(false);
    }

    return false;
}

void Config::initializeChild() {
    const char* env = getenv("__FELIX86_CONFIG");
    if (!env) {
        printf("Failed to initialize config. __FELIX86_CONFIG from parent is null\n");
        exit(1);
    }

    std::string senv_hex = env;
    if (senv_hex.empty()) {
        printf("Config hex string is empty\n");
        exit(1);
    }

    if (senv_hex.size() % 2 != 0) {
        printf("Config hex string is bad: %s\n", env);
        exit(1);
    }

    // The config string is a hex string so it can contain any character and newlines with no potential issues
    Config config = {};
    std::string senv = hex_to_string(senv_hex);
    std::unordered_map<std::string, std::string> env_map;
    std::vector<std::string> envs = split_string(senv, '\n');
    for (auto& str : envs) {
        auto it = str.find("=");
        ASSERT(it != std::string::npos);
        std::string name = str.substr(0, it);
        std::string value = str.substr(it + 1);
        env_map[name] = value;
    }

#define X(group, type, name, default_value, env_name, description)                                                                                   \
    {                                                                                                                                                \
        bool loaded = false;                                                                                                                         \
        loaded = setFromString<type>(config, config.name, env_map.at(#env_name).c_str());                                                            \
        if (!loaded) {                                                                                                                               \
            ERROR("Failed to load option " #env_name);                                                                                               \
        }                                                                                                                                            \
    }
#include "config.inc"
#undef X

    g_config = config;
    g_initial_config = config;
}

Config Config::load(const std::filesystem::path& path, bool ignore_envs) {
    Config config = {};

    bool no_config_path = path.empty();
    toml_result_t result;
    if (!no_config_path) {
        result = toml_parse_file_ex(path.c_str());
        if (!result.ok) {
            WARN("Failed to parse toml file %s with error: %s", path.c_str(), result.errmsg);
            return config;
        }
    } else if (ignore_envs) {
        // Ignore environment variables and no file... nothing to load, return default config
        return config;
    }

#define X(group, type, name, default_value, env_name, description)                                                                                   \
    {                                                                                                                                                \
        [[maybe_unused]] bool loaded = false;                                                                                                        \
        const char* env = getenv(#env_name);                                                                                                         \
        if (env && !ignore_envs) {                                                                                                                   \
            loaded = setFromString<type>(config, config.name, env);                                                                                  \
            if (loaded) config.src.name = ConfigSource::Env;                                                                                         \
        } else if (!no_config_path) {                                                                                                                \
            loaded = loadFromToml<type>(result, #group, #name, config.name);                                                                         \
            if (loaded) config.src.name = ConfigSource::File;                                                                                        \
        }                                                                                                                                            \
    }
#include "config.inc"
#undef X

    if (!no_config_path) {
        toml_free(result);
    }
    return config;
}

bool Config::loadProfile(Config& config, const std::filesystem::path& profile) {
    ASSERT(!profile.empty());
    toml_result_t result = toml_parse_file_ex(profile.c_str());
    if (!result.ok) {
        WARN("Failed to parse toml file %s with error: %s", profile.c_str(), result.errmsg);
        return false;
    }

#define X(group, type, name, default_value, env_name, description)                                                                                   \
    {                                                                                                                                                \
        (void)loadFromToml<type>(result, #group, #name, config.name);                                                                                \
    }
#include "config.inc"
#undef X
    return true;
}

template <typename T>
std::string stringify_toml(const T& value) {
    static_assert(false);
    return "";
}

template <>
std::string stringify_toml<std::string>(const std::string& value) {
    return '\"' + value + '\"';
}

template <>
std::string stringify_toml<std::filesystem::path>(const std::filesystem::path& value) {
    return '\"' + value.string() + '\"';
}

template <>
std::string stringify_toml<bool>(const bool& value) {
    return value ? "true" : "false";
}

template <>
std::string stringify_toml<u64>(const u64& value) {
    return fmt::format("{:#x}", value);
}

void Config::save(const std::filesystem::path& path, const Config& config, bool only_non_default) {
    std::string toml;
    std::string current_group;

#define X(group, type, name, default_value, env_name, description)                                                                                   \
    if (!only_non_default || config.name != default_value) {                                                                                         \
        if (current_group != #group) {                                                                                                               \
            current_group = #group;                                                                                                                  \
            toml += "[" #group "]\n";                                                                                                                \
        }                                                                                                                                            \
        toml += "# " #name " (" #type ")\n";                                                                                                         \
        toml += "# Description: " description "\n";                                                                                                  \
        toml += "# Environment variable: " #env_name "\n";                                                                                           \
        toml += #name " = " + stringify_toml<type>(config.name) + "\n\n";                                                                            \
    }
#include "config.inc"
#undef X

    std::ofstream ofs(path);
    if (!ofs.is_open()) {
        WARN("Failed to open config file %s to edit it", path.c_str());
        return;
    }
    ofs << "# Autogenerated TOML configuration file for felix86\n";
    ofs << "# You may change any values here, or their respective environment variable\n";
    ofs << "# The environment variables override the values here\n\n";
    ofs << toml;
}

static std::string lowercase(const char* str) {
    std::string s(str);
    for_each(s.begin(), s.end(), [](char& c) { c = tolower(c); });
    return s;
}

template <typename T>
std::string stringify_shell(const T& value) {
    static_assert(false);
    return "";
}

template <>
std::string stringify_shell<std::string>(const std::string& value) {
    return value;
}

template <>
std::string stringify_shell<std::filesystem::path>(const std::filesystem::path& value) {
    return value;
}

template <>
std::string stringify_shell<bool>(const bool& value) {
    return value ? "true" : "false";
}

template <>
std::string stringify_shell<u64>(const u64& value) {
    return fmt::format("{:#x}", value);
}

std::optional<std::string> Config::getConfigString(const char* group, const char* field) {
    std::string group_lower = lowercase(group);
    std::string field_lower = lowercase(field);
    // cmp_group and cmp_name are marked 'static' to avoid initializing more than once (first call of function). This is thread safe.
#define X(group_, type_, name_, ...)                                                                                                                 \
    {                                                                                                                                                \
        static const std::string cmp_group = lowercase(#group_);                                                                                     \
        static const std::string cmp_name = lowercase(#name_);                                                                                       \
        if (group_lower == cmp_group && field_lower == cmp_name) {                                                                                   \
            return std::optional{stringify_shell<type_>(this->name_)};                                                                               \
        }                                                                                                                                            \
    }
#include "config.inc"
#undef X
    return std::optional<std::string>{};
}

std::optional<ConfigSource> Config::getConfigSource(const char* group, const char* field) {
    std::string group_lower = lowercase(group);
    std::string field_lower = lowercase(field);
    // cmp_group and cmp_name are marked 'static' to avoid initializing more than once (first call of function). This is thread safe.
#define X(group_, type_, name_, ...)                                                                                                                 \
    {                                                                                                                                                \
        static const std::string cmp_group = lowercase(#group_);                                                                                     \
        static const std::string cmp_name = lowercase(#name_);                                                                                       \
        if (group_lower == cmp_group && field_lower == cmp_name) {                                                                                   \
            return std::optional{this->src.name_};                                                                                                   \
        }                                                                                                                                            \
    }
#include "config.inc"
#undef X
    return std::optional<ConfigSource>{};
}

std::optional<std::string> Config::getConfigSourceString(const char* group, const char* field) {
    std::optional<ConfigSource> src = getConfigSource(group, field);
    if (src.has_value()) {
        const char* s = "";
        switch (src.value()) {
            case ConfigSource::Default: s = "default"; break;
            case ConfigSource::Env: s = "env"; break;
            case ConfigSource::File: s = "file"; break;
        }
        return std::optional<std::string>(s);
    } else {
        return std::optional<std::string>();
    }
}


bool Config::setConfigString(const char* group, const char* field, const char* value) {
    std::string group_lower = lowercase(group);
    std::string field_lower = lowercase(field);
    // cmp_group and cmp_name are marked 'static' to avoid initializing more than once (first call of function). This is thread safe.
#define X(group_, type_, name_, ...)                                                                                                                 \
    {                                                                                                                                                \
        static const std::string cmp_group = lowercase(#group_);                                                                                     \
        static const std::string cmp_name = lowercase(#name_);                                                                                       \
        if (group_lower == cmp_group && field_lower == cmp_name) {                                                                                   \
            return setFromString<type_>(*this, this->name_, value);                                                                                  \
        }                                                                                                                                            \
    }
#include "config.inc"
#undef X
    return false;
}
