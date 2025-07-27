/*
 * env_persistent.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-12-16

Description: Persistent environment variable management implementation

**************************************************/

#include "env_persistent.hpp"

#include <filesystem>
#include <fstream>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

#include "env_system.hpp"
#include <spdlog/spdlog.h>

namespace atom::utils {

// Static member initializations
HashMap<String, HashMap<String, String>> EnvPersistent::sPersistentBackups;
std::mutex EnvPersistent::sBackupMutex;

auto EnvPersistent::setPersistentEnv(const String& key, const String& val,
                                     PersistLevel level) -> PersistenceResult {
    spdlog::info("Setting persistent environment variable: {}={} at level {}",
                 key, val, static_cast<int>(level));

    // Validate input
    if (!validateKey(key)) {
        spdlog::error("Invalid key: {}", key);
        return PersistenceResult::INVALID_KEY;
    }

    if (!validateValue(val)) {
        spdlog::error("Invalid value for key {}", key);
        return PersistenceResult::INVALID_VALUE;
    }

    if (level == PersistLevel::PROCESS) {
        // Just set in current process
        bool result = EnvCore::setEnv(key, val);
        return result ? PersistenceResult::SUCCESS : PersistenceResult::SYSTEM_ERROR;
    }

#ifdef _WIN32
    bool result = setPersistentEnvWindows(key, val, level);
#else
    bool result = setPersistentEnvUnix(key, val, level);
#endif

    return result ? PersistenceResult::SUCCESS : PersistenceResult::SYSTEM_ERROR;
}

auto EnvPersistent::deletePersistentEnv(const String& key,
                                        PersistLevel level) -> PersistenceResult {
    spdlog::info("Deleting persistent environment variable: {} at level {}",
                 key, static_cast<int>(level));

    if (!validateKey(key)) {
        spdlog::error("Invalid key: {}", key);
        return PersistenceResult::INVALID_KEY;
    }

    if (level == PersistLevel::PROCESS) {
        // Just unset in current process
        EnvCore::unsetEnv(key);
        return PersistenceResult::SUCCESS;
    }

#ifdef _WIN32
    bool result = deletePersistentEnvWindows(key, level);
#else
    bool result = deletePersistentEnvUnix(key, level);
#endif

    return result ? PersistenceResult::SUCCESS : PersistenceResult::SYSTEM_ERROR;
}

#ifdef _WIN32
auto EnvPersistent::setPersistentEnvWindows(const String& key, const String& val,
                                            PersistLevel level) -> bool {
    HKEY hKey;
    LONG result;

    const char* subKey = (level == PersistLevel::USER)
        ? "Environment"
        : "SYSTEM\\CurrentControlSet\\Control\\Session Manager\\Environment";

    HKEY rootKey = (level == PersistLevel::USER) ? HKEY_CURRENT_USER : HKEY_LOCAL_MACHINE;

    result = RegOpenKeyExA(rootKey, subKey, 0, KEY_SET_VALUE, &hKey);
    if (result != ERROR_SUCCESS) {
        spdlog::error("Failed to open registry key");
        return false;
    }

    result = RegSetValueExA(hKey, key.c_str(), 0, REG_EXPAND_SZ,
                           reinterpret_cast<const BYTE*>(val.c_str()),
                           static_cast<DWORD>(val.length() + 1));

    RegCloseKey(hKey);

    if (result != ERROR_SUCCESS) {
        spdlog::error("Failed to set registry value");
        return false;
    }

    // Notify system of environment change
    SendMessageTimeoutA(HWND_BROADCAST, WM_SETTINGCHANGE, 0,
                        reinterpret_cast<LPARAM>("Environment"),
                        SMTO_ABORTIFHUNG, 5000, nullptr);

    // Also set in current process
    EnvCore::setEnv(key, val);

    spdlog::info("Successfully set persistent environment variable in registry");
    return true;
}

auto EnvPersistent::deletePersistentEnvWindows(const String& key,
                                               PersistLevel level) -> bool {
    HKEY hKey;
    LONG result;

    const char* subKey = (level == PersistLevel::USER)
        ? "Environment"
        : "SYSTEM\\CurrentControlSet\\Control\\Session Manager\\Environment";

    HKEY rootKey = (level == PersistLevel::USER) ? HKEY_CURRENT_USER : HKEY_LOCAL_MACHINE;

    result = RegOpenKeyExA(rootKey, subKey, 0, KEY_SET_VALUE, &hKey);
    if (result != ERROR_SUCCESS) {
        spdlog::error("Failed to open registry key");
        return false;
    }

    result = RegDeleteValueA(hKey, key.c_str());
    RegCloseKey(hKey);

    if (result != ERROR_SUCCESS && result != ERROR_FILE_NOT_FOUND) {
        spdlog::error("Failed to delete registry value");
        return false;
    }

    SendMessageTimeoutA(HWND_BROADCAST, WM_SETTINGCHANGE, 0,
                        reinterpret_cast<LPARAM>("Environment"),
                        SMTO_ABORTIFHUNG, 5000, nullptr);
    EnvCore::unsetEnv(key);
    return true;
}
#else
auto EnvPersistent::setPersistentEnvUnix(const String& key, const String& val,
                                         PersistLevel level) -> bool {
    String homeDir = EnvSystem::getHomeDir();
    if (homeDir.empty()) {
        spdlog::error("Failed to get home directory");
        return false;
    }

    std::string filePath;
    if (level == PersistLevel::USER) {
        filePath = getShellProfilePath(homeDir);
    } else {
        filePath = "/etc/environment";
        if (access(filePath.c_str(), W_OK) != 0) {
            spdlog::error("No write permission for system environment file");
            return false;
        }
    }

    // Read existing file
    std::vector<std::string> lines;
    std::ifstream inFile(filePath);
    bool found = false;

    if (inFile.is_open()) {
        std::string line;
        while (std::getline(inFile, line)) {
            std::string pattern = std::string(key.c_str()) + "=";
            if (line.find(pattern) == 0) {
                // Replace existing line
                lines.push_back(pattern + std::string(val.c_str()));
                found = true;
            } else {
                lines.push_back(line);
            }
        }
        inFile.close();
    }

    if (!found) {
        // Add new line
        if (level == PersistLevel::USER) {
            lines.push_back("export " + std::string(key.c_str()) + "=" + std::string(val.c_str()));
        } else {
            lines.push_back(std::string(key.c_str()) + "=" + std::string(val.c_str()));
        }
    }

    // Write back to file
    std::ofstream outFile(filePath);
    if (!outFile.is_open()) {
        spdlog::error("Failed to open file for writing: {}", filePath);
        return false;
    }

    for (const auto& line : lines) {
        outFile << line << std::endl;
    }
    outFile.close();

    // Set in current process
    EnvCore::setEnv(key, val);

    spdlog::info("Successfully set persistent environment variable in {}", filePath);
    return true;
}

auto EnvPersistent::deletePersistentEnvUnix(const String& key,
                                            PersistLevel level) -> bool {
    String homeDir = EnvSystem::getHomeDir();
    if (homeDir.empty()) {
        spdlog::error("Failed to get home directory");
        return false;
    }

    std::string filePath;
    if (level == PersistLevel::USER) {
        filePath = getShellProfilePath(homeDir);
    } else {
        filePath = "/etc/environment";
        if (access(filePath.c_str(), W_OK) != 0) {
            spdlog::error("No write permission for system environment file");
            return false;
        }
    }

    std::vector<std::string> lines;
    std::ifstream inFile(filePath);
    bool found = false;

    if (inFile.is_open()) {
        std::string line;
        while (std::getline(inFile, line)) {
            std::string pattern = std::string(key.c_str());
            pattern += "=";
            if (line.find(pattern) == 0 ||
                line.find("export " + pattern) == 0) {
                found = true;
                continue;  // Skip this line
            }
            lines.push_back(line);
        }
        inFile.close();
    } else {
        spdlog::error("Failed to open file: {}", filePath);
        return false;
    }

    if (!found) {
        spdlog::info("Key not found in {}", filePath);
        return true;
    }

    std::ofstream outFile(filePath);
    if (!outFile.is_open()) {
        spdlog::error("Failed to open file for writing: {}", filePath);
        return false;
    }

    for (const auto& line : lines) {
        outFile << line << std::endl;
    }
    outFile.close();

    EnvCore::unsetEnv(key);
    spdlog::info("Successfully deleted persistent environment variable from {}",
                 filePath);
    return true;
}

auto EnvPersistent::getShellProfilePath(const String& homeDir) -> String {
    std::vector<std::string> profileFiles = {
        std::string(homeDir.c_str()) + "/.bash_profile",
        std::string(homeDir.c_str()) + "/.profile",
        std::string(homeDir.c_str()) + "/.bashrc"
    };

    for (const auto& file : profileFiles) {
        if (std::filesystem::exists(file)) {
            return file;
        }
    }

    // Default to .bashrc if none exist
    return std::string(homeDir.c_str()) + "/.bashrc";
}
#endif

// ========== NEW ENHANCED FUNCTIONALITY ==========

auto EnvPersistent::setPersistentEnvBatch(const HashMap<String, String>& vars,
                                           PersistLevel level) -> size_t {
    size_t successCount = 0;

    for (const auto& [key, value] : vars) {
        if (setPersistentEnv(key, value, level) == PersistenceResult::SUCCESS) {
            successCount++;
        }
    }

    spdlog::info("Successfully set {}/{} persistent variables at level {}",
                 successCount, vars.size(), static_cast<int>(level));
    return successCount;
}

auto EnvPersistent::deletePersistentEnvBatch(const Vector<String>& keys,
                                              PersistLevel level) -> size_t {
    size_t successCount = 0;

    for (const auto& key : keys) {
        auto result = deletePersistentEnv(key, level);
        if (result == PersistenceResult::SUCCESS || result == PersistenceResult::NOT_FOUND) {
            successCount++;
        }
    }

    spdlog::info("Successfully processed {}/{} persistent variables for deletion at level {}",
                 successCount, keys.size(), static_cast<int>(level));
    return successCount;
}

auto EnvPersistent::getPersistentEnv(const String& key, PersistLevel level) -> String {
    if (!validateKey(key)) {
        spdlog::error("Invalid key: {}", key);
        return "";
    }

#ifdef _WIN32
    auto [rootKey, subKey] = getRegistryPath(level);
    return readRegistryValue(rootKey, subKey, key);
#else
    if (level == PersistLevel::USER) {
        String homeDir = EnvSystem::getHomeDir();
        if (homeDir.empty()) {
            return "";
        }
        String filePath = getShellProfilePath(homeDir);
        auto vars = readShellProfile(filePath);
        auto it = vars.find(key);
        return (it != vars.end()) ? it->second : "";
    } else {
        // System level not implemented for Unix
        spdlog::warn("System level persistent variables not supported on Unix");
        return "";
    }
#endif
}

auto EnvPersistent::listPersistentEnv(PersistLevel level) -> HashMap<String, String> {
#ifdef _WIN32
    auto [rootKey, subKey] = getRegistryPath(level);
    return listRegistryValues(rootKey, subKey);
#else
    if (level == PersistLevel::USER) {
        String homeDir = EnvSystem::getHomeDir();
        if (homeDir.empty()) {
            return {};
        }
        String filePath = getShellProfilePath(homeDir);
        return readShellProfile(filePath);
    } else {
        // System level not implemented for Unix
        spdlog::warn("System level persistent variables not supported on Unix");
        return {};
    }
#endif
}

auto EnvPersistent::validatePersistentEnv(const String& key, const String& value,
                                           PersistLevel level) -> bool {
    (void)level;  // Suppress unused parameter warning
    return validateKey(key) && validateValue(value);
}

auto EnvPersistent::backupPersistentEnv(const String& name, PersistLevel level) -> bool {
    try {
        std::lock_guard<std::mutex> lock(sBackupMutex);

        auto vars = listPersistentEnv(level);
        String backupKey = name + "_" + std::to_string(static_cast<int>(level));
        sPersistentBackups[backupKey] = vars;

        spdlog::debug("Created persistent environment backup: {} (level {})", name, static_cast<int>(level));
        return true;
    } catch (const std::exception& e) {
        spdlog::error("Failed to create persistent environment backup '{}': {}", name, e.what());
        return false;
    }
}

auto EnvPersistent::restorePersistentEnv(const String& name, PersistLevel level) -> bool {
    try {
        std::lock_guard<std::mutex> lock(sBackupMutex);

        String backupKey = name + "_" + std::to_string(static_cast<int>(level));
        auto it = sPersistentBackups.find(backupKey);
        if (it == sPersistentBackups.end()) {
            spdlog::error("Persistent environment backup '{}' not found for level {}", name, static_cast<int>(level));
            return false;
        }

        // Clear current persistent variables and restore from backup
        auto currentVars = listPersistentEnv(level);
        for (const auto& [key, value] : currentVars) {
            deletePersistentEnv(key, level);
        }

        size_t restored = setPersistentEnvBatch(it->second, level);
        spdlog::debug("Restored {} persistent variables from backup: {} (level {})",
                      restored, name, static_cast<int>(level));
        return restored > 0;
    } catch (const std::exception& e) {
        spdlog::error("Failed to restore persistent environment backup '{}': {}", name, e.what());
        return false;
    }
}

auto EnvPersistent::listPersistentBackups(PersistLevel level) -> Vector<String> {
    std::lock_guard<std::mutex> lock(sBackupMutex);
    Vector<String> names;

    String levelSuffix = "_" + std::to_string(static_cast<int>(level));

    for (const auto& [backupKey, vars] : sPersistentBackups) {
        if (backupKey.ends_with(levelSuffix)) {
            String name = backupKey.substr(0, backupKey.length() - levelSuffix.length());
            names.push_back(name);
        }
    }

    return names;
}

// Helper method implementations
auto EnvPersistent::validateKey(const String& key) -> bool {
    if (key.empty()) {
        return false;
    }

    // Check for invalid characters
    for (char c : key) {
        if (!std::isalnum(c) && c != '_') {
            return false;
        }
    }

    // Key should not start with a digit
    return !std::isdigit(key[0]);
}

auto EnvPersistent::validateValue(const String& value) -> bool {
    // Check for null bytes
    if (value.find('\0') != String::npos) {
        return false;
    }

    // Check reasonable length limit
    return value.length() <= 32768;  // 32KB limit
}

#ifdef _WIN32
// Windows-specific helper implementations
auto EnvPersistent::getRegistryPath(PersistLevel level) -> std::pair<HKEY, String> {
    if (level == PersistLevel::USER) {
        return {HKEY_CURRENT_USER, "Environment"};
    } else {
        return {HKEY_LOCAL_MACHINE, "SYSTEM\\CurrentControlSet\\Control\\Session Manager\\Environment"};
    }
}

auto EnvPersistent::readRegistryValue(HKEY rootKey, const String& subKey, const String& valueName) -> String {
    HKEY hKey;
    LONG result = RegOpenKeyExA(rootKey, subKey.c_str(), 0, KEY_READ, &hKey);
    if (result != ERROR_SUCCESS) {
        return "";
    }

    DWORD dataSize = 0;
    result = RegQueryValueExA(hKey, valueName.c_str(), nullptr, nullptr, nullptr, &dataSize);
    if (result != ERROR_SUCCESS) {
        RegCloseKey(hKey);
        return "";
    }

    std::vector<char> buffer(dataSize);
    result = RegQueryValueExA(hKey, valueName.c_str(), nullptr, nullptr,
                              reinterpret_cast<LPBYTE>(buffer.data()), &dataSize);
    RegCloseKey(hKey);

    if (result == ERROR_SUCCESS) {
        return String(buffer.data());
    }
    return "";
}

auto EnvPersistent::writeRegistryValue(HKEY rootKey, const String& subKey,
                                       const String& valueName, const String& value) -> bool {
    HKEY hKey;
    LONG result = RegOpenKeyExA(rootKey, subKey.c_str(), 0, KEY_SET_VALUE, &hKey);
    if (result != ERROR_SUCCESS) {
        return false;
    }

    result = RegSetValueExA(hKey, valueName.c_str(), 0, REG_SZ,
                            reinterpret_cast<const BYTE*>(value.c_str()),
                            static_cast<DWORD>(value.length() + 1));
    RegCloseKey(hKey);

    return result == ERROR_SUCCESS;
}

auto EnvPersistent::deleteRegistryValue(HKEY rootKey, const String& subKey, const String& valueName) -> bool {
    HKEY hKey;
    LONG result = RegOpenKeyExA(rootKey, subKey.c_str(), 0, KEY_SET_VALUE, &hKey);
    if (result != ERROR_SUCCESS) {
        return false;
    }

    result = RegDeleteValueA(hKey, valueName.c_str());
    RegCloseKey(hKey);

    return result == ERROR_SUCCESS || result == ERROR_FILE_NOT_FOUND;
}

auto EnvPersistent::listRegistryValues(HKEY rootKey, const String& subKey) -> HashMap<String, String> {
    HashMap<String, String> result;

    HKEY hKey;
    LONG openResult = RegOpenKeyExA(rootKey, subKey.c_str(), 0, KEY_READ, &hKey);
    if (openResult != ERROR_SUCCESS) {
        return result;
    }

    DWORD index = 0;
    char valueName[256];
    DWORD valueNameSize;
    DWORD dataSize;

    while (true) {
        valueNameSize = sizeof(valueName);
        dataSize = 0;

        LONG enumResult = RegEnumValueA(hKey, index, valueName, &valueNameSize,
                                        nullptr, nullptr, nullptr, &dataSize);
        if (enumResult != ERROR_SUCCESS) {
            break;
        }

        std::vector<char> data(dataSize);
        LONG queryResult = RegQueryValueExA(hKey, valueName, nullptr, nullptr,
                                            reinterpret_cast<LPBYTE>(data.data()), &dataSize);
        if (queryResult == ERROR_SUCCESS) {
            result[String(valueName)] = String(data.data());
        }

        index++;
    }

    RegCloseKey(hKey);
    return result;
}

#else
// Unix-specific helper implementations
auto EnvPersistent::readShellProfile(const String& filePath) -> HashMap<String, String> {
    HashMap<String, String> result;

    std::ifstream file(std::string(filePath.c_str()));
    if (!file.is_open()) {
        return result;
    }

    std::string line;
    while (std::getline(file, line)) {
        // Look for export statements
        if (line.find("export ") == 0) {
            size_t eqPos = line.find('=');
            if (eqPos != std::string::npos) {
                String key = String(line.substr(7, eqPos - 7));  // Skip "export "
                String value = String(line.substr(eqPos + 1));

                // Remove quotes if present
                if (value.length() >= 2 && value.front() == '"' && value.back() == '"') {
                    value = value.substr(1, value.length() - 2);
                }

                result[key] = value;
            }
        }
    }

    return result;
}

auto EnvPersistent::writeShellProfile(const String& filePath, const HashMap<String, String>& vars) -> bool {
    // This is a simplified implementation - in practice, you'd want to preserve existing content
    std::ofstream file(std::string(filePath.c_str()));
    if (!file.is_open()) {
        return false;
    }

    file << "# Environment variables\n";
    for (const auto& [key, value] : vars) {
        file << "export " << key << "=\"" << value << "\"\n";
    }

    return true;
}

auto EnvPersistent::appendToShellProfile(const String& filePath, const String& key, const String& value) -> bool {
    std::ofstream file(std::string(filePath.c_str()), std::ios::app);
    if (!file.is_open()) {
        return false;
    }

    file << "export " << key << "=\"" << value << "\"\n";
    return true;
}

auto EnvPersistent::removeFromShellProfile(const String& filePath, const String& key) -> bool {
    // This is a simplified implementation - in practice, you'd want to edit the file in place
    auto vars = readShellProfile(filePath);
    vars.erase(key);
    return writeShellProfile(filePath, vars);
}
#endif

}  // namespace atom::utils
