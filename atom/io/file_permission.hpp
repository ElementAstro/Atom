/**
 * @file file_permission.hpp
 * @brief Defines utilities for managing and comparing file permissions.
 *
 * This file provides functions to get file permissions, get the current
 * process's permissions, compare them, and change file permissions. It
 * leverages C++20 concepts and modern C++ features for robust and type-safe
 * operations.
 */
#pragma once

#include <concepts>
#include <filesystem>
#include <optional>
#include <string_view>
#include <functional>
#include <future>
#include <chrono>
#include <unordered_map>
#include <mutex>
#include <vector>
#include <memory>

#include "atom/containers/high_performance.hpp"

namespace atom::io {

using atom::containers::String;
template <typename T>
using Vector = atom::containers::Vector<T>;

// Forward declarations
struct PermissionInfo;
struct PermissionOptions;
class PermissionCache;
class PermissionAnalyzer;

// Callback types
using PermissionCallback = std::function<void(const PermissionInfo&)>;
using PermissionErrorCallback = std::function<void(const String& error)>;
using ProgressCallback = std::function<void(size_t processed, size_t total, double percentage)>;

/**
 * @brief Concept for types that can be converted to a filesystem path
 * @tparam T The type to check for path conversion compatibility
 */
template <typename T>
concept PathLike = std::convertible_to<T, std::filesystem::path> ||
                   std::convertible_to<T, std::string_view>;

/**
 * @brief Enhanced permission information structure
 */
struct PermissionInfo {
    String filePath;                    ///< File path
    String permissionString;            ///< Permission string (rwxrwxrwx format)
    uint32_t octalPermissions{0};       ///< Octal representation (e.g., 0755)
    bool isReadable{false};             ///< Whether file is readable
    bool isWritable{false};             ///< Whether file is writable
    bool isExecutable{false};           ///< Whether file is executable

    // Enhanced metadata
    std::chrono::steady_clock::time_point retrievalTime; ///< When this info was retrieved
    std::chrono::milliseconds retrievalDuration{0};      ///< Time taken to retrieve info
    String owner;                       ///< File owner (if available)
    String group;                       ///< File group (if available)

    // Platform-specific information
#ifdef _WIN32
    String windowsAcl;                  ///< Windows ACL information
#else
    mode_t unixMode{0};                 ///< Unix file mode
    uid_t uid{0};                       ///< User ID
    gid_t gid{0};                       ///< Group ID
#endif

    /**
     * @brief Checks if the permission info is still valid (not expired)
     */
    bool isValid(std::chrono::milliseconds maxAge = std::chrono::milliseconds(5000)) const {
        auto now = std::chrono::steady_clock::now();
        return (now - retrievalTime) < maxAge;
    }

    /**
     * @brief Converts permission string to octal
     */
    uint32_t toOctal() const;

    /**
     * @brief Checks if has specific permission
     */
    bool hasPermission(char permission, int position) const;

    /**
     * @brief Gets human-readable permission description
     */
    String getDescription() const;
};

/**
 * @brief Configuration options for permission operations
 */
struct PermissionOptions {
    bool enableCaching{true};           ///< Enable result caching
    bool includeOwnership{false};       ///< Include owner/group information
    bool followSymlinks{true};          ///< Follow symbolic links
    std::chrono::milliseconds cacheMaxAge{5000}; ///< Cache expiration time
    bool enableStatistics{true};        ///< Enable performance statistics

    /**
     * @brief Creates options optimized for performance
     */
    static PermissionOptions createFastOptions() {
        PermissionOptions options;
        options.includeOwnership = false;
        options.enableCaching = true;
        return options;
    }

    /**
     * @brief Creates options for comprehensive information
     */
    static PermissionOptions createDetailedOptions() {
        PermissionOptions options;
        options.includeOwnership = true;
        options.enableCaching = true;
        options.enableStatistics = true;
        return options;
    }
};

/**
 * @brief Compare file permissions with current process permissions
 * @param filePath Path to the file for permission comparison
 * @return Optional boolean indicating comparison result:
 *         - true: process has equal or greater permissions than file
 *         - false: process has lesser permissions than file
 *         - nullopt: error occurred during comparison
 * @noexcept Does not throw exceptions; errors indicated by return value
 */
auto compareFileAndSelfPermissions(std::string_view filePath) noexcept
    -> std::optional<bool>;

/**
 * @brief Enhanced permission information retrieval
 * @param filePath Path to the file
 * @param options Options for permission retrieval
 * @return PermissionInfo structure containing detailed permission information
 */
PermissionInfo getPermissionInfo(const std::filesystem::path& filePath,
                                const PermissionOptions& options = {});

/**
 * @brief Asynchronous permission information retrieval
 * @param filePath Path to the file
 * @param callback Callback function to receive the result
 * @param errorCallback Error callback function
 * @param options Options for permission retrieval
 */
void getPermissionInfoAsync(const std::filesystem::path& filePath,
                           PermissionCallback callback,
                           PermissionErrorCallback errorCallback = nullptr,
                           const PermissionOptions& options = {});

/**
 * @brief Retrieve permission information for multiple files
 * @param filePaths Vector of file paths
 * @param options Options for permission retrieval
 * @return Vector of PermissionInfo structures
 */
Vector<PermissionInfo> getMultiplePermissionInfo(const Vector<std::filesystem::path>& filePaths,
                                                 const PermissionOptions& options = {});

/**
 * @brief Asynchronous multiple file permission retrieval
 * @param filePaths Vector of file paths
 * @param callback Callback for each processed file
 * @param progressCallback Progress callback
 * @param options Options for permission retrieval
 * @return Future that completes when all files are processed
 */
std::future<Vector<PermissionInfo>> getMultiplePermissionInfoAsync(
    const Vector<std::filesystem::path>& filePaths,
    PermissionCallback callback = nullptr,
    ProgressCallback progressCallback = nullptr,
    const PermissionOptions& options = {});

/**
 * @brief Template wrapper for comparing file and process permissions
 * @tparam T Type satisfying PathLike concept
 * @param filePath Path-like object representing the file path
 * @return Optional boolean as described in primary function
 * @noexcept Does not throw exceptions
 */
template <PathLike T>
auto compareFileAndSelfPermissions(const T &filePath) noexcept
    -> std::optional<bool> {
    return compareFileAndSelfPermissions(
        std::filesystem::path(filePath).string());
}

/**
 * @brief Retrieve file permissions as a readable string
 * @param filePath Path to the file
 * @return Permission string in format "rwxrwxrwx" or empty string on error
 * @noexcept Does not throw exceptions; errors indicated by empty return
 */
std::string getFilePermissions(std::string_view filePath) noexcept;

/**
 * @brief Retrieve current process permissions as a readable string
 * @return Permission string in format "rwxrwxrwx" or empty string on error
 * @noexcept Does not throw exceptions; errors indicated by empty return
 */
std::string getSelfPermissions() noexcept;

/**
 * @brief Modify file permissions using permission string
 * @param filePath Filesystem path to the target file
 * @param permissions Permission string in format "rwxrwxrwx"
 * @throws std::invalid_argument If permission string format is invalid
 * @throws std::runtime_error If file doesn't exist or permission change fails
 */
void changeFilePermissions(const std::filesystem::path &filePath,
                           const atom::containers::String &permissions);

/**
 * @brief Enhanced permission modification with options
 * @param filePath Filesystem path to the target file
 * @param permissions Permission string or octal value
 * @param options Options for permission modification
 */
void changeFilePermissionsEx(const std::filesystem::path& filePath,
                            const String& permissions,
                            const PermissionOptions& options = {});

/**
 * @brief Permission cache for performance optimization
 */
class PermissionCache {
public:
    static PermissionCache& getInstance();

    std::optional<PermissionInfo> get(const std::filesystem::path& path) const;
    void put(const std::filesystem::path& path, const PermissionInfo& info);
    void clear();
    void cleanup(); // Remove expired entries

    size_t size() const;
    size_t getHitCount() const;
    size_t getMissCount() const;
    void resetStats();

private:
    PermissionCache() = default;
    mutable std::mutex mutex_;
    std::unordered_map<String, PermissionInfo> cache_;
    mutable size_t hit_count_{0};
    mutable size_t miss_count_{0};
};

/**
 * @brief Permission analyzer for advanced operations
 */
class PermissionAnalyzer {
public:
    /**
     * @brief Analyzes permission differences between two files
     */
    static String comparePermissions(const PermissionInfo& info1, const PermissionInfo& info2);

    /**
     * @brief Suggests optimal permissions for a file type
     */
    static String suggestPermissions(const std::filesystem::path& filePath);

    /**
     * @brief Validates permission string format
     */
    static bool validatePermissionString(const String& permissions);

    /**
     * @brief Converts between permission formats
     */
    static String convertPermissionFormat(const String& input, const String& fromFormat, const String& toFormat);

    /**
     * @brief Checks if permissions are secure
     */
    static bool arePermissionsSecure(const PermissionInfo& info);
};

/**
 * @brief Utility functions for permission operations
 */
namespace utils {

/**
 * @brief Converts octal permissions to string format
 */
String octalToString(uint32_t octal);

/**
 * @brief Converts string permissions to octal format
 */
uint32_t stringToOctal(const String& permissions);

/**
 * @brief Checks if a permission string is valid
 */
bool isValidPermissionString(const String& permissions);

/**
 * @brief Gets default permissions for file type
 */
String getDefaultPermissions(const std::filesystem::path& filePath);

/**
 * @brief Formats permissions for display
 */
String formatPermissions(const PermissionInfo& info, const String& format = "detailed");

/**
 * @brief Gets optimal options for given use case
 */
PermissionOptions getOptimalOptions(const String& useCase = "balanced");

} // namespace utils

}  // namespace atom::io
