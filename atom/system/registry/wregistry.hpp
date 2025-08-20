/*
 * wregistry.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-6-17

Description: Some registry functions for Windows

**************************************************/

#ifndef ATOM_SYSTEM_WREGISTRY_HPP
#define ATOM_SYSTEM_WREGISTRY_HPP

#ifdef _WIN32
#include <string_view>
#include <utility>
#include <vector>

// Forward declaration to avoid including windows.h in the header
struct HKEY__;

// Alias for HKEY
using HKEY = HKEY__*;

namespace atom::system {

/**
 * @brief Gets all subkey names under the specified registry key.
 * @param hRootKey Root key handle.
 * @param subKey The name of the specified key, which can include multiple
 * nested keys separated by backslashes.
 * @param subKeys Vector of subkey name strings.
 * @return true if successful, false if failed.
 */
[[nodiscard]] auto getRegistrySubKeys(HKEY hRootKey, std::string_view subKey,
                                      std::vector<std::string>& subKeys)
    -> bool;

/**
 * @brief Gets all value names and data under the specified registry key.
 * @param hRootKey Root key handle.
 * @param subKey The name of the specified key, which can include multiple
 * nested keys separated by backslashes.
 * @param values Vector of string pairs containing names and data.
 * @return true if successful, false if failed.
 */
[[nodiscard]] auto getRegistryValues(
    HKEY hRootKey, std::string_view subKey,
    std::vector<std::pair<std::string, std::string>>& values) -> bool;

/**
 * @brief Modifies the data of a specified value under the specified registry
 * key.
 * @param hRootKey Root key handle.
 * @param subKey The name of the specified key, which can include multiple
 * nested keys separated by backslashes.
 * @param valueName The name of the value to modify.
 * @param newValue The new value data.
 * @return true if successful, false if failed.
 */
[[nodiscard]] auto modifyRegistryValue(HKEY hRootKey, std::string_view subKey,
                                       std::string_view valueName,
                                       std::string_view newValue) -> bool;

/**
 * @brief Deletes the specified registry key and all its subkeys.
 * @param hRootKey Root key handle.
 * @param subKey The name of the key to delete, which can include multiple
 * nested keys separated by backslashes.
 * @return true if successful, false if failed.
 */
[[nodiscard]] auto deleteRegistrySubKey(HKEY hRootKey, std::string_view subKey)
    -> bool;

/**
 * @brief Deletes the specified value under the specified registry key.
 * @param hRootKey Root key handle.
 * @param subKey The name of the specified key, which can include multiple
 * nested keys separated by backslashes.
 * @param valueName The name of the value to delete.
 * @return true if successful, false if failed.
 */
[[nodiscard]] auto deleteRegistryValue(HKEY hRootKey, std::string_view subKey,
                                       std::string_view valueName) -> bool;

/**
 * @brief Recursively enumerates all subkeys and values under the specified
 * registry key.
 * @param hRootKey Root key handle.
 * @param subKey The name of the specified key, which can include multiple
 * nested keys separated by backslashes.
 */
void recursivelyEnumerateRegistrySubKeys(HKEY hRootKey,
                                         std::string_view subKey);

/**
 * @brief Backs up the specified registry key and all its subkeys and values.
 * @param hRootKey Root key handle.
 * @param subKey The name of the key to back up, which can include multiple
 * nested keys separated by backslashes.
 * @param backupFilePath The full path of the backup file.
 * @return true if successful, false if failed.
 */
[[nodiscard]] auto backupRegistry(HKEY hRootKey, std::string_view subKey,
                                  std::string_view backupFilePath) -> bool;

/**
 * @brief Recursively searches for subkey names containing the specified string
 * under the specified registry key.
 * @param hRootKey Root key handle.
 * @param subKey The name of the specified key, which can include multiple
 * nested keys separated by backslashes.
 * @param searchKey The string to search for.
 * @param foundKeys Vector to store found key paths.
 */
void findRegistryKey(HKEY hRootKey, std::string_view subKey,
                     std::string_view searchKey,
                     std::vector<std::string>& foundKeys);

/**
 * @brief Recursively searches for value names and data containing the specified
 * string under the specified registry key.
 * @param hRootKey Root key handle.
 * @param subKey The name of the specified key, which can include multiple
 * nested keys separated by backslashes.
 * @param searchValue The string to search for.
 * @param foundValues Vector to store found value information.
 */
void findRegistryValue(
    HKEY hRootKey, std::string_view subKey, std::string_view searchValue,
    std::vector<std::pair<std::string, std::string>>& foundValues);

/**
 * @brief Exports the specified registry key and all its subkeys and values as a
 * REG file.
 * @param hRootKey Root key handle.
 * @param subKey The name of the key to export, which can include multiple
 * nested keys separated by backslashes.
 * @param exportFilePath The full path of the export file.
 * @return true if successful, false if failed.
 */
[[nodiscard]] auto exportRegistry(HKEY hRootKey, std::string_view subKey,
                                  std::string_view exportFilePath) -> bool;

/**
 * @brief Creates a new registry key.
 * @param hRootKey Root key handle.
 * @param subKey The name of the key to create.
 * @return true if successful, false if failed.
 */
[[nodiscard]] auto createRegistryKey(HKEY hRootKey, std::string_view subKey)
    -> bool;

/**
 * @brief Checks if a registry key exists.
 * @param hRootKey Root key handle.
 * @param subKey The name of the key to check.
 * @return true if key exists, false otherwise.
 */
[[nodiscard]] auto registryKeyExists(HKEY hRootKey, std::string_view subKey)
    -> bool;

/**
 * @brief Gets the type of a registry value.
 * @param hRootKey Root key handle.
 * @param subKey The name of the key containing the value.
 * @param valueName The name of the value.
 * @return The registry value type, or 0 if failed.
 */
[[nodiscard]] auto getRegistryValueType(HKEY hRootKey, std::string_view subKey,
                                        std::string_view valueName)
    -> unsigned long;

}  // namespace atom::system

#endif

#endif  // ATOM_SYSTEM_WREGISTRY_HPP
