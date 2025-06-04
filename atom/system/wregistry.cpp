/*
 * wregistry.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-6-17

Description: Some registry functions for Windows

**************************************************/

#ifdef _WIN32

#include "wregistry.hpp"

#include <array>
#include <format>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <windows.h>

#include <spdlog/spdlog.h>

namespace atom::system {

constexpr DWORD MAX_KEY_LENGTH = 255;
constexpr DWORD MAX_VALUE_NAME = 16383;
constexpr DWORD MAX_PATH_LENGTH = MAX_PATH;

auto getRegistrySubKeys(HKEY hRootKey, std::string_view subKey,
                        std::vector<std::string>& subKeys) -> bool {
    spdlog::info("Getting registry subkeys for hRootKey: {}, subKey: {}",
                 reinterpret_cast<void*>(hRootKey), subKey);

    HKEY hKey;
    LONG lRes = RegOpenKeyExA(hRootKey, std::string(subKey).c_str(), 0,
                              KEY_READ, &hKey);
    if (lRes != ERROR_SUCCESS) {
        spdlog::error("Could not open registry key: {}", lRes);
        return false;
    }

    std::array<char, MAX_KEY_LENGTH> achKey;
    DWORD cchKey = MAX_KEY_LENGTH;
    DWORD index = 0;

    subKeys.clear();
    subKeys.reserve(32);

    while (true) {
        cchKey = MAX_KEY_LENGTH;
        lRes = RegEnumKeyExA(hKey, index, achKey.data(), &cchKey, nullptr,
                             nullptr, nullptr, nullptr);
        if (lRes == ERROR_NO_MORE_ITEMS) {
            break;
        }
        if (lRes == ERROR_SUCCESS) {
            subKeys.emplace_back(achKey.data(), cchKey);
            ++index;
        } else {
            spdlog::error("Could not enumerate registry key: {}", lRes);
            RegCloseKey(hKey);
            return false;
        }
    }

    RegCloseKey(hKey);
    spdlog::info("Found {} registry subkeys", subKeys.size());
    return true;
}

auto getRegistryValues(HKEY hRootKey, std::string_view subKey,
                       std::vector<std::pair<std::string, std::string>>& values)
    -> bool {
    spdlog::info("Getting registry values for hRootKey: {}, subKey: {}",
                 reinterpret_cast<void*>(hRootKey), subKey);

    HKEY hKey;
    LONG lRes = RegOpenKeyExA(hRootKey, std::string(subKey).c_str(), 0,
                              KEY_READ, &hKey);
    if (lRes != ERROR_SUCCESS) {
        spdlog::error("Could not open registry key: {}", lRes);
        return false;
    }

    std::array<char, MAX_VALUE_NAME> achValue;
    DWORD cchValue = MAX_VALUE_NAME;
    DWORD dwType;
    std::array<BYTE, MAX_PATH_LENGTH> lpData;
    DWORD dwDataSize = sizeof(lpData);
    DWORD index = 0;

    values.clear();
    values.reserve(16);

    while (true) {
        cchValue = MAX_VALUE_NAME;
        dwDataSize = sizeof(lpData);

        lRes = RegEnumValueA(hKey, index, achValue.data(), &cchValue, nullptr,
                             &dwType, lpData.data(), &dwDataSize);
        if (lRes == ERROR_NO_MORE_ITEMS) {
            break;
        }
        if (lRes == ERROR_SUCCESS) {
            std::string valueName(achValue.data(), cchValue);
            std::string valueData;

            switch (dwType) {
                case REG_SZ:
                case REG_EXPAND_SZ:
                    valueData.assign(reinterpret_cast<char*>(lpData.data()),
                                     dwDataSize > 0 ? dwDataSize - 1 : 0);
                    break;
                case REG_DWORD:
                    if (dwDataSize >= sizeof(DWORD)) {
                        DWORD data = *reinterpret_cast<DWORD*>(lpData.data());
                        valueData = std::to_string(data);
                    }
                    break;
                case REG_QWORD:
                    if (dwDataSize >= sizeof(ULONGLONG)) {
                        ULONGLONG data =
                            *reinterpret_cast<ULONGLONG*>(lpData.data());
                        valueData = std::to_string(data);
                    }
                    break;
                default:
                    valueData = "<unsupported type>";
                    break;
            }

            values.emplace_back(std::move(valueName), std::move(valueData));
            ++index;
        } else {
            spdlog::error("Could not enumerate registry value: {}", lRes);
            RegCloseKey(hKey);
            return false;
        }
    }

    RegCloseKey(hKey);
    spdlog::info("Found {} registry values", values.size());
    return true;
}

auto modifyRegistryValue(HKEY hRootKey, std::string_view subKey,
                         std::string_view valueName, std::string_view newValue)
    -> bool {
    spdlog::info(
        "Modifying registry value: hRootKey: {}, subKey: {}, valueName: {}, "
        "newValue: {}",
        reinterpret_cast<void*>(hRootKey), subKey, valueName, newValue);

    HKEY hKey;
    LONG lRes = RegOpenKeyExA(hRootKey, std::string(subKey).c_str(), 0,
                              KEY_SET_VALUE, &hKey);
    if (lRes != ERROR_SUCCESS) {
        spdlog::error("Could not open registry key for writing: {}", lRes);
        return false;
    }

    std::string newValueStr(newValue);
    const BYTE* data = reinterpret_cast<const BYTE*>(newValueStr.c_str());
    auto dataSize = static_cast<DWORD>(newValueStr.size() + 1);

    lRes = RegSetValueExA(hKey, std::string(valueName).c_str(), 0, REG_SZ, data,
                          dataSize);
    if (lRes != ERROR_SUCCESS) {
        spdlog::error("Could not set registry value: {}", lRes);
        RegCloseKey(hKey);
        return false;
    }

    RegCloseKey(hKey);
    spdlog::info("Registry value modified successfully");
    return true;
}

auto deleteRegistrySubKey(HKEY hRootKey, std::string_view subKey) -> bool {
    spdlog::info("Deleting registry subkey: hRootKey: {}, subKey: {}",
                 reinterpret_cast<void*>(hRootKey), subKey);

    LONG lRes = RegDeleteKeyA(hRootKey, std::string(subKey).c_str());
    if (lRes != ERROR_SUCCESS) {
        spdlog::error("Could not delete registry subkey: {}", lRes);
        return false;
    }

    spdlog::info("Registry subkey deleted successfully");
    return true;
}

auto deleteRegistryValue(HKEY hRootKey, std::string_view subKey,
                         std::string_view valueName) -> bool {
    spdlog::info(
        "Deleting registry value: hRootKey: {}, subKey: {}, valueName: {}",
        reinterpret_cast<void*>(hRootKey), subKey, valueName);

    HKEY hKey;
    LONG lRes = RegOpenKeyExA(hRootKey, std::string(subKey).c_str(), 0,
                              KEY_SET_VALUE, &hKey);
    if (lRes != ERROR_SUCCESS) {
        spdlog::error("Could not open registry key for deletion: {}", lRes);
        return false;
    }

    lRes = RegDeleteValueA(hKey, std::string(valueName).c_str());
    if (lRes != ERROR_SUCCESS) {
        spdlog::error("Could not delete registry value: {}", lRes);
        RegCloseKey(hKey);
        return false;
    }

    RegCloseKey(hKey);
    spdlog::info("Registry value deleted successfully");
    return true;
}

void recursivelyEnumerateRegistrySubKeys(HKEY hRootKey,
                                         std::string_view subKey) {
    spdlog::info(
        "Recursively enumerating registry subkeys: hRootKey: {}, subKey: {}",
        reinterpret_cast<void*>(hRootKey), subKey);

    HKEY hKey;
    LONG lRes = RegOpenKeyExA(hRootKey, std::string(subKey).c_str(), 0,
                              KEY_READ, &hKey);
    if (lRes != ERROR_SUCCESS) {
        spdlog::error("Could not open registry key: {}", lRes);
        return;
    }

    std::array<char, MAX_KEY_LENGTH> achKey;
    DWORD cchKey = MAX_KEY_LENGTH;
    DWORD index = 0;

    while (true) {
        cchKey = MAX_KEY_LENGTH;
        lRes = RegEnumKeyExA(hKey, index, achKey.data(), &cchKey, nullptr,
                             nullptr, nullptr, nullptr);
        if (lRes == ERROR_NO_MORE_ITEMS) {
            break;
        }
        if (lRes == ERROR_SUCCESS) {
            spdlog::debug("Found subkey: {}", achKey.data());
            std::string newSubKey =
                std::format("{}\\{}", subKey, achKey.data());
            recursivelyEnumerateRegistrySubKeys(hRootKey, newSubKey);
            ++index;
        } else {
            spdlog::error("Could not enumerate registry key: {}", lRes);
            break;
        }
    }

    RegCloseKey(hKey);
    spdlog::info("Recursive enumeration completed");
}

auto backupRegistry(HKEY hRootKey, std::string_view subKey,
                    std::string_view backupFilePath) -> bool {
    spdlog::info(
        "Backing up registry: hRootKey: {}, subKey: {}, backupFilePath: {}",
        reinterpret_cast<void*>(hRootKey), subKey, backupFilePath);

    HKEY hKey;
    LONG lRes = RegOpenKeyExA(hRootKey, std::string(subKey).c_str(), 0,
                              KEY_READ, &hKey);
    if (lRes != ERROR_SUCCESS) {
        spdlog::error("Could not open registry key for backup: {}", lRes);
        return false;
    }

    lRes = RegSaveKeyA(hKey, std::string(backupFilePath).c_str(), nullptr);
    if (lRes != ERROR_SUCCESS) {
        spdlog::error("Could not save registry key: {}", lRes);
        RegCloseKey(hKey);
        return false;
    }

    RegCloseKey(hKey);
    spdlog::info("Registry backup completed successfully");
    return true;
}

void findRegistryKey(HKEY hRootKey, std::string_view subKey,
                     std::string_view searchKey,
                     std::vector<std::string>& foundKeys) {
    spdlog::info(
        "Searching for registry key: hRootKey: {}, subKey: {}, searchKey: {}",
        reinterpret_cast<void*>(hRootKey), subKey, searchKey);

    HKEY hKey;
    LONG lRes = RegOpenKeyExA(hRootKey, std::string(subKey).c_str(), 0,
                              KEY_READ, &hKey);
    if (lRes != ERROR_SUCCESS) {
        spdlog::error("Could not open registry key: {}", lRes);
        return;
    }

    std::array<char, MAX_KEY_LENGTH> achKey;
    DWORD cchKey = MAX_KEY_LENGTH;
    DWORD index = 0;

    while (true) {
        cchKey = MAX_KEY_LENGTH;
        lRes = RegEnumKeyExA(hKey, index, achKey.data(), &cchKey, nullptr,
                             nullptr, nullptr, nullptr);
        if (lRes == ERROR_NO_MORE_ITEMS) {
            break;
        }
        if (lRes == ERROR_SUCCESS) {
            std::string_view keyName(achKey.data(), cchKey);
            if (keyName.find(searchKey) != std::string_view::npos) {
                std::string fullPath = std::format("{}\\{}", subKey, keyName);
                foundKeys.push_back(fullPath);
                spdlog::debug("Found matching key: {}", fullPath);
            }

            std::string newSubKey = std::format("{}\\{}", subKey, keyName);
            findRegistryKey(hRootKey, newSubKey, searchKey, foundKeys);
            ++index;
        } else {
            spdlog::error("Could not enumerate registry key: {}", lRes);
            break;
        }
    }

    RegCloseKey(hKey);
    spdlog::info("Registry key search completed");
}

void findRegistryValue(
    HKEY hRootKey, std::string_view subKey, std::string_view searchValue,
    std::vector<std::pair<std::string, std::string>>& foundValues) {
    spdlog::info(
        "Searching for registry value: hRootKey: {}, subKey: {}, searchValue: "
        "{}",
        reinterpret_cast<void*>(hRootKey), subKey, searchValue);

    HKEY hKey;
    LONG lRes = RegOpenKeyExA(hRootKey, std::string(subKey).c_str(), 0,
                              KEY_READ, &hKey);
    if (lRes != ERROR_SUCCESS) {
        spdlog::error("Could not open registry key: {}", lRes);
        return;
    }

    std::array<char, MAX_VALUE_NAME> achValue;
    DWORD cchValue = MAX_VALUE_NAME;
    DWORD dwType;
    std::array<BYTE, MAX_PATH_LENGTH> lpData;
    DWORD dwDataSize = sizeof(lpData);
    DWORD index = 0;

    while (true) {
        cchValue = MAX_VALUE_NAME;
        dwDataSize = sizeof(lpData);

        lRes = RegEnumValueA(hKey, index, achValue.data(), &cchValue, nullptr,
                             &dwType, lpData.data(), &dwDataSize);
        if (lRes == ERROR_NO_MORE_ITEMS) {
            break;
        }
        if (lRes == ERROR_SUCCESS) {
            std::string_view valueName(achValue.data(), cchValue);
            if (valueName.find(searchValue) != std::string_view::npos) {
                std::string fullPath = std::format("{}\\{}", subKey, valueName);
                std::string valueData;

                if (dwType == REG_SZ || dwType == REG_EXPAND_SZ) {
                    valueData.assign(reinterpret_cast<char*>(lpData.data()),
                                     dwDataSize > 0 ? dwDataSize - 1 : 0);
                } else if (dwType == REG_DWORD && dwDataSize >= sizeof(DWORD)) {
                    DWORD data = *reinterpret_cast<DWORD*>(lpData.data());
                    valueData = std::to_string(data);
                }

                foundValues.emplace_back(fullPath, std::move(valueData));
                spdlog::debug("Found matching value: {}", fullPath);
            }
            ++index;
        } else {
            spdlog::error("Could not enumerate registry value: {}", lRes);
            break;
        }
    }

    RegCloseKey(hKey);
    spdlog::info("Registry value search completed");
}

auto exportRegistry(HKEY hRootKey, std::string_view subKey,
                    std::string_view exportFilePath) -> bool {
    spdlog::info(
        "Exporting registry: hRootKey: {}, subKey: {}, exportFilePath: {}",
        reinterpret_cast<void*>(hRootKey), subKey, exportFilePath);

    HKEY hKey;
    LONG lRes = RegOpenKeyExA(hRootKey, std::string(subKey).c_str(), 0,
                              KEY_READ, &hKey);
    if (lRes != ERROR_SUCCESS) {
        spdlog::error("Could not open registry key for export: {}", lRes);
        return false;
    }

    lRes = RegSaveKeyA(hKey, std::string(exportFilePath).c_str(), nullptr);
    if (lRes != ERROR_SUCCESS) {
        spdlog::error("Could not export registry key: {}", lRes);
        RegCloseKey(hKey);
        return false;
    }

    RegCloseKey(hKey);
    spdlog::info("Registry export completed successfully");
    return true;
}

auto createRegistryKey(HKEY hRootKey, std::string_view subKey) -> bool {
    spdlog::info("Creating registry key: hRootKey: {}, subKey: {}",
                 reinterpret_cast<void*>(hRootKey), subKey);

    HKEY hKey;
    DWORD dwDisposition;
    LONG lRes = RegCreateKeyExA(hRootKey, std::string(subKey).c_str(), 0,
                                nullptr, REG_OPTION_NON_VOLATILE, KEY_WRITE,
                                nullptr, &hKey, &dwDisposition);
    if (lRes != ERROR_SUCCESS) {
        spdlog::error("Could not create registry key: {}", lRes);
        return false;
    }

    RegCloseKey(hKey);
    spdlog::info("Registry key created successfully (disposition: {})",
                 dwDisposition);
    return true;
}

auto registryKeyExists(HKEY hRootKey, std::string_view subKey) -> bool {
    HKEY hKey;
    LONG lRes = RegOpenKeyExA(hRootKey, std::string(subKey).c_str(), 0,
                              KEY_READ, &hKey);
    if (lRes == ERROR_SUCCESS) {
        RegCloseKey(hKey);
        return true;
    }
    return false;
}

auto getRegistryValueType(HKEY hRootKey, std::string_view subKey,
                          std::string_view valueName) -> unsigned long {
    HKEY hKey;
    LONG lRes = RegOpenKeyExA(hRootKey, std::string(subKey).c_str(), 0,
                              KEY_READ, &hKey);
    if (lRes != ERROR_SUCCESS) {
        spdlog::error("Could not open registry key for type query: {}", lRes);
        return 0;
    }

    DWORD dwType;
    lRes = RegQueryValueExA(hKey, std::string(valueName).c_str(), nullptr,
                            &dwType, nullptr, nullptr);
    RegCloseKey(hKey);

    if (lRes != ERROR_SUCCESS) {
        spdlog::error("Could not query registry value type: {}", lRes);
        return 0;
    }

    return dwType;
}

}  // namespace atom::system

#endif
