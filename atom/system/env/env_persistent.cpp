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

auto EnvPersistent::setPersistentEnv(const String& key, const String& val,
                                     PersistLevel level) -> bool {
    spdlog::info("Setting persistent environment variable: {}={} at level {}",
                 key, val, static_cast<int>(level));

    if (level == PersistLevel::PROCESS) {
        // Just set in current process
        return EnvCore::setEnv(key, val);
    }

#ifdef _WIN32
    return setPersistentEnvWindows(key, val, level);
#else
    return setPersistentEnvUnix(key, val, level);
#endif
}

auto EnvPersistent::deletePersistentEnv(const String& key,
                                        PersistLevel level) -> bool {
    spdlog::info("Deleting persistent environment variable: {} at level {}",
                 key, static_cast<int>(level));

    if (level == PersistLevel::PROCESS) {
        // Just unset in current process
        EnvCore::unsetEnv(key);
        return true;
    }

#ifdef _WIN32
    return deletePersistentEnvWindows(key, level);
#else
    return deletePersistentEnvUnix(key, level);
#endif
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

}  // namespace atom::utils
