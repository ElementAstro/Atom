#ifndef ATOM_SECRET_PASSWORD_ENTRY_HPP
#define ATOM_SECRET_PASSWORD_ENTRY_HPP

#include <chrono>
#include <string>
#include <vector>

#include "common.hpp"

namespace atom::secret {

/**
 * @brief Structure representing a password entry.
 */
struct PasswordEntry {
    std::string password;  ///< The stored password.
    std::string username;  ///< Associated username.
    std::string url;       ///< Associated URL.
    std::string notes;     ///< Additional notes.
    std::string title;     ///< Entry title.
    PasswordCategory category{
        PasswordCategory::General};  ///< Password category.
    std::vector<std::string> tags;   ///< Tags for categorization and search.
    std::chrono::system_clock::time_point created;  ///< Creation timestamp.
    std::chrono::system_clock::time_point
        modified;  ///< Last modification timestamp.
    std::chrono::system_clock::time_point expires;  ///< Expiration timestamp.
    std::vector<std::string> previousPasswords;     ///< Password history.

    // Move constructor and assignment support
    PasswordEntry() = default;
    PasswordEntry(const PasswordEntry&) = default;
    PasswordEntry& operator=(const PasswordEntry&) = default;
    PasswordEntry(PasswordEntry&&) noexcept = default;
    PasswordEntry& operator=(PasswordEntry&&) noexcept = default;

    /**
     * @brief Checks if the entry is empty.
     * @return True if the entry is empty, false otherwise.
     */
    bool isEmpty() const noexcept {
        return password.empty() && username.empty() && url.empty() &&
               notes.empty() && previousPasswords.empty();
    }
};

}  // namespace atom::secret

#endif  // ATOM_SECRET_PASSWORD_ENTRY_HPP
