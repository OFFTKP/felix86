#include <filesystem>
#include <system_error>
#include <pwd.h>
#include <sys/types.h>
#include <toml.hpp>
#include "felix86/common/config.hpp"
#include "felix86/common/log.hpp"
#include "felix86/common/types.hpp"
#include "felix86/common/utility.hpp"
#include "fmt/format.h"

Config g_config{};
Config g_initial_config{};

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

std::filesystem::path Config::getConfigDir() {
    const char* homedir;

    // If SUDO_HOME is defined, use that as the home directory
    // `sudo` sets SUDO_HOME to the original HOME, and HOME to /root
    // We want felix86 instances running under sudo to use the original HOME so they can find the config file
    if (getenv("SUDO_HOME")) {
        homedir = getenv("SUDO_HOME");
    } else {
        homedir = getenv("HOME");
    }

    if (!homedir) {
        return {};
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

bool Config::initialize(bool ignore_envs) {
    const std::filesystem::path config_dir = getConfigDir();
    if (config_dir.empty()) {
        return false;
    }

    std::filesystem::path config_path = config_dir / "config.toml";
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
                        owner_changed = true;
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
    std::filesystem::path profiles_path = config_dir / "profiles";
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
        extreme_config.pclmulqdq = true;
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
        safe_config.pclmulqdq = false;
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
        paranoid_config.pclmulqdq = false;
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

    // g_config can be changed, c_initial_config won't be changed
    g_initial_config = g_config;

#define X(group, type, name, default_value, env_name, description, required)                                                                         \
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
#define X(group, type, name, default_value, env_name, description, required)                                                                         \
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

bool Config::addTrustedPath(const std::filesystem::path& path) {
    const std::filesystem::path config_dir = getConfigDir();
    const std::filesystem::path trusted_paths = config_dir / "trusted.txt";
    {
        std::string line;
        std::ifstream file(trusted_paths);
        while (std::getline(file, line)) {
            if (line == path) {
                return true;
            }
        }
    }

    std::ofstream out(trusted_paths, std::ios_base::app | std::ios_base::out);
    out << path.string() << "\n";
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
            } else if constexpr (std::is_same_v<Type, std::string>) {
                value = value_toml.as_string();
                return true;
            } else {
                static_assert(false);
            }
        }
    }
    return false;
}

template <typename Type>
bool loadFromEnv(Config& config, Type& value, const char* env) {
    if constexpr (std::is_same_v<Type, bool>) {
        value = is_truthy(env);
        return true;
    } else if constexpr (std::is_same_v<Type, u64>) {
        value = get_int(env);
        return true;
    } else if constexpr (std::is_same_v<Type, std::filesystem::path>) {
        value = env;
        return true;
    } else if constexpr (std::is_same_v<Type, std::string>) {
        value = env;
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

#define X(group, type, name, default_value, env_name, description, required)                                                                         \
    {                                                                                                                                                \
        bool loaded = false;                                                                                                                         \
        loaded = loadFromEnv<type>(config, config.name, env_map.at(#env_name).c_str());                                                              \
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

    auto attempt = toml::try_parse(path);
    if (attempt.is_err()) {
        return config;
    }

    auto toml = attempt.unwrap();

#define X(group, type, name, default_value, env_name, description, required)                                                                         \
    {                                                                                                                                                \
        bool loaded = false;                                                                                                                         \
        const char* env = getenv(#env_name);                                                                                                         \
        if (env && !ignore_envs) {                                                                                                                   \
            loaded = loadFromEnv<type>(config, config.name, env);                                                                                    \
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

bool Config::loadProfile(Config& config, const std::filesystem::path& profile) {
    auto attempt = toml::try_parse(profile);
    if (attempt.is_err()) {
        return false;
    }

    auto toml = attempt.unwrap();

#define X(group, type, name, default_value, env_name, description, required)                                                                         \
    {                                                                                                                                                \
        (void)loadFromToml<type>(toml, #group, #name, config.name);                                                                                  \
    }
#include "config.inc"
#undef X
    return true;
}

void Config::save(const std::filesystem::path& path, const Config& config, bool only_non_default) {
    toml::ordered_table toml;

#define X(group, type, name, default_value, env_name, description, required)                                                                         \
    if (!only_non_default || config.name != default_value) {                                                                                         \
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
    if (!ofs.is_open()) {
        WARN("Failed to open config file %s to edit it", path.c_str());
        return;
    }
    ofs << "# Autogenerated TOML configuration file for felix86\n";
    ofs << "# You may change any values here, or their respective environment variable\n";
    ofs << "# The environment variables override the values here\n";
    ofs << toml::ordered_value{toml};
}