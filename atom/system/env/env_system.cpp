/*
 * env_system.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-12-16

Description: System information and directories implementation

**************************************************/

#include "env_system.hpp"

#ifdef _WIN32
#include <shlobj.h>
#include <windows.h>
#else
#include <limits.h>
#include <pwd.h>
#include <sys/types.h>
#include <unistd.h>
#endif

#include "env_core.hpp"
#include <spdlog/spdlog.h>

namespace atom::utils {

auto EnvSystem::getHomeDir() -> String {
    spdlog::debug("Getting home directory");
    String homePath;

#ifdef _WIN32
    homePath = EnvCore::getEnv("USERPROFILE", "");
    if (homePath.empty()) {
        String homeDrive = EnvCore::getEnv("HOMEDRIVE", "");
        String homePath2 = EnvCore::getEnv("HOMEPATH", "");
        if (!homeDrive.empty() && !homePath2.empty()) {
            homePath = homeDrive + homePath2;
        }
    }
#else
    homePath = EnvCore::getEnv("HOME", "");
    if (homePath.empty()) {
        // Try to get from passwd
        uid_t uid = geteuid();
        struct passwd* pw = getpwuid(uid);
        if (pw && pw->pw_dir) {
            homePath = pw->pw_dir;
        }
    }
#endif

    if (homePath.empty()) {
        spdlog::error("Failed to determine home directory");
    } else {
        spdlog::debug("Home directory: {}", homePath);
    }

    return homePath;
}

auto EnvSystem::getTempDir() -> String {
    spdlog::debug("Getting temporary directory");
    String tempPath;

#ifdef _WIN32
    tempPath = EnvCore::getEnv("TEMP", "");
    if (tempPath.empty()) {
        tempPath = EnvCore::getEnv("TMP", "");
    }
    if (tempPath.empty()) {
        tempPath = "C:\\Windows\\Temp";
    }
#else
    tempPath = EnvCore::getEnv("TMPDIR", "");
    if (tempPath.empty()) {
        tempPath = "/tmp";
    }
#endif

    spdlog::debug("Temporary directory: {}", tempPath);
    return tempPath;
}

auto EnvSystem::getConfigDir() -> String {
    spdlog::debug("Getting configuration directory");
    String configPath;

#ifdef _WIN32
    configPath = EnvCore::getEnv("APPDATA", "");
    if (configPath.empty()) {
        String userProfile = getHomeDir();
        if (!userProfile.empty()) {
            configPath = userProfile + "\\AppData\\Roaming";
        }
    }
#elif defined(__APPLE__)
    String home = getHomeDir();
    if (!home.empty()) {
        configPath = home + "/Library/Application Support";
    }
#else
    configPath = EnvCore::getEnv("XDG_CONFIG_HOME", "");
    if (configPath.empty()) {
        String home = getHomeDir();
        if (!home.empty()) {
            configPath = home + "/.config";
        }
    }
#endif

    spdlog::debug("Configuration directory: {}", configPath);
    return configPath;
}

auto EnvSystem::getDataDir() -> String {
    spdlog::debug("Getting data directory");
    String dataPath;

#ifdef _WIN32
    dataPath = EnvCore::getEnv("LOCALAPPDATA", "");
    if (dataPath.empty()) {
        String userProfile = getHomeDir();
        if (!userProfile.empty()) {
            dataPath = userProfile + "\\AppData\\Local";
        }
    }
#elif defined(__APPLE__)
    String home = getHomeDir();
    if (!home.empty()) {
        dataPath = home + "/Library/Application Support";
    }
#else
    dataPath = EnvCore::getEnv("XDG_DATA_HOME", "");
    if (dataPath.empty()) {
        String home = getHomeDir();
        if (!home.empty()) {
            dataPath = home + "/.local/share";
        }
    }
#endif

    spdlog::debug("Data directory: {}", dataPath);
    return dataPath;
}

auto EnvSystem::getSystemName() -> String {
#ifdef _WIN32
    return "Windows";
#elif defined(__APPLE__)
    return "macOS";
#elif defined(__linux__)
    return "Linux";
#elif defined(__FreeBSD__)
    return "FreeBSD";
#elif defined(__unix__)
    return "Unix";
#else
    return "Unknown";
#endif
}

auto EnvSystem::getSystemArch() -> String {
#if defined(__x86_64__) || defined(_M_X64)
    return "x86_64";
#elif defined(__i386) || defined(_M_IX86)
    return "x86";
#elif defined(__aarch64__) || defined(_M_ARM64)
    return "arm64";
#elif defined(__arm__) || defined(_M_ARM)
    return "arm";
#else
    return "unknown";
#endif
}

auto EnvSystem::getCurrentUser() -> String {
    String username;

#ifdef _WIN32
    DWORD size = 256;
    char buffer[256];
    if (GetUserNameA(buffer, &size)) {
        username = String(buffer);
    } else {
        spdlog::error("getCurrentUser: GetUserNameA failed with error {}",
                      GetLastError());
        username = EnvCore::getEnv("USERNAME", "unknown");
    }
#else
    username = EnvCore::getEnv("USER", "");
    if (username.empty()) {
        username = EnvCore::getEnv("LOGNAME", "");
    }

    if (username.empty()) {
        // Try to get from passwd
        uid_t uid = geteuid();
        struct passwd* pw = getpwuid(uid);
        if (pw) {
            username = pw->pw_name;
        } else {
            username = "unknown";
        }
    }
#endif

    spdlog::info("getCurrentUser returning: {}", username);
    return username;
}

auto EnvSystem::getHostName() -> String {
    spdlog::info("getHostName called");

    String hostname;

#ifdef _WIN32
    DWORD size = MAX_COMPUTERNAME_LENGTH + 1;
    char buffer[MAX_COMPUTERNAME_LENGTH + 1];
    if (GetComputerNameA(buffer, &size)) {
        hostname = String(buffer, size);
    } else {
        spdlog::error("getHostName: GetComputerNameA failed with error {}",
                      GetLastError());

        hostname = EnvCore::getEnv("COMPUTERNAME", "unknown");
    }
#else
    char buffer[HOST_NAME_MAX + 1];
    if (gethostname(buffer, sizeof(buffer)) == 0) {
        hostname = buffer;
    } else {
        spdlog::error("getHostName: gethostname failed with error {}", errno);
        hostname = EnvCore::getEnv("HOSTNAME", "unknown");
    }
#endif

    spdlog::info("getHostName returning: {}", hostname);
    return hostname;
}

}  // namespace atom::utils
