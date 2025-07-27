#ifndef ATOM_SECRET_STORAGE_HPP
#define ATOM_SECRET_STORAGE_HPP

#include <memory>
#include <shared_mutex>
#include <string>
#include <string_view>
#include <vector>
#include <unordered_map>
#include <chrono>
#include <functional>

#include "result.hpp"

namespace atom::secret {

/**
 * @brief Storage operation result with detailed error information.
 */
using StorageResult = Result<void>;

/**
 * @brief Batch operation for multiple storage operations.
 */
struct BatchOperation {
    enum class Type { Store, Remove };

    Type operation;
    std::string key;
    std::string data;  // Only used for Store operations

    BatchOperation(Type op, std::string k, std::string d = "")
        : operation(op), key(std::move(k)), data(std::move(d)) {}
};

/**
 * @brief Storage configuration options.
 */
struct StorageOptions {
    bool enableCaching = true;              ///< Enable in-memory caching
    size_t maxCacheSize = 100;              ///< Maximum number of cached entries
    std::chrono::minutes cacheExpiry{30};   ///< Cache entry expiry time
    bool enableCompression = false;         ///< Enable data compression
    bool enableBackup = false;              ///< Enable automatic backups
    size_t maxBackupFiles = 5;              ///< Maximum backup files to keep
    std::chrono::hours backupInterval{24};  ///< Backup interval
    bool enableMetrics = true;              ///< Enable performance metrics
};

/**
 * @brief Storage performance metrics.
 */
struct StorageMetrics {
    size_t totalOperations = 0;
    size_t successfulOperations = 0;
    size_t cacheHits = 0;
    size_t cacheMisses = 0;
    std::chrono::milliseconds totalLatency{0};
    std::chrono::system_clock::time_point lastOperation;

    double getSuccessRate() const {
        return totalOperations > 0 ?
            static_cast<double>(successfulOperations) / totalOperations : 0.0;
    }

    double getCacheHitRate() const {
        size_t totalCacheOps = cacheHits + cacheMisses;
        return totalCacheOps > 0 ?
            static_cast<double>(cacheHits) / totalCacheOps : 0.0;
    }

    double getAverageLatency() const {
        return totalOperations > 0 ?
            static_cast<double>(totalLatency.count()) / totalOperations : 0.0;
    }
};

/**
 * @brief Interface for platform-specific secure storage with enhanced features.
 * This class is thread-safe and includes caching, batch operations, and compression.
 */
class SecureStorage {
public:
    virtual ~SecureStorage() = default;

    /**
     * @brief Stores encrypted data in the platform's secure storage.
     * @param key The key or identifier for the data.
     * @param data The encrypted data to store.
     * @return StorageResult indicating success or failure with error details.
     */
    virtual StorageResult store(std::string_view key, std::string_view data) = 0;

    /**
     * @brief Retrieves encrypted data from the platform's secure storage.
     * @param key The key or identifier for the data.
     * @return Result containing the retrieved data or error.
     */
    virtual Result<std::string> retrieve(std::string_view key) = 0;

    /**
     * @brief Deletes data from the platform's secure storage.
     * @param key The key or identifier for the data to delete.
     * @return StorageResult indicating success or failure with error details.
     */
    virtual StorageResult remove(std::string_view key) = 0;

    /**
     * @brief Gets all keys/identifiers stored in the platform's secure storage.
     * @return Result containing a vector of key strings or error.
     */
    virtual Result<std::vector<std::string>> getAllKeys() = 0;

    /**
     * @brief Performs multiple storage operations in a batch.
     * @param operations Vector of batch operations to perform.
     * @return Result containing a vector of individual operation results.
     */
    virtual Result<std::vector<StorageResult>> batchOperation(
        const std::vector<BatchOperation>& operations) = 0;

    /**
     * @brief Checks if a key exists in storage.
     * @param key The key to check.
     * @return Result containing true if key exists, false otherwise.
     */
    virtual Result<bool> exists(std::string_view key) = 0;

    /**
     * @brief Gets the size of stored data for a key.
     * @param key The key to check.
     * @return Result containing the size in bytes or error.
     */
    virtual Result<size_t> getSize(std::string_view key) = 0;

    /**
     * @brief Clears all cached data (if caching is enabled).
     */
    virtual void clearCache() = 0;

    /**
     * @brief Gets current storage metrics.
     * @return StorageMetrics structure with performance data.
     */
    virtual StorageMetrics getMetrics() const = 0;

    /**
     * @brief Configures storage options.
     * @param options The storage options to apply.
     */
    virtual void configure(const StorageOptions& options) = 0;

    /**
     * @brief Creates a backup of all stored data.
     * @param backupPath Optional custom backup path.
     * @return StorageResult indicating success or failure.
     */
    virtual StorageResult createBackup(const std::string& backupPath = "") = 0;

    /**
     * @brief Restores data from a backup.
     * @param backupPath Path to the backup file.
     * @return StorageResult indicating success or failure.
     */
    virtual StorageResult restoreFromBackup(const std::string& backupPath) = 0;

    /**
     * @brief Creates and returns a platform-appropriate instance of SecureStorage.
     * @param appName The application name for storage categorization.
     * @param options Storage configuration options.
     * @return A unique_ptr to a SecureStorage instance.
     */
    static std::unique_ptr<SecureStorage> create(
        std::string_view appName,
        const StorageOptions& options = StorageOptions{});

protected:
    mutable std::shared_mutex mutex_;
};

/**
 * @brief Enhanced storage implementation with caching and compression.
 */
class EnhancedSecureStorage : public SecureStorage {
public:
    explicit EnhancedSecureStorage(
        std::unique_ptr<SecureStorage> backend,
        const StorageOptions& options = StorageOptions{});

    // SecureStorage interface implementation
    StorageResult store(std::string_view key, std::string_view data) override;
    Result<std::string> retrieve(std::string_view key) override;
    StorageResult remove(std::string_view key) override;
    Result<std::vector<std::string>> getAllKeys() override;
    Result<std::vector<StorageResult>> batchOperation(
        const std::vector<BatchOperation>& operations) override;
    Result<bool> exists(std::string_view key) override;
    Result<size_t> getSize(std::string_view key) override;
    void clearCache() override;
    StorageMetrics getMetrics() const override;
    void configure(const StorageOptions& options) override;
    StorageResult createBackup(const std::string& backupPath = "") override;
    StorageResult restoreFromBackup(const std::string& backupPath) override;

private:
    struct CacheEntry {
        std::string data;
        std::chrono::system_clock::time_point timestamp;
        size_t accessCount = 0;

        bool isExpired(std::chrono::minutes expiry) const {
            auto now = std::chrono::system_clock::now();
            return (now - timestamp) > expiry;
        }
    };

    // Helper methods
    std::string compressData(std::string_view data) const;
    std::string decompressData(std::string_view data) const;
    void updateMetrics(bool success, std::chrono::milliseconds latency);
    void evictExpiredCacheEntries();
    void evictLRUCacheEntry();
    std::string generateBackupPath() const;

    std::unique_ptr<SecureStorage> backend_;
    StorageOptions options_;
    mutable std::unordered_map<std::string, CacheEntry> cache_;
    mutable StorageMetrics metrics_;
    std::string appName_;
};

}  // namespace atom::secret

#endif  // ATOM_SECRET_STORAGE_HPP
