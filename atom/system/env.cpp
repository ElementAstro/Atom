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

#include <algorithm>  // For std::find, std::replace
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <shared_mutex>
#include <sstream>
#include <string_view>  // Keep for Environ parsing

#ifdef _WIN32
// clang-format off
#include <windows.h>
#include <shlobj.h>   // For SHGetKnownFolderPath
#include <userenv.h>  // For persistent environment variables
// clang-format on
#pragma comment(lib, "userenv.lib")
#else
#include <limits.h>  // For HOST_NAME_MAX
#include <pwd.h>     // For getpwuid
#include <sys/types.h>
#include <unistd.h>  // For readlink on Linux/macOS, gethostname
#if defined(__APPLE__)
#include <mach-o/dyld.h>  // For _NSGetExecutablePath on macOS
#endif
extern char** environ;  // POSIX environment variables
#endif

#include "atom/log/loguru.hpp"
// #include "atom/utils/argsview.hpp" // Assuming ArgumentParser is not used or
// adapted separately

namespace fs = std::filesystem;

namespace atom::utils {

// Initialize static members
HashMap<size_t, Env::EnvChangeCallback> Env::sChangeCallbacks;
std::mutex Env::sCallbackMutex;
size_t Env::sNextCallbackId = 1;

// 用于通知环境变量变化的辅助函数
void Env::notifyChangeCallbacks(const String& key, const String& oldValue,
                                const String& newValue) {
    LOG_F(INFO, "notifyChangeCallbacks called for key: {}", key);
    std::lock_guard<std::mutex> lock(sCallbackMutex);
    for (const auto& [id, callback] : sChangeCallbacks) {
        try {
            callback(key, oldValue, newValue);
        } catch (const std::exception& e) {
            LOG_F(ERROR, "Exception in change callback: {}", e.what());
        }
    }
}

// Use type aliases from high_performance.hpp within the implementation
using atom::containers::String;
template <typename K, typename V>
using HashMap = atom::containers::HashMap<K, V>;
template <typename T>
using Vector = atom::containers::Vector<T>;

class Env::Impl {
public:
    String mExe;      ///< Full path of the executable file.
    String mCwd;      ///< Working directory.
    String mProgram;  ///< Program name.

    HashMap<String, String> mArgs;  ///< List of command-line arguments.
    mutable std::shared_mutex
        mMutex;  ///< Shared mutex to protect member variables.
    // ArgumentParser mParser; ///< Argument parser - needs adaptation if used
};

Env::Env() : Env(0, nullptr) { LOG_F(INFO, "Env default constructor called"); }

Env::Env(int argc, char** argv) : impl_(std::make_shared<Impl>()) {
    // Use std::ostringstream for logging complex messages if String doesn't
    // have easy stream insertion
    std::ostringstream oss_log;
    oss_log << "Env constructor called with argc: " << argc << ", argv: [";
    for (int i = 0; i < argc; ++i) {
        oss_log << "\"" << (argv[i] ? argv[i] : "nullptr") << "\"";
        if (i < argc - 1) {
            oss_log << ", ";
        }
    }
    oss_log << "]";
    LOG_F(INFO, "{}", oss_log.str());

    fs::path exePath;

#ifdef _WIN32
    wchar_t buf[MAX_PATH];
    if (GetModuleFileNameW(nullptr, buf, MAX_PATH) == 0U) {
        LOG_F(ERROR, "GetModuleFileNameW failed with error {}", GetLastError());
        // Handle error, maybe throw or set a default path
    } else {
        exePath = buf;
    }
#else
    char linkBuf[1024];
    ssize_t count = readlink("/proc/self/exe", linkBuf,
                             sizeof(linkBuf) - 1);  // -1 for null terminator
    if (count != -1) {
        linkBuf[count] = '\0';
        exePath = linkBuf;
    } else {
        LOG_F(ERROR, "readlink /proc/self/exe failed");
        // Handle error, maybe try argv[0] or throw
        if (argc > 0 && argv != nullptr && argv[0] != nullptr) {
            // Fallback using argv[0], might be relative
            exePath = fs::absolute(argv[0]);
        }
    }
#endif

// Convert fs::path to String
// Using string() since String is basic_string<char>
#ifdef _WIN32
    impl_->mExe = String(exePath.string());  // Using string() instead of
                                             // wstring() for compatibility
#else
    impl_->mExe = String(exePath.string());
#endif
// Using string() and regular char '/' since String is basic_string<char>
#ifdef _WIN32
    impl_->mCwd = String(exePath.parent_path().string()) +
                  '/';  // Using string() and regular char
#else
    impl_->mCwd = String(exePath.parent_path().string()) + '/';
#endif

    if (argc > 0 && argv != nullptr && argv[0] != nullptr) {
        // Construct String from argv[0]
        impl_->mProgram = String(argv[0]);
    } else {
        impl_->mProgram = "";  // Default empty String
    }

    // Assuming String is compatible with LOG_F formatting
    LOG_F(INFO, "Executable path: {}",
          impl_->mExe);  // Use  if needed
    LOG_F(INFO, "Current working directory: {}", impl_->mCwd);
    LOG_F(INFO, "Program name: {}", impl_->mProgram);

    // Parse command-line arguments (simplified example)
    if (argc > 1 && argv != nullptr) {
        int i = 1;
        while (i < argc) {
            if (argv[i][0] == '-') {
                String key(argv[i] + 1);  // Skip '-'
                if (i + 1 < argc && argv[i + 1][0] != '-') {
                    // Argument with value
                    String value(argv[i + 1]);
                    add(key, value);
                    i += 2;
                } else {
                    // Argument without value (flag)
                    add(key, "");  // Add with empty value
                    i += 1;
                }
            } else {
                // Positional argument, ignore for this simple parsing
                LOG_F(WARNING, "Ignoring positional argument: {}", argv[i]);
                i += 1;
            }
        }
    }
    LOG_F(INFO, "Env constructor completed");
}

auto Env::createShared(int argc, char** argv) -> std::shared_ptr<Env> {
    return std::make_shared<Env>(argc, argv);
}

// Use String for parameters and internal map keys/values
void Env::add(const String& key, const String& val) {
    // Assuming String is compatible with LOG_F
    LOG_F(INFO, "Env::add called with key: {}, val: {}", key, val);
    std::unique_lock lock(impl_->mMutex);
    // Use contains method if HashMap provides it, otherwise use find
    if (impl_->mArgs.contains(key)) {  // Assumes HashMap has contains()
        LOG_F(ERROR, "Env::add: Duplicate key: {}", key);
    } else {
        DLOG_F(INFO, "Env::add: Add key: {} with value: {}", key, val);
        impl_->mArgs[key] =
            val;  // Assumes HashMap supports operator[] or use emplace
        // impl_->mArgs.emplace(key, val); // Alternative
    }
}

// Use HashMap<String, String> for parameter
void Env::addMultiple(const HashMap<String, String>& vars) {
    LOG_F(INFO, "Env::addMultiple called with {} variables", vars.size());
    std::unique_lock lock(impl_->mMutex);
    for (const auto& [key, val] : vars) {
        // Use contains or find
        if (!impl_->mArgs.contains(key)) {
            DLOG_F(INFO, "Env::addMultiple: Add key: {} with value: {}", key,
                   val);
            impl_->mArgs[key] = val;  // Or emplace
        } else {
            LOG_F(ERROR, "Env::addMultiple: Duplicate key: {}", key);
        }
    }
}

// Use String for parameter
bool Env::has(const String& key) {
    LOG_F(INFO, "Env::has called with key: {}", key);
    std::shared_lock lock(impl_->mMutex);
    bool result = impl_->mArgs.contains(key);  // Assumes HashMap has contains()
    LOG_F(INFO, "Env::has returning: {}", result);
    return result;
}

// Use Vector<String> for parameter
bool Env::hasAll(const Vector<String>& keys) {
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

// Use Vector<String> for parameter
bool Env::hasAny(const Vector<String>& keys) {
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

// Use String for parameter
void Env::del(const String& key) {
    LOG_F(INFO, "Env::del called with key: {}", key);
    std::unique_lock lock(impl_->mMutex);
    impl_->mArgs.erase(key);  // Assumes HashMap has erase(key)
    DLOG_F(INFO, "Env::del: Remove key: {}", key);
}

// Use Vector<String> for parameter
void Env::delMultiple(const Vector<String>& keys) {
    LOG_F(INFO, "Env::delMultiple called with {} keys", keys.size());
    std::unique_lock lock(impl_->mMutex);
    for (const auto& key : keys) {
        impl_->mArgs.erase(key);
        DLOG_F(INFO, "Env::delMultiple: Remove key: {}", key);
    }
}

// Use String for parameters and return type
auto Env::get(const String& key, const String& default_value) -> String {
    LOG_F(INFO, "Env::get called with key: {}, default_value: {}", key,
          default_value);
    std::shared_lock lock(impl_->mMutex);
    auto it = impl_->mArgs.find(key);  // Assumes HashMap has find()
    if (it == impl_->mArgs.end()) {
        DLOG_F(INFO, "Env::get: Key: {} not found, return default value: {}",
               key, default_value);
        return default_value;
    }
    // Assuming String copy constructor/assignment is efficient or uses COW
    String value = it->second;
    LOG_F(INFO, "Env::get returning: {}", value);
    return value;
}

// Use String for parameters, use  for C API calls
auto Env::setEnv(const String& key, const String& val) -> bool {
    LOG_F(INFO, "Env::setEnv called with key: {}, val: {}", key, val);
    DLOG_F(INFO, "Env::setEnv: Set key: {} with value: {}", key, val);

    // 保存旧值用于通知
    String oldValue = getEnv(key, "");

#ifdef _WIN32
    // Use _putenv_s for safety if available and preferred
    // bool result = _putenv_s(key, val) == 0;
    // Or stick to SetEnvironmentVariableA if String guarantees null termination
    bool result = SetEnvironmentVariableA(key.c_str(), val.c_str()) != 0;
#else
    bool result = ::setenv(key.c_str(), val.c_str(), 1) == 0;
#endif

    if (result) {
        // 触发通知回调
        notifyChangeCallbacks(key, oldValue, val);
    }

    LOG_F(INFO, "Env::setEnv returning: {}", result);
    return result;
}

// Use HashMap<String, String> for parameter, use  for C API calls
auto Env::setEnvMultiple(const HashMap<String, String>& vars) -> bool {
    LOG_F(INFO, "Env::setEnvMultiple called with {} variables", vars.size());
    bool allSuccess = true;
    for (const auto& [key, val] : vars) {
        DLOG_F(INFO, "Env::setEnvMultiple: Setting key: {} with value: {}", key,
               val);
#ifdef _WIN32
        bool result = SetEnvironmentVariableA(key.c_str(), val.c_str()) != 0;
#else
        bool result = ::setenv(key.c_str(), val.c_str(), 1) == 0;
#endif
        if (!result) {
            LOG_F(ERROR, "Env::setEnvMultiple: Failed to set key: {}", key);
            allSuccess = false;
        }
    }
    LOG_F(INFO, "Env::setEnvMultiple returning: {}", allSuccess);
    return allSuccess;
}

// Use String for parameters and return type, use  for C API calls
auto Env::getEnv(const String& key, const String& default_value) -> String {
    LOG_F(INFO, "Env::getEnv called with key: {}, default_value: {}", key,
          default_value);
    DLOG_F(INFO, "Env::getEnv: Get key: {} with default value: {}", key,
           default_value);
#ifdef _WIN32
    // Need dynamic allocation for potentially large values
    DWORD needed = GetEnvironmentVariableA(key.c_str(), nullptr, 0);
    if (needed == 0) {
        // Variable not found or error
        if (GetLastError() == ERROR_ENVVAR_NOT_FOUND) {
            DLOG_F(INFO, "Env::getEnv: Key: {} not found, returning default",
                   key);
        } else {
            LOG_F(ERROR,
                  "Env::getEnv: GetEnvironmentVariableA failed for key {} with "
                  "error {}",
                  key, GetLastError());
        }
        return default_value;
    }
    // Allocate buffer using high-performance vector if suitable, or
    // std::vector/unique_ptr
    std::vector<char> buf(needed);  // std::vector for simplicity here
    DWORD ret = GetEnvironmentVariableA(key.c_str(), buf.data(), needed);
    if (ret == 0 || ret >= needed) {  // Should not happen if needed > 0
        LOG_F(ERROR,
              "Env::getEnv: GetEnvironmentVariableA failed on second call for "
              "key {}",
              key);
        return default_value;
    }
    // Construct String from the buffer
    String value(buf.data(),
                 ret);  // Assuming String constructor from char*, length
    DLOG_F(INFO, "Env::getEnv: Get key: {} with value: {}", key, value);
    return value;

#else
    const char* v = ::getenv(key.c_str());
    if (v == nullptr) {
        DLOG_F(INFO, "Env::getEnv: Key: {} not found, returning default", key);
        return default_value;
    }
    // Construct String from const char*
    String value(v);
    DLOG_F(INFO, "Env::getEnv: Get key: {} with value: {}", key, value);
    return value;
#endif
}

// Return HashMap<String, String>, parse results into String
auto Env::Environ() -> HashMap<String, String> {
    LOG_F(INFO, "Env::Environ called");
    HashMap<String, String> envMap;

#ifdef _WIN32
    LPCH envStrings = GetEnvironmentStringsA();  // Use ANSI version
    if (envStrings == nullptr) {
        LOG_F(ERROR, "Env::Environ: GetEnvironmentStringsA failed");
        return envMap;
    }

    LPCH var = envStrings;
    while (*var != '\0') {
        // Use string_view for efficient parsing
        std::string_view envVar(var);
        auto pos = envVar.find('=');
        // Skip variables starting with '=' (like '=C:=C:\')
        if (pos != std::string_view::npos && pos > 0) {
            // Construct String from substrings
            String key(envVar.substr(0, pos).data(), pos);
            String value(envVar.substr(pos + 1).data(),
                         envVar.length() - (pos + 1));
            envMap.emplace(key, value);
        }
        var += envVar.length() + 1;  // Move to the next string
    }

    FreeEnvironmentStringsA(envStrings);  // Use ANSI version

#else  // POSIX
    if (environ != nullptr) {
        for (char** current = environ; *current; ++current) {
            std::string_view envVar(*current);
            auto pos = envVar.find('=');
            if (pos != std::string_view::npos) {
                // Construct String from substrings
                String key(envVar.substr(0, pos).data(), pos);
                String value(envVar.substr(pos + 1).data(),
                             envVar.length() - (pos + 1));
                envMap.emplace(key, value);
            }
        }
    } else {
        LOG_F(WARNING, "Env::Environ: POSIX environ is NULL");
    }
#endif

    LOG_F(INFO, "Env::Environ returning environment map with {} entries",
          envMap.size());
    return envMap;
}

// Use String for parameter, use  for C API calls
void Env::unsetEnv(const String& name) {
    LOG_F(INFO, "Env::unsetEnv called with name: {}", name);
#ifdef _WIN32
    // Setting to empty value effectively unsets on some systems, but
    // SetEnvironmentVariableA with NULL is correct
    if (SetEnvironmentVariableA(name.c_str(), nullptr) == 0) {
        // Check if the variable simply didn't exist
        if (GetLastError() != ERROR_ENVVAR_NOT_FOUND) {
            LOG_F(ERROR, "Failed to unset environment variable: {}, Error: {}",
                  name, GetLastError());
        } else {
            DLOG_F(INFO, "Env::unsetEnv: Variable {} did not exist.", name);
        }
    }
#else
    if (::unsetenv(name.c_str()) != 0) {
        // errno might be set, e.g., EINVAL if name is invalid
        // Don't log error if it simply didn't exist, check getenv first if
        // needed
        LOG_F(ERROR, "Failed to unset environment variable: {}, errno: {}",
              name, errno);
    }
#endif
}

// Use Vector<String> for parameter, use  for C API calls
void Env::unsetEnvMultiple(const Vector<String>& names) {
    LOG_F(INFO, "Env::unsetEnvMultiple called with {} names", names.size());
    for (const auto& name : names) {
        DLOG_F(INFO, "Env::unsetEnvMultiple: Unsetting variable: {}", name);
        unsetEnv(name);  // Call the single unset function
    }
}

// Return Vector<String>, parse results into String
auto Env::listVariables() -> Vector<String> {
    LOG_F(INFO, "Env::listVariables called");
    Vector<String> vars;

#ifdef _WIN32
    LPCH envStrings = GetEnvironmentStringsA();  // Use ANSI
    if (envStrings != nullptr) {
        for (LPCH var = envStrings; *var != '\0'; var += strlen(var) + 1) {
            // Construct String from char*
            vars.emplace_back(
                var);  // Assumes Vector has emplace_back or push_back accepting
                       // String convertible from char*
        }
        FreeEnvironmentStringsA(envStrings);  // Use ANSI
    }
#else  // POSIX
    if (environ != nullptr) {
        for (char** current = environ; *current; ++current) {
            vars.emplace_back(*current);
        }
    }
#endif

    LOG_F(INFO, "Env::listVariables returning {} variables", vars.size());
    return vars;
}

// Return HashMap<String, String>, adapt predicate signature
auto Env::filterVariables(
    const std::function<bool(const String&, const String&)>& predicate)
    -> HashMap<String, String> {
    LOG_F(INFO, "Env::filterVariables called");
    HashMap<String, String> filteredVars;
    auto allVars = Environ();  // Gets HashMap<String, String>

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

// Use String for parameter, return HashMap<String, String>, adapt lambda
// signature
auto Env::getVariablesWithPrefix(const String& prefix)
    -> HashMap<String, String> {
    LOG_F(INFO, "Env::getVariablesWithPrefix called with prefix: {}", prefix);
    return filterVariables([&prefix](const String& key,
                                     const String& /*value*/) {
        // Assuming String has starts_with or equivalent (like rfind)
        // return key.starts_with(prefix); // C++20 style if available
        return key.rfind(prefix, 0) == 0;  // Check if key starts with prefix
    });
}

// Use HashMap<String, String> for parameter, handle String output to file
auto Env::saveToFile(const std::filesystem::path& filePath,
                     const HashMap<String, String>& vars) -> bool {
    LOG_F(INFO, "Env::saveToFile called with filePath: {}",
          filePath.string());  // Use  for log

    try {
        std::ofstream file(
            filePath,
            std::ios::out | std::ios::binary);  // Use binary mode potentially
        if (!file.is_open()) {
            LOG_F(ERROR, "Env::saveToFile: Failed to open file: {}",
                  filePath.string());
            return false;
        }

        const auto& varsToSave = vars.empty() ? Environ() : vars;

        for (const auto& [key, value] : varsToSave) {
            // Write String to file, assuming operator<< or need for
            // /.data() Using .data() and .length() for potentially
            // binary-safe writing
            file.write(key.data(), key.length());
            file.put('=');
            file.write(value.data(), value.length());
            file.put('\n');  // Use '\n' consistently
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

// Use String internally, HashMap<String, String> for loaded vars
auto Env::loadFromFile(const std::filesystem::path& filePath, bool overwrite)
    -> bool {
    LOG_F(INFO, "Env::loadFromFile called with filePath: {}, overwrite: {}",
          filePath.string(), overwrite);

    try {
        std::ifstream file(filePath, std::ios::binary);  // Use binary mode
        if (!file.is_open()) {
            LOG_F(ERROR, "Env::loadFromFile: Failed to open file: {}",
                  filePath.string());
            return false;
        }

        // Use std::string for reading lines, then convert to String
        std::string line_std;
        HashMap<String, String> loadedVars;

        while (std::getline(file, line_std)) {
            // Trim whitespace (optional but good practice)
            // line_std.erase(0, line_std.find_first_not_of(" \t\r\n"));
            // line_std.erase(line_std.find_last_not_of(" \t\r\n") + 1);

            // Skip empty lines and comments
            if (line_std.empty() || line_std[0] == '#') {
                continue;
            }

            auto pos = line_std.find('=');
            if (pos != std::string::npos) {
                // Construct String from std::string substrings
                String key(line_std.substr(0, pos));
                String value(line_std.substr(pos + 1));

                loadedVars[key] = value;  // Or emplace
                DLOG_F(INFO, "Env::loadFromFile: Loaded variable: {} = {}", key,
                       value);
            } else {
                DLOG_F(WARNING,
                       "Env::loadFromFile: Skipping malformed line: {}",
                       line_std);
            }
        }

        file.close();

        // Set environment variables using static methods
        for (const auto& [key, value] : loadedVars) {
            // Check if variable exists using getEnv
            String currentValueStr = getEnv(key, "");  // Check existence
            bool exists =
                !currentValueStr
                     .empty();  // Assuming empty means not found or empty value

            if (overwrite || !exists) {
                if (!setEnv(key, value)) {  // Use static setEnv
                    LOG_F(WARNING,
                          "Env::loadFromFile: Failed to set variable: {}", key);
                } else {
                    DLOG_F(INFO, "Env::loadFromFile: Set variable: {} = {}",
                           key, value);
                }
            } else {
                DLOG_F(INFO,
                       "Env::loadFromFile: Skipping existing variable: {}",
                       key);
            }
        }

        LOG_F(INFO,
              "Env::loadFromFile: Successfully processed {} variables from {}",
              loadedVars.size(), filePath.string());
        return true;
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Env::loadFromFile: Exception: {}", e.what());
        return false;
    }
}

// Return String
auto Env::getExecutablePath() const -> String {
    std::shared_lock lock(impl_->mMutex);
    return impl_->mExe;  // Assuming String copy is efficient
}

// Return String
auto Env::getWorkingDirectory() const -> String {
    std::shared_lock lock(impl_->mMutex);
    return impl_->mCwd;
}

// Return String
auto Env::getProgramName() const -> String {
    std::shared_lock lock(impl_->mMutex);
    return impl_->mProgram;
}

// Return HashMap<String, String>
auto Env::getAllArgs() const -> HashMap<String, String> {
    std::shared_lock lock(impl_->mMutex);
    return impl_->mArgs;  // Assuming HashMap copy is efficient or intended
}

#if ATOM_ENABLE_DEBUG
// Use Vector<String>
void Env::printAllVariables() {
    LOG_F(INFO, "Env::printAllVariables called");
    Vector<String> vars = listVariables();
    for (const auto& var : vars) {
        // Assuming String is compatible with LOG_F
        DLOG_F(INFO, "{}", var);
    }
}

// Use HashMap<String, String>
void Env::printAllArgs() const {
    LOG_F(INFO, "Env::printAllArgs called");
    std::shared_lock lock(impl_->mMutex);
    for (const auto& [key, value] : impl_->mArgs) {
        DLOG_F(INFO, "Arg: {} = {}", key, value);
    }
}
#endif

Env::ScopedEnv::ScopedEnv(const String& key, const String& value)
    : mKey(key), mHadValue(false) {
    LOG_F(INFO, "ScopedEnv constructor called with key: {}, value: {}", key,
          value);
    // 保存原始值
    mOriginalValue = getEnv(key, "");
    mHadValue = !mOriginalValue.empty();

    // 设置新值
    setEnv(key, value);
}

Env::ScopedEnv::~ScopedEnv() {
    LOG_F(INFO, "ScopedEnv destructor called for key: {}", mKey);
    if (mHadValue) {
        // 恢复原始值
        setEnv(mKey, mOriginalValue);
    } else {
        // 如果之前没有这个环境变量，则删除它
        unsetEnv(mKey);
    }
}

auto Env::createScopedEnv(const String& key, const String& value)
    -> std::shared_ptr<ScopedEnv> {
    LOG_F(INFO, "createScopedEnv called with key: {}, value: {}", key, value);
    return std::make_shared<ScopedEnv>(key, value);
}

auto Env::registerChangeNotification(EnvChangeCallback callback) -> size_t {
    LOG_F(INFO, "registerChangeNotification called");
    std::lock_guard<std::mutex> lock(sCallbackMutex);
    size_t id = sNextCallbackId++;
    sChangeCallbacks[id] = callback;
    return id;
}

auto Env::unregisterChangeNotification(size_t id) -> bool {
    LOG_F(INFO, "unregisterChangeNotification called with id: {}", id);
    std::lock_guard<std::mutex> lock(sCallbackMutex);
    return sChangeCallbacks.erase(id) > 0;
}

// 实现获取系统特定环境目录功能
auto Env::getHomeDir() -> String {
    LOG_F(INFO, "getHomeDir called");

#ifdef _WIN32
    // 在Windows上使用USERPROFILE环境变量
    String homePath = getEnv("USERPROFILE", "");
    if (homePath.empty()) {
        // 备用方案：使用HOMEDRIVE+HOMEPATH组合
        String homeDrive = getEnv("HOMEDRIVE", "");
        String homePath2 = getEnv("HOMEPATH", "");
        if (!homeDrive.empty() && !homePath2.empty()) {
            homePath = homeDrive + homePath2;
        }
    }
#else
    // 在POSIX系统上使用HOME环境变量
    String homePath = getEnv("HOME", "");
    if (homePath.empty()) {
        // 备用方案：使用passwd文件
        struct passwd* pw = getpwuid(getuid());
        if (pw && pw->pw_dir) {
            homePath = pw->pw_dir;
        }
    }
#endif

    LOG_F(INFO, "getHomeDir returning: {}", homePath);
    return homePath;
}

auto Env::getTempDir() -> String {
    LOG_F(INFO, "getTempDir called");
    String tempPath;

#ifdef _WIN32
    // 在Windows上使用GetTempPath API
    DWORD bufferLength = MAX_PATH + 1;
    std::vector<char> buffer(bufferLength);
    DWORD length = GetTempPathA(bufferLength, buffer.data());
    if (length > 0 && length <= bufferLength) {
        tempPath = String(buffer.data(), length);
    } else {
        // 备用方案：使用环境变量
        tempPath = getEnv("TEMP", "");
        if (tempPath.empty()) {
            tempPath = getEnv("TMP", "C:\\Temp");
        }
    }
#else
    // 在POSIX系统上使用标准环境变量
    tempPath = getEnv("TMPDIR", "");
    if (tempPath.empty()) {
        tempPath = "/tmp";  // 标准POSIX临时目录
    }
#endif

    LOG_F(INFO, "getTempDir returning: {}", tempPath);
    return tempPath;
}

auto Env::getConfigDir() -> String {
    LOG_F(INFO, "getConfigDir called");
    String configPath;

#ifdef _WIN32
    // 在Windows上使用APPDATA或LOCALAPPDATA
    configPath = getEnv("APPDATA", "");
    if (configPath.empty()) {
        configPath = getEnv("LOCALAPPDATA", "");
    }
#else
    // 在Linux上使用XDG_CONFIG_HOME或默认的~/.config
    configPath = getEnv("XDG_CONFIG_HOME", "");
    if (configPath.empty()) {
        String home = getHomeDir();
        if (!home.empty()) {
            configPath = home + "/.config";
        }
    }
#endif

    LOG_F(INFO, "getConfigDir returning: {}", configPath);
    return configPath;
}

auto Env::getDataDir() -> String {
    LOG_F(INFO, "getDataDir called");
    String dataPath;

#ifdef _WIN32
    // 在Windows上使用LOCALAPPDATA
    dataPath = getEnv("LOCALAPPDATA", "");
    if (dataPath.empty()) {
        dataPath = getEnv("APPDATA", "");
    }
#else
    // 在Linux上使用XDG_DATA_HOME或默认的~/.local/share
    dataPath = getEnv("XDG_DATA_HOME", "");
    if (dataPath.empty()) {
        String home = getHomeDir();
        if (!home.empty()) {
            dataPath = home + "/.local/share";
        }
    }
#endif

    LOG_F(INFO, "getDataDir returning: {}", dataPath);
    return dataPath;
}

// 实现环境变量扩展功能
auto Env::expandVariables(const String& str, VariableFormat format) -> String {
    LOG_F(INFO, "expandVariables called with format: {}",
          static_cast<int>(format));

    if (str.empty()) {
        return str;
    }

    // 自动检测格式
    if (format == VariableFormat::AUTO) {
#ifdef _WIN32
        format = VariableFormat::WINDOWS;
#else
        format = VariableFormat::UNIX;
#endif
    }

    String result;
    result.reserve(str.length() * 2);  // 预留足够空间避免频繁重新分配

    if (format == VariableFormat::UNIX) {
        // 处理UNIX风格变量 ($VAR或${VAR})
        size_t pos = 0;
        while (pos < str.length()) {
            if (str[pos] == '$' && pos + 1 < str.length()) {
                if (str[pos + 1] == '{') {  // ${VAR}形式
                    size_t closePos = str.find('}', pos + 2);
                    if (closePos != String::npos) {
                        String varName =
                            str.substr(pos + 2, closePos - pos - 2);
                        String varValue = getEnv(varName, "");
                        result += varValue;
                        pos = closePos + 1;
                        continue;
                    }
                } else if (isalpha(str[pos + 1]) ||
                           str[pos + 1] == '_') {  // $VAR形式
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
            // 如果不是变量引用，直接添加字符
            result += str[pos++];
        }
    } else {  // VariableFormat::WINDOWS
        // 处理Windows风格变量 (%VAR%)
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
            // 如果不是变量引用，直接添加字符
            result += str[pos++];
        }
    }

    LOG_F(INFO, "expandVariables returning expanded string");
    return result;
}

// 实现持久化环境变量功能
auto Env::setPersistentEnv(const String& key, const String& val,
                           PersistLevel level) -> bool {
    LOG_F(INFO, "setPersistentEnv called with key: {}, val: {}, level: {}", key,
          val, static_cast<int>(level));

    // PROCESS级别就是普通的setEnv
    if (level == PersistLevel::PROCESS) {
        return setEnv(key, val);
    }

#ifdef _WIN32
    // Windows平台上的持久化设置
    HKEY hKey;
    DWORD dwDisposition;

    // 根据级别选择注册表位置
    const char* subKey = (level == PersistLevel::USER)
                             ? "Environment"
                             : "SYSTEM\\CurrentControlSet\\Control\\Session "
                               "Manager\\Environment";
    REGSAM sam = KEY_WRITE;
    HKEY rootKey =
        (level == PersistLevel::USER) ? HKEY_CURRENT_USER : HKEY_LOCAL_MACHINE;

    if (level == PersistLevel::SYSTEM && !IsUserAnAdmin()) {
        LOG_F(ERROR,
              "setPersistentEnv: Setting SYSTEM level environment requires "
              "admin privileges");
        return false;
    }

    // 打开或创建注册表项
    if (RegCreateKeyExA(rootKey, subKey, 0, NULL, 0, sam, NULL, &hKey,
                        &dwDisposition) != ERROR_SUCCESS) {
        LOG_F(ERROR, "setPersistentEnv: Failed to open registry key");
        return false;
    }

    // 设置注册表值
    LONG result = RegSetValueExA(hKey, key.c_str(), 0, REG_SZ,
                                 (LPBYTE)val.c_str(), val.length() + 1);
    RegCloseKey(hKey);

    if (result != ERROR_SUCCESS) {
        LOG_F(ERROR, "setPersistentEnv: Failed to set registry value");
        return false;
    }

    // 广播环境变量改变的消息
    SendMessageTimeoutA(HWND_BROADCAST, WM_SETTINGCHANGE, 0,
                        (LPARAM) "Environment", SMTO_ABORTIFHUNG, 5000, NULL);

    // 同时设置当前进程的环境变量
    setEnv(key, val);
    return true;
#else
    // POSIX系统上的持久化设置
    // 通常是修改配置文件，如~/.profile, ~/.bashrc等

    String homeDir = getHomeDir();
    if (homeDir.empty()) {
        LOG_F(ERROR, "setPersistentEnv: Failed to get home directory");
        return false;
    }

    std::string filePath;
    if (level == PersistLevel::USER) {
        // 选择适当的配置文件
        if (std::filesystem::exists(homeDir + "/.bash_profile")) {
            filePath = homeDir + "/.bash_profile";
        } else if (std::filesystem::exists(homeDir + "/.profile")) {
            filePath = homeDir + "/.profile";
        } else {
            filePath = homeDir + "/.bashrc";  // 使用最常见的
        }
    } else {  // PersistLevel::SYSTEM
        // 系统级别需要管理员权限，通常写入/etc/environment或/etc/profile.d/
        filePath = "/etc/environment";
        // 检查是否有写权限
        if (access(filePath.c_str(), W_OK) != 0) {
            LOG_F(ERROR,
                  "setPersistentEnv: No write permission for system "
                  "environment file");
            return false;
        }
    }

    // 读取现有文件
    std::vector<std::string> lines;
    std::ifstream inFile(filePath);
    if (inFile.is_open()) {
        std::string line;
        while (std::getline(inFile, line)) {
            // 跳过注释以及要设置的变量的现有定义
            if (line.empty() || line[0] == '#') {
                lines.push_back(line);
                continue;
            }

            // 查找"KEY="形式的行
            std::string pattern = key.c_str();
            pattern += "=";
            if (line.find(pattern) == 0) {
                continue;  // 跳过要替换的行
            }

            lines.push_back(line);
        }
        inFile.close();
    }

    // 添加新的环境变量定义
    std::string newLine = key.c_str();
    newLine += "=";
    newLine += val.c_str();
    lines.push_back(newLine);

    // 写回文件
    std::ofstream outFile(filePath);
    if (!outFile.is_open()) {
        LOG_F(ERROR, "setPersistentEnv: Failed to open file for writing: {}",
              filePath);
        return false;
    }

    for (const auto& line : lines) {
        outFile << line << std::endl;
    }
    outFile.close();

    // 同时设置当前进程的环境变量
    setEnv(key, val);

    LOG_F(INFO,
          "setPersistentEnv: Successfully set persistent environment variable "
          "in {}",
          filePath);
    return true;
#endif
}

auto Env::deletePersistentEnv(const String& key, PersistLevel level) -> bool {
    LOG_F(INFO, "deletePersistentEnv called with key: {}, level: {}", key,
          static_cast<int>(level));

    // PROCESS级别就是普通的unsetEnv
    if (level == PersistLevel::PROCESS) {
        unsetEnv(key);
        return true;
    }

#ifdef _WIN32
    // Windows平台上的删除
    HKEY hKey;

    // 根据级别选择注册表位置
    const char* subKey = (level == PersistLevel::USER)
                             ? "Environment"
                             : "SYSTEM\\CurrentControlSet\\Control\\Session "
                               "Manager\\Environment";
    REGSAM sam = KEY_WRITE;
    HKEY rootKey =
        (level == PersistLevel::USER) ? HKEY_CURRENT_USER : HKEY_LOCAL_MACHINE;

    if (level == PersistLevel::SYSTEM && !IsUserAnAdmin()) {
        LOG_F(ERROR,
              "deletePersistentEnv: Deleting SYSTEM level environment requires "
              "admin privileges");
        return false;
    }

    // 打开注册表项
    if (RegOpenKeyExA(rootKey, subKey, 0, sam, &hKey) != ERROR_SUCCESS) {
        LOG_F(ERROR, "deletePersistentEnv: Failed to open registry key");
        return false;
    }

    // 删除注册表值
    LONG result = RegDeleteValueA(hKey, key.c_str());
    RegCloseKey(hKey);

    if (result != ERROR_SUCCESS && result != ERROR_FILE_NOT_FOUND) {
        LOG_F(ERROR, "deletePersistentEnv: Failed to delete registry value");
        return false;
    }

    // 广播环境变量改变的消息
    SendMessageTimeoutA(HWND_BROADCAST, WM_SETTINGCHANGE, 0,
                        (LPARAM) "Environment", SMTO_ABORTIFHUNG, 5000, NULL);

    // 同时从当前进程中删除
    unsetEnv(key);
    return true;
#else
    // POSIX系统上的删除
    String homeDir = getHomeDir();
    if (homeDir.empty()) {
        LOG_F(ERROR, "deletePersistentEnv: Failed to get home directory");
        return false;
    }

    std::string filePath;
    if (level == PersistLevel::USER) {
        // 选择适当的配置文件
        if (std::filesystem::exists(homeDir + "/.bash_profile")) {
            filePath = homeDir + "/.bash_profile";
        } else if (std::filesystem::exists(homeDir + "/.profile")) {
            filePath = homeDir + "/.profile";
        } else {
            filePath = homeDir + "/.bashrc";
        }
    } else {  // PersistLevel::SYSTEM
        filePath = "/etc/environment";
        // 检查是否有写权限
        if (access(filePath.c_str(), W_OK) != 0) {
            LOG_F(ERROR,
                  "deletePersistentEnv: No write permission for system "
                  "environment file");
            return false;
        }
    }

    // 读取现有文件
    std::vector<std::string> lines;
    std::ifstream inFile(filePath);
    bool found = false;

    if (inFile.is_open()) {
        std::string line;
        while (std::getline(inFile, line)) {
            // 查找"KEY="形式的行
            std::string pattern = key.c_str();
            pattern += "=";
            if (line.find(pattern) == 0) {
                found = true;
                continue;  // 跳过要删除的行
            }
            lines.push_back(line);
        }
        inFile.close();
    } else {
        LOG_F(ERROR, "deletePersistentEnv: Failed to open file: {}", filePath);
        return false;
    }

    if (!found) {
        LOG_F(INFO, "deletePersistentEnv: Key not found in {}", filePath);
        return true;  // 不存在也算成功
    }

    // 写回文件
    std::ofstream outFile(filePath);
    if (!outFile.is_open()) {
        LOG_F(ERROR, "deletePersistentEnv: Failed to open file for writing: {}",
              filePath);
        return false;
    }

    for (const auto& line : lines) {
        outFile << line << std::endl;
    }
    outFile.close();

    // 同时从当前进程中删除
    unsetEnv(key);

    LOG_F(INFO,
          "deletePersistentEnv: Successfully deleted persistent environment "
          "variable from {}",
          filePath);
    return true;
#endif
}

// 实现PATH操作功能
auto Env::getPathSeparator() -> char {
#ifdef _WIN32
    return ';';
#else
    return ':';
#endif
}

auto Env::splitPathString(const String& pathStr) -> Vector<String> {
    LOG_F(INFO, "splitPathString called");
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
            // 去除首尾空格
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

    // 添加最后一段
    if (start < pathStr.length()) {
        String path = pathStr.substr(start);
        // 去除首尾空格
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
    LOG_F(INFO, "joinPathString called with {} paths", paths.size());
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
    LOG_F(INFO, "getPathEntries called");

#ifdef _WIN32
    String pathVar = getEnv("Path", "");
#else
    String pathVar = getEnv("PATH", "");
#endif

    return splitPathString(pathVar);
}

auto Env::isInPath(const String& path) -> bool {
    LOG_F(INFO, "isInPath called with path: {}", path);
    Vector<String> paths = getPathEntries();

    // 标准化给定的路径以进行比较
    std::filesystem::path normalizedPath;
    try {
        normalizedPath =
            std::filesystem::absolute(path.c_str()).lexically_normal();
    } catch (const std::exception& e) {
        LOG_F(ERROR, "isInPath: Failed to normalize path: {}", e.what());
        return false;
    }

    String normalizedPathStr = normalizedPath.string();

    for (const auto& entry : paths) {
        try {
            std::filesystem::path entryPath =
                std::filesystem::absolute(entry.c_str()).lexically_normal();
            if (entryPath == normalizedPath) {
                LOG_F(INFO, "isInPath: Path found in PATH");
                return true;
            }
        } catch (const std::exception& e) {
            LOG_F(WARNING, "isInPath: Failed to normalize PATH entry: {}",
                  e.what());
            continue;
        }
    }

    // 尝试直接字符串比较（不区分大小写）
    for (const auto& entry : paths) {
        String lowerEntry = entry;
        String lowerPath = path;

        std::transform(lowerEntry.begin(), lowerEntry.end(), lowerEntry.begin(),
                       ::tolower);
        std::transform(lowerPath.begin(), lowerPath.end(), lowerPath.begin(),
                       ::tolower);

        if (lowerEntry == lowerPath) {
            LOG_F(INFO,
                  "isInPath: Path found in PATH (case-insensitive match)");
            return true;
        }
    }

    LOG_F(INFO, "isInPath: Path not found in PATH");
    return false;
}

auto Env::addToPath(const String& path, bool prepend) -> bool {
    LOG_F(INFO, "addToPath called with path: {}, prepend: {}", path, prepend);

    // 检查路径是否已经存在于PATH中
    if (isInPath(path)) {
        LOG_F(INFO, "addToPath: Path already exists in PATH");
        return true;  // 已经存在，无需添加
    }

    // 获取PATH环境变量名称
#ifdef _WIN32
    String pathVarName = "Path";
#else
    String pathVarName = "PATH";
#endif

    // 获取当前PATH
    String currentPath = getEnv(pathVarName, "");
    String newPath;

    // 构造新的PATH
    if (currentPath.empty()) {
        newPath = path;
    } else {
        if (prepend) {
            newPath = path + getPathSeparator() + currentPath;
        } else {
            newPath = currentPath + getPathSeparator() + path;
        }
    }

    // 更新PATH
    bool result = setEnv(pathVarName, newPath);
    if (result) {
        LOG_F(INFO, "addToPath: Successfully added path to PATH");
    } else {
        LOG_F(ERROR, "addToPath: Failed to update PATH");
    }

    return result;
}

auto Env::removeFromPath(const String& path) -> bool {
    LOG_F(INFO, "removeFromPath called with path: {}", path);

    // 检查路径是否存在于PATH中
    if (!isInPath(path)) {
        LOG_F(INFO, "removeFromPath: Path does not exist in PATH");
        return true;  // 不存在，无需删除
    }

    // 获取PATH环境变量名称
#ifdef _WIN32
    String pathVarName = "Path";
#else
    String pathVarName = "PATH";
#endif

    // 获取当前PATH条目
    Vector<String> paths = getPathEntries();
    Vector<String> newPaths;

    // 标准化给定的路径以进行比较
    std::filesystem::path normalizedPath;
    try {
        normalizedPath =
            std::filesystem::absolute(path.c_str()).lexically_normal();
    } catch (const std::exception& e) {
        LOG_F(ERROR, "removeFromPath: Failed to normalize path: {}", e.what());
        return false;
    }

    String normalizedPathStr = normalizedPath.string();

    // 过滤掉要删除的路径
    for (const auto& entry : paths) {
        try {
            std::filesystem::path entryPath =
                std::filesystem::absolute(entry.c_str()).lexically_normal();
            if (entryPath != normalizedPath) {
                newPaths.push_back(entry);
            }
        } catch (const std::exception& e) {
            LOG_F(WARNING, "removeFromPath: Failed to normalize PATH entry: {}",
                  e.what());

            // 使用简单的字符串比较作为备选方案
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

    // 更新PATH
    String newPath = joinPathString(newPaths);
    bool result = setEnv(pathVarName, newPath);

    if (result) {
        LOG_F(INFO, "removeFromPath: Successfully removed path from PATH");
    } else {
        LOG_F(ERROR, "removeFromPath: Failed to update PATH");
    }

    return result;
}

// 实现环境变量比较和合并功能
auto Env::diffEnvironments(const HashMap<String, String>& env1,
                           const HashMap<String, String>& env2)
    -> std::tuple<HashMap<String, String>, HashMap<String, String>,
                  HashMap<String, String>> {
    LOG_F(INFO, "diffEnvironments called");

    HashMap<String, String> added;
    HashMap<String, String> removed;
    HashMap<String, String> modified;

    // 查找env2中添加或修改的变量
    for (const auto& [key, val2] : env2) {
        auto it = env1.find(key);
        if (it == env1.end()) {
            // 添加的变量
            added[key] = val2;
        } else if (it->second != val2) {
            // 修改的变量
            modified[key] = val2;
        }
    }

    // 查找在env1中存在但在env2中不存在的变量
    for (const auto& [key, val1] : env1) {
        if (env2.find(key) == env2.end()) {
            removed[key] = val1;
        }
    }

    LOG_F(INFO,
          "diffEnvironments: Found {} added, {} removed, {} modified variables",
          added.size(), removed.size(), modified.size());

    return std::make_tuple(added, removed, modified);
}

auto Env::mergeEnvironments(const HashMap<String, String>& baseEnv,
                            const HashMap<String, String>& overlayEnv,
                            bool override) -> HashMap<String, String> {
    LOG_F(INFO, "mergeEnvironments called with override: {}", override);

    HashMap<String, String> result = baseEnv;  // 从基础环境开始

    for (const auto& [key, val] : overlayEnv) {
        auto it = result.find(key);
        if (it == result.end() || override) {
            // 如果键不存在或者允许覆盖，则添加/覆盖
            result[key] = val;
        }
    }

    LOG_F(INFO,
          "mergeEnvironments: Created merged environment with {} variables",
          result.size());
    return result;
}

// 实现系统信息功能
auto Env::getSystemName() -> String {
    LOG_F(INFO, "getSystemName called");

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
    LOG_F(INFO, "getSystemArch called");

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
    LOG_F(INFO, "getCurrentUser called");

    String username;

#ifdef _WIN32
    DWORD size = 256;
    char buffer[256];
    if (GetUserNameA(buffer, &size)) {
        username = String(buffer);
    } else {
        LOG_F(ERROR, "getCurrentUser: GetUserNameA failed with error {}",
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

    LOG_F(INFO, "getCurrentUser returning: {}", username);
    return username;
}

auto Env::getHostName() -> String {
    LOG_F(INFO, "getHostName called");

    String hostname;

#ifdef _WIN32
    DWORD size = MAX_COMPUTERNAME_LENGTH + 1;
    char buffer[MAX_COMPUTERNAME_LENGTH + 1];
    if (GetComputerNameA(buffer, &size)) {
        hostname = String(buffer, size);
    } else {
        LOG_F(ERROR, "getHostName: GetComputerNameA failed with error {}",
              GetLastError());
        hostname = getEnv("COMPUTERNAME", "unknown");
    }
#else
    char buffer[HOST_NAME_MAX + 1];
    if (gethostname(buffer, sizeof(buffer)) == 0) {
        hostname = buffer;
    } else {
        LOG_F(ERROR, "getHostName: gethostname failed with error {}", errno);
        hostname = getEnv("HOSTNAME", "unknown");
    }
#endif

    LOG_F(INFO, "getHostName returning: {}", hostname);
    return hostname;
}

}  // namespace atom::utils
