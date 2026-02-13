/*
 * env_advanced.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-12-16

Description: Advanced environment management features

**************************************************/

#ifndef ATOM_SYSTEM_ENV_ADVANCED_HPP
#define ATOM_SYSTEM_ENV_ADVANCED_HPP

#include <chrono>
#include <functional>
#include <memory>
#include <optional>

#include "atom/containers/high_performance.hpp"
#include "atom/macro.hpp"
#include "env_core.hpp"
#include "env_file_io.hpp"
#include "env_path.hpp"
#include "env_persistent.hpp"
#include "env_scoped.hpp"
#include "env_system.hpp"
#include "env_utils.hpp"

namespace atom::utils {

using atom::containers::String;
template <typename K, typename V>
using HashMap = atom::containers::HashMap<K, V>;
template <typename T>
using Vector = atom::containers::Vector<T>;

/**
 * @brief Environment configuration profile
 */
struct EnvProfile {
    String name;
    String description;
    HashMap<String, String> variables;
    Vector<String> pathEntries;
    HashMap<String, String> persistentVars;
    EnvTemplate envTemplate;
    std::chrono::system_clock::time_point createdAt;
    std::chrono::system_clock::time_point lastModified;

    EnvProfile(const String& n = "", const String& desc = "")
        : name(n), description(desc),
          createdAt(std::chrono::system_clock::now()),
          lastModified(std::chrono::system_clock::now()) {}
};

/**
 * @brief Environment monitoring callback
 */
using EnvMonitorCallback = std::function<void(const String& event, const HashMap<String, String>& data)>;

/**
 * @brief Environment encryption provider interface
 */
class EnvEncryptionProvider {
public:
    virtual ~EnvEncryptionProvider() = default;
    virtual auto encrypt(const String& plaintext) -> String = 0;
    virtual auto decrypt(const String& ciphertext) -> String = 0;
    virtual auto isEncrypted(const String& data) -> bool = 0;
};

/**
 * @brief Advanced environment management features
 */
class EnvAdvanced {
public:
    /**
     * @brief Creates and applies an environment profile
     * @param profile Profile to apply
     * @param persistent Whether to make changes persistent
     * @return True if profile was applied successfully
     */
    static auto applyProfile(const EnvProfile& profile, bool persistent = false) -> bool;

    /**
     * @brief Creates a profile from current environment
     * @param name Profile name
     * @param description Profile description
     * @param includeSystem Whether to include system variables
     * @return Created environment profile
     */
    static auto createProfileFromCurrent(const String& name, const String& description,
                                         bool includeSystem = false) -> EnvProfile;

    /**
     * @brief Saves a profile to file
     * @param profile Profile to save
     * @param filePath File path to save to
     * @param format File format to use
     * @return True if saved successfully
     */
    static auto saveProfile(const EnvProfile& profile, const String& filePath,
                            EnvFileFormat format = EnvFileFormat::JSON) -> bool;

    /**
     * @brief Loads a profile from file
     * @param filePath File path to load from
     * @param format File format (AUTO for auto-detection)
     * @return Loaded profile or empty profile if failed
     */
    static auto loadProfile(const String& filePath,
                            EnvFileFormat format = EnvFileFormat::AUTO) -> EnvProfile;

    /**
     * @brief Lists available profiles in a directory
     * @param directory Directory to search
     * @return Vector of profile names
     */
    static auto listProfiles(const String& directory) -> Vector<String>;

    /**
     * @brief Starts environment monitoring
     * @param callback Callback to invoke on environment changes
     * @param interval Monitoring interval in milliseconds
     * @return Monitoring session ID
     */
    static auto startMonitoring(EnvMonitorCallback callback, int interval = 1000) -> size_t;

    /**
     * @brief Stops environment monitoring
     * @param sessionId Monitoring session ID
     * @return True if stopped successfully
     */
    static auto stopMonitoring(size_t sessionId) -> bool;

    /**
     * @brief Sets encryption provider for sensitive variables
     * @param provider Encryption provider instance
     */
    static void setEncryptionProvider(std::shared_ptr<EnvEncryptionProvider> provider);

    /**
     * @brief Sets an encrypted environment variable
     * @param key Variable name
     * @param value Variable value (will be encrypted)
     * @param persistent Whether to persist the encrypted value
     * @return True if set successfully
     */
    static auto setEncryptedVar(const String& key, const String& value, bool persistent = false) -> bool;

    /**
     * @brief Gets and decrypts an environment variable
     * @param key Variable name
     * @param defaultValue Default value if not found
     * @return Decrypted variable value
     */
    static auto getEncryptedVar(const String& key, const String& defaultValue = "") -> String;

    /**
     * @brief Performs environment health check
     * @return Health check results
     */
    static auto performHealthCheck() -> HashMap<String, String>;

    /**
     * @brief Optimizes environment performance
     * @return Optimization results
     */
    static auto optimizeEnvironment() -> HashMap<String, String>;

    /**
     * @brief Creates environment diff between two states
     * @param before Environment state before
     * @param after Environment state after
     * @return Diff results showing changes
     */
    static auto createEnvironmentDiff(const HashMap<String, String>& before,
                                      const HashMap<String, String>& after)
        -> HashMap<String, String>;

    /**
     * @brief Exports environment in various formats
     * @param format Export format
     * @param includeSystem Whether to include system variables
     * @param filePath Output file path
     * @return True if exported successfully
     */
    static auto exportEnvironment(EnvFileFormat format, bool includeSystem = true,
                                  const String& filePath = "") -> bool;

    /**
     * @brief Imports environment from various sources
     * @param source Source file or data
     * @param format Source format
     * @param merge Whether to merge with existing environment
     * @return True if imported successfully
     */
    static auto importEnvironment(const String& source, EnvFileFormat format,
                                  bool merge = true) -> bool;

    /**
     * @brief Gets comprehensive environment statistics
     * @return Statistics map
     */
    static auto getEnvironmentStatistics() -> HashMap<String, String>;

    /**
     * @brief Validates environment against requirements
     * @param requirements Requirements specification
     * @return Validation results
     */
    static auto validateEnvironmentRequirements(const HashMap<String, String>& requirements)
        -> HashMap<String, bool>;

private:
    static std::shared_ptr<EnvEncryptionProvider> sEncryptionProvider;
    static HashMap<size_t, EnvMonitorCallback> sMonitorCallbacks;
    static size_t sNextMonitorId;
    static std::mutex sMonitorMutex;

    // Helper methods
    static auto serializeProfile(const EnvProfile& profile, EnvFileFormat format) -> String;
    static auto deserializeProfile(const String& data, EnvFileFormat format) -> EnvProfile;
    static auto detectProfileFormat(const String& filePath) -> EnvFileFormat;
    static auto generateProfileId() -> String;
    static void runMonitoringLoop(size_t sessionId, EnvMonitorCallback callback, int interval);
};

/**
 * @brief Simple AES encryption provider implementation
 */
class SimpleEncryptionProvider : public EnvEncryptionProvider {
public:
    explicit SimpleEncryptionProvider(const String& key);

    auto encrypt(const String& plaintext) -> String override;
    auto decrypt(const String& ciphertext) -> String override;
    auto isEncrypted(const String& data) -> bool override;

private:
    String mKey;
    static constexpr const char* ENCRYPTED_PREFIX = "ENC:";
};

}  // namespace atom::utils

#endif  // ATOM_SYSTEM_ENV_ADVANCED_HPP
