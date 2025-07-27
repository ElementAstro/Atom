#ifndef ATOM_SECRET_PASSWORD_MANAGER_HPP
#define ATOM_SECRET_PASSWORD_MANAGER_HPP

#include <memory>
#include <shared_mutex>
#include <string>
#include <vector>
#include <string_view>
#include <functional>
#include <regex>
#include <unordered_map>
#include <chrono>

#include "common.hpp"
#include "password_entry.hpp"
#include "result.hpp"

namespace atom::secret {

class SecureStorage;

/**
 * @brief Bulk operation for multiple password entries.
 */
struct BulkPasswordOperation {
    enum class Type { Add, Update, Remove };

    Type operation;
    PasswordEntry entry;  // Used for Add and Update operations
    std::string title;    // Used for Remove operations

    BulkPasswordOperation(Type op, PasswordEntry e)
        : operation(op), entry(std::move(e)) {}

    BulkPasswordOperation(Type op, std::string t)
        : operation(op), title(std::move(t)) {}
};

/**
 * @brief Password manager performance metrics.
 */
struct PasswordManagerMetrics {
    size_t totalEntries = 0;
    size_t totalOperations = 0;
    size_t successfulOperations = 0;
    size_t searchOperations = 0;
    std::chrono::milliseconds totalLatency{0};
    std::chrono::system_clock::time_point lastOperation;
    std::unordered_map<PasswordCategory, size_t> entriesByCategory;

    double getSuccessRate() const {
        return totalOperations > 0 ?
            static_cast<double>(successfulOperations) / totalOperations : 0.0;
    }

    double getAverageLatency() const {
        return totalOperations > 0 ?
            static_cast<double>(totalLatency.count()) / totalOperations : 0.0;
    }
};

/**
 * @brief Enhanced password manager with search, utilities, and bulk operations.
 * Provides a thread-safe interface for storing and retrieving secrets.
 */
class PasswordManager {
public:
    /**
     * @brief Constructs a PasswordManager.
     * @param appName The name of the application, used for namespacing secrets.
     * @param settings Optional password manager settings.
     */
    explicit PasswordManager(std::string_view appName,
                           const PasswordManagerSettings& settings = PasswordManagerSettings{});

    /**
     * @brief Adds a new password entry.
     * @param entry The PasswordEntry to add.
     * @param masterPassword The master password for encryption.
     * @return A Result indicating success or failure.
     */
    Result<void> addEntry(const PasswordEntry& entry,
                          std::string_view masterPassword);

    /**
     * @brief Retrieves a password entry by its title.
     * @param title The title of the entry to retrieve.
     * @param masterPassword The master password for decryption.
     * @return A Result containing the PasswordEntry or an error.
     */
    Result<PasswordEntry> getEntry(std::string_view title,
                                   std::string_view masterPassword) const;

    /**
     * @brief Updates an existing password entry.
     * @param entry The PasswordEntry to update.
     * @param masterPassword The master password for encryption.
     * @return A Result indicating success or failure.
     */
    Result<void> updateEntry(const PasswordEntry& entry,
                             std::string_view masterPassword);

    /**
     * @brief Removes a password entry by its title.
     * @param title The title of the entry to remove.
     * @return A Result indicating success or failure.
     */
    Result<void> removeEntry(std::string_view title);

    /**
     * @brief Retrieves all password entries.
     * @param masterPassword The master password for decryption.
     * @return A Result containing a vector of PasswordEntries or an error.
     */
    Result<std::vector<PasswordEntry>> getAllEntries(
        std::string_view masterPassword) const;

    // Enhanced search and filtering methods
    /**
     * @brief Searches for password entries based on search options.
     * @param options Search and filter criteria.
     * @param masterPassword The master password for decryption.
     * @return A Result containing matching PasswordEntries or an error.
     */
    Result<std::vector<PasswordEntry>> searchEntries(
        const SearchOptions& options,
        std::string_view masterPassword) const;

    /**
     * @brief Finds entries by category.
     * @param category The category to filter by.
     * @param masterPassword The master password for decryption.
     * @return A Result containing matching PasswordEntries or an error.
     */
    Result<std::vector<PasswordEntry>> getEntriesByCategory(
        PasswordCategory category,
        std::string_view masterPassword) const;

    /**
     * @brief Finds entries by tags.
     * @param tags Vector of tags to search for.
     * @param matchAll If true, entry must have all tags; if false, any tag matches.
     * @param masterPassword The master password for decryption.
     * @return A Result containing matching PasswordEntries or an error.
     */
    Result<std::vector<PasswordEntry>> getEntriesByTags(
        const std::vector<std::string>& tags,
        bool matchAll,
        std::string_view masterPassword) const;

    /**
     * @brief Gets entries that are expiring soon.
     * @param daysAhead Number of days ahead to check for expiration.
     * @param masterPassword The master password for decryption.
     * @return A Result containing expiring PasswordEntries or an error.
     */
    Result<std::vector<PasswordEntry>> getExpiringEntries(
        int daysAhead,
        std::string_view masterPassword) const;

    // Bulk operations
    /**
     * @brief Performs multiple operations in a batch.
     * @param operations Vector of bulk operations to perform.
     * @param masterPassword The master password for encryption/decryption.
     * @return A Result containing a vector of individual operation results.
     */
    Result<std::vector<Result<void>>> bulkOperation(
        const std::vector<BulkPasswordOperation>& operations,
        std::string_view masterPassword);

    /**
     * @brief Adds multiple entries in a batch.
     * @param entries Vector of entries to add.
     * @param masterPassword The master password for encryption.
     * @return A Result containing a vector of individual operation results.
     */
    Result<std::vector<Result<void>>> bulkAddEntries(
        const std::vector<PasswordEntry>& entries,
        std::string_view masterPassword);

    /**
     * @brief Removes multiple entries in a batch.
     * @param titles Vector of entry titles to remove.
     * @return A Result containing a vector of individual operation results.
     */
    Result<std::vector<Result<void>>> bulkRemoveEntries(
        const std::vector<std::string>& titles);

    // Password utilities
    /**
     * @brief Generates a secure password based on settings.
     * @param options Optional custom generation options.
     * @return A Result containing the generated password or an error.
     */
    Result<std::string> generatePassword(
        const PasswordGenerationOptions& options = PasswordGenerationOptions{}) const;

    /**
     * @brief Analyzes password strength.
     * @param password The password to analyze.
     * @return PasswordStrength enum value.
     */
    PasswordStrength analyzePasswordStrength(std::string_view password) const;

    /**
     * @brief Validates a password against current settings.
     * @param password The password to validate.
     * @return A Result indicating if the password is valid or error details.
     */
    Result<void> validatePassword(std::string_view password) const;

    /**
     * @brief Checks for duplicate passwords across entries.
     * @param masterPassword The master password for decryption.
     * @return A Result containing a map of passwords to entry titles that use them.
     */
    Result<std::unordered_map<std::string, std::vector<std::string>>>
        findDuplicatePasswords(std::string_view masterPassword) const;

    /**
     * @brief Finds weak passwords based on current settings.
     * @param masterPassword The master password for decryption.
     * @return A Result containing entries with weak passwords.
     */
    Result<std::vector<PasswordEntry>> findWeakPasswords(
        std::string_view masterPassword) const;

    // Import/Export functionality
    /**
     * @brief Exports all entries to a JSON string.
     * @param masterPassword The master password for decryption.
     * @param includePasswords Whether to include actual passwords in export.
     * @return A Result containing the JSON export string or an error.
     */
    Result<std::string> exportToJson(
        std::string_view masterPassword,
        bool includePasswords = false) const;

    /**
     * @brief Imports entries from a JSON string.
     * @param jsonData The JSON data to import.
     * @param masterPassword The master password for encryption.
     * @param overwriteExisting Whether to overwrite existing entries with same titles.
     * @return A Result indicating success or failure with import statistics.
     */
    Result<std::string> importFromJson(
        std::string_view jsonData,
        std::string_view masterPassword,
        bool overwriteExisting = false);

    // Settings and metrics
    /**
     * @brief Updates password manager settings.
     * @param newSettings The new settings to apply.
     */
    void updateSettings(const PasswordManagerSettings& newSettings);

    /**
     * @brief Gets current password manager settings.
     * @return Current PasswordManagerSettings.
     */
    PasswordManagerSettings getSettings() const;

    /**
     * @brief Gets performance metrics.
     * @return PasswordManagerMetrics structure with performance data.
     */
    PasswordManagerMetrics getMetrics() const;

    /**
     * @brief Clears all cached data and resets metrics.
     */
    void clearCache();

private:
    Result<PasswordEntry> getEntry_nolock(std::string_view title, std::string_view masterPassword) const;
    bool matchesSearchCriteria(const PasswordEntry& entry, const SearchOptions& options) const;
    void updateMetrics(bool success, std::chrono::milliseconds latency);
    std::string sanitizeForExport(const PasswordEntry& entry, bool includePasswords) const;

    std::unique_ptr<SecureStorage> storage_;
    mutable std::shared_mutex mutex_;
    PasswordManagerSettings settings_;
    std::string appName_;
    mutable PasswordManagerMetrics metrics_;
};

}  // namespace atom::secret

#endif  // ATOM_SECRET_PASSWORD_MANAGER_HPP
