/**
 * @file password_manager.hpp
 * @brief Main password manager interface for the Atom Secret module.
 *
 * This file contains the primary PasswordManager class that provides a complete
 * password management solution with secure storage, encryption, and various
 * password management features.
 *
 * Key Features:
 * - Secure password storage with encryption
 * - Master password protection with PBKDF2 key derivation
 * - Cross-platform secure storage backends
 * - Password generation and strength analysis
 * - Search and filtering capabilities
 * - Import/export functionality
 * - Auto-lock and session management
 * - Password expiration tracking
 *
 * @author Atom Development Team
 * @version 1.0.0
 * @date 2024
 * @copyright GPL3 License
 */

#ifndef ATOM_SECRET_PASSWORD_MANAGER_HPP
#define ATOM_SECRET_PASSWORD_MANAGER_HPP

#include <chrono>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

#include "common.hpp"
#include "encryption.hpp"
#include "password_entry.hpp"
#include "password_utils.hpp"
#include "result.hpp"
#include "storage.hpp"

namespace atom::secret {

/**
 * @brief Main password manager class providing secure password storage and
 * management.
 */
class PasswordManager {
public:
    /**
     * @brief Constructs a new PasswordManager instance.
     */
    PasswordManager();

    /**
     * @brief Destructor that ensures secure cleanup.
     */
    ~PasswordManager();

    // Disable copy construction and assignment
    PasswordManager(const PasswordManager&) = delete;
    PasswordManager& operator=(const PasswordManager&) = delete;

    // Enable move construction and assignment
    PasswordManager(PasswordManager&&) noexcept;
    PasswordManager& operator=(PasswordManager&&) noexcept;

    /**
     * @brief Initializes the password manager with a master password.
     * @param masterPassword The master password for encryption.
     * @param settings Optional password manager settings.
     * @return True if initialization succeeded, false otherwise.
     */
    bool initialize(std::string_view masterPassword,
                    const PasswordManagerSettings& settings = {});

    /**
     * @brief Locks the password manager, clearing sensitive data from memory.
     */
    void lock();

    /**
     * @brief Unlocks the password manager with the master password.
     * @param masterPassword The master password.
     * @return True if unlock succeeded, false otherwise.
     */
    bool unlock(std::string_view masterPassword);

    /**
     * @brief Checks if the password manager is currently locked.
     * @return True if locked, false if unlocked.
     */
    bool isLocked() const noexcept;

    /**
     * @brief Changes the master password.
     * @param currentPassword Current master password.
     * @param newPassword New master password.
     * @return True if change succeeded, false otherwise.
     */
    bool changeMasterPassword(std::string_view currentPassword,
                              std::string_view newPassword);

    /**
     * @brief Stores a password entry.
     * @param key Unique key for the entry.
     * @param entry Password entry to store.
     * @return True if storage succeeded, false otherwise.
     */
    bool storePassword(std::string_view key, const PasswordEntry& entry);

    /**
     * @brief Retrieves a password entry.
     * @param key Key of the entry to retrieve.
     * @return Retrieved password entry or empty entry if not found.
     */
    PasswordEntry retrievePassword(std::string_view key);

    /**
     * @brief Removes a password entry.
     * @param key Key of the entry to remove.
     * @return True if removal succeeded, false otherwise.
     */
    bool removePassword(std::string_view key);

    /**
     * @brief Gets all stored password keys.
     * @return Vector of all password keys.
     */
    std::vector<std::string> getAllKeys();

    /**
     * @brief Searches for password entries by various criteria.
     * @param query Search query.
     * @param searchInTitle Search in entry titles.
     * @param searchInUsername Search in usernames.
     * @param searchInUrl Search in URLs.
     * @param searchInNotes Search in notes.
     * @param searchInTags Search in tags.
     * @return Vector of matching password entries with their keys.
     */
    std::vector<std::pair<std::string, PasswordEntry>> searchPasswords(
        std::string_view query, bool searchInTitle = true,
        bool searchInUsername = true, bool searchInUrl = true,
        bool searchInNotes = false, bool searchInTags = true);

    /**
     * @brief Filters password entries by category.
     * @param category Category to filter by.
     * @return Vector of matching password entries with their keys.
     */
    std::vector<std::pair<std::string, PasswordEntry>> filterByCategory(
        PasswordCategory category);

    /**
     * @brief Gets password entries that are expiring soon.
     * @param daysAhead Number of days ahead to check (default: 30).
     * @return Vector of expiring password entries with their keys.
     */
    std::vector<std::pair<std::string, PasswordEntry>> getExpiringPasswords(
        int daysAhead = 30);

    /**
     * @brief Generates a secure password.
     * @param length Password length (0 uses settings default).
     * @param includeUppercase Include uppercase letters.
     * @param includeNumbers Include numbers.
     * @param includeSpecial Include special characters.
     * @return Generated password or empty string on failure.
     */
    std::string generatePassword(int length = 0, bool includeUppercase = true,
                                 bool includeNumbers = true,
                                 bool includeSpecial = true);

    /**
     * @brief Analyzes password strength.
     * @param password Password to analyze.
     * @return Password analysis results.
     */
    PasswordValidator::AnalysisResult analyzePassword(
        std::string_view password);

    /**
     * @brief Exports all password entries to JSON.
     * @return Result containing JSON string or error message.
     */
    Result<std::string> exportToJson();

    /**
     * @brief Imports password entries from JSON.
     * @param json JSON string containing password entries.
     * @param overwriteExisting Whether to overwrite existing entries.
     * @return Result containing number of imported entries or error message.
     */
    Result<int> importFromJson(const std::string& json,
                               bool overwriteExisting = false);

    /**
     * @brief Gets the current password manager settings.
     * @return Current settings.
     */
    const PasswordManagerSettings& getSettings() const noexcept;

    /**
     * @brief Updates password manager settings.
     * @param settings New settings to apply.
     * @return True if update succeeded, false otherwise.
     */
    bool updateSettings(const PasswordManagerSettings& settings);

    /**
     * @brief Gets statistics about stored passwords.
     * @return Statistics structure.
     */
    struct Statistics {
        int totalEntries = 0;
        int expiredEntries = 0;
        int weakPasswords = 0;
        int duplicatePasswords = 0;
        std::chrono::system_clock::time_point lastModified;
    };
    Statistics getStatistics();

private:
    /**
     * @brief Derives the master key from the master password.
     * @param masterPassword Master password.
     * @return Result containing derived key or error message.
     */
    Result<std::vector<uint8_t>> deriveMasterKey(
        std::string_view masterPassword);

    /**
     * @brief Encrypts data using the master key.
     * @param data Data to encrypt.
     * @return Result containing encrypted data or error message.
     */
    Result<EncryptedData> encryptData(const std::string& data);

    /**
     * @brief Decrypts data using the master key.
     * @param encryptedData Encrypted data to decrypt.
     * @return Result containing decrypted data or error message.
     */
    Result<std::string> decryptData(const EncryptedData& encryptedData);

    /**
     * @brief Checks if auto-lock timeout has been reached.
     */
    void checkAutoLock();

    /**
     * @brief Updates the last activity timestamp.
     */
    void updateLastActivity();

    /**
     * @brief Validates that the manager is unlocked.
     * @return True if unlocked, false if locked.
     */
    bool ensureUnlocked();

    // Private member variables
    std::unique_ptr<SecureStorage> storage_;
    PasswordManagerSettings settings_;
    std::vector<uint8_t> masterKey_;
    std::vector<uint8_t> masterSalt_;
    bool isLocked_;
    std::chrono::system_clock::time_point lastActivity_;
    mutable std::mutex mutex_;
};

}  // namespace atom::secret

#endif  // ATOM_SECRET_PASSWORD_MANAGER_HPP
