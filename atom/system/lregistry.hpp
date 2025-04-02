/*
 * lregistry.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-6-17

Description: A self-contained registry manager.

**************************************************/

#ifndef ATOM_SYSTEM_REGISTRY_HPP
#define ATOM_SYSTEM_REGISTRY_HPP

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>
#include "atom/macro.hpp"

namespace atom::system {

/**
 * @brief Enum for different registry data formats
 */
enum class RegistryFormat { TEXT, JSON, XML, BINARY };

/**
 * @brief Enum for registry operation results
 */
enum class RegistryResult {
    SUCCESS,
    KEY_NOT_FOUND,
    VALUE_NOT_FOUND,
    FILE_ERROR,
    PERMISSION_DENIED,
    INVALID_FORMAT,
    ENCRYPTION_ERROR,
    ALREADY_EXISTS,
    UNKNOWN_ERROR
};

/**
 * @brief Struct for registry value metadata
 */
struct RegistryValueInfo {
    std::string name;
    std::string type;
    std::time_t lastModified;
    size_t size;
};

/**
 * @brief The Registry class handles registry operations.
 */
class Registry {
public:
    using EventCallback =
        std::function<void(const std::string&, const std::string&)>;

    Registry();
    ~Registry();

    /**
     * @brief Initializes the registry with specified settings
     *
     * @param filePath Path to registry file
     * @param useEncryption Whether to use encryption
     * @return RegistryResult Operation result
     */
    RegistryResult initialize(const std::string& filePath,
                              bool useEncryption = false);

    /**
     * @brief Loads registry data from a file.
     *
     * @param filePath Path to the registry file
     * @param format Format of the registry file
     * @return RegistryResult Operation result
     */
    RegistryResult loadRegistryFromFile(
        const std::string& filePath = "",
        RegistryFormat format = RegistryFormat::TEXT);

    /**
     * @brief Creates a new key in the registry.
     *
     * @param keyPath The path of the key to create
     * @return RegistryResult Operation result
     */
    RegistryResult createKey(const std::string& keyPath);

    /**
     * @brief Deletes a key from the registry.
     *
     * @param keyPath The path of the key to delete
     * @return RegistryResult Operation result
     */
    RegistryResult deleteKey(const std::string& keyPath);

    /**
     * @brief Sets a value for a key in the registry.
     *
     * @param keyPath The path of the key
     * @param valueName The name of the value to set
     * @param data The data to set for the value
     * @return RegistryResult Operation result
     */
    RegistryResult setValue(const std::string& keyPath,
                            const std::string& valueName,
                            const std::string& data);

    /**
     * @brief Sets a value with a specific type for a key in the registry.
     *
     * @param keyPath The path of the key
     * @param valueName The name of the value to set
     * @param data The data to set for the value
     * @param type The data type
     * @return RegistryResult Operation result
     */
    RegistryResult setTypedValue(const std::string& keyPath,
                                 const std::string& valueName,
                                 const std::string& data,
                                 const std::string& type);

    /**
     * @brief Gets the value associated with a key and value name from the
     * registry.
     *
     * @param keyPath The path of the key
     * @param valueName The name of the value to retrieve
     * @return Optional containing the value if found
     */
    ATOM_NODISCARD std::optional<std::string> getValue(
        const std::string& keyPath, const std::string& valueName);

    /**
     * @brief Gets the value and type associated with a key and value name.
     *
     * @param keyPath The path of the key
     * @param valueName The name of the value to retrieve
     * @param type Output parameter for the value type
     * @return Optional containing the value if found
     */
    ATOM_NODISCARD std::optional<std::string> getTypedValue(
        const std::string& keyPath, const std::string& valueName,
        std::string& type);

    /**
     * @brief Deletes a value from a key in the registry.
     *
     * @param keyPath The path of the key
     * @param valueName The name of the value to delete
     * @return RegistryResult Operation result
     */
    RegistryResult deleteValue(const std::string& keyPath,
                               const std::string& valueName);

    /**
     * @brief Backs up the registry data.
     *
     * @param backupPath Path for the backup file
     * @return RegistryResult Operation result
     */
    RegistryResult backupRegistryData(const std::string& backupPath = "");

    /**
     * @brief Restores the registry data from a backup file.
     *
     * @param backupFile The backup file to restore data from
     * @return RegistryResult Operation result
     */
    RegistryResult restoreRegistryData(const std::string& backupFile);

    /**
     * @brief Checks if a key exists in the registry.
     *
     * @param keyPath The path of the key to check for existence
     * @return true if the key exists, false otherwise
     */
    ATOM_NODISCARD bool keyExists(const std::string& keyPath) const;

    /**
     * @brief Checks if a value exists for a key in the registry.
     *
     * @param keyPath The path of the key
     * @param valueName The name of the value to check for existence
     * @return true if the value exists, false otherwise
     */
    ATOM_NODISCARD bool valueExists(const std::string& keyPath,
                                    const std::string& valueName) const;

    /**
     * @brief Retrieves all value names for a given key from the registry.
     *
     * @param keyPath The path of the key
     * @return A vector of value names associated with the given key
     */
    ATOM_NODISCARD std::vector<std::string> getValueNames(
        const std::string& keyPath) const;

    /**
     * @brief Gets all keys in the registry
     *
     * @return Vector of key paths
     */
    ATOM_NODISCARD std::vector<std::string> getAllKeys() const;

    /**
     * @brief Gets detailed information about a registry value
     *
     * @param keyPath The path of the key
     * @param valueName The name of the value
     * @return Optional containing value information if found
     */
    ATOM_NODISCARD std::optional<RegistryValueInfo> getValueInfo(
        const std::string& keyPath, const std::string& valueName) const;

    /**
     * @brief Registers a callback for registry events
     *
     * @param callback The function to call on events
     * @return Unique ID for the callback registration
     */
    size_t registerEventCallback(EventCallback callback);

    /**
     * @brief Unregisters a previously registered callback
     *
     * @param callbackId The ID returned from registerEventCallback
     * @return true if successfully unregistered
     */
    bool unregisterEventCallback(size_t callbackId);

    /**
     * @brief Begins a transaction for atomic operations
     *
     * @return true if transaction started successfully
     */
    bool beginTransaction();

    /**
     * @brief Commits the current transaction
     *
     * @return RegistryResult Operation result
     */
    RegistryResult commitTransaction();

    /**
     * @brief Rolls back the current transaction
     *
     * @return RegistryResult Operation result
     */
    RegistryResult rollbackTransaction();

    /**
     * @brief Exports registry data to a specified format
     *
     * @param filePath Export file path
     * @param format Format to export to
     * @return RegistryResult Operation result
     */
    RegistryResult exportRegistry(const std::string& filePath,
                                  RegistryFormat format);

    /**
     * @brief Imports registry data from a file
     *
     * @param filePath Import file path
     * @param format Format to import from
     * @param mergeStrategy How to handle conflicts (replace/merge)
     * @return RegistryResult Operation result
     */
    RegistryResult importRegistry(const std::string& filePath,
                                  RegistryFormat format,
                                  bool mergeExisting = false);

    /**
     * @brief Searches for keys matching a pattern
     *
     * @param pattern Search pattern
     * @return Vector of matching key paths
     */
    ATOM_NODISCARD std::vector<std::string> searchKeys(
        const std::string& pattern) const;

    /**
     * @brief Searches for values matching criteria
     *
     * @param valuePattern Pattern to match against value content
     * @return Vector of key-value pairs that match
     */
    ATOM_NODISCARD std::vector<std::pair<std::string, std::string>>
    searchValues(const std::string& valuePattern) const;

    /**
     * @brief Enables or disables automatic saving
     *
     * @param enable Whether to enable auto-save
     */
    void setAutoSave(bool enable);

    /**
     * @brief Gets the error message for the last failed operation
     *
     * @return Error message string
     */
    ATOM_NODISCARD std::string getLastError() const;

private:
    class RegistryImpl;  // Forward declaration of the implementation class
    std::unique_ptr<RegistryImpl>
        pImpl_;  // Pointer to the implementation class
};

}  // namespace atom::system

#endif