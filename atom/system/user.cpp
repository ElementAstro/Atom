/*
 * user.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#include "user.hpp"

#include <algorithm>
#include <array>
#include <cstdlib>

#ifdef _WIN32
// clang-format off
#include <windows.h>
#include <lmcons.h>
#include <tchar.h>
#include <userenv.h>
#include <wtsapi32.h>
#include <lm.h>
#include <memory>
#include "atom/utils/string.hpp"
// clang-format on
#ifdef _MSC_VER
#pragma comment(lib, "wtsapi32.lib")
#endif
#else
#include <grp.h>
#include <pwd.h>
#include <sys/sysinfo.h>
#include <sys/types.h>
#include <unistd.h>
#include <utmp.h>
#include <climits>
#include <codecvt>
#include <locale>
#endif

#include <spdlog/spdlog.h>

namespace std {
template <>
struct formatter<std::wstring> {
    constexpr auto parse(format_parse_context &ctx) { return ctx.end(); }

    template <typename FormatContext>
    auto format(const std::wstring &wstr, FormatContext &ctx) {
        return format_to(ctx.out(), "{}",
                         std::wstring_view(wstr.data(), wstr.size()));
    }
};
}  // namespace std

namespace atom::system {
auto isRoot() -> bool {
    spdlog::debug("Checking if current user has root/administrator privileges");
#ifdef _WIN32
    HANDLE hToken;
    TOKEN_ELEVATION elevation;
    DWORD dwSize;

    if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken) == 0) {
        spdlog::error("Failed to open process token for elevation check");
        return false;
    }

    if (GetTokenInformation(hToken, TokenElevation, &elevation,
                            sizeof(elevation), &dwSize) == 0) {
        spdlog::error("Failed to get token elevation information");
        CloseHandle(hToken);
        return false;
    }

    bool elevated = (elevation.TokenIsElevated != 0);
    CloseHandle(hToken);
    spdlog::debug("User elevation status: {}", elevated ? "elevated" : "not elevated");
    return elevated;
#else
    bool result = (getuid() == 0);
    spdlog::debug("User root status: {}", result ? "root" : "not root");
    return result;
#endif
}

auto getUserGroups() -> std::vector<std::wstring> {
    spdlog::debug("Retrieving user groups");
    std::vector<std::wstring> groups;

#ifdef _WIN32
    HANDLE hToken;
    if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken) == 0) {
        spdlog::error("Failed to open process token for group enumeration");
        return groups;
    }
    
    DWORD bufferSize = 0;
    GetTokenInformation(hToken, TokenGroups, nullptr, 0, &bufferSize);
    if (GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
        spdlog::error("Failed to get required buffer size for token groups");
        CloseHandle(hToken);
        return groups;
    }

    std::vector<BYTE> buffer(bufferSize);
    if (GetTokenInformation(hToken, TokenGroups, buffer.data(), bufferSize, &bufferSize) == 0) {
        spdlog::error("Failed to retrieve token group information");
        CloseHandle(hToken);
        return groups;
    }

    auto *pTokenGroups = reinterpret_cast<PTOKEN_GROUPS>(buffer.data());

    for (DWORD i = 0; i < pTokenGroups->GroupCount; i++) {
        SID_NAME_USE sidUse;
        DWORD nameLength = 0;
        DWORD domainLength = 0;
        LookupAccountSid(nullptr, pTokenGroups->Groups[i].Sid, nullptr,
                         &nameLength, nullptr, &domainLength, &sidUse);
        if (GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
            continue;
        }

        std::vector<TCHAR> nameBuffer(nameLength);
        std::vector<TCHAR> domainBuffer(domainLength);
        if (LookupAccountSid(nullptr, pTokenGroups->Groups[i].Sid,
                            nameBuffer.data(), &nameLength,
                            domainBuffer.data(), &domainLength, &sidUse)) {
            std::wstring nameStr(nameBuffer.begin(), nameBuffer.end());
            groups.push_back(nameStr);
            spdlog::debug("Found group: {}", atom::utils::wstringToString(nameStr));
        }
    }

    CloseHandle(hToken);
#else
    gid_t *groupsArray = nullptr;
    int groupCount = getgroups(0, nullptr);
    if (groupCount == -1) {
        spdlog::error("Failed to get user group count");
        return groups;
    }

    groupsArray = new gid_t[groupCount];
    if (getgroups(groupCount, groupsArray) == -1) {
        spdlog::error("Failed to retrieve user groups");
        delete[] groupsArray;
        return groups;
    }

    for (int i = 0; i < groupCount; i++) {
        struct group *grp = getgrgid(groupsArray[i]);
        if (grp != nullptr) {
            std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
            std::wstring nameStr = converter.from_bytes(grp->gr_name);
            groups.push_back(nameStr);
            spdlog::debug("Found group: {}", grp->gr_name);
        }
    }

    delete[] groupsArray;
#endif

    spdlog::debug("Retrieved {} user groups", groups.size());
    return groups;
}

auto getUsername() -> std::string {
    spdlog::debug("Retrieving current username");
    std::string username;
#ifdef _WIN32
    std::array<char, UNLEN + 1> buffer;
    DWORD size = UNLEN + 1;
    if (GetUserNameA(buffer.data(), &size) != 0) {
        username = std::string(buffer.data(), size - 1);
    } else {
        spdlog::error("Failed to get username on Windows");
    }
#else
    char *buffer = getlogin();
    if (buffer != nullptr) {
        username = buffer;
    } else {
        spdlog::error("Failed to get username on Unix");
    }
#endif
    spdlog::debug("Username: {}", username);
    return username;
}

auto getHostname() -> std::string {
    spdlog::debug("Retrieving system hostname");
    std::string hostname;
#ifdef _WIN32
    std::array<char, MAX_COMPUTERNAME_LENGTH + 1> buffer;
    DWORD size = MAX_COMPUTERNAME_LENGTH + 1;
    if (GetComputerNameA(buffer.data(), &size) != 0) {
        hostname = std::string(buffer.data(), size);
    } else {
        spdlog::error("Failed to get hostname on Windows");
    }
#else
    std::array<char, 256> buffer;
    if (gethostname(buffer.data(), buffer.size()) == 0) {
        hostname = buffer.data();
    } else {
        spdlog::error("Failed to get hostname on Unix");
    }
#endif
    spdlog::debug("Hostname: {}", hostname);
    return hostname;
}

auto getUserId() -> int {
    spdlog::debug("Retrieving current user ID");
    int userId = 0;
#ifdef _WIN32
    HANDLE hToken;
    if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken) != 0) {
        DWORD dwLengthNeeded;
        GetTokenInformation(hToken, TokenUser, nullptr, 0, &dwLengthNeeded);
        auto pTokenUser = std::unique_ptr<TOKEN_USER, decltype(&free)>(
            static_cast<TOKEN_USER *>(malloc(dwLengthNeeded)), free);
        if (GetTokenInformation(hToken, TokenUser, pTokenUser.get(),
                                dwLengthNeeded, &dwLengthNeeded) != 0) {
            PSID sid = pTokenUser->User.Sid;
            DWORD subAuthorityCount = *GetSidSubAuthorityCount(sid);
            DWORD *subAuthority = GetSidSubAuthority(sid, subAuthorityCount - 1);
            userId = static_cast<int>(*subAuthority);
        } else {
            spdlog::error("Failed to get user token information");
        }
        CloseHandle(hToken);
    } else {
        spdlog::error("Failed to open process token for user ID");
    }
#else
    userId = getuid();
#endif
    spdlog::debug("User ID: {}", userId);
    return userId;
}

auto getGroupId() -> int {
    spdlog::debug("Retrieving current group ID");
    int groupId = 0;
#ifdef _WIN32
    HANDLE hToken;
    if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken) != 0) {
        DWORD dwLengthNeeded;
        GetTokenInformation(hToken, TokenPrimaryGroup, nullptr, 0, &dwLengthNeeded);
        auto pTokenPrimaryGroup = std::unique_ptr<TOKEN_PRIMARY_GROUP, decltype(&free)>(
            static_cast<TOKEN_PRIMARY_GROUP *>(malloc(dwLengthNeeded)), free);
        if (GetTokenInformation(hToken, TokenPrimaryGroup,
                                pTokenPrimaryGroup.get(), dwLengthNeeded, &dwLengthNeeded) != 0) {
            PSID sid = pTokenPrimaryGroup->PrimaryGroup;
            DWORD subAuthorityCount = *GetSidSubAuthorityCount(sid);
            DWORD *subAuthority = GetSidSubAuthority(sid, subAuthorityCount - 1);
            groupId = static_cast<int>(*subAuthority);
        } else {
            spdlog::error("Failed to get primary group token information");
        }
        CloseHandle(hToken);
    } else {
        spdlog::error("Failed to open process token for group ID");
    }
#else
    groupId = getgid();
#endif
    spdlog::debug("Group ID: {}", groupId);
    return groupId;
}

#ifdef _WIN32
auto getUserProfileDirectory() -> std::string {
    spdlog::debug("Retrieving user profile directory (Windows)");
    std::string userProfileDir;
    HANDLE hToken;
    if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken) != 0) {
        DWORD dwSize = 0;
        GetUserProfileDirectoryA(hToken, nullptr, &dwSize);
        auto buffer = std::make_unique<char[]>(dwSize);
        if (GetUserProfileDirectoryA(hToken, buffer.get(), &dwSize) != 0) {
            userProfileDir = buffer.get();
        } else {
            spdlog::error("Failed to get user profile directory");
        }
        CloseHandle(hToken);
    } else {
        spdlog::error("Failed to open process token for profile directory");
    }
    spdlog::debug("User profile directory: {}", userProfileDir);
    return userProfileDir;
}
#endif

auto getHomeDirectory() -> std::string {
    spdlog::debug("Retrieving user home directory");
    std::string homeDir;
#ifdef _WIN32
    homeDir = getUserProfileDirectory();
#else
    int userId = getUserId();
    struct passwd *userInfo = getpwuid(userId);
    if (userInfo != nullptr) {
        homeDir = std::string(userInfo->pw_dir);
    } else {
        spdlog::error("Failed to get user information for home directory");
    }
#endif
    spdlog::debug("Home directory: {}", homeDir);
    return homeDir;
}

auto getCurrentWorkingDirectory() -> std::string {
    spdlog::debug("Retrieving current working directory");
#ifdef _WIN32
    std::array<char, MAX_PATH> cwd;
    if (GetCurrentDirectory(cwd.size(), cwd.data())) {
        std::string result = cwd.data();
        spdlog::debug("Current working directory: {}", result);
        return result;
    }
    spdlog::error("Failed to get current working directory on Windows");
    return "";
#else
    std::array<char, PATH_MAX> cwd;
    if (getcwd(cwd.data(), cwd.size()) != nullptr) {
        std::string result = cwd.data();
        spdlog::debug("Current working directory: {}", result);
        return result;
    }
    spdlog::error("Failed to get current working directory on Unix");
    return "";
#endif
}

auto getLoginShell() -> std::string {
    spdlog::debug("Retrieving login shell");
    std::string loginShell;
#ifdef _WIN32
    std::array<char, MAX_PATH> buf;
    DWORD bufSize = buf.size();
    if (GetEnvironmentVariableA("COMSPEC", buf.data(), bufSize) != 0) {
        loginShell = std::string(buf.data());
    } else {
        spdlog::error("Failed to get COMSPEC environment variable");
    }
#else
    int userId = getUserId();
    struct passwd *userInfo = getpwuid(userId);
    if (userInfo != nullptr) {
        loginShell = std::string(userInfo->pw_shell);
    } else {
        spdlog::error("Failed to get user information for login shell");
    }
#endif
    spdlog::debug("Login shell: {}", loginShell);
    return loginShell;
}

auto getLogin() -> std::string {
    spdlog::debug("Retrieving login name");
#ifdef _WIN32
    std::array<char, UNLEN + 1> buffer;
    DWORD bufferSize = buffer.size();
    if (GetUserNameA(buffer.data(), &bufferSize) != 0) {
        std::string result = buffer.data();
        spdlog::debug("Login name: {}", result);
        return result;
    }
    spdlog::error("Failed to get login name on Windows");
#else
    char *username = ::getlogin();
    if (username != nullptr) {
        std::string result = username;
        spdlog::debug("Login name: {}", result);
        return result;
    }
    spdlog::error("Failed to get login name on Unix");
#endif
    return "";
}

auto getEnvironmentVariable(const std::string &name) -> std::string {
    spdlog::debug("Getting environment variable: {}", name);
    std::string value;

#ifdef _WIN32
    DWORD size = GetEnvironmentVariableA(name.c_str(), nullptr, 0);
    if (size > 0) {
        std::vector<char> buffer(size);
        GetEnvironmentVariableA(name.c_str(), buffer.data(), size);
        value = std::string(buffer.data(), buffer.size() - 1);
    } else {
        spdlog::debug("Environment variable '{}' not found", name);
    }
#else
    const char *envValue = std::getenv(name.c_str());
    if (envValue != nullptr) {
        value = envValue;
    } else {
        spdlog::debug("Environment variable '{}' not found", name);
    }
#endif

    spdlog::debug("Environment variable '{}' = '{}'", name, value);
    return value;
}

auto getAllEnvironmentVariables() -> std::unordered_map<std::string, std::string> {
    spdlog::debug("Retrieving all environment variables");
    std::unordered_map<std::string, std::string> envVars;

#ifdef _WIN32
    LPCH envStrings = GetEnvironmentStrings();
    if (envStrings != nullptr) {
        LPCH current = envStrings;
        while (*current) {
            std::string envString(current);
            size_t pos = envString.find('=');
            if (pos != std::string::npos && pos > 0) {
                std::string name = envString.substr(0, pos);
                std::string value = envString.substr(pos + 1);
                envVars[name] = value;
            }
            current += envString.length() + 1;
        }
        FreeEnvironmentStrings(envStrings);
    } else {
        spdlog::error("Failed to get environment strings on Windows");
    }
#else
    extern char **environ;
    for (char **env = environ; *env != nullptr; ++env) {
        std::string envString(*env);
        size_t pos = envString.find('=');
        if (pos != std::string::npos) {
            std::string name = envString.substr(0, pos);
            std::string value = envString.substr(pos + 1);
            envVars[name] = value;
        }
    }
#endif

    spdlog::debug("Retrieved {} environment variables", envVars.size());
    return envVars;
}

auto setEnvironmentVariable(const std::string &name, const std::string &value) -> bool {
    spdlog::debug("Setting environment variable '{}' = '{}'", name, value);
    bool success = false;

#ifdef _WIN32
    success = (SetEnvironmentVariableA(name.c_str(), value.c_str()) != 0);
#else
    success = (setenv(name.c_str(), value.c_str(), 1) == 0);
#endif

    if (success) {
        spdlog::debug("Successfully set environment variable '{}'", name);
    } else {
        spdlog::error("Failed to set environment variable '{}'", name);
    }
    return success;
}

auto getSystemUptime() -> uint64_t {
    spdlog::debug("Retrieving system uptime");
    uint64_t uptime = 0;

#ifdef _WIN32
    uptime = GetTickCount64() / 1000;
#else
    struct sysinfo info;
    if (sysinfo(&info) == 0) {
        uptime = static_cast<uint64_t>(info.uptime);
    } else {
        spdlog::error("Failed to get system uptime");
    }
#endif

    spdlog::debug("System uptime: {} seconds", uptime);
    return uptime;
}

auto getLoggedInUsers() -> std::vector<std::string> {
    spdlog::debug("Retrieving logged-in users");
    std::vector<std::string> users;

#ifdef _WIN32
    WTS_SESSION_INFO *sessionInfo = nullptr;
    DWORD sessionCount = 0;

    if (WTSEnumerateSessions(WTS_CURRENT_SERVER_HANDLE, 0, 1, &sessionInfo, &sessionCount)) {
        for (DWORD i = 0; i < sessionCount; i++) {
            if (sessionInfo[i].State == WTSActive) {
                LPSTR buffer = nullptr;
                DWORD bytesReturned = 0;

                if (WTSQuerySessionInformationA(WTS_CURRENT_SERVER_HANDLE, sessionInfo[i].SessionId,
                                              WTSUserName, &buffer, &bytesReturned)) {
                    if (buffer && bytesReturned > 1) {
                        std::string username(buffer);
                        if (!username.empty() && 
                            std::find(users.begin(), users.end(), username) == users.end()) {
                            users.push_back(username);
                            spdlog::debug("Found logged-in user: {}", username);
                        }
                    }
                    WTSFreeMemory(buffer);
                }
            }
        }
        WTSFreeMemory(sessionInfo);
    } else {
        spdlog::error("Failed to enumerate WTS sessions");
    }
#else
    setutent();
    struct utmp *entry;

    while ((entry = getutent()) != nullptr) {
        if (entry->ut_type == USER_PROCESS) {
            std::string username(entry->ut_user);
            if (!username.empty() && 
                std::find(users.begin(), users.end(), username) == users.end()) {
                users.push_back(username);
                spdlog::debug("Found logged-in user: {}", username);
            }
        }
    }
    endutent();
#endif

    spdlog::debug("Found {} logged-in users", users.size());
    return users;
}

auto userExists(const std::string &username) -> bool {
    spdlog::debug("Checking if user exists: {}", username);
    bool exists = false;

#ifdef _WIN32
    DWORD level = 1;
    USER_INFO_1 *userInfo = nullptr;
    std::wstring wUsername(username.begin(), username.end());
    NET_API_STATUS status = NetUserGetInfo(nullptr, wUsername.c_str(), level, (LPBYTE *)&userInfo);

    exists = (status == NERR_Success);

    if (userInfo != nullptr) {
        NetApiBufferFree(userInfo);
    }
#else
    struct passwd *pwd = getpwnam(username.c_str());
    exists = (pwd != nullptr);
#endif

    spdlog::debug("User '{}' exists: {}", username, exists ? "yes" : "no");
    return exists;
}
}  // namespace atom::system
