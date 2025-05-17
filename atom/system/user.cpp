/*
 * user.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-6-17

Description: Some system functions to get user information.

**************************************************/

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

#include "atom/log/loguru.hpp"

namespace std {
template <>
struct formatter<std::wstring> {
    constexpr auto parse(format_parse_context &ctx) { return ctx.end(); }

    // 格式化输出
    template <typename FormatContext>
    auto format(const std::wstring &wstr, FormatContext &ctx) {
        return format_to(ctx.out(), "{}",
                         std::wstring_view(wstr.data(), wstr.size()));
    }
};
}  // namespace std

namespace atom::system {
auto isRoot() -> bool {
    LOG_F(INFO, "isRoot called");
#ifdef _WIN32
    HANDLE hToken;
    TOKEN_ELEVATION elevation;
    DWORD dwSize;

    if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken) == 0) {
        LOG_F(ERROR, "isRoot error: OpenProcessToken error");
        return false;
    }

    if (GetTokenInformation(hToken, TokenElevation, &elevation,
                            sizeof(elevation), &dwSize) == 0) {
        LOG_F(ERROR, "isRoot error: GetTokenInformation error");
        CloseHandle(hToken);
        return false;
    }

    bool elevated = (elevation.TokenIsElevated != 0);
    CloseHandle(hToken);
    LOG_F(INFO, "isRoot completed with result: {}", elevated);
    return elevated;
#else
    bool result = (getuid() == 0);
    LOG_F(INFO, "isRoot completed with result: {}", result);
    return result;
#endif
}

auto getUserGroups() -> std::vector<std::wstring> {
    LOG_F(INFO, "getUserGroups called");
    std::vector<std::wstring> groups;

#ifdef _WIN32
    HANDLE hToken;
    if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken) == 0) {
        LOG_F(ERROR, "Failed to open process token.");
        return groups;
    }
    DWORD bufferSize = 0;
    GetTokenInformation(hToken, TokenGroups, nullptr, 0, &bufferSize);
    if (GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
        LOG_F(ERROR, "Failed to get token information size.");
        CloseHandle(hToken);
        return groups;
    }

    std::vector<BYTE> buffer(bufferSize);
    if (GetTokenInformation(hToken, TokenGroups, buffer.data(), bufferSize,
                            &bufferSize) == 0) {
        LOG_F(ERROR, "Failed to get token information.");
        CloseHandle(hToken);
        return groups;
    }

    // 解析用户组信息
    auto *pTokenGroups = reinterpret_cast<PTOKEN_GROUPS>(buffer.data());

    for (DWORD i = 0; i < pTokenGroups->GroupCount; i++) {
        SID_NAME_USE sidUse;
        DWORD nameLength = 0;
        DWORD domainLength = 0;
        LookupAccountSid(nullptr, pTokenGroups->Groups[i].Sid, nullptr,
                         &nameLength, nullptr, &domainLength, &sidUse);
        if (GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
            LOG_F(ERROR, "Failed to get account name and domain length.");
            CloseHandle(hToken);
            return groups;
        }

        std::vector<TCHAR> nameBuffer(nameLength);
        std::vector<TCHAR> domainBuffer(domainLength);
        if (!LookupAccountSid(nullptr, pTokenGroups->Groups[i].Sid,
                              nameBuffer.data(), &nameLength,
                              domainBuffer.data(), &domainLength, &sidUse)) {
            LOG_F(ERROR, "Failed to lookup account SID.");
            CloseHandle(hToken);
            return groups;
        }

        std::wstring groupName;
        std::wstring nameStr(nameBuffer.begin(), nameBuffer.end());
        groupName += nameStr;
        groups.push_back(groupName);
        LOG_F(INFO, "Found group: {}", atom::utils::wstringToString(nameStr));
    }

    CloseHandle(hToken);
#else
    // 获取用户组信息
    gid_t *groupsArray = nullptr;
    int groupCount = getgroups(0, nullptr);
    if (groupCount == -1) {
        LOG_F(ERROR, "Failed to get user group count.");
        return groups;
    }

    groupsArray = new gid_t[groupCount];
    if (getgroups(groupCount, groupsArray) == -1) {
        LOG_F(ERROR, "Failed to get user groups.");
        delete[] groupsArray;
        return groups;
    }

    // 解析用户组信息
    for (int i = 0; i < groupCount; i++) {
        struct group *grp = getgrgid(groupsArray[i]);
        if (grp != nullptr) {
            std::wstring groupName;
            std::wstring_convert<std::codecvt_utf8_utf16<wchar_t> > converter;
            std::wstring nameStr = converter.from_bytes(grp->gr_name);
            groupName += nameStr;
            groups.push_back(groupName);
        }
    }

    delete[] groupsArray;
#endif

    LOG_F(INFO, "getUserGroups completed with {} groups found", groups.size());
    return groups;
}

auto getUsername() -> std::string {
    LOG_F(INFO, "getUsername called");
    std::string username;
#ifdef _WIN32
    std::array<char, UNLEN + 1> buffer;
    DWORD size = UNLEN + 1;
    if (GetUserNameA(buffer.data(), &size) != 0) {
        username = std::string(buffer.data(), size - 1);
    }
#else
    char *buffer;
    buffer = getlogin();
    if (buffer != nullptr) {
        username = buffer;
    }
#endif
    LOG_F(INFO, "getUsername completed with result: {}", username);
    return username;
}

auto getHostname() -> std::string {
    LOG_F(INFO, "getHostname called");
    std::string hostname;
#ifdef _WIN32
    std::array<char, MAX_COMPUTERNAME_LENGTH + 1> buffer;
    DWORD size = MAX_COMPUTERNAME_LENGTH + 1;
    if (GetComputerNameA(buffer.data(), &size) != 0) {
        hostname = std::string(buffer.data(), size);
    }
#else
    std::array<char, 256> buffer;
    if (gethostname(buffer.data(), buffer.size()) == 0) {
        hostname = buffer.data();
    }
#endif
    LOG_F(INFO, "getHostname completed with result: {}", hostname);
    return hostname;
}

auto getUserId() -> int {
    LOG_F(INFO, "getUserId called");
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
            DWORD *subAuthority =
                GetSidSubAuthority(sid, subAuthorityCount - 1);
            userId = static_cast<int>(*subAuthority);
        }
        CloseHandle(hToken);
    }
#else
    userId = getuid();
#endif
    LOG_F(INFO, "getUserId completed with result: {}", userId);
    return userId;
}

auto getGroupId() -> int {
    LOG_F(INFO, "getGroupId called");
    int groupId = 0;
#ifdef _WIN32
    HANDLE hToken;
    if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken) != 0) {
        DWORD dwLengthNeeded;
        GetTokenInformation(hToken, TokenPrimaryGroup, nullptr, 0,
                            &dwLengthNeeded);
        auto pTokenPrimaryGroup =
            std::unique_ptr<TOKEN_PRIMARY_GROUP, decltype(&free)>(
                static_cast<TOKEN_PRIMARY_GROUP *>(malloc(dwLengthNeeded)),
                free);
        if (GetTokenInformation(hToken, TokenPrimaryGroup,
                                pTokenPrimaryGroup.get(), dwLengthNeeded,
                                &dwLengthNeeded) != 0) {
            PSID sid = pTokenPrimaryGroup->PrimaryGroup;
            DWORD subAuthorityCount = *GetSidSubAuthorityCount(sid);
            DWORD *subAuthority =
                GetSidSubAuthority(sid, subAuthorityCount - 1);
            groupId = static_cast<int>(*subAuthority);
        }
        CloseHandle(hToken);
    }
#else
    groupId = getgid();
#endif
    LOG_F(INFO, "getGroupId completed with result: {}", groupId);
    return groupId;
}

#ifdef _WIN32
auto getUserProfileDirectory() -> std::string {
    LOG_F(INFO, "getUserProfileDirectory called");
    std::string userProfileDir;
    HANDLE hToken;
    if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken) != 0) {
        DWORD dwSize = 0;
        GetUserProfileDirectoryA(hToken, nullptr, &dwSize);
        auto buffer = std::make_unique<char[]>(dwSize);
        if (GetUserProfileDirectoryA(hToken, buffer.get(), &dwSize) != 0) {
            userProfileDir = buffer.get();
        }
        CloseHandle(hToken);
    }
    LOG_F(INFO, "getUserProfileDirectory completed with result: {}",
          userProfileDir);
    return userProfileDir;
}
#endif

auto getHomeDirectory() -> std::string {
    LOG_F(INFO, "getHomeDirectory called");
    std::string homeDir;
#ifdef _WIN32
    homeDir = getUserProfileDirectory();
#else
    int userId = getUserId();
    struct passwd *userInfo = getpwuid(userId);
    homeDir = std::string(userInfo->pw_dir);
#endif
    LOG_F(INFO, "getHomeDirectory completed with result: {}", homeDir);
    return homeDir;
}

auto getCurrentWorkingDirectory() -> std::string {
    LOG_F(INFO, "getCurrentWorkingDirectory called");
#ifdef _WIN32
    // Windows-specific code
    std::array<char, MAX_PATH> cwd;
    if (GetCurrentDirectory(cwd.size(), cwd.data())) {
        std::string result = cwd.data();
        LOG_F(INFO, "getCurrentWorkingDirectory completed with result: {}",
              result);
        return result;
    }
    LOG_F(ERROR, "Error getting current working directory");
    return "Error getting current working directory";
#else
    // POSIX (Linux, macOS) specific code
    std::array<char, PATH_MAX> cwd;
    if (getcwd(cwd.data(), cwd.size()) != nullptr) {
        std::string result = cwd.data();
        LOG_F(INFO, "getCurrentWorkingDirectory completed with result: {}",
              result);
        return result;
    }
    LOG_F(ERROR, "Error getting current working directory");
    return "Error getting current working directory";
#endif
}

auto getLoginShell() -> std::string {
    LOG_F(INFO, "getLoginShell called");
    std::string loginShell;
#ifdef _WIN32
    std::array<char, MAX_PATH> buf;
    DWORD bufSize = buf.size();
    if (GetEnvironmentVariableA("COMSPEC", buf.data(), bufSize) != 0) {
        loginShell = std::string(buf.data());
    }
#else
    int userId = getUserId();
    struct passwd *userInfo = getpwuid(userId);
    loginShell = std::string(userInfo->pw_shell);
#endif
    LOG_F(INFO, "getLoginShell completed with result: {}", loginShell);
    return loginShell;
}

auto getLogin() -> std::string {
    LOG_F(INFO, "getLogin called");
#ifdef _WIN32
    std::array<char, UNLEN + 1> buffer;
    DWORD bufferSize = buffer.size();
    if (GetUserNameA(buffer.data(), &bufferSize) != 0) {
        std::string result = buffer.data();
        LOG_F(INFO, "getLogin completed with result: {}", result);
        return result;
    }
#else
    char *username = ::getlogin();
    if (username != nullptr) {
        std::string result = username;
        LOG_F(INFO, "getLogin completed with result: {}", result);
        return result;
    }
#endif
    LOG_F(ERROR, "Error getting login name");
    return "";
}

auto getEnvironmentVariable(const std::string &name) -> std::string {
    LOG_F(INFO, "getEnvironmentVariable called with name: {}", name);
    std::string value;

#ifdef _WIN32
    DWORD size = GetEnvironmentVariableA(name.c_str(), nullptr, 0);
    if (size > 0) {
        std::vector<char> buffer(size);
        GetEnvironmentVariableA(name.c_str(), buffer.data(), size);
        value = std::string(buffer.data(),
                            buffer.size() - 1);  // Remove null terminator
    }
#else
    const char *envValue = std::getenv(name.c_str());
    if (envValue != nullptr) {
        value = envValue;
    }
#endif

    LOG_F(INFO, "getEnvironmentVariable completed with result: {}", value);
    return value;
}

auto getAllEnvironmentVariables()
    -> std::unordered_map<std::string, std::string> {
    LOG_F(INFO, "getAllEnvironmentVariables called");
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

    LOG_F(INFO, "getAllEnvironmentVariables completed with {} variables",
          envVars.size());
    return envVars;
}

auto setEnvironmentVariable(const std::string &name, const std::string &value)
    -> bool {
    LOG_F(INFO, "setEnvironmentVariable called with name: {} and value: {}",
          name, value);
    bool success = false;

#ifdef _WIN32
    success = (SetEnvironmentVariableA(name.c_str(), value.c_str()) != 0);
#else
    success = (setenv(name.c_str(), value.c_str(), 1) == 0);
#endif

    LOG_F(INFO, "setEnvironmentVariable completed with result: {}", success);
    return success;
}

auto getSystemUptime() -> uint64_t {
    LOG_F(INFO, "getSystemUptime called");
    uint64_t uptime = 0;

#ifdef _WIN32
    uptime = GetTickCount64() / 1000;  // Convert milliseconds to seconds
#else
    struct sysinfo info;
    if (sysinfo(&info) == 0) {
        uptime = static_cast<uint64_t>(info.uptime);
    }
#endif

    LOG_F(INFO, "getSystemUptime completed with result: {} seconds", uptime);
    return uptime;
}

auto getLoggedInUsers() -> std::vector<std::string> {
    LOG_F(INFO, "getLoggedInUsers called");
    std::vector<std::string> users;

#ifdef _WIN32
    WTS_SESSION_INFO *sessionInfo = nullptr;
    DWORD sessionCount = 0;

    if (WTSEnumerateSessions(WTS_CURRENT_SERVER_HANDLE, 0, 1, &sessionInfo,
                             &sessionCount)) {
        for (DWORD i = 0; i < sessionCount; i++) {
            if (sessionInfo[i].State == WTSActive) {
                LPSTR buffer = nullptr;
                DWORD bytesReturned = 0;

                if (WTSQuerySessionInformationA(
                        WTS_CURRENT_SERVER_HANDLE, sessionInfo[i].SessionId,
                        WTSUserName, &buffer, &bytesReturned)) {
                    if (buffer && bytesReturned > 1) {
                        std::string username(buffer);
                        if (!username.empty() &&
                            std::find(users.begin(), users.end(), username) ==
                                users.end()) {
                            users.push_back(username);
                        }
                    }

                    WTSFreeMemory(buffer);
                }
            }
        }

        WTSFreeMemory(sessionInfo);
    }
#else
    setutent();
    struct utmp *entry;

    while ((entry = getutent()) != nullptr) {
        if (entry->ut_type == USER_PROCESS) {
            std::string username(entry->ut_user);
            if (!username.empty() && std::find(users.begin(), users.end(),
                                               username) == users.end()) {
                users.push_back(username);
            }
        }
    }

    endutent();
#endif

    LOG_F(INFO, "getLoggedInUsers completed with {} users found", users.size());
    return users;
}

auto userExists(const std::string &username) -> bool {
    LOG_F(INFO, "userExists called with username: {}", username);
    bool exists = false;

#ifdef _WIN32
    DWORD level = 1;
    USER_INFO_1 *userInfo = nullptr;
    std::wstring wUsername(username.begin(), username.end());
    NET_API_STATUS status =
        NetUserGetInfo(nullptr, wUsername.c_str(), level, (LPBYTE *)&userInfo);

    exists = (status == NERR_Success);

    if (userInfo != nullptr) {
        NetApiBufferFree(userInfo);
    }
#else
    struct passwd *pwd = getpwnam(username.c_str());
    exists = (pwd != nullptr);
#endif

    LOG_F(INFO, "userExists completed for username: {} with result: {}",
          username, exists);
    return exists;
}
}  // namespace atom::system
