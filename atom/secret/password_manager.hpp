#ifndef ATOM_SECRET_PASSWORD_MANAGER_HPP
#define ATOM_SECRET_PASSWORD_MANAGER_HPP

#include <memory>
#include <shared_mutex>
#include <string>
#include <vector>
#include <string_view>

#include "common.hpp"
#include "password_entry.hpp"
#include "result.hpp"

namespace atom::secret {

class SecureStorage;

/**
 * @brief Manages password entries, providing a thread-safe interface for
 * storing and retrieving secrets.
 */
class PasswordManager {
public:
    /**
     * @brief Constructs a PasswordManager.
     * @param appName The name of the application, used for namespacing secrets.
     */
    explicit PasswordManager(std::string_view appName);

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

private:
    Result<PasswordEntry> getEntry_nolock(std::string_view title, std::string_view masterPassword) const;
    std::unique_ptr<SecureStorage> storage_;
    mutable std::shared_mutex mutex_;
    PasswordManagerSettings settings_;
    std::string appName_;
};

}  // namespace atom::secret

#endif  // ATOM_SECRET_PASSWORD_MANAGER_HPP
