#ifndef ATOM_IO_FILE_INFO_HPP
#define ATOM_IO_FILE_INFO_HPP

#include <filesystem>
#include <functional>
#include <future>
#include <optional>
#include <chrono>
#include <unordered_map>
#include <mutex>
#include <vector>
#include <memory>

#include "atom/containers/high_performance.hpp"
#include "atom/macro.hpp"

namespace atom::io {
namespace fs = std::filesystem;

using atom::containers::String;
template <typename T>
using Vector = atom::containers::Vector<T>;

// Forward declarations
struct FileInfoOptions;
class FileInfoCache;
class FileInfoFormatter;

// Callback types
using FileInfoCallback = std::function<void(const struct FileInfo&)>;
using FileInfoErrorCallback = std::function<void(const String& error)>;
using ProgressCallback = std::function<void(size_t processed, size_t total, double percentage)>;

/**
 * @brief Enhanced structure to store detailed file information.
 */
struct FileInfo {
    String filePath;          ///< Absolute path of the file.
    String fileName;          ///< Name of the file.
    String extension;         ///< File extension.
    std::uintmax_t fileSize;  ///< Size of the file in bytes.
    String fileType;      ///< Type of the file (e.g., Regular file, Directory).
    String creationTime;  ///< Creation timestamp.
    String lastModifiedTime;  ///< Last modification timestamp.
    String lastAccessTime;    ///< Last access timestamp.
    String permissions;       ///< File permissions (e.g., rwxr-xr-x).
    bool isHidden;            ///< Indicates if the file is hidden.

    // Enhanced metadata
    String mimeType;          ///< MIME type of the file
    String checksum;          ///< File checksum (optional)
    std::uintmax_t inodeNumber; ///< Inode number (Unix) or file index (Windows)
    std::uintmax_t hardLinkCount; ///< Number of hard links
    bool isExecutable;        ///< Whether the file is executable
    bool isReadable;          ///< Whether the file is readable
    bool isWritable;          ///< Whether the file is writable

    // Performance metadata
    std::chrono::steady_clock::time_point retrievalTime; ///< When this info was retrieved
    std::chrono::milliseconds retrievalDuration{0};      ///< Time taken to retrieve info

    // Platform-specific information
#ifdef _WIN32
    String owner;             ///< Owner of the file (Windows only).
    String attributes;        ///< Windows file attributes
    std::uintmax_t fileIndex; ///< Windows file index
#else
    String owner;             ///< Owner of the file (Linux only).
    String group;             ///< Group of the file (Linux only).
    String symlinkTarget;     ///< Target of the symbolic link, if applicable.
    mode_t mode;              ///< Unix file mode
    uid_t uid;                ///< User ID
    gid_t gid;                ///< Group ID
#endif

    /**
     * @brief Checks if the file info is still valid (not expired)
     */
    bool isValid(std::chrono::milliseconds maxAge = std::chrono::milliseconds(5000)) const {
        auto now = std::chrono::steady_clock::now();
        return (now - retrievalTime) < maxAge;
    }

    /**
     * @brief Gets a human-readable file size string
     */
    String getFormattedSize() const;

    /**
     * @brief Gets file age since last modification
     */
    std::chrono::seconds getAge() const;

    /**
     * @brief Checks if file has specific permission
     */
    bool hasPermission(char permission, int position) const;

} ATOM_ALIGNAS(128);

/**
 * @brief Configuration options for file information retrieval
 */
struct FileInfoOptions {
    bool includeChecksum{false};        ///< Calculate file checksum
    bool includeMimeType{true};         ///< Detect MIME type
    bool includeExtendedAttributes{false}; ///< Include extended attributes
    bool enableCaching{true};           ///< Enable result caching
    bool followSymlinks{true};          ///< Follow symbolic links
    std::chrono::milliseconds cacheMaxAge{5000}; ///< Cache expiration time
    String checksumAlgorithm{"md5"};    ///< Checksum algorithm (md5, sha1, sha256)

    /**
     * @brief Creates options optimized for performance
     */
    static FileInfoOptions createFastOptions() {
        FileInfoOptions options;
        options.includeChecksum = false;
        options.includeMimeType = false;
        options.includeExtendedAttributes = false;
        options.enableCaching = true;
        return options;
    }

    /**
     * @brief Creates options for comprehensive information
     */
    static FileInfoOptions createDetailedOptions() {
        FileInfoOptions options;
        options.includeChecksum = true;
        options.includeMimeType = true;
        options.includeExtendedAttributes = true;
        options.enableCaching = true;
        options.checksumAlgorithm = "sha256";
        return options;
    }
};

/**
 * @brief Retrieves detailed information about a file.
 *
 * @param filePath The path to the file.
 * @param options Options for information retrieval
 * @return FileInfo structure containing the file's information.
 * @throws std::runtime_error if the file does not exist or cannot be accessed.
 */
FileInfo getFileInfo(const fs::path& filePath, const FileInfoOptions& options = {});

/**
 * @brief Retrieves file information asynchronously
 *
 * @param filePath The path to the file
 * @param callback Callback function to receive the result
 * @param errorCallback Error callback function
 * @param options Options for information retrieval
 */
void getFileInfoAsync(const fs::path& filePath,
                     FileInfoCallback callback,
                     FileInfoErrorCallback errorCallback = nullptr,
                     const FileInfoOptions& options = {});

/**
 * @brief Retrieves information for multiple files
 *
 * @param filePaths Vector of file paths
 * @param options Options for information retrieval
 * @return Vector of FileInfo structures
 */
Vector<FileInfo> getMultipleFileInfo(const Vector<fs::path>& filePaths,
                                    const FileInfoOptions& options = {});

/**
 * @brief Retrieves information for multiple files asynchronously
 *
 * @param filePaths Vector of file paths
 * @param callback Callback for each processed file
 * @param progressCallback Progress callback
 * @param options Options for information retrieval
 * @return Future that completes when all files are processed
 */
std::future<Vector<FileInfo>> getMultipleFileInfoAsync(
    const Vector<fs::path>& filePaths,
    FileInfoCallback callback = nullptr,
    ProgressCallback progressCallback = nullptr,
    const FileInfoOptions& options = {});

/**
 * @brief Prints the file information to the console.
 *
 * @param info The FileInfo structure containing file details.
 */
void printFileInfo(const FileInfo& info);

/**
 * @brief File information cache for performance optimization
 */
class FileInfoCache {
public:
    static FileInfoCache& getInstance();

    std::optional<FileInfo> get(const fs::path& path) const;
    void put(const fs::path& path, const FileInfo& info);
    void clear();
    void cleanup(); // Remove expired entries

    size_t size() const;
    size_t getHitCount() const;
    size_t getMissCount() const;
    void resetStats();

private:
    FileInfoCache() = default;
    mutable std::mutex mutex_;
    std::unordered_map<String, FileInfo> cache_;
    mutable size_t hit_count_{0};
    mutable size_t miss_count_{0};
};

/**
 * @brief File information formatter for different output formats
 */
class FileInfoFormatter {
public:
    enum class Format {
        CONSOLE,
        JSON,
        XML,
        CSV,
        MARKDOWN
    };

    static String format(const FileInfo& info, Format format = Format::CONSOLE);
    static String formatMultiple(const Vector<FileInfo>& infos, Format format = Format::CONSOLE);

private:
    static String formatConsole(const FileInfo& info);
    static String formatJSON(const FileInfo& info);
    static String formatXML(const FileInfo& info);
    static String formatCSV(const FileInfo& info);
    static String formatMarkdown(const FileInfo& info);
};

/**
 * @brief MIME type detector utility
 */
class MimeTypeDetector {
public:
    static String detectMimeType(const fs::path& filePath);
    static String detectMimeTypeFromExtension(const String& extension);
    static String detectMimeTypeFromContent(const fs::path& filePath);

private:
    static std::unordered_map<String, String> getExtensionMimeMap();
};

/**
 * @brief File checksum calculator
 */
class FileChecksumCalculator {
public:
    enum class Algorithm {
        MD5,
        SHA1,
        SHA256,
        CRC32
    };

    static String calculateChecksum(const fs::path& filePath, Algorithm algorithm = Algorithm::MD5);
    static String calculateChecksumAsync(const fs::path& filePath, Algorithm algorithm = Algorithm::MD5);

private:
    static Algorithm parseAlgorithm(const String& algorithmName);
};

/**
 * @brief Utility functions for file information operations
 */
namespace utils {

/**
 * @brief Formats file size in human-readable format
 */
String formatFileSize(std::uintmax_t bytes);

/**
 * @brief Converts file permissions to human-readable string
 */
String formatPermissions(const fs::perms& permissions);

/**
 * @brief Gets file type description
 */
String getFileTypeDescription(const fs::path& filePath);

/**
 * @brief Checks if a file is a text file
 */
bool isTextFile(const fs::path& filePath);

/**
 * @brief Checks if a file is a binary file
 */
bool isBinaryFile(const fs::path& filePath);

/**
 * @brief Gets optimal options for given use case
 */
FileInfoOptions getOptimalOptions(const String& useCase = "balanced");

} // namespace utils

}  // namespace atom::io

#endif  // ATOM_IO_FILE_INFO_HPP
