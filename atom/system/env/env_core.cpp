/*
 * env_core.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-12-16

Description: Core environment variable management implementation

**************************************************/

#include "env_core.hpp"

#include <cstdlib>
#include <filesystem>
#include <shared_mutex>

#ifdef _WIN32
#include <windows.h>
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

HashMap<size_t, EnvChangeCallback> EnvCore::sChangeCallbacks;
std::mutex EnvCore::sCallbackMutex;
size_t EnvCore::sNextCallbackId = 1;

void EnvCore::notifyChangeCallbacks(const String& key, const String& oldValue,
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

class EnvCore::Impl {
public:
    String mExe;
    String mCwd;
    String mProgram;
    HashMap<String, String> mArgs;
    mutable std::shared_mutex mMutex;
};

EnvCore::EnvCore() : EnvCore(0, nullptr) {
    spdlog::debug("EnvCore default constructor called");
}

EnvCore::EnvCore(int argc, char** argv) : impl_(std::make_shared<Impl>()) {
    spdlog::debug("EnvCore constructor called with argc={}", argc);

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
    char buf[PATH_MAX];
#if defined(__linux__)
    ssize_t len = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
    if (len != -1) {
        buf[len] = '\0';
        exePath = buf;
    } else {
        spdlog::error("Failed to read /proc/self/exe");
    }
#elif defined(__APPLE__)
    uint32_t size = sizeof(buf);
    if (_NSGetExecutablePath(buf, &size) == 0) {
        exePath = buf;
    } else {
        spdlog::error("_NSGetExecutablePath failed");
    }
#endif
#endif

    impl_->mExe = exePath.string();
    impl_->mCwd = fs::current_path().string();

    if (argc > 0 && argv != nullptr) {
        impl_->mProgram = fs::path(argv[0]).filename().string();

        // Parse command line arguments
        for (int i = 1; i < argc; ++i) {
            String arg(argv[i]);
            size_t eq_pos = arg.find('=');
            if (eq_pos != String::npos) {
                String key = arg.substr(0, eq_pos);
                String value = arg.substr(eq_pos + 1);
                impl_->mArgs[key] = value;
            } else {
                impl_->mArgs[arg] = "";
            }
        }
    }

    spdlog::debug("EnvCore initialized: exe={}, cwd={}, program={}",
                  impl_->mExe, impl_->mCwd, impl_->mProgram);
}

auto EnvCore::Environ() -> HashMap<String, String> {
    HashMap<String, String> result;

#ifdef _WIN32
    wchar_t* env_block = GetEnvironmentStringsW();
    if (env_block) {
        wchar_t* env = env_block;
        while (*env) {
            std::wstring line(env);
            size_t eq_pos = line.find(L'=');
            if (eq_pos != std::wstring::npos) {
                std::wstring key = line.substr(0, eq_pos);
                std::wstring value = line.substr(eq_pos + 1);

                // Convert to narrow strings
                int key_size = WideCharToMultiByte(CP_UTF8, 0, key.c_str(), -1, nullptr, 0, nullptr, nullptr);
                int val_size = WideCharToMultiByte(CP_UTF8, 0, value.c_str(), -1, nullptr, 0, nullptr, nullptr);

                if (key_size > 0 && val_size > 0) {
                    std::string key_str(key_size - 1, '\0');
                    std::string val_str(val_size - 1, '\0');

                    WideCharToMultiByte(CP_UTF8, 0, key.c_str(), -1, &key_str[0], key_size, nullptr, nullptr);
                    WideCharToMultiByte(CP_UTF8, 0, value.c_str(), -1, &val_str[0], val_size, nullptr, nullptr);

                    result[String(key_str)] = String(val_str);
                }
            }
            env += line.length() + 1;
        }
        FreeEnvironmentStringsW(env_block);
    }
#else
    if (environ) {
        for (char** env = environ; *env; ++env) {
            String line(*env);
            size_t eq_pos = line.find('=');
            if (eq_pos != String::npos) {
                String key = line.substr(0, eq_pos);
                String value = line.substr(eq_pos + 1);
                result[key] = value;
            }
        }
    }
#endif

    spdlog::debug("Retrieved {} environment variables", result.size());
    return result;
}

void EnvCore::add(const String& key, const String& val) {
    spdlog::debug("Adding environment variable: {}={}", key, val);
    std::unique_lock lock(impl_->mMutex);
    impl_->mArgs[key] = val;
}

void EnvCore::addMultiple(const HashMap<String, String>& vars) {
    spdlog::debug("Adding {} environment variables", vars.size());
    std::unique_lock lock(impl_->mMutex);
    for (const auto& [key, val] : vars) {
        impl_->mArgs[key] = val;
    }
}

bool EnvCore::has(const String& key) {
    std::shared_lock lock(impl_->mMutex);
    bool exists = impl_->mArgs.find(key) != impl_->mArgs.end();
    spdlog::debug("Checking key existence: {}={}", key, exists);
    return exists;
}

bool EnvCore::hasAll(const Vector<String>& keys) {
    std::shared_lock lock(impl_->mMutex);
    for (const auto& key : keys) {
        if (impl_->mArgs.find(key) == impl_->mArgs.end()) {
            spdlog::debug("Key not found in hasAll: {}", key);
            return false;
        }
    }
    return true;
}

bool EnvCore::hasAny(const Vector<String>& keys) {
    std::shared_lock lock(impl_->mMutex);
    for (const auto& key : keys) {
        if (impl_->mArgs.find(key) != impl_->mArgs.end()) {
            spdlog::debug("Key found in hasAny: {}", key);
            return true;
        }
    }
    return false;
}

void EnvCore::del(const String& key) {
    spdlog::debug("Deleting environment variable: {}", key);
    std::unique_lock lock(impl_->mMutex);
    impl_->mArgs.erase(key);
}

void EnvCore::delMultiple(const Vector<String>& keys) {
    spdlog::debug("Deleting {} environment variables", keys.size());
    std::unique_lock lock(impl_->mMutex);
    for (const auto& key : keys) {
        impl_->mArgs.erase(key);
    }
}

auto EnvCore::get(const String& key, const String& default_value) -> String {
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

auto EnvCore::setEnv(const String& key, const String& val) -> bool {
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

auto EnvCore::setEnvMultiple(const HashMap<String, String>& vars) -> bool {
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

auto EnvCore::getEnv(const String& key, const String& default_value) -> String {
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

void EnvCore::unsetEnv(const String& name) {
    spdlog::debug("Unsetting environment variable: {}", name);

    String oldValue = getEnv(name, "");

#ifdef _WIN32
    SetEnvironmentVariableA(name.c_str(), nullptr);
#else
    ::unsetenv(name.c_str());
#endif

    notifyChangeCallbacks(name, oldValue, "");
}

void EnvCore::unsetEnvMultiple(const Vector<String>& names) {
    spdlog::debug("Unsetting {} environment variables", names.size());
    for (const auto& name : names) {
        unsetEnv(name);
    }
}

auto EnvCore::listVariables() -> Vector<String> {
    Vector<String> result;
    HashMap<String, String> envVars = Environ();

    result.reserve(envVars.size());
    for (const auto& [key, value] : envVars) {
        result.push_back(key);
    }

    spdlog::debug("Listed {} environment variables", result.size());
    return result;
}

auto EnvCore::filterVariables(
    const std::function<bool(const String&, const String&)>& predicate)
    -> HashMap<String, String> {
    HashMap<String, String> result;
    HashMap<String, String> envVars = Environ();

    for (const auto& [key, value] : envVars) {
        if (predicate(key, value)) {
            result[key] = value;
        }
    }

    spdlog::debug("Filtered {} environment variables from {} total",
                  result.size(), envVars.size());
    return result;
}

auto EnvCore::getVariablesWithPrefix(const String& prefix)
    -> HashMap<String, String> {
    return filterVariables([&prefix](const String& key, const String&) {
        return key.length() >= prefix.length() &&
               key.substr(0, prefix.length()) == prefix;
    });
}

auto EnvCore::getExecutablePath() const -> String {
    std::shared_lock lock(impl_->mMutex);
    return impl_->mExe;
}

auto EnvCore::getWorkingDirectory() const -> String {
    std::shared_lock lock(impl_->mMutex);
    return impl_->mCwd;
}

auto EnvCore::getProgramName() const -> String {
    std::shared_lock lock(impl_->mMutex);
    return impl_->mProgram;
}

auto EnvCore::getAllArgs() const -> HashMap<String, String> {
    std::shared_lock lock(impl_->mMutex);
    return impl_->mArgs;
}

auto EnvCore::registerChangeNotification(EnvChangeCallback callback) -> size_t {
    std::lock_guard<std::mutex> lock(sCallbackMutex);
    size_t id = sNextCallbackId++;
    sChangeCallbacks[id] = callback;
    spdlog::debug("Registered environment change notification with id: {}", id);
    return id;
}

auto EnvCore::unregisterChangeNotification(size_t id) -> bool {
    std::lock_guard<std::mutex> lock(sCallbackMutex);
    bool result = sChangeCallbacks.erase(id) > 0;
    spdlog::debug(
        "Unregistered environment change notification id: {}, success: {}", id,
        result);
    return result;
}

#if ATOM_ENABLE_DEBUG
void EnvCore::printAllVariables() {
    spdlog::debug("Printing all environment variables");
    Vector<String> vars = listVariables();
    for (const auto& var : vars) {
        spdlog::debug("Environment variable: {}", var);
    }
}

void EnvCore::printAllArgs() const {
    spdlog::debug("Printing all command-line arguments");
    std::shared_lock lock(impl_->mMutex);
    for (const auto& [key, value] : impl_->mArgs) {
        spdlog::debug("Argument: {}={}", key, value);
    }
}
#endif

}  // namespace atom::utils
