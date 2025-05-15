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
#include <string_view>  // Keep for Environ parsing

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>  // For readlink on Linux/macOS
extern char** environ;  // POSIX environment variables
#endif

#include "atom/log/loguru.hpp"
// #include "atom/utils/argsview.hpp" // Assuming ArgumentParser is not used or
// adapted separately

namespace fs = std::filesystem;

namespace atom::utils {

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
#ifdef _WIN32
    // Use _putenv_s for safety if available and preferred
    // bool result = _putenv_s(key, val) == 0;
    // Or stick to SetEnvironmentVariableA if String guarantees null termination
    bool result = SetEnvironmentVariableA(key.c_str(), val.c_str()) != 0;
#else
    bool result = ::setenv(key.c_str(), val.c_str(), 1) == 0;
#endif
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

}  // namespace atom::utils
