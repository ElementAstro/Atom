/*
 * env.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-12-16

Description: Environment variable management

**************************************************/

#include "env.hpp"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <shared_mutex>
#include <string_view>

#ifdef _WIN32
#include <shlobj.h>
#include <userenv.h>
#include <windows.h>
#pragma comment(lib, "userenv.lib")
#else
#include <limits.h>
#include <pwd.h>
#include <sys/types.h>
#include <unistd.h>
#if defined(__APPLE__)
#include <mach-o/dyld.h>
#endif
extern char** environ;
#endif

#include <spdlog/spdlog.h>

namespace fs = std::filesystem;

namespace atom::utils {

HashMap<size_t, Env::EnvChangeCallback> Env::sChangeCallbacks;
std::mutex Env::sCallbackMutex;
size_t Env::sNextCallbackId = 1;

void Env::notifyChangeCallbacks(const String& key, const String& oldValue,
                                const String& newValue) {
    spdlog::info(
        "Environment variable change notification: key={}, old_value={}, "
        "new_value={}",
        key, oldValue, newValue);
    std::lock_guard<std::mutex> lock(sCallbackMutex);
    for (const auto& [id, callback] : sChangeCallbacks) {
        try {
            callback(key, oldValue, newValue);
        } catch (const std::exception& e) {
            spdlog::error("Exception in environment change callback: {}",
                          e.what());
        }
    }
}

using atom::containers::String;
template <typename K, typename V>
using HashMap = atom::containers::HashMap<K, V>;
template <typename T>
using Vector = atom::containers::Vector<T>;

class Env::Impl {
public:
    String mExe;
    String mCwd;
    String mProgram;
    HashMap<String, String> mArgs;
    mutable std::shared_mutex mMutex;
};

Env::Env() : Env(0, nullptr) {
    spdlog::debug("Env default constructor called");
}

Env::Env(int argc, char** argv) : impl_(std::make_shared<Impl>()) {
    spdlog::debug("Env constructor called with argc={}", argc);

    fs::path exePath;

#ifdef _WIN32
    wchar_t buf[MAX_PATH];
    if (GetModuleFileNameW(nullptr, buf, MAX_PATH) == 0U) {
        spdlog::error("GetModuleFileNameW failed with error {}",
                      GetLastError());
    } else {
        exePath = buf;
    }
#else
    char linkBuf[1024];
    ssize_t count = readlink("/proc/self/exe", linkBuf, sizeof(linkBuf) - 1);
    if (count != -1) {
        linkBuf[count] = '\0';
        exePath = linkBuf;
    } else {
        spdlog::error("readlink /proc/self/exe failed");
        if (argc > 0 && argv != nullptr && argv[0] != nullptr) {
            exePath = fs::absolute(argv[0]);
        }
    }
#endif

    impl_->mExe = String(exePath.string());
    impl_->mCwd = String(exePath.parent_path().string()) + '/';

    if (argc > 0 && argv != nullptr && argv[0] != nullptr) {
        impl_->mProgram = String(argv[0]);
    } else {
        impl_->mProgram = "";
    }

    spdlog::debug("Executable path: {}", impl_->mExe);
    spdlog::debug("Current working directory: {}", impl_->mCwd);
    spdlog::debug("Program name: {}", impl_->mProgram);

    if (argc > 1 && argv != nullptr) {
        int i = 1;
        while (i < argc) {
            if (argv[i][0] == '-') {
                String key(argv[i] + 1);
                if (i + 1 < argc && argv[i + 1][0] != '-') {
                    String value(argv[i + 1]);
                    add(key, value);
                    i += 2;
                } else {
                    add(key, "");
                    i += 1;
                }
            } else {
                spdlog::warn("Ignoring positional argument: {}", argv[i]);
                i += 1;
            }
        }
    }
    spdlog::debug("Env constructor completed");
}

auto Env::createShared(int argc, char** argv) -> std::shared_ptr<Env> {
    return std::make_shared<Env>(argc, argv);
}

void Env::add(const String& key, const String& val) {
    spdlog::debug("Adding environment variable: {}={}", key, val);
    std::unique_lock lock(impl_->mMutex);
    if (impl_->mArgs.contains(key)) {
        spdlog::warn("Duplicate key found: {}", key);
    } else {
        impl_->mArgs[key] = val;
    }
}

void Env::addMultiple(const HashMap<String, String>& vars) {
    spdlog::debug("Adding {} environment variables", vars.size());
    std::unique_lock lock(impl_->mMutex);
    for (const auto& [key, val] : vars) {
        if (!impl_->mArgs.contains(key)) {
            impl_->mArgs[key] = val;
        } else {
            spdlog::warn("Duplicate key found: {}", key);
        }
    }
}

bool Env::has(const String& key) {
    std::shared_lock lock(impl_->mMutex);
    bool result = impl_->mArgs.contains(key);
    spdlog::debug("Checking key existence: {}={}", key, result);
    return result;
}

bool Env::hasAll(const Vector<String>& keys) {
    std::shared_lock lock(impl_->mMutex);
    for (const auto& key : keys) {
        if (!impl_->mArgs.contains(key)) {
            spdlog::debug("Missing key in hasAll check: {}", key);
            return false;
        }
    }
    return true;
}

bool Env::hasAny(const Vector<String>& keys) {
    std::shared_lock lock(impl_->mMutex);
    for (const auto& key : keys) {
        if (impl_->mArgs.contains(key)) {
            spdlog::debug("Found key in hasAny check: {}", key);
            return true;
        }
    }
    return false;
}

void Env::del(const String& key) {
    spdlog::debug("Deleting environment variable: {}", key);
    std::unique_lock lock(impl_->mMutex);
    impl_->mArgs.erase(key);
}

void Env::delMultiple(const Vector<String>& keys) {
    spdlog::debug("Deleting {} environment variables", keys.size());
    std::unique_lock lock(impl_->mMutex);
    for (const auto& key : keys) {
        impl_->mArgs.erase(key);
    }
}

auto Env::get(const String& key, const String& default_value) -> String {
    std::shared_lock lock(impl_->mMutex);
    auto it = impl_->mArgs.find(key);
    if (it == impl_->mArgs.end()) {
        spdlog::debug("Key not found, returning default: {}={}", key,
                      default_value);
        return default_value;
    }
    String value = it->second;
    spdlog::debug("Retrieved value: {}={}", key, value);
    return value;
}

auto Env::setEnv(const String& key, const String& val) -> bool {
    spdlog::debug("Setting environment variable: {}={}", key, val);

    String oldValue = getEnv(key, "");

#ifdef _WIN32
    bool result = SetEnvironmentVariableA(key.c_str(), val.c_str()) != 0;
#else
    bool result = ::setenv(key.c_str(), val.c_str(), 1) == 0;
#endif

    if (result) {
        notifyChangeCallbacks(key, oldValue, val);
        spdlog::debug("Successfully set environment variable: {}", key);
    } else {
        spdlog::error("Failed to set environment variable: {}", key);
    }

    return result;
}

auto Env::setEnvMultiple(const HashMap<String, String>& vars) -> bool {
    spdlog::debug("Setting {} environment variables", vars.size());
    bool allSuccess = true;
    for (const auto& [key, val] : vars) {
#ifdef _WIN32
        bool result = SetEnvironmentVariableA(key.c_str(), val.c_str()) != 0;
#else
        bool result = ::setenv(key.c_str(), val.c_str(), 1) == 0;
#endif
        if (!result) {
            spdlog::error("Failed to set environment variable: {}", key);
            allSuccess = false;
        }
    }
    return allSuccess;
}

auto Env::getEnv(const String& key, const String& default_value) -> String {
#ifdef _WIN32
    DWORD needed = GetEnvironmentVariableA(key.c_str(), nullptr, 0);
    if (needed == 0) {
        if (GetLastError() == ERROR_ENVVAR_NOT_FOUND) {
            spdlog::debug("Environment variable not found: {}", key);
        } else {
            spdlog::error(
                "GetEnvironmentVariableA failed for key {} with error {}", key,
                GetLastError());
        }
        return default_value;
    }
    std::vector<char> buf(needed);
    DWORD ret = GetEnvironmentVariableA(key.c_str(), buf.data(), needed);
    if (ret == 0 || ret >= needed) {
        spdlog::error(
            "GetEnvironmentVariableA failed on second call for key {}", key);
        return default_value;
    }
    String value(buf.data(), ret);
    spdlog::debug("Retrieved environment variable: {}={}", key, value);
    return value;
#else
    const char* v = ::getenv(key.c_str());
    if (v == nullptr) {
        spdlog::debug("Environment variable not found: {}", key);
        return default_value;
    }
    String value(v);
    spdlog::debug("Retrieved environment variable: {}={}", key, value);
    return value;
#endif
}

auto Env::Environ() -> HashMap<String, String> {
    spdlog::debug("Getting all environment variables");
    HashMap<String, String> envMap;

#ifdef _WIN32
    LPCH envStrings = GetEnvironmentStringsA();
    if (envStrings == nullptr) {
        spdlog::error("GetEnvironmentStringsA failed");
        return envMap;
    }

    LPCH var = envStrings;
    while (*var != '\0') {
        std::string_view envVar(var);
        auto pos = envVar.find('=');
        if (pos != std::string_view::npos && pos > 0) {
            String key(envVar.substr(0, pos).data(), pos);
            String value(envVar.substr(pos + 1).data(),
                         envVar.length() - (pos + 1));
            envMap.emplace(key, value);
        }
        var += envVar.length() + 1;
    }

    FreeEnvironmentStringsA(envStrings);
#else
    if (environ != nullptr) {
        for (char** current = environ; *current; ++current) {
            std::string_view envVar(*current);
            auto pos = envVar.find('=');
            if (pos != std::string_view::npos) {
                String key(envVar.substr(0, pos).data(), pos);
                String value(envVar.substr(pos + 1).data(),
                             envVar.length() - (pos + 1));
                envMap.emplace(key, value);
            }
        }
    } else {
        spdlog::warn("POSIX environ is NULL");
    }
#endif

    spdlog::debug("Retrieved {} environment variables", envMap.size());
    return envMap;
}

void Env::unsetEnv(const String& name) {
    spdlog::debug("Unsetting environment variable: {}", name);
#ifdef _WIN32
    if (SetEnvironmentVariableA(name.c_str(), nullptr) == 0) {
        if (GetLastError() != ERROR_ENVVAR_NOT_FOUND) {
            spdlog::error("Failed to unset environment variable: {}, Error: {}",
                          name, GetLastError());
        }
    }
#else
    if (::unsetenv(name.c_str()) != 0) {
        spdlog::error("Failed to unset environment variable: {}, errno: {}",
                      name, errno);
    }
#endif
}

void Env::unsetEnvMultiple(const Vector<String>& names) {
    spdlog::debug("Unsetting {} environment variables", names.size());
    for (const auto& name : names) {
        unsetEnv(name);
    }
}

auto Env::listVariables() -> Vector<String> {
    spdlog::debug("Listing all environment variables");
    Vector<String> vars;

#ifdef _WIN32
    LPCH envStrings = GetEnvironmentStringsA();
    if (envStrings != nullptr) {
        for (LPCH var = envStrings; *var != '\0'; var += strlen(var) + 1) {
            vars.emplace_back(var);
        }
        FreeEnvironmentStringsA(envStrings);
    }
#else
    if (environ != nullptr) {
        for (char** current = environ; *current; ++current) {
            vars.emplace_back(*current);
        }
    }
#endif

    spdlog::debug("Found {} environment variables", vars.size());
    return vars;
}

auto Env::filterVariables(
    const std::function<bool(const String&, const String&)>& predicate)
    -> HashMap<String, String> {
    spdlog::debug("Filtering environment variables");
    HashMap<String, String> filteredVars;
    auto allVars = Environ();

    for (const auto& [key, value] : allVars) {
        if (predicate(key, value)) {
            filteredVars.emplace(key, value);
        }
    }

    spdlog::debug("Filtered {} variables from {} total", filteredVars.size(),
                  allVars.size());
    return filteredVars;
}

auto Env::getVariablesWithPrefix(const String& prefix)
    -> HashMap<String, String> {
    spdlog::debug("Getting variables with prefix: {}", prefix);
    return filterVariables(
        [&prefix](const String& key, const String& /*value*/) {
            return key.rfind(prefix, 0) == 0;
        });
}

auto Env::saveToFile(const std::filesystem::path& filePath,
                     const HashMap<String, String>& vars) -> bool {
    spdlog::debug("Saving environment variables to file: {}",
                  filePath.string());

    try {
        std::ofstream file(filePath, std::ios::out | std::ios::binary);
        if (!file.is_open()) {
            spdlog::error("Failed to open file for writing: {}",
                          filePath.string());
            return false;
        }

        const auto& varsToSave = vars.empty() ? Environ() : vars;

        for (const auto& [key, value] : varsToSave) {
            file.write(key.data(), key.length());
            file.put('=');
            file.write(value.data(), value.length());
            file.put('\n');
        }

        file.close();
        spdlog::info("Successfully saved {} variables to {}", varsToSave.size(),
                     filePath.string());
        return true;
    } catch (const std::exception& e) {
        spdlog::error("Exception while saving to file: {}", e.what());
        return false;
    }
}

auto Env::loadFromFile(const std::filesystem::path& filePath, bool overwrite)
    -> bool {
    spdlog::debug("Loading environment variables from file: {}, overwrite: {}",
                  filePath.string(), overwrite);

    try {
        std::ifstream file(filePath, std::ios::binary);
        if (!file.is_open()) {
            spdlog::error("Failed to open file for reading: {}",
                          filePath.string());
            return false;
        }

        std::string line_std;
        HashMap<String, String> loadedVars;

        while (std::getline(file, line_std)) {
            if (line_std.empty() || line_std[0] == '#') {
                continue;
            }

            auto pos = line_std.find('=');
            if (pos != std::string::npos) {
                String key(line_std.substr(0, pos));
                String value(line_std.substr(pos + 1));
                loadedVars[key] = value;
            }
        }

        file.close();

        for (const auto& [key, value] : loadedVars) {
            String currentValueStr = getEnv(key, "");
            bool exists = !currentValueStr.empty();

            if (overwrite || !exists) {
                if (!setEnv(key, value)) {
                    spdlog::warn("Failed to set variable: {}", key);
                }
            }
        }

        spdlog::info("Successfully processed {} variables from {}",
                     loadedVars.size(), filePath.string());
        return true;
    } catch (const std::exception& e) {
        spdlog::error("Exception while loading from file: {}", e.what());
        return false;
    }
}

auto Env::getExecutablePath() const -> String {
    std::shared_lock lock(impl_->mMutex);
    return impl_->mExe;
}

auto Env::getWorkingDirectory() const -> String {
    std::shared_lock lock(impl_->mMutex);
    return impl_->mCwd;
}

auto Env::getProgramName() const -> String {
    std::shared_lock lock(impl_->mMutex);
    return impl_->mProgram;
}

auto Env::getAllArgs() const -> HashMap<String, String> {
    std::shared_lock lock(impl_->mMutex);
    return impl_->mArgs;
}

#if ATOM_ENABLE_DEBUG
void Env::printAllVariables() {
    spdlog::debug("Printing all environment variables");
    Vector<String> vars = listVariables();
    for (const auto& var : vars) {
        spdlog::debug("Environment variable: {}", var);
    }
}

void Env::printAllArgs() const {
    spdlog::debug("Printing all command-line arguments");
    std::shared_lock lock(impl_->mMutex);
    for (const auto& [key, value] : impl_->mArgs) {
        spdlog::debug("Argument: {}={}", key, value);
    }
}
#endif

Env::ScopedEnv::ScopedEnv(const String& key, const String& value)
    : mKey(key), mHadValue(false) {
    spdlog::debug("Creating scoped environment variable: {}={}", key, value);
    mOriginalValue = getEnv(key, "");
    mHadValue = !mOriginalValue.empty();
    setEnv(key, value);
}

Env::ScopedEnv::~ScopedEnv() {
    spdlog::debug("Destroying scoped environment variable: {}", mKey);
    if (mHadValue) {
        setEnv(mKey, mOriginalValue);
    } else {
        unsetEnv(mKey);
    }
}

auto Env::createScopedEnv(const String& key, const String& value)
    -> std::shared_ptr<ScopedEnv> {
    return std::make_shared<ScopedEnv>(key, value);
}

auto Env::registerChangeNotification(EnvChangeCallback callback) -> size_t {
    std::lock_guard<std::mutex> lock(sCallbackMutex);
    size_t id = sNextCallbackId++;
    sChangeCallbacks[id] = callback;
    spdlog::debug("Registered environment change notification with id: {}", id);
    return id;
}

auto Env::unregisterChangeNotification(size_t id) -> bool {
    std::lock_guard<std::mutex> lock(sCallbackMutex);
    bool result = sChangeCallbacks.erase(id) > 0;
    spdlog::debug(
        "Unregistered environment change notification id: {}, success: {}", id,
        result);
    return result;
}

auto Env::getHomeDir() -> String {
    spdlog::debug("Getting home directory");
    String homePath;

#ifdef _WIN32
    homePath = getEnv("USERPROFILE", "");
    if (homePath.empty()) {
        String homeDrive = getEnv("HOMEDRIVE", "");
        String homePath2 = getEnv("HOMEPATH", "");
        if (!homeDrive.empty() && !homePath2.empty()) {
            homePath = homeDrive + homePath2;
        }
    }
#else
    homePath = getEnv("HOME", "");
    if (homePath.empty()) {
        struct passwd* pw = getpwuid(getuid());
        if (pw && pw->pw_dir) {
            homePath = pw->pw_dir;
        }
    }
#endif

    spdlog::debug("Home directory: {}", homePath);
    return homePath;
}

auto Env::getTempDir() -> String {
    spdlog::debug("Getting temporary directory");
    String tempPath;

#ifdef _WIN32
    DWORD bufferLength = MAX_PATH + 1;
    std::vector<char> buffer(bufferLength);
    DWORD length = GetTempPathA(bufferLength, buffer.data());
    if (length > 0 && length <= bufferLength) {
        tempPath = String(buffer.data(), length);
    } else {
        tempPath = getEnv("TEMP", "");
        if (tempPath.empty()) {
            tempPath = getEnv("TMP", "C:\\Temp");
        }
    }
#else
    tempPath = getEnv("TMPDIR", "");
    if (tempPath.empty()) {
        tempPath = "/tmp";
    }
#endif

    spdlog::debug("Temporary directory: {}", tempPath);
    return tempPath;
}

auto Env::getConfigDir() -> String {
    spdlog::debug("Getting configuration directory");
    String configPath;

#ifdef _WIN32
    configPath = getEnv("APPDATA", "");
    if (configPath.empty()) {
        configPath = getEnv("LOCALAPPDATA", "");
    }
#else
    configPath = getEnv("XDG_CONFIG_HOME", "");
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

auto Env::getDataDir() -> String {
    spdlog::debug("Getting data directory");
    String dataPath;

#ifdef _WIN32
    dataPath = getEnv("LOCALAPPDATA", "");
    if (dataPath.empty()) {
        dataPath = getEnv("APPDATA", "");
    }
#else
    dataPath = getEnv("XDG_DATA_HOME", "");
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

auto Env::expandVariables(const String& str, VariableFormat format) -> String {
    spdlog::debug("Expanding variables in string with format: {}",
                  static_cast<int>(format));

    if (str.empty()) {
        return str;
    }

    if (format == VariableFormat::AUTO) {
#ifdef _WIN32
        format = VariableFormat::WINDOWS;
#else
        format = VariableFormat::UNIX;
#endif
    }

    String result;
    result.reserve(str.length() * 2);

    if (format == VariableFormat::UNIX) {
        size_t pos = 0;
        while (pos < str.length()) {
            if (str[pos] == '$' && pos + 1 < str.length()) {
                if (str[pos + 1] == '{') {
                    size_t closePos = str.find('}', pos + 2);
                    if (closePos != String::npos) {
                        String varName =
                            str.substr(pos + 2, closePos - pos - 2);
                        String varValue = getEnv(varName, "");
                        result += varValue;
                        pos = closePos + 1;
                        continue;
                    }
                } else if (isalpha(str[pos + 1]) || str[pos + 1] == '_') {
                    size_t endPos = pos + 1;
                    while (endPos < str.length() &&
                           (isalnum(str[endPos]) || str[endPos] == '_')) {
                        endPos++;
                    }
                    String varName = str.substr(pos + 1, endPos - pos - 1);
                    String varValue = getEnv(varName, "");
                    result += varValue;
                    pos = endPos;
                    continue;
                }
            }
            result += str[pos++];
        }
    } else {
        size_t pos = 0;
        while (pos < str.length()) {
            if (str[pos] == '%') {
                size_t endPos = str.find('%', pos + 1);
                if (endPos != String::npos) {
                    String varName = str.substr(pos + 1, endPos - pos - 1);
                    String varValue = getEnv(varName, "");
                    result += varValue;
                    pos = endPos + 1;
                    continue;
                }
            }
            result += str[pos++];
        }
    }

    return result;
}

auto Env::setPersistentEnv(const String& key, const String& val,
                           PersistLevel level) -> bool {
    spdlog::debug("Setting persistent environment variable: {}={}, level: {}",
                  key, val, static_cast<int>(level));

    if (level == PersistLevel::PROCESS) {
        return setEnv(key, val);
    }

#ifdef _WIN32
    HKEY hKey;
    DWORD dwDisposition;

    const char* subKey = (level == PersistLevel::USER)
                             ? "Environment"
                             : "SYSTEM\\CurrentControlSet\\Control\\Session "
                               "Manager\\Environment";
    REGSAM sam = KEY_WRITE;
    HKEY rootKey =
        (level == PersistLevel::USER) ? HKEY_CURRENT_USER : HKEY_LOCAL_MACHINE;

    if (level == PersistLevel::SYSTEM && !IsUserAnAdmin()) {
        spdlog::error(
            "Setting SYSTEM level environment requires admin privileges");
        return false;
    }

    if (RegCreateKeyExA(rootKey, subKey, 0, NULL, 0, sam, NULL, &hKey,
                        &dwDisposition) != ERROR_SUCCESS) {
        spdlog::error("Failed to open registry key");
        return false;
    }

    LONG result = RegSetValueExA(hKey, key.c_str(), 0, REG_SZ,
                                 (LPBYTE)val.c_str(), val.length() + 1);
    RegCloseKey(hKey);

    if (result != ERROR_SUCCESS) {
        spdlog::error("Failed to set registry value");
        return false;
    }

    SendMessageTimeoutA(HWND_BROADCAST, WM_SETTINGCHANGE, 0,
                        (LPARAM) "Environment", SMTO_ABORTIFHUNG, 5000, NULL);
    setEnv(key, val);
    return true;
#else
    String homeDir = getHomeDir();
    if (homeDir.empty()) {
        spdlog::error("Failed to get home directory");
        return false;
    }

    std::string filePath;
    if (level == PersistLevel::USER) {
        if (std::filesystem::exists(homeDir + "/.bash_profile")) {
            filePath = homeDir + "/.bash_profile";
        } else if (std::filesystem::exists(homeDir + "/.profile")) {
            filePath = homeDir + "/.profile";
        } else {
            filePath = homeDir + "/.bashrc";
        }
    } else {
        filePath = "/etc/environment";
        if (access(filePath.c_str(), W_OK) != 0) {
            spdlog::error("No write permission for system environment file");
            return false;
        }
    }

    std::vector<std::string> lines;
    std::ifstream inFile(filePath);
    if (inFile.is_open()) {
        std::string line;
        while (std::getline(inFile, line)) {
            if (line.empty() || line[0] == '#') {
                lines.push_back(line);
                continue;
            }

            std::string pattern = key.c_str();
            pattern += "=";
            if (line.find(pattern) == 0) {
                continue;
            }

            lines.push_back(line);
        }
        inFile.close();
    }

    std::string newLine = key.c_str();
    newLine += "=";
    newLine += val.c_str();
    lines.push_back(newLine);

    std::ofstream outFile(filePath);
    if (!outFile.is_open()) {
        spdlog::error("Failed to open file for writing: {}", filePath);
        return false;
    }

    for (const auto& line : lines) {
        outFile << line << std::endl;
    }
    outFile.close();

    setEnv(key, val);
    spdlog::info("Successfully set persistent environment variable in {}",
                 filePath);
    return true;
#endif
}

auto Env::deletePersistentEnv(const String& key, PersistLevel level) -> bool {
    spdlog::debug("Deleting persistent environment variable: {}, level: {}",
                  key, static_cast<int>(level));

    if (level == PersistLevel::PROCESS) {
        unsetEnv(key);
        return true;
    }

#ifdef _WIN32
    HKEY hKey;
    const char* subKey = (level == PersistLevel::USER)
                             ? "Environment"
                             : "SYSTEM\\CurrentControlSet\\Control\\Session "
                               "Manager\\Environment";
    REGSAM sam = KEY_WRITE;
    HKEY rootKey =
        (level == PersistLevel::USER) ? HKEY_CURRENT_USER : HKEY_LOCAL_MACHINE;

    if (level == PersistLevel::SYSTEM && !IsUserAnAdmin()) {
        spdlog::error(
            "Deleting SYSTEM level environment requires admin privileges");
        return false;
    }

    if (RegOpenKeyExA(rootKey, subKey, 0, sam, &hKey) != ERROR_SUCCESS) {
        spdlog::error("Failed to open registry key");
        return false;
    }

    LONG result = RegDeleteValueA(hKey, key.c_str());
    RegCloseKey(hKey);

    if (result != ERROR_SUCCESS && result != ERROR_FILE_NOT_FOUND) {
        spdlog::error("Failed to delete registry value");
        return false;
    }

    SendMessageTimeoutA(HWND_BROADCAST, WM_SETTINGCHANGE, 0,
                        (LPARAM) "Environment", SMTO_ABORTIFHUNG, 5000, NULL);
    unsetEnv(key);
    return true;
#else
    String homeDir = getHomeDir();
    if (homeDir.empty()) {
        spdlog::error("Failed to get home directory");
        return false;
    }

    std::string filePath;
    if (level == PersistLevel::USER) {
        if (std::filesystem::exists(homeDir + "/.bash_profile")) {
            filePath = homeDir + "/.bash_profile";
        } else if (std::filesystem::exists(homeDir + "/.profile")) {
            filePath = homeDir + "/.profile";
        } else {
            filePath = homeDir + "/.bashrc";
        }
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
            std::string pattern = key.c_str();
            pattern += "=";
            if (line.find(pattern) == 0) {
                found = true;
                continue;
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

    unsetEnv(key);
    spdlog::info("Successfully deleted persistent environment variable from {}",
                 filePath);
    return true;
#endif
}

auto Env::getPathSeparator() -> char {
#ifdef _WIN32
    return ';';
#else
    return ':';
#endif
}

auto Env::splitPathString(const String& pathStr) -> Vector<String> {
    Vector<String> result;
    if (pathStr.empty()) {
        return result;
    }

    char separator = getPathSeparator();
    size_t start = 0;
    size_t end = pathStr.find(separator);

    while (end != String::npos) {
        String path = pathStr.substr(start, end - start);
        if (!path.empty()) {
            while (!path.empty() && std::isspace(path.front())) {
                path.erase(0, 1);
            }
            while (!path.empty() && std::isspace(path.back())) {
                path.pop_back();
            }

            if (!path.empty()) {
                result.push_back(path);
            }
        }
        start = end + 1;
        end = pathStr.find(separator, start);
    }

    if (start < pathStr.length()) {
        String path = pathStr.substr(start);
        while (!path.empty() && std::isspace(path.front())) {
            path.erase(0, 1);
        }
        while (!path.empty() && std::isspace(path.back())) {
            path.pop_back();
        }

        if (!path.empty()) {
            result.push_back(path);
        }
    }

    return result;
}

auto Env::joinPathString(const Vector<String>& paths) -> String {
    if (paths.empty()) {
        return "";
    }

    char separator = getPathSeparator();
    String result;

    for (size_t i = 0; i < paths.size(); ++i) {
        result += paths[i];
        if (i < paths.size() - 1) {
            result += separator;
        }
    }

    return result;
}

auto Env::getPathEntries() -> Vector<String> {
#ifdef _WIN32
    String pathVar = getEnv("Path", "");
#else
    String pathVar = getEnv("PATH", "");
#endif

    return splitPathString(pathVar);
}

auto Env::isInPath(const String& path) -> bool {
    Vector<String> paths = getPathEntries();

    std::filesystem::path normalizedPath;
    try {
        normalizedPath =
            std::filesystem::absolute(path.c_str()).lexically_normal();
    } catch (const std::exception& e) {
        spdlog::error("Failed to normalize path: {}", e.what());
        return false;
    }

    for (const auto& entry : paths) {
        try {
            std::filesystem::path entryPath =
                std::filesystem::absolute(entry.c_str()).lexically_normal();
            if (entryPath == normalizedPath) {
                return true;
            }
        } catch (const std::exception& e) {
            continue;
        }
    }

    for (const auto& entry : paths) {
        String lowerEntry = entry;
        String lowerPath = path;

        std::transform(lowerEntry.begin(), lowerEntry.end(), lowerEntry.begin(),
                       ::tolower);
        std::transform(lowerPath.begin(), lowerPath.end(), lowerPath.begin(),
                       ::tolower);

        if (lowerEntry == lowerPath) {
            return true;
        }
    }

    return false;
}

auto Env::addToPath(const String& path, bool prepend) -> bool {
    spdlog::debug("Adding path to PATH: {}, prepend: {}", path, prepend);

    if (isInPath(path)) {
        spdlog::debug("Path already exists in PATH");
        return true;
    }

#ifdef _WIN32
    String pathVarName = "Path";
#else
    String pathVarName = "PATH";
#endif

    String currentPath = getEnv(pathVarName, "");
    String newPath;

    if (currentPath.empty()) {
        newPath = path;
    } else {
        if (prepend) {
            newPath = path + getPathSeparator() + currentPath;
        } else {
            newPath = currentPath + getPathSeparator() + path;
        }
    }

    bool result = setEnv(pathVarName, newPath);
    if (result) {
        spdlog::info("Successfully added path to PATH: {}", path);
    } else {
        spdlog::error("Failed to update PATH");
    }

    return result;
}

auto Env::removeFromPath(const String& path) -> bool {
    spdlog::debug("Removing path from PATH: {}", path);

    if (!isInPath(path)) {
        spdlog::debug("Path does not exist in PATH");
        return true;
    }

#ifdef _WIN32
    String pathVarName = "Path";
#else
    String pathVarName = "PATH";
#endif

    Vector<String> paths = getPathEntries();
    Vector<String> newPaths;

    std::filesystem::path normalizedPath;
    try {
        normalizedPath =
            std::filesystem::absolute(path.c_str()).lexically_normal();
    } catch (const std::exception& e) {
        spdlog::error("Failed to normalize path: {}", e.what());
        return false;
    }

    for (const auto& entry : paths) {
        try {
            std::filesystem::path entryPath =
                std::filesystem::absolute(entry.c_str()).lexically_normal();
            if (entryPath != normalizedPath) {
                newPaths.push_back(entry);
            }
        } catch (const std::exception& e) {
            String lowerEntry = entry;
            String lowerPath = path;

            std::transform(lowerEntry.begin(), lowerEntry.end(),
                           lowerEntry.begin(), ::tolower);
            std::transform(lowerPath.begin(), lowerPath.end(),
                           lowerPath.begin(), ::tolower);

            if (lowerEntry != lowerPath) {
                newPaths.push_back(entry);
            }
        }
    }

    String newPath = joinPathString(newPaths);
    bool result = setEnv(pathVarName, newPath);

    if (result) {
        spdlog::info("Successfully removed path from PATH: {}", path);
    } else {
        spdlog::error("Failed to update PATH");
    }

    return result;
}

auto Env::diffEnvironments(const HashMap<String, String>& env1,
                           const HashMap<String, String>& env2)
    -> std::tuple<HashMap<String, String>, HashMap<String, String>,
                  HashMap<String, String>> {
    HashMap<String, String> added;
    HashMap<String, String> removed;
    HashMap<String, String> modified;

    for (const auto& [key, val2] : env2) {
        auto it = env1.find(key);
        if (it == env1.end()) {
            added[key] = val2;
        } else if (it->second != val2) {
            modified[key] = val2;
        }
    }

    for (const auto& [key, val1] : env1) {
        if (env2.find(key) == env2.end()) {
            removed[key] = val1;
        }
    }

    spdlog::debug("Environment diff: {} added, {} removed, {} modified",
                  added.size(), removed.size(), modified.size());
    return std::make_tuple(added, removed, modified);
}

auto Env::mergeEnvironments(const HashMap<String, String>& baseEnv,
                            const HashMap<String, String>& overlayEnv,
                            bool override) -> HashMap<String, String> {
    HashMap<String, String> result = baseEnv;

    for (const auto& [key, val] : overlayEnv) {
        auto it = result.find(key);
        if (it == result.end() || override) {
            result[key] = val;
        }
    }

    spdlog::debug("Merged environments: {} total variables", result.size());
    return result;
}

auto Env::getSystemName() -> String {
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

auto Env::getSystemArch() -> String {
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

auto Env::getCurrentUser() -> String {
    String username;

#ifdef _WIN32
    DWORD size = 256;
    char buffer[256];
    if (GetUserNameA(buffer, &size)) {
        username = String(buffer);
    } else {
        spdlog::error("getCurrentUser: GetUserNameA failed with error {}",
                      GetLastError());
        username = getEnv("USERNAME", "unknown");
    }
#else
    username = getEnv("USER", "");
    if (username.empty()) {
        username = getEnv("LOGNAME", "");
    }

    if (username.empty()) {
        // 尝试从passwd获取
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

auto Env::getHostName() -> String {
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

        hostname = getEnv("COMPUTERNAME", "unknown");
    }
#else
    char buffer[HOST_NAME_MAX + 1];
    if (gethostname(buffer, sizeof(buffer)) == 0) {
        hostname = buffer;
    } else {
        spdlog::error("getHostName: gethostname failed with error {}", errno);
        hostname = getEnv("HOSTNAME", "unknown");
    }
#endif

    spdlog::info("getHostName returning: {}", hostname);
    return hostname;
}

}  // namespace atom::utils
