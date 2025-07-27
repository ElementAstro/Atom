#ifndef CRON_STORAGE_HPP
#define CRON_STORAGE_HPP

#include <string>
#include <vector>
#include <chrono>
#include <optional>
#include "cron_job.hpp"

/**
 * @brief Storage format enumeration
 */
enum class StorageFormat {
    JSON = 0,
    BINARY = 1,
    COMPRESSED_JSON = 2,
    COMPRESSED_BINARY = 3,
    DATABASE = 4
};

/**
 * @brief Storage configuration options
 */
struct StorageConfig {
    StorageFormat format = StorageFormat::JSON;
    bool enable_compression = false;
    bool enable_encryption = false;
    bool enable_backup = true;
    size_t max_backup_files = 5;
    std::chrono::minutes backup_interval{60};
    std::string encryption_key;
    std::string database_connection_string;

    StorageConfig() = default;
};

/**
 * @brief Backup metadata information
 */
struct BackupInfo {
    std::string filename;
    std::chrono::system_clock::time_point timestamp;
    size_t file_size;
    size_t job_count;
    std::string checksum;

    BackupInfo(std::string fname, std::chrono::system_clock::time_point ts,
               size_t size, size_t count, std::string hash)
        : filename(std::move(fname)), timestamp(ts), file_size(size),
          job_count(count), checksum(std::move(hash)) {}
};

/**
 * @brief Storage statistics for monitoring
 */
struct StorageStats {
    size_t total_saves = 0;
    size_t total_loads = 0;
    size_t failed_saves = 0;
    size_t failed_loads = 0;
    std::chrono::milliseconds avg_save_time{0};
    std::chrono::milliseconds avg_load_time{0};
    size_t total_bytes_written = 0;
    size_t total_bytes_read = 0;

    double getSuccessRate() const {
        size_t total_ops = total_saves + total_loads;
        size_t failed_ops = failed_saves + failed_loads;
        return total_ops > 0 ? 1.0 - (static_cast<double>(failed_ops) / total_ops) : 0.0;
    }
};

/**
 * @brief Advanced cron job storage with multiple persistence formats and features.
 *
 * Features:
 * - Multiple storage formats (JSON, Binary, Compressed)
 * - Automatic backup and restore capabilities
 * - Incremental saves for large datasets
 * - Database support for enterprise environments
 * - Encryption support for sensitive data
 * - Compression for space efficiency
 * - Performance monitoring and statistics
 */
class CronStorage {
public:
    /**
     * @brief Sets the global storage configuration.
     * @param config Storage configuration options.
     */
    static void setConfig(const StorageConfig& config);

    /**
     * @brief Gets the current storage configuration.
     * @return Current storage configuration.
     */
    static auto getConfig() -> const StorageConfig&;

    // Legacy JSON methods (maintained for compatibility)
    static auto exportToJSON(const std::vector<CronJob>& jobs,
                            const std::string& filename) -> bool;
    static auto importFromJSON(const std::string& filename) -> std::vector<CronJob>;

    // Enhanced storage methods
    /**
     * @brief Saves jobs using the configured storage format.
     * @param jobs Vector of jobs to save.
     * @param filename Base filename (extension added based on format).
     * @return True if successful, false otherwise.
     */
    static auto saveJobs(const std::vector<CronJob>& jobs,
                        const std::string& filename) -> bool;

    /**
     * @brief Loads jobs using auto-detection of storage format.
     * @param filename Filename to load from.
     * @return Vector of loaded jobs, empty if failed.
     */
    static auto loadJobs(const std::string& filename) -> std::vector<CronJob>;

    /**
     * @brief Performs incremental save (only changed jobs).
     * @param jobs Current jobs.
     * @param filename Base filename.
     * @param last_save_time Time of last save for comparison.
     * @return True if successful, false otherwise.
     */
    static auto saveJobsIncremental(const std::vector<CronJob>& jobs,
                                   const std::string& filename,
                                   std::chrono::system_clock::time_point last_save_time) -> bool;

    // Backup and restore functionality
    /**
     * @brief Creates a backup of the current job data.
     * @param jobs Jobs to backup.
     * @param backup_dir Directory to store backups.
     * @return Backup info if successful, nullopt otherwise.
     */
    static auto createBackup(const std::vector<CronJob>& jobs,
                            const std::string& backup_dir) -> std::optional<BackupInfo>;

    /**
     * @brief Restores jobs from a backup.
     * @param backup_info Backup information.
     * @return Vector of restored jobs, empty if failed.
     */
    static auto restoreFromBackup(const BackupInfo& backup_info) -> std::vector<CronJob>;

    /**
     * @brief Lists available backups in a directory.
     * @param backup_dir Directory to scan for backups.
     * @return Vector of backup information, sorted by timestamp.
     */
    static auto listBackups(const std::string& backup_dir) -> std::vector<BackupInfo>;

    /**
     * @brief Cleans up old backups based on configuration.
     * @param backup_dir Directory containing backups.
     * @return Number of backups removed.
     */
    static auto cleanupOldBackups(const std::string& backup_dir) -> size_t;

    // Database operations (when database storage is enabled)
    /**
     * @brief Initializes database storage.
     * @param connection_string Database connection string.
     * @return True if successful, false otherwise.
     */
    static auto initializeDatabase(const std::string& connection_string) -> bool;

    /**
     * @brief Saves jobs to database.
     * @param jobs Jobs to save.
     * @return True if successful, false otherwise.
     */
    static auto saveJobsToDatabase(const std::vector<CronJob>& jobs) -> bool;

    /**
     * @brief Loads jobs from database.
     * @return Vector of loaded jobs, empty if failed.
     */
    static auto loadJobsFromDatabase() -> std::vector<CronJob>;

    // Utility and monitoring methods
    /**
     * @brief Gets storage statistics.
     * @return Current storage statistics.
     */
    static auto getStats() -> const StorageStats&;

    /**
     * @brief Resets storage statistics.
     */
    static void resetStats();

    /**
     * @brief Validates file integrity using checksums.
     * @param filename File to validate.
     * @return True if file is valid, false otherwise.
     */
    static auto validateFileIntegrity(const std::string& filename) -> bool;

    /**
     * @brief Compresses data using configured compression algorithm.
     * @param data Data to compress.
     * @return Compressed data, empty if failed.
     */
    static auto compressData(const std::vector<uint8_t>& data) -> std::vector<uint8_t>;

    /**
     * @brief Decompresses data.
     * @param compressed_data Compressed data to decompress.
     * @return Decompressed data, empty if failed.
     */
    static auto decompressData(const std::vector<uint8_t>& compressed_data) -> std::vector<uint8_t>;

private:
    static StorageConfig config_;
    static StorageStats stats_;

    // Internal helper methods
    static auto detectStorageFormat(const std::string& filename) -> StorageFormat;
    static auto generateChecksum(const std::string& filename) -> std::string;
    static auto getBackupFilename(const std::string& backup_dir) -> std::string;
    static auto serializeToBinary(const std::vector<CronJob>& jobs) -> std::vector<uint8_t>;
    static auto deserializeFromBinary(const std::vector<uint8_t>& data) -> std::vector<CronJob>;
    static void updateStats(bool is_save, bool success, std::chrono::milliseconds duration, size_t bytes);
};

#endif // CRON_STORAGE_HPP
