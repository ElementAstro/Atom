#ifndef ATOM_SECRET_MANAGER_PASSWORD_MANAGER_HPP
#define ATOM_SECRET_MANAGER_PASSWORD_MANAGER_HPP

#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "../core/result.hpp"
#include "../crypto/encryption.hpp"
#include "../password/entry.hpp"
#include "../storage/storage.hpp"
#include "audit_log.hpp"
#include "session.hpp"

namespace atom::secret {

/**
 * @brief Password manager settings.
 */
struct PasswordManagerSettings {
    EncryptionParams encryption = EncryptionParams::defaults();
    SessionConfig session = SessionConfig::defaults();
    bool enableAuditLog = true;
    bool autoBackup = true;
    int maxPasswordHistory = 10;
    int passwordExpiryDays = 90;  ///< 0 = no expiry

    static PasswordManagerSettings defaults() {
        return PasswordManagerSettings{};
    }
};

/**
 * @brief Search filter for password entries.
 */
struct SearchFilter {
    std::string query;  ///< Search query (matches title, username, url, notes)
    std::vector<PasswordCategory> categories;  ///< Filter by categories
    std::vector<std::string> tags;             ///< Filter by tags
    bool favoritesOnly = false;                ///< Only show favorites
    bool excludeArchived = true;               ///< Exclude archived entries
    bool expiredOnly = false;                  ///< Only show expired entries
    bool expiringSoonDays = 0;  ///< Show entries expiring within N days

    static SearchFilter all() {
        SearchFilter f;
        f.excludeArchived = false;
        return f;
    }
};

/**
 * @brief Main password manager class.
 *
 * Provides high-level password management functionality including:
 * - Secure storage of password entries
 * - Master password protection
 * - Search and filtering
 * - Import/export
 * - Session management
 * - Audit logging
 */
class PasswordManager {
public:
    /**
     * @brief Constructs a new PasswordManager.
     * @param storage Secure storage backend.
     * @param settings Manager settings.
     */
    explicit PasswordManager(std::unique_ptr<SecureStorage> storage,
                             const PasswordManagerSettings& settings =
                                 PasswordManagerSettings::defaults());

    ~PasswordManager();

    // Disable copy
    PasswordManager(const PasswordManager&) = delete;
    PasswordManager& operator=(const PasswordManager&) = delete;

    // ========================================================================
    // Initialization and Locking
    // ========================================================================

    /**
     * @brief Initializes a new password database with a master password.
     * @param masterPassword Master password for the database.
     * @return Result indicating success or error.
     */
    Result<void> initialize(std::string_view masterPassword);

    /**
     * @brief Unlocks an existing password database.
     * @param masterPassword Master password.
     * @return Result indicating success or error.
     */
    Result<void> unlock(std::string_view masterPassword);

    /**
     * @brief Locks the password database.
     */
    void lock();

    /**
     * @brief Checks if the database is unlocked.
     * @return True if unlocked.
     */
    bool isUnlocked() const;

    /**
     * @brief Changes the master password.
     * @param currentPassword Current master password.
     * @param newPassword New master password.
     * @return Result indicating success or error.
     */
    Result<void> changeMasterPassword(std::string_view currentPassword,
                                      std::string_view newPassword);

    // ========================================================================
    // Entry Management
    // ========================================================================

    /**
     * @brief Adds a new password entry.
     * @param entry Entry to add.
     * @return Result containing the entry ID or error.
     */
    Result<std::string> addEntry(const PasswordEntry& entry);

    /**
     * @brief Updates an existing password entry.
     * @param entry Entry with updated data.
     * @return Result indicating success or error.
     */
    Result<void> updateEntry(const PasswordEntry& entry);

    /**
     * @brief Deletes a password entry.
     * @param entryId ID of entry to delete.
     * @return Result indicating success or error.
     */
    Result<void> deleteEntry(const std::string& entryId);

    /**
     * @brief Gets a password entry by ID.
     * @param entryId Entry ID.
     * @return Result containing the entry or error.
     */
    Result<PasswordEntry> getEntry(const std::string& entryId);

    /**
     * @brief Gets all password entries.
     * @return Result containing all entries or error.
     */
    Result<std::vector<PasswordEntry>> getAllEntries();

    /**
     * @brief Searches for entries matching a filter.
     * @param filter Search filter.
     * @return Result containing matching entries or error.
     */
    Result<std::vector<PasswordEntry>> search(const SearchFilter& filter);

    /**
     * @brief Gets the total number of entries.
     * @return Entry count.
     */
    size_t getEntryCount() const;

    // ========================================================================
    // Password Operations
    // ========================================================================

    /**
     * @brief Gets the password for an entry (marks as accessed).
     * @param entryId Entry ID.
     * @return Result containing the password or error.
     */
    Result<std::string> getPassword(const std::string& entryId);

    /**
     * @brief Updates the password for an entry.
     * @param entryId Entry ID.
     * @param newPassword New password.
     * @return Result indicating success or error.
     */
    Result<void> updatePassword(const std::string& entryId,
                                const std::string& newPassword);

    // ========================================================================
    // Import/Export
    // ========================================================================

    /**
     * @brief Exports all entries to JSON.
     * @param password Optional encryption password.
     * @return Result containing JSON string or error.
     */
    Result<std::string> exportToJson(const std::string& password = "");

    /**
     * @brief Imports entries from JSON.
     * @param json JSON string.
     * @param password Decryption password if encrypted.
     * @param overwrite Whether to overwrite existing entries.
     * @return Result containing number of imported entries or error.
     */
    Result<int> importFromJson(const std::string& json,
                               const std::string& password = "",
                               bool overwrite = false);

    // ========================================================================
    // Settings and Session
    // ========================================================================

    /**
     * @brief Gets the current settings.
     * @return Current settings.
     */
    const PasswordManagerSettings& getSettings() const;

    /**
     * @brief Updates settings.
     * @param settings New settings.
     */
    void updateSettings(const PasswordManagerSettings& settings);

    /**
     * @brief Gets the session manager.
     * @return Reference to session manager.
     */
    SessionManager& getSession();

    /**
     * @brief Gets the audit log.
     * @return Pointer to audit log or nullptr if disabled.
     */
    AuditLog* getAuditLog();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace atom::secret

#endif  // ATOM_SECRET_MANAGER_PASSWORD_MANAGER_HPP
