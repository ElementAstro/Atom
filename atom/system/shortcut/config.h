#pragma once

#include <string>
#include <unordered_map>
#include <vector>
#include <memory>
#include <functional>
#include <chrono>
#include <variant>
#include "advanced_shortcut.h"
#include "monitoring.h"

namespace shortcut_detector {

/**
 * @brief Configuration value types
 */
using ConfigValue = std::variant<bool, int, double, std::string, std::vector<std::string>>;

/**
 * @brief Configuration change callback
 */
using ConfigChangeCallback = std::function<void(const std::string& key, const ConfigValue& oldValue, const ConfigValue& newValue)>;

/**
 * @brief Configuration validation function
 */
using ConfigValidator = std::function<bool(const ConfigValue& value)>;

/**
 * @brief Configuration entry with metadata
 */
struct ConfigEntry {
    ConfigValue value;
    std::string description;
    ConfigValidator validator;
    bool isRequired{false};
    bool isReadOnly{false};
    std::vector<std::string> tags;
    
    ConfigEntry() = default;
    ConfigEntry(const ConfigValue& val, const std::string& desc = "", 
               ConfigValidator valid = nullptr, bool required = false, bool readOnly = false)
        : value(val), description(desc), validator(valid), isRequired(required), isReadOnly(readOnly) {}
    
    bool isValid() const {
        return !validator || validator(value);
    }
    
    void addTag(const std::string& tag) {
        tags.push_back(tag);
    }
};

/**
 * @brief Shortcut profile containing related shortcuts and settings
 */
struct ShortcutProfile {
    std::string name;
    std::string description;
    std::string category;
    std::vector<AdvancedShortcut> shortcuts;
    std::unordered_map<std::string, ConfigValue> settings;
    bool isActive{false};
    
    ShortcutProfile(const std::string& n = "", const std::string& desc = "", const std::string& cat = "")
        : name(n), description(desc), category(cat) {}
    
    void addShortcut(const AdvancedShortcut& shortcut) {
        shortcuts.push_back(shortcut);
    }
    
    void setSetting(const std::string& key, const ConfigValue& value) {
        settings[key] = value;
    }
    
    ConfigValue getSetting(const std::string& key, const ConfigValue& defaultValue = ConfigValue{}) const {
        auto it = settings.find(key);
        return it != settings.end() ? it->second : defaultValue;
    }
};

/**
 * @brief Configuration format types
 */
enum class ConfigFormat {
    JSON,
    XML,
    INI,
    YAML
};

/**
 * @brief Configuration manager for shortcut detector
 */
class ConfigManager {
public:
    ConfigManager();
    ~ConfigManager();
    
    /**
     * @brief Load configuration from file
     */
    bool loadFromFile(const std::string& filename, ConfigFormat format = ConfigFormat::JSON);
    
    /**
     * @brief Save configuration to file
     */
    bool saveToFile(const std::string& filename, ConfigFormat format = ConfigFormat::JSON) const;
    
    /**
     * @brief Load configuration from string
     */
    bool loadFromString(const std::string& data, ConfigFormat format = ConfigFormat::JSON);
    
    /**
     * @brief Export configuration to string
     */
    std::string exportToString(ConfigFormat format = ConfigFormat::JSON) const;
    
    /**
     * @brief Set configuration value
     */
    bool setValue(const std::string& key, const ConfigValue& value);
    
    /**
     * @brief Get configuration value
     */
    ConfigValue getValue(const std::string& key, const ConfigValue& defaultValue = ConfigValue{}) const;
    
    /**
     * @brief Get typed configuration value
     */
    template<typename T>
    T getValue(const std::string& key, const T& defaultValue = T{}) const {
        auto value = getValue(key, ConfigValue{defaultValue});
        if (std::holds_alternative<T>(value)) {
            return std::get<T>(value);
        }
        return defaultValue;
    }
    
    /**
     * @brief Check if key exists
     */
    bool hasKey(const std::string& key) const;
    
    /**
     * @brief Remove configuration key
     */
    bool removeKey(const std::string& key);
    
    /**
     * @brief Get all configuration keys
     */
    std::vector<std::string> getKeys() const;
    
    /**
     * @brief Get keys by tag
     */
    std::vector<std::string> getKeysByTag(const std::string& tag) const;
    
    /**
     * @brief Register configuration entry with metadata
     */
    void registerEntry(const std::string& key, const ConfigEntry& entry);
    
    /**
     * @brief Get configuration entry
     */
    const ConfigEntry* getEntry(const std::string& key) const;
    
    /**
     * @brief Register change callback
     */
    void registerChangeCallback(const std::string& key, ConfigChangeCallback callback);
    
    /**
     * @brief Register global change callback
     */
    void registerGlobalChangeCallback(ConfigChangeCallback callback);
    
    /**
     * @brief Clear all callbacks
     */
    void clearCallbacks();
    
    /**
     * @brief Validate all configuration entries
     */
    std::vector<std::string> validateAll() const;
    
    /**
     * @brief Reset to default values
     */
    void resetToDefaults();
    
    /**
     * @brief Clear all configuration
     */
    void clear();
    
    /**
     * @brief Merge configuration from another manager
     */
    void merge(const ConfigManager& other, bool overwrite = true);
    
    /**
     * @brief Create configuration backup
     */
    std::string createBackup() const;
    
    /**
     * @brief Restore from backup
     */
    bool restoreFromBackup(const std::string& backup);

private:
    std::unordered_map<std::string, ConfigEntry> entries_;
    std::unordered_map<std::string, std::vector<ConfigChangeCallback>> keyCallbacks_;
    std::vector<ConfigChangeCallback> globalCallbacks_;
    
    void notifyChange(const std::string& key, const ConfigValue& oldValue, const ConfigValue& newValue);
    void initializeDefaults();
};

/**
 * @brief Profile manager for shortcut profiles
 */
class ProfileManager {
public:
    ProfileManager();
    ~ProfileManager();
    
    /**
     * @brief Create new profile
     */
    bool createProfile(const ShortcutProfile& profile);
    
    /**
     * @brief Delete profile
     */
    bool deleteProfile(const std::string& name);
    
    /**
     * @brief Get profile
     */
    const ShortcutProfile* getProfile(const std::string& name) const;
    
    /**
     * @brief Update profile
     */
    bool updateProfile(const ShortcutProfile& profile);
    
    /**
     * @brief Get all profiles
     */
    std::vector<ShortcutProfile> getAllProfiles() const;
    
    /**
     * @brief Get profiles by category
     */
    std::vector<ShortcutProfile> getProfilesByCategory(const std::string& category) const;
    
    /**
     * @brief Set active profile
     */
    bool setActiveProfile(const std::string& name);
    
    /**
     * @brief Get active profile
     */
    const ShortcutProfile* getActiveProfile() const;
    
    /**
     * @brief Import profiles from file
     */
    bool importFromFile(const std::string& filename, ConfigFormat format = ConfigFormat::JSON);
    
    /**
     * @brief Export profiles to file
     */
    bool exportToFile(const std::string& filename, ConfigFormat format = ConfigFormat::JSON) const;
    
    /**
     * @brief Import profiles from string
     */
    bool importFromString(const std::string& data, ConfigFormat format = ConfigFormat::JSON);
    
    /**
     * @brief Export profiles to string
     */
    std::string exportToString(ConfigFormat format = ConfigFormat::JSON) const;
    
    /**
     * @brief Clear all profiles
     */
    void clear();
    
    /**
     * @brief Initialize default profiles
     */
    void initializeDefaults();

private:
    std::unordered_map<std::string, ShortcutProfile> profiles_;
    std::string activeProfileName_;
    
    void createDefaultProfiles();
};

/**
 * @brief User preferences manager
 */
class UserPreferences {
public:
    UserPreferences();
    ~UserPreferences();
    
    /**
     * @brief Load user preferences
     */
    bool load();
    
    /**
     * @brief Save user preferences
     */
    bool save() const;
    
    /**
     * @brief Get preference value
     */
    template<typename T>
    T getPreference(const std::string& key, const T& defaultValue = T{}) const {
        return configManager_.getValue<T>(key, defaultValue);
    }
    
    /**
     * @brief Set preference value
     */
    template<typename T>
    bool setPreference(const std::string& key, const T& value) {
        return configManager_.setValue(key, ConfigValue{value});
    }
    
    /**
     * @brief Get configuration manager
     */
    ConfigManager& getConfigManager() { return configManager_; }
    const ConfigManager& getConfigManager() const { return configManager_; }
    
    /**
     * @brief Get profile manager
     */
    ProfileManager& getProfileManager() { return profileManager_; }
    const ProfileManager& getProfileManager() const { return profileManager_; }
    
    /**
     * @brief Reset to defaults
     */
    void resetToDefaults();
    
    /**
     * @brief Get preferences file path
     */
    std::string getPreferencesFilePath() const;

private:
    ConfigManager configManager_;
    ProfileManager profileManager_;
    std::string preferencesPath_;
    
    void initializeDefaultPreferences();
    std::string getDefaultPreferencesPath() const;
};

/**
 * @brief Configuration serialization utilities
 */
namespace config_utils {
    /**
     * @brief Convert ConfigValue to string
     */
    std::string configValueToString(const ConfigValue& value);
    
    /**
     * @brief Parse ConfigValue from string
     */
    ConfigValue parseConfigValue(const std::string& str, const std::string& type = "auto");
    
    /**
     * @brief Get ConfigValue type name
     */
    std::string getConfigValueType(const ConfigValue& value);
    
    /**
     * @brief Serialize configuration to JSON
     */
    std::string serializeToJson(const ConfigManager& config);
    
    /**
     * @brief Deserialize configuration from JSON
     */
    bool deserializeFromJson(ConfigManager& config, const std::string& json);
    
    /**
     * @brief Serialize profiles to JSON
     */
    std::string serializeProfilesToJson(const ProfileManager& profiles);
    
    /**
     * @brief Deserialize profiles from JSON
     */
    bool deserializeProfilesFromJson(ProfileManager& profiles, const std::string& json);
    
    /**
     * @brief Validate configuration format
     */
    bool validateConfigFormat(const std::string& data, ConfigFormat format);
    
    /**
     * @brief Get file extension for format
     */
    std::string getFileExtension(ConfigFormat format);
}

/**
 * @brief Global configuration instances
 */
extern std::unique_ptr<UserPreferences> g_userPreferences;

/**
 * @brief Get global user preferences
 */
UserPreferences& getUserPreferences();

/**
 * @brief Initialize global configuration system
 */
bool initializeConfigSystem();

/**
 * @brief Shutdown global configuration system
 */
void shutdownConfigSystem();

}  // namespace shortcut_detector
