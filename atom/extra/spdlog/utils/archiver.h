#pragma once

#include <chrono>
#include <filesystem>
#include <string>
#include <vector>

namespace modern_log {

/**
 * @class LogArchiver
 * @brief Log archive manager for managing the lifecycle of log files.
 *
 * This class provides functionality for archiving, compressing, and cleaning up
 * log files according to configurable policies. It supports file retention
 * based on age, count, and total size, as well as compression and decompression
 * of log files. The archiver can be used to automate log rotation and storage
 * management in logging systems.
 */
class LogArchiver {
public:
    /**
     * @struct ArchiveConfig
     * @brief Configuration options for log archiving.
     *
     * Specifies retention policies, compression settings, and naming patterns
     * for archived files.
     */
    struct ArchiveConfig {
        std::chrono::seconds max_age{std::chrono::hours(
            24 * 7)};  ///< Maximum age for log files (default: 7 days).
        size_t max_files = 100;  ///< Maximum number of log files to retain.
        size_t max_total_size =
            1024 * 1024 *
            1024;  ///< Maximum total size of log files (default: 1GB).
        bool compress = true;  ///< Whether to compress archived files.
        std::string archive_pattern =
            "{name}_{date}.log";  ///< Pattern for naming archived files.
        std::string compress_format =
            "gzip";  ///< Compression format ("gzip", "zip", "lz4").

        ArchiveConfig()
            : max_age(std::chrono::hours(24 * 7)),
              max_files(100),
              max_total_size(1024 * 1024 * 1024),
              compress(true),
              archive_pattern("{name}_{date}.log"),
              compress_format("gzip") {}
        ArchiveConfig(std::chrono::seconds age, size_t files, size_t total_size,
                      bool compress = true,
                      std::string pattern = "{name}_{date}.log",
                      std::string format = "gzip")
            : max_age(age),
              max_files(files),
              max_total_size(total_size),
              compress(compress),
              archive_pattern(std::move(pattern)),
              compress_format(std::move(format)) {}
    };

private:
    std::filesystem::path log_dir_;  ///< Directory containing log files.
    ArchiveConfig config_;           ///< Current archive configuration.

public:
    /**
     * @brief Construct a LogArchiver for a given log directory and
     * configuration.
     * @param log_dir Path to the directory containing log files.
     * @param config Archive configuration options (optional).
     */
    explicit LogArchiver(std::filesystem::path log_dir,
                         ArchiveConfig config = {});

    /**
     * @brief Perform the archiving operation.
     *
     * Archives old log files according to the configured retention and
     * compression policies. This may include compressing, renaming, or deleting
     * files as needed.
     */
    void archive_old_files();

    /**
     * @brief Compress a specified log file.
     * @param file Path to the file to compress.
     * @return True if compression succeeded, false otherwise.
     */
    bool compress_file(const std::filesystem::path& file);

    /**
     * @brief Decompress a specified log file.
     * @param file Path to the file to decompress.
     * @return True if decompression succeeded, false otherwise.
     */
    bool decompress_file(const std::filesystem::path& file);

    /**
     * @brief Get the total size of the log directory in bytes.
     * @return Total size of all files in the directory.
     */
    size_t get_directory_size() const;

    /**
     * @brief Clean up files exceeding retention limits.
     *
     * Removes files if the number of files or total size exceeds the configured
     * limits.
     */
    void cleanup_excess_files();

    /**
     * @brief Get a list of files eligible for archiving.
     * @return Vector of paths to archivable files.
     */
    std::vector<std::filesystem::path> get_archivable_files() const;

    /**
     * @brief Set the archive configuration.
     * @param config The new archive configuration to apply.
     */
    void set_config(const ArchiveConfig& config);

    /**
     * @struct ArchiveStats
     * @brief Statistics about archiving operations.
     *
     * Contains information about the number of files, archived and compressed
     * files, total size, and the time of the last archive operation.
     */
    struct ArchiveStats {
        size_t total_files;       ///< Total number of files in the directory.
        size_t archived_files;    ///< Number of files archived.
        size_t compressed_files;  ///< Number of files compressed.
        size_t total_size;        ///< Total size of files in bytes.
        std::chrono::system_clock::time_point
            last_archive_time;  ///< Time of the last archive operation.
    };

    /**
     * @brief Get statistics about the archiving process.
     * @return ArchiveStats structure with current statistics.
     */
    ArchiveStats get_stats() const;

private:
    /**
     * @brief Check if a file is considered old according to the archive policy.
     * @param file Path to the file to check.
     * @return True if the file is old and eligible for archiving, false
     * otherwise.
     */
    bool is_file_old(const std::filesystem::path& file) const;

    /**
     * @brief Generate an archive file name based on the original file and
     * archive pattern.
     * @param original Path to the original file.
     * @return Generated archive file name as a string.
     */
    std::string generate_archive_name(
        const std::filesystem::path& original) const;
};

}  // namespace modern_log
