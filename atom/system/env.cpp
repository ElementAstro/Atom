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

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <shared_mutex>
#include <sstream>
#include <string_view>

#ifdef _WIN32
#include <windows.h>
#endif

#include "atom/log/loguru.hpp"
#include "atom/utils/argsview.hpp"

namespace fs = std::filesystem;

namespace atom::utils {
class Env::Impl {
public:
    std::string mExe;      ///< Full path of the executable file.
    std::string mCwd;      ///< Working directory.
    std::string mProgram;  ///< Program name.

    std::unordered_map<std::string, std::string>
        mArgs;                         ///< List of command-line arguments.
    mutable std::shared_mutex mMutex;  ///< Shared mutex to protect member
                                       ///< variables (improved over mutex).
    ArgumentParser mParser;  ///< Argument parser for command-line arguments.
};

Env::Env() : Env(0, nullptr) { LOG_F(INFO, "Env default constructor called"); }

Env::Env(int argc, char** argv) : impl_(std::make_shared<Impl>()) {
    std::ostringstream oss;
    oss << "Env constructor called with argc: " << argc << ", argv: [";
    for (int i = 0; i < argc; ++i) {
        oss << "\"" << argv[i] << "\"";
        if (i < argc - 1) {
            oss << ", ";
        }
    }
    oss << "]";
    LOG_F(INFO, "{}", oss.str());
    fs::path exePath;

#ifdef _WIN32
    wchar_t buf[MAX_PATH];
    if (GetModuleFileNameW(nullptr, buf, MAX_PATH) == 0U) {
        LOG_F(ERROR, "GetModuleFileNameW failed with error {}", GetLastError());
        exePath = buf;
    } else {
        exePath = buf;
    }
#else
    char linkBuf[1024];
    ssize_t count = readlink("/proc/self/exe", linkBuf, sizeof(linkBuf));
    if (count != -1) {
        linkBuf[count] = '\0';
        exePath = linkBuf;
    }
#endif

    impl_->mExe = exePath.string();
    impl_->mCwd = exePath.parent_path().string() + '/';
    if (argv != nullptr) {
        impl_->mProgram = argv[0];
    } else {
        impl_->mProgram = "";
    }

    LOG_F(INFO, "Executable path: {}", impl_->mExe);
    LOG_F(INFO, "Current working directory: {}", impl_->mCwd);
    LOG_F(INFO, "Program name: {}", impl_->mProgram);

    if (argc > 1) {
        int i = 1;
        int j;
        for (j = 2; j < argc; ++j) {
            if (argv[i][0] == '-' && argv[j][0] == '-') {
                add(std::string(argv[i] + 1), "");
                i = j;
            } else if (argv[i][0] == '-' && argv[j][0] != '-') {
                add(std::string(argv[i] + 1), std::string(argv[j]));
                ++j;
                i = j;
            } else {
                return;
            }
        }

        if (i < argc) {
            if (argv[i][0] == '-') {
                add(std::string(argv[i] + 1), "");
            } else {
                return;
            }
        }
    }
    LOG_F(INFO, "Env constructor completed");
}

auto Env::createShared(int argc, char** argv) -> std::shared_ptr<Env> {
    return std::make_shared<Env>(argc, argv);
}

void Env::add(const std::string& key, const std::string& val) {
    LOG_F(INFO, "Env::add called with key: {}, val: {}", key, val);
    std::unique_lock lock(impl_->mMutex);
    if (has(key)) {
        LOG_F(ERROR, "Env::add: Duplicate key: {}", key);
    } else {
        DLOG_F(INFO, "Env::add: Add key: {} with value: {}", key, val);
        impl_->mArgs[key] = val;
    }
}

void Env::addMultiple(
    const std::unordered_map<std::string, std::string>& vars) {
    LOG_F(INFO, "Env::addMultiple called with {} variables", vars.size());
    std::unique_lock lock(impl_->mMutex);
    for (const auto& [key, val] : vars) {
        if (impl_->mArgs.find(key) == impl_->mArgs.end()) {
            DLOG_F(INFO, "Env::addMultiple: Add key: {} with value: {}", key,
                   val);
            impl_->mArgs[key] = val;
        } else {
            LOG_F(ERROR, "Env::addMultiple: Duplicate key: {}", key);
        }
    }
}

auto Env::has(const std::string& key) -> bool {
    LOG_F(INFO, "Env::has called with key: {}", key);
    std::shared_lock lock(impl_->mMutex);
    bool result = impl_->mArgs.contains(key);
    LOG_F(INFO, "Env::has returning: {}", result);
    return result;
}

auto Env::hasAll(const std::vector<std::string>& keys) -> bool {
    LOG_F(INFO, "Env::hasAll called with {} keys", keys.size());
    std::shared_lock lock(impl_->mMutex);
    for (const auto& key : keys) {
        if (!impl_->mArgs.contains(key)) {
            LOG_F(INFO, "Env::hasAll returning false, missing key: {}", key);
            return false;
        }
    }
    LOG_F(INFO, "Env::hasAll returning true");
    return true;
}

auto Env::hasAny(const std::vector<std::string>& keys) -> bool {
    LOG_F(INFO, "Env::hasAny called with {} keys", keys.size());
    std::shared_lock lock(impl_->mMutex);
    for (const auto& key : keys) {
        if (impl_->mArgs.contains(key)) {
            LOG_F(INFO, "Env::hasAny returning true, found key: {}", key);
            return true;
        }
    }
    LOG_F(INFO, "Env::hasAny returning false");
    return false;
}

void Env::del(const std::string& key) {
    LOG_F(INFO, "Env::del called with key: {}", key);
    std::unique_lock lock(impl_->mMutex);
    impl_->mArgs.erase(key);
    DLOG_F(INFO, "Env::del: Remove key: {}", key);
}

void Env::delMultiple(const std::vector<std::string>& keys) {
    LOG_F(INFO, "Env::delMultiple called with {} keys", keys.size());
    std::unique_lock lock(impl_->mMutex);
    for (const auto& key : keys) {
        impl_->mArgs.erase(key);
        DLOG_F(INFO, "Env::delMultiple: Remove key: {}", key);
    }
}

auto Env::get(const std::string& key,
              const std::string& default_value) -> std::string {
    LOG_F(INFO, "Env::get called with key: {}, default_value: {}", key,
          default_value);
    std::shared_lock lock(impl_->mMutex);
    auto it = impl_->mArgs.find(key);
    if (it == impl_->mArgs.end()) {
        DLOG_F(INFO, "Env::get: Key: {} not found, return default value: {}",
               key, default_value);
        return default_value;
    }
    std::string value = it->second;
    LOG_F(INFO, "Env::get returning: {}", value);
    return value;
}

auto Env::setEnv(const std::string& key, const std::string& val) -> bool {
    LOG_F(INFO, "Env::setEnv called with key: {}, val: {}", key, val);
    DLOG_F(INFO, "Env::setEnv: Set key: {} with value: {}", key, val);
#ifdef _WIN32
    bool result = SetEnvironmentVariableA(key.c_str(), val.c_str()) != 0;
#else
    bool result = setenv(key.c_str(), val.c_str(), 1) == 0;
#endif
    LOG_F(INFO, "Env::setEnv returning: {}", result);
    return result;
}

auto Env::setEnvMultiple(
    const std::unordered_map<std::string, std::string>& vars) -> bool {
    LOG_F(INFO, "Env::setEnvMultiple called with {} variables", vars.size());
    bool allSuccess = true;
    for (const auto& [key, val] : vars) {
        DLOG_F(INFO, "Env::setEnvMultiple: Setting key: {} with value: {}", key,
               val);
#ifdef _WIN32
        bool result = SetEnvironmentVariableA(key.c_str(), val.c_str()) != 0;
#else
        bool result = setenv(key.c_str(), val.c_str(), 1) == 0;
#endif
        if (!result) {
            LOG_F(ERROR, "Env::setEnvMultiple: Failed to set key: {}", key);
            allSuccess = false;
        }
    }
    LOG_F(INFO, "Env::setEnvMultiple returning: {}", allSuccess);
    return allSuccess;
}

auto Env::getEnv(const std::string& key,
                 const std::string& default_value) -> std::string {
    LOG_F(INFO, "Env::getEnv called with key: {}, default_value: {}", key,
          default_value);
    DLOG_F(INFO, "Env::getEnv: Get key: {} with default value: {}", key,
           default_value);
#ifdef _WIN32
    char buf[1024];
    DWORD ret = GetEnvironmentVariableA(key.c_str(), buf, sizeof(buf));
    if (ret == 0 || ret >= sizeof(buf)) {
        LOG_F(ERROR, "Env::getEnv: Get key: {} failed", key);
        return default_value;
    }
    DLOG_F(INFO, "Env::getEnv: Get key: {} with value: {}", key, buf);
    return buf;
#else
    const char* v = getenv(key.c_str());
    if (v == nullptr) {
        LOG_F(ERROR, "Env::getEnv: Get key: {} failed", key);
        return default_value;
    }
    DLOG_F(INFO, "Env::getEnv: Get key: {} with value: {}", key, v);
    return v;
#endif
}

auto Env::Environ() -> std::unordered_map<std::string, std::string> {
    LOG_F(INFO, "Env::Environ called");
    std::unordered_map<std::string, std::string> envMap;

#ifdef _WIN32
    LPCH envStrings = GetEnvironmentStrings();
    if (envStrings == nullptr) {
        LOG_F(ERROR, "Env::Environ: GetEnvironmentStrings failed");
        return envMap;
    }

    LPCH var = envStrings;
    while (*var != '\0') {
        std::string_view envVar(var);
        auto pos = envVar.find('=');
        if (pos != std::string_view::npos) {
            std::string key = std::string(envVar.substr(0, pos));
            std::string value = std::string(envVar.substr(pos + 1));
            envMap.emplace(key, value);
        }
        var += envVar.length() + 1;
    }

    FreeEnvironmentStrings(envStrings);

#elif __APPLE__ || __linux__ || __ANDROID__
    // Use POSIX API to get environment variables
    for (char** current = environ; *current; ++current) {
        std::string_view envVar(*current);
        auto pos = envVar.find('=');
        if (pos != std::string_view::npos) {
            std::string key = std::string(envVar.substr(0, pos));
            std::string value = std::string(envVar.substr(pos + 1));
            envMap.emplace(key, value);
        }
    }
#endif

    LOG_F(INFO, "Env::Environ returning environment map with {} entries",
          envMap.size());
    return envMap;
}

void Env::unsetEnv(const std::string& name) {
    LOG_F(INFO, "Env::unsetVariable called with name: {}", name);
#if defined(_WIN32) || defined(_WIN64)
    if (SetEnvironmentVariableA(name.c_str(), nullptr) == 0) {
        LOG_F(ERROR, "Failed to unset environment variable: {}", name);
    }
#else
    if (unsetenv(name.c_str()) != 0) {
        LOG_F(ERROR, "Failed to unset environment variable: {}", name);
    }
#endif
}

void Env::unsetEnvMultiple(const std::vector<std::string>& names) {
    LOG_F(INFO, "Env::unsetEnvMultiple called with {} names", names.size());
    for (const auto& name : names) {
        DLOG_F(INFO, "Env::unsetEnvMultiple: Unsetting variable: {}", name);
#if defined(_WIN32) || defined(_WIN64)
        if (SetEnvironmentVariableA(name.c_str(), nullptr) == 0) {
            LOG_F(ERROR, "Failed to unset environment variable: {}", name);
        }
#else
        if (unsetenv(name.c_str()) != 0) {
            LOG_F(ERROR, "Failed to unset environment variable: {}", name);
        }
#endif
    }
}

auto Env::listVariables() -> std::vector<std::string> {
    LOG_F(INFO, "Env::listVariables called");
    std::vector<std::string> vars;

#if defined(_WIN32) || defined(_WIN64)
    LPCH envStrings = GetEnvironmentStringsA();
    if (envStrings != nullptr) {
        for (LPCH var = envStrings; *var != '\0'; var += strlen(var) + 1) {
            vars.emplace_back(var);
        }
        FreeEnvironmentStringsA(envStrings);
    }
#else
    char** env = environ;
    while (*env != nullptr) {
        vars.emplace_back(*env);
        ++env;
    }
#endif

    LOG_F(INFO, "Env::listVariables returning {} variables", vars.size());
    return vars;
}

auto Env::filterVariables(
    const std::function<bool(const std::string&, const std::string&)>&
        predicate) -> std::unordered_map<std::string, std::string> {
    LOG_F(INFO, "Env::filterVariables called");
    std::unordered_map<std::string, std::string> filteredVars;
    auto allVars = Environ();

    for (const auto& [key, value] : allVars) {
        if (predicate(key, value)) {
            filteredVars.emplace(key, value);
            DLOG_F(INFO, "Env::filterVariables: Including variable: {} = {}",
                   key, value);
        }
    }

    LOG_F(INFO, "Env::filterVariables returning {} filtered variables",
          filteredVars.size());
    return filteredVars;
}

auto Env::getVariablesWithPrefix(const std::string& prefix)
    -> std::unordered_map<std::string, std::string> {
    LOG_F(INFO, "Env::getVariablesWithPrefix called with prefix: {}", prefix);
    return filterVariables(
        [&prefix](const std::string& key, const std::string&) {
            return key.rfind(prefix, 0) == 0;
        });
}

auto Env::saveToFile(const std::filesystem::path& filePath,
                     const std::unordered_map<std::string, std::string>& vars)
    -> bool {
    LOG_F(INFO, "Env::saveToFile called with filePath: {}", filePath.string());

    try {
        std::ofstream file(filePath, std::ios::out);
        if (!file.is_open()) {
            LOG_F(ERROR, "Env::saveToFile: Failed to open file: {}",
                  filePath.string());
            return false;
        }

        const auto& varsToSave = vars.empty() ? Environ() : vars;

        for (const auto& [key, value] : varsToSave) {
            file << key << "=" << value << std::endl;
            DLOG_F(INFO, "Env::saveToFile: Saving variable: {} = {}", key,
                   value);
        }

        file.close();
        LOG_F(INFO, "Env::saveToFile: Successfully saved {} variables to {}",
              varsToSave.size(), filePath.string());
        return true;
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Env::saveToFile: Exception: {}", e.what());
        return false;
    }
}

auto Env::loadFromFile(const std::filesystem::path& filePath,
                       bool overwrite) -> bool {
    LOG_F(INFO, "Env::loadFromFile called with filePath: {}, overwrite: {}",
          filePath.string(), overwrite);

    try {
        std::ifstream file(filePath);
        if (!file.is_open()) {
            LOG_F(ERROR, "Env::loadFromFile: Failed to open file: {}",
                  filePath.string());
            return false;
        }

        std::string line;
        std::unordered_map<std::string, std::string> loadedVars;

        while (std::getline(file, line)) {
            // Skip empty lines and comments
            if (line.empty() || line[0] == '#') {
                continue;
            }

            auto pos = line.find('=');
            if (pos != std::string::npos) {
                std::string key = line.substr(0, pos);
                std::string value = line.substr(pos + 1);

                loadedVars[key] = value;
                DLOG_F(INFO, "Env::loadFromFile: Loaded variable: {} = {}", key,
                       value);
            }
        }

        file.close();

        // 创建临时的Env对象或使用环境函数来设置环境变量
        for (const auto& [key, value] : loadedVars) {
            const char* currentValue = nullptr;
#ifdef _WIN32
            char buf[1024];
            if (GetEnvironmentVariableA(key.c_str(), buf, sizeof(buf)) > 0) {
                currentValue = buf;
            }
#else
            currentValue = getenv(key.c_str());
#endif
            bool exists = (currentValue != nullptr);

            if (overwrite || !exists) {
                bool success = false;
#ifdef _WIN32
                success =
                    SetEnvironmentVariableA(key.c_str(), value.c_str()) != 0;
#else
                success = setenv(key.c_str(), value.c_str(), 1) == 0;
#endif
                if (!success) {
                    LOG_F(WARNING,
                          "Env::loadFromFile: Failed to set variable: {}", key);
                }
            } else {
                DLOG_F(INFO,
                       "Env::loadFromFile: Skipping existing variable: {}",
                       key);
            }
        }

        LOG_F(INFO,
              "Env::loadFromFile: Successfully loaded {} variables from {}",
              loadedVars.size(), filePath.string());
        return true;
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Env::loadFromFile: Exception: {}", e.what());
        return false;
    }
}

auto Env::getExecutablePath() const -> std::string {
    std::shared_lock lock(impl_->mMutex);
    return impl_->mExe;
}

auto Env::getWorkingDirectory() const -> std::string {
    std::shared_lock lock(impl_->mMutex);
    return impl_->mCwd;
}

auto Env::getProgramName() const -> std::string {
    std::shared_lock lock(impl_->mMutex);
    return impl_->mProgram;
}

auto Env::getAllArgs() const -> std::unordered_map<std::string, std::string> {
    std::shared_lock lock(impl_->mMutex);
    return impl_->mArgs;
}

#if ATOM_ENABLE_DEBUG
void Env::printAllVariables() {
    LOG_F(INFO, "Env::printAllVariables called");
    std::vector<std::string> vars = listVariables();
    for (const auto& var : vars) {
        DLOG_F(INFO, "{}", var);
    }
}

void Env::printAllArgs() const {
    LOG_F(INFO, "Env::printAllArgs called");
    std::shared_lock lock(impl_->mMutex);
    for (const auto& [key, value] : impl_->mArgs) {
        DLOG_F(INFO, "Arg: {} = {}", key, value);
    }
}
#endif
}  // namespace atom::utils
