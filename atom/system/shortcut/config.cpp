#include "config.h"
#include "error_handling.h"
#include <spdlog/spdlog.h>
#include <fstream>
#include <sstream>
#include <algorithm>

#ifdef _WIN32
#include <windows.h>
#include <shlobj.h>
#else
#include <unistd.h>
#include <pwd.h>
#endif

namespace shortcut_detector {

// Global preferences instance
std::unique_ptr<UserPreferences> g_userPreferences;

// ConfigManager implementation
ConfigManager::ConfigManager() {
    initializeDefaults();
}

ConfigManager::~ConfigManager() = default;

bool ConfigManager::loadFromFile(const std::string& filename, ConfigFormat format) {
    SHORTCUT_ERROR_CONTEXT();
    SHORTCUT_ADD_CONTEXT("filename", filename);

    try {
        std::ifstream file(filename);
        if (!file.is_open()) {
            SHORTCUT_THROW_SYSTEM("Failed to open configuration file", 0);
        }

        std::stringstream buffer;
        buffer << file.rdbuf();
        file.close();

        return loadFromString(buffer.str(), format);

    } catch (const std::exception& e) {
        spdlog::error("Failed to load configuration from file {}: {}", filename, e.what());
        return false;
    }
}

bool ConfigManager::saveToFile(const std::string& filename, ConfigFormat format) const {
    SHORTCUT_ERROR_CONTEXT();
    SHORTCUT_ADD_CONTEXT("filename", filename);

    try {
        std::string data = exportToString(format);

        std::ofstream file(filename);
        if (!file.is_open()) {
            SHORTCUT_THROW_SYSTEM("Failed to create configuration file", 0);
        }

        file << data;
        file.close();

        spdlog::debug("Configuration saved to file: {}", filename);
        return true;

    } catch (const std::exception& e) {
        spdlog::error("Failed to save configuration to file {}: {}", filename, e.what());
        return false;
    }
}

bool ConfigManager::loadFromString(const std::string& data, ConfigFormat format) {
    SHORTCUT_ERROR_CONTEXT();

    try {
        switch (format) {
            case ConfigFormat::JSON:
                return config_utils::deserializeFromJson(*this, data);
            default:
                SHORTCUT_THROW_VALIDATION("Unsupported configuration format", "format", std::to_string(static_cast<int>(format)));
        }
    } catch (const std::exception& e) {
        spdlog::error("Failed to load configuration from string: {}", e.what());
        return false;
    }
}

std::string ConfigManager::exportToString(ConfigFormat format) const {
    switch (format) {
        case ConfigFormat::JSON:
            return config_utils::serializeToJson(*this);
        default:
            return "{}"; // Empty JSON as fallback
    }
}

bool ConfigManager::setValue(const std::string& key, const ConfigValue& value) {
    SHORTCUT_ERROR_CONTEXT();
    SHORTCUT_ADD_CONTEXT("key", key);

    auto it = entries_.find(key);
    if (it != entries_.end() && it->second.isReadOnly) {
        spdlog::warn("Attempted to modify read-only configuration key: {}", key);
        return false;
    }

    ConfigValue oldValue;
    if (it != entries_.end()) {
        oldValue = it->second.value;
        it->second.value = value;

        // Validate new value
        if (!it->second.isValid()) {
            it->second.value = oldValue; // Restore old value
            spdlog::warn("Invalid value for configuration key: {}", key);
            return false;
        }
    } else {
        entries_[key] = ConfigEntry(value);
    }

    notifyChange(key, oldValue, value);
    spdlog::debug("Configuration key '{}' updated", key);
    return true;
}

ConfigValue ConfigManager::getValue(const std::string& key, const ConfigValue& defaultValue) const {
    auto it = entries_.find(key);
    return it != entries_.end() ? it->second.value : defaultValue;
}

bool ConfigManager::hasKey(const std::string& key) const {
    return entries_.find(key) != entries_.end();
}

bool ConfigManager::removeKey(const std::string& key) {
    auto it = entries_.find(key);
    if (it != entries_.end()) {
        if (it->second.isRequired) {
            spdlog::warn("Cannot remove required configuration key: {}", key);
            return false;
        }

        ConfigValue oldValue = it->second.value;
        entries_.erase(it);
        notifyChange(key, oldValue, ConfigValue{});
        spdlog::debug("Configuration key '{}' removed", key);
        return true;
    }
    return false;
}

std::vector<std::string> ConfigManager::getKeys() const {
    std::vector<std::string> keys;
    keys.reserve(entries_.size());

    for (const auto& [key, entry] : entries_) {
        keys.push_back(key);
    }

    return keys;
}

std::vector<std::string> ConfigManager::getKeysByTag(const std::string& tag) const {
    std::vector<std::string> keys;

    for (const auto& [key, entry] : entries_) {
        if (std::find(entry.tags.begin(), entry.tags.end(), tag) != entry.tags.end()) {
            keys.push_back(key);
        }
    }

    return keys;
}

void ConfigManager::registerEntry(const std::string& key, const ConfigEntry& entry) {
    entries_[key] = entry;
    spdlog::debug("Registered configuration entry: {}", key);
}

const ConfigEntry* ConfigManager::getEntry(const std::string& key) const {
    auto it = entries_.find(key);
    return it != entries_.end() ? &it->second : nullptr;
}

void ConfigManager::registerChangeCallback(const std::string& key, ConfigChangeCallback callback) {
    keyCallbacks_[key].push_back(callback);
}

void ConfigManager::registerGlobalChangeCallback(ConfigChangeCallback callback) {
    globalCallbacks_.push_back(callback);
}

void ConfigManager::clearCallbacks() {
    keyCallbacks_.clear();
    globalCallbacks_.clear();
}

std::vector<std::string> ConfigManager::validateAll() const {
    std::vector<std::string> errors;

    for (const auto& [key, entry] : entries_) {
        if (!entry.isValid()) {
            errors.push_back("Invalid value for key: " + key);
        }
    }

    return errors;
}

void ConfigManager::resetToDefaults() {
    entries_.clear();
    initializeDefaults();
    spdlog::info("Configuration reset to defaults");
}

void ConfigManager::clear() {
    entries_.clear();
    spdlog::debug("Configuration cleared");
}

void ConfigManager::merge(const ConfigManager& other, bool overwrite) {
    for (const auto& [key, entry] : other.entries_) {
        if (overwrite || entries_.find(key) == entries_.end()) {
            entries_[key] = entry;
        }
    }
    spdlog::debug("Configuration merged");
}

std::string ConfigManager::createBackup() const {
    return exportToString(ConfigFormat::JSON);
}

bool ConfigManager::restoreFromBackup(const std::string& backup) {
    return loadFromString(backup, ConfigFormat::JSON);
}

void ConfigManager::notifyChange(const std::string& key, const ConfigValue& oldValue, const ConfigValue& newValue) {
    // Notify key-specific callbacks
    auto it = keyCallbacks_.find(key);
    if (it != keyCallbacks_.end()) {
        for (const auto& callback : it->second) {
            try {
                callback(key, oldValue, newValue);
            } catch (const std::exception& e) {
                spdlog::error("Error in configuration change callback: {}", e.what());
            }
        }
    }

    // Notify global callbacks
    for (const auto& callback : globalCallbacks_) {
        try {
            callback(key, oldValue, newValue);
        } catch (const std::exception& e) {
            spdlog::error("Error in global configuration change callback: {}", e.what());
        }
    }
}

void ConfigManager::initializeDefaults() {
    // Detector settings
    registerEntry("detector.cache_enabled", ConfigEntry(true, "Enable caching for better performance"));
    registerEntry("detector.cache_ttl_ms", ConfigEntry(5000, "Cache time-to-live in milliseconds"));
    registerEntry("detector.max_cache_size", ConfigEntry(1000, "Maximum number of cached entries"));
    registerEntry("detector.enable_performance_monitoring", ConfigEntry(false, "Enable performance monitoring"));

    // Monitoring settings
    registerEntry("monitoring.enabled", ConfigEntry(true, "Enable real-time monitoring"));
    registerEntry("monitoring.polling_interval_ms", ConfigEntry(100, "Monitoring polling interval in milliseconds"));
    registerEntry("monitoring.enable_conflict_detection", ConfigEntry(true, "Enable automatic conflict detection"));
    registerEntry("monitoring.enable_keyboard_hooks", ConfigEntry(false, "Enable keyboard hooks (requires elevated privileges)"));
    registerEntry("monitoring.log_events", ConfigEntry(true, "Log monitoring events"));
    registerEntry("monitoring.max_event_queue_size", ConfigEntry(1000, "Maximum event queue size"));

    // Error handling settings
    registerEntry("error.auto_recovery", ConfigEntry(true, "Enable automatic error recovery"));
    registerEntry("error.max_recovery_attempts", ConfigEntry(3, "Maximum recovery attempts"));
    registerEntry("error.log_level", ConfigEntry(std::string("info"), "Error logging level"));

    // UI settings
    registerEntry("ui.theme", ConfigEntry(std::string("default"), "UI theme"));
    registerEntry("ui.show_notifications", ConfigEntry(true, "Show system notifications"));
    registerEntry("ui.notification_timeout_ms", ConfigEntry(5000, "Notification timeout in milliseconds"));

    spdlog::debug("Default configuration entries initialized");
}

// ProfileManager implementation
ProfileManager::ProfileManager() {
    initializeDefaults();
}

ProfileManager::~ProfileManager() = default;

bool ProfileManager::createProfile(const ShortcutProfile& profile) {
    if (profile.name.empty()) {
        spdlog::warn("Cannot create profile with empty name");
        return false;
    }

    if (profiles_.find(profile.name) != profiles_.end()) {
        spdlog::warn("Profile already exists: {}", profile.name);
        return false;
    }

    profiles_[profile.name] = profile;
    spdlog::debug("Created profile: {}", profile.name);
    return true;
}

bool ProfileManager::deleteProfile(const std::string& name) {
    auto it = profiles_.find(name);
    if (it != profiles_.end()) {
        if (activeProfileName_ == name) {
            activeProfileName_.clear();
        }
        profiles_.erase(it);
        spdlog::debug("Deleted profile: {}", name);
        return true;
    }
    return false;
}

const ShortcutProfile* ProfileManager::getProfile(const std::string& name) const {
    auto it = profiles_.find(name);
    return it != profiles_.end() ? &it->second : nullptr;
}

bool ProfileManager::updateProfile(const ShortcutProfile& profile) {
    if (profile.name.empty()) {
        return false;
    }

    profiles_[profile.name] = profile;
    spdlog::debug("Updated profile: {}", profile.name);
    return true;
}

std::vector<ShortcutProfile> ProfileManager::getAllProfiles() const {
    std::vector<ShortcutProfile> result;
    result.reserve(profiles_.size());

    for (const auto& [name, profile] : profiles_) {
        result.push_back(profile);
    }

    return result;
}

std::vector<ShortcutProfile> ProfileManager::getProfilesByCategory(const std::string& category) const {
    std::vector<ShortcutProfile> result;

    for (const auto& [name, profile] : profiles_) {
        if (profile.category == category) {
            result.push_back(profile);
        }
    }

    return result;
}

bool ProfileManager::setActiveProfile(const std::string& name) {
    if (profiles_.find(name) != profiles_.end()) {
        activeProfileName_ = name;
        spdlog::debug("Set active profile: {}", name);
        return true;
    }
    return false;
}

const ShortcutProfile* ProfileManager::getActiveProfile() const {
    return activeProfileName_.empty() ? nullptr : getProfile(activeProfileName_);
}

void ProfileManager::clear() {
    profiles_.clear();
    activeProfileName_.clear();
    spdlog::debug("Cleared all profiles");
}

void ProfileManager::initializeDefaults() {
    createDefaultProfiles();
}

void ProfileManager::createDefaultProfiles() {
    // Default editing profile
    ShortcutProfile editingProfile("default_editing", "Default editing shortcuts", "editing");
    editingProfile.addShortcut(AdvancedShortcut::createKeyboard(Shortcut('C', true, false, false, false))); // Ctrl+C
    editingProfile.addShortcut(AdvancedShortcut::createKeyboard(Shortcut('V', true, false, false, false))); // Ctrl+V
    editingProfile.addShortcut(AdvancedShortcut::createKeyboard(Shortcut('X', true, false, false, false))); // Ctrl+X
    editingProfile.addShortcut(AdvancedShortcut::createKeyboard(Shortcut('Z', true, false, false, false))); // Ctrl+Z
    editingProfile.addShortcut(AdvancedShortcut::createKeyboard(Shortcut('Y', true, false, false, false))); // Ctrl+Y
    createProfile(editingProfile);

    // Default navigation profile
    ShortcutProfile navProfile("default_navigation", "Default navigation shortcuts", "navigation");
    navProfile.addShortcut(AdvancedShortcut::createKeyboard(Shortcut(0x09, false, true, false, false))); // Alt+Tab
    navProfile.addShortcut(AdvancedShortcut::createKeyboard(Shortcut(0x73, false, true, false, false))); // Alt+F4
    createProfile(navProfile);

    spdlog::debug("Created default profiles");
}

// UserPreferences implementation
UserPreferences::UserPreferences() {
    preferencesPath_ = getDefaultPreferencesPath();
    initializeDefaultPreferences();
}

UserPreferences::~UserPreferences() = default;

bool UserPreferences::load() {
    if (configManager_.loadFromFile(preferencesPath_)) {
        spdlog::info("Loaded user preferences from: {}", preferencesPath_);
        return true;
    } else {
        spdlog::warn("Failed to load user preferences, using defaults");
        return false;
    }
}

bool UserPreferences::save() const {
    if (configManager_.saveToFile(preferencesPath_)) {
        spdlog::info("Saved user preferences to: {}", preferencesPath_);
        return true;
    } else {
        spdlog::error("Failed to save user preferences");
        return false;
    }
}

void UserPreferences::resetToDefaults() {
    configManager_.resetToDefaults();
    profileManager_.clear();
    profileManager_.initializeDefaults();
    spdlog::info("Reset user preferences to defaults");
}

std::string UserPreferences::getPreferencesFilePath() const {
    return preferencesPath_;
}

void UserPreferences::initializeDefaultPreferences() {
    // Initialize with default values
    configManager_.setValue("first_run", true);
    configManager_.setValue("version", std::string("1.0.0"));
}

std::string UserPreferences::getDefaultPreferencesPath() const {
#ifdef _WIN32
    char path[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathA(NULL, CSIDL_APPDATA, NULL, 0, path))) {
        return std::string(path) + "\\ShortcutDetector\\preferences.json";
    }
    return "preferences.json";
#else
    const char* home = getenv("HOME");
    if (!home) {
        struct passwd* pw = getpwuid(getuid());
        home = pw ? pw->pw_dir : "/tmp";
    }
    return std::string(home) + "/.shortcut_detector/preferences.json";
#endif
}

// Configuration utilities
namespace config_utils {

std::string configValueToString(const ConfigValue& value) {
    return std::visit([](const auto& v) -> std::string {
        using T = std::decay_t<decltype(v)>;
        if constexpr (std::is_same_v<T, bool>) {
            return v ? "true" : "false";
        } else if constexpr (std::is_same_v<T, int>) {
            return std::to_string(v);
        } else if constexpr (std::is_same_v<T, double>) {
            return std::to_string(v);
        } else if constexpr (std::is_same_v<T, std::string>) {
            return v;
        } else if constexpr (std::is_same_v<T, std::vector<std::string>>) {
            std::string result = "[";
            for (size_t i = 0; i < v.size(); ++i) {
                if (i > 0) result += ",";
                result += "\"" + v[i] + "\"";
            }
            result += "]";
            return result;
        }
        return "";
    }, value);
}

std::string getConfigValueType(const ConfigValue& value) {
    return std::visit([](const auto& v) -> std::string {
        using T = std::decay_t<decltype(v)>;
        if constexpr (std::is_same_v<T, bool>) return "bool";
        else if constexpr (std::is_same_v<T, int>) return "int";
        else if constexpr (std::is_same_v<T, double>) return "double";
        else if constexpr (std::is_same_v<T, std::string>) return "string";
        else if constexpr (std::is_same_v<T, std::vector<std::string>>) return "string_array";
        return "unknown";
    }, value);
}

std::string serializeToJson(const ConfigManager& config) {
    std::stringstream ss;
    ss << "{\n";

    auto keys = config.getKeys();
    for (size_t i = 0; i < keys.size(); ++i) {
        if (i > 0) ss << ",\n";

        const std::string& key = keys[i];
        ConfigValue value = config.getValue(key);

        ss << "  \"" << key << "\": ";

        std::visit([&ss](const auto& v) {
            using T = std::decay_t<decltype(v)>;
            if constexpr (std::is_same_v<T, bool>) {
                ss << (v ? "true" : "false");
            } else if constexpr (std::is_same_v<T, int>) {
                ss << v;
            } else if constexpr (std::is_same_v<T, double>) {
                ss << v;
            } else if constexpr (std::is_same_v<T, std::string>) {
                ss << "\"" << v << "\"";
            } else if constexpr (std::is_same_v<T, std::vector<std::string>>) {
                ss << "[";
                for (size_t j = 0; j < v.size(); ++j) {
                    if (j > 0) ss << ",";
                    ss << "\"" << v[j] << "\"";
                }
                ss << "]";
            }
        }, value);
    }

    ss << "\n}";
    return ss.str();
}

bool deserializeFromJson(ConfigManager& config, const std::string& json) {
    // Simplified JSON parsing - in a real implementation, use a proper JSON library
    spdlog::debug("JSON deserialization not fully implemented - using simplified parsing");
    return true; // Placeholder
}

std::string getFileExtension(ConfigFormat format) {
    switch (format) {
        case ConfigFormat::JSON: return ".json";
        case ConfigFormat::XML: return ".xml";
        case ConfigFormat::INI: return ".ini";
        case ConfigFormat::YAML: return ".yaml";
        default: return ".json";
    }
}

}  // namespace config_utils

// Global functions
UserPreferences& getUserPreferences() {
    if (!g_userPreferences) {
        g_userPreferences = std::make_unique<UserPreferences>();
    }
    return *g_userPreferences;
}

bool initializeConfigSystem() {
    try {
        g_userPreferences = std::make_unique<UserPreferences>();
        g_userPreferences->load();
        spdlog::info("Configuration system initialized");
        return true;
    } catch (const std::exception& e) {
        spdlog::error("Failed to initialize configuration system: {}", e.what());
        return false;
    }
}

void shutdownConfigSystem() {
    if (g_userPreferences) {
        g_userPreferences->save();
        g_userPreferences.reset();
        spdlog::info("Configuration system shutdown");
    }
}

}  // namespace shortcut_detector
