#ifndef ATOM_SECRET_PASSWORD_ENTRY_HPP
#define ATOM_SECRET_PASSWORD_ENTRY_HPP

#include <chrono>
#include <string>
#include <vector>

#include "atom/utils/uuid.hpp"

namespace atom::secret {

/**
 * @brief Password strength levels.
 */
enum class PasswordStrength {
    VeryWeak,   ///< Very weak password (score 0-20)
    Weak,       ///< Weak password (score 21-40)
    Medium,     ///< Medium strength password (score 41-60)
    Strong,     ///< Strong password (score 61-80)
    VeryStrong  ///< Very strong password (score 81-100)
};

/**
 * @brief Password categories for organization.
 */
enum class PasswordCategory {
    General,        ///< General/uncategorized
    Finance,        ///< Banking, financial services
    Work,           ///< Work-related accounts
    Personal,       ///< Personal accounts
    Social,         ///< Social media
    Entertainment,  ///< Entertainment services
    Shopping,       ///< E-commerce, shopping
    Email,          ///< Email accounts
    Development,    ///< Developer tools, APIs
    Server,         ///< Server/infrastructure
    Other           ///< Other category
};

/**
 * @brief Structure representing a previous password with change timestamp.
 */
struct PreviousPassword {
    std::string password;  ///< The previous password value
    std::chrono::system_clock::time_point
        changed;  ///< When the password was changed

    PreviousPassword() = default;
    PreviousPassword(const std::string& pwd,
                     std::chrono::system_clock::time_point time)
        : password(pwd), changed(time) {}
};

/**
 * @brief Custom field for storing additional data.
 */
struct CustomField {
    std::string name;   ///< Field name
    std::string value;  ///< Field value
    bool isProtected;   ///< Whether the value should be hidden/encrypted
    bool isMultiline;   ///< Whether the value is multiline

    CustomField() : isProtected(false), isMultiline(false) {}
    CustomField(const std::string& n, const std::string& v,
                bool protect = false, bool multiline = false)
        : name(n), value(v), isProtected(protect), isMultiline(multiline) {}
};

/**
 * @brief Structure representing a password entry.
 */
struct PasswordEntry {
    std::string id;        ///< Unique identifier (UUID)
    std::string title;     ///< Entry title/name
    std::string username;  ///< Associated username
    std::string password;  ///< The stored password
    std::string url;       ///< Associated URL
    std::string notes;     ///< Additional notes
    std::string email;     ///< Associated email address
    std::string totp;      ///< TOTP secret (if 2FA enabled)

    PasswordCategory category{
        PasswordCategory::General};  ///< Password category
    std::vector<std::string> tags;   ///< Tags for categorization and search
    std::vector<CustomField> customFields;  ///< Custom fields

    std::chrono::system_clock::time_point created;  ///< Creation timestamp
    std::chrono::system_clock::time_point
        modified;  ///< Last modification timestamp
    std::chrono::system_clock::time_point expires;  ///< Expiration timestamp
    std::chrono::system_clock::time_point
        lastAccessed;  ///< Last access timestamp

    std::vector<PreviousPassword> passwordHistory;  ///< Password history
    int accessCount{0};  ///< Number of times accessed

    bool isFavorite{false};  ///< Marked as favorite
    bool isArchived{false};  ///< Archived entry
    std::string iconUrl;     ///< Custom icon URL

    // Constructors
    PasswordEntry() = default;
    PasswordEntry(const PasswordEntry&) = default;
    PasswordEntry& operator=(const PasswordEntry&) = default;
    PasswordEntry(PasswordEntry&&) noexcept = default;
    PasswordEntry& operator=(PasswordEntry&&) noexcept = default;

    /**
     * @brief Checks if the entry is empty.
     * @return True if the entry has no meaningful data.
     */
    bool isEmpty() const noexcept {
        return password.empty() && username.empty() && url.empty() &&
               notes.empty() && title.empty();
    }

    /**
     * @brief Checks if the password has expired.
     * @return True if expired.
     */
    bool isExpired() const noexcept {
        if (expires == std::chrono::system_clock::time_point{}) {
            return false;  // No expiration set
        }
        return std::chrono::system_clock::now() > expires;
    }

    /**
     * @brief Checks if the password will expire within the given days.
     * @param days Number of days to check.
     * @return True if expiring soon.
     */
    bool isExpiringSoon(int days) const noexcept {
        if (expires == std::chrono::system_clock::time_point{}) {
            return false;
        }
        auto threshold =
            std::chrono::system_clock::now() + std::chrono::hours(24 * days);
        return expires <= threshold && !isExpired();
    }

    /**
     * @brief Adds a tag if not already present.
     * @param tag Tag to add.
     */
    void addTag(const std::string& tag) {
        for (const auto& t : tags) {
            if (t == tag)
                return;
        }
        tags.push_back(tag);
    }

    /**
     * @brief Removes a tag.
     * @param tag Tag to remove.
     * @return True if tag was found and removed.
     */
    bool removeTag(const std::string& tag) {
        for (auto it = tags.begin(); it != tags.end(); ++it) {
            if (*it == tag) {
                tags.erase(it);
                return true;
            }
        }
        return false;
    }

    /**
     * @brief Checks if entry has a specific tag.
     * @param tag Tag to check.
     * @return True if tag exists.
     */
    bool hasTag(const std::string& tag) const noexcept {
        for (const auto& t : tags) {
            if (t == tag)
                return true;
        }
        return false;
    }

    /**
     * @brief Adds a custom field.
     * @param field Custom field to add.
     */
    void addCustomField(const CustomField& field) {
        customFields.push_back(field);
    }

    /**
     * @brief Gets a custom field by name.
     * @param name Field name.
     * @return Pointer to field or nullptr if not found.
     */
    const CustomField* getCustomField(const std::string& name) const {
        for (const auto& field : customFields) {
            if (field.name == name) {
                return &field;
            }
        }
        return nullptr;
    }

    /**
     * @brief Updates the password and adds old one to history.
     * @param newPassword New password.
     * @param maxHistory Maximum history entries to keep.
     */
    void updatePassword(const std::string& newPassword,
                        size_t maxHistory = 10) {
        if (!password.empty()) {
            passwordHistory.insert(
                passwordHistory.begin(),
                PreviousPassword(password, std::chrono::system_clock::now()));

            while (passwordHistory.size() > maxHistory) {
                passwordHistory.pop_back();
            }
        }
        password = newPassword;
        modified = std::chrono::system_clock::now();
    }

    /**
     * @brief Marks the entry as accessed.
     */
    void markAccessed() {
        lastAccessed = std::chrono::system_clock::now();
        ++accessCount;
    }

    /**
     * @brief Generates a new unique ID for this entry.
     *
     * Uses atom::utils::UUID for UUID generation.
     */
    void generateId() {
        atom::utils::UUID uuid;
        id = uuid.toString();
    }

    /**
     * @brief Creates a new entry with a generated ID.
     * @return New PasswordEntry with unique ID and timestamps set.
     */
    static PasswordEntry create() {
        PasswordEntry entry;
        entry.generateId();
        entry.created = std::chrono::system_clock::now();
        entry.modified = entry.created;
        return entry;
    }
};

/**
 * @brief Converts PasswordCategory to string.
 * @param category Category to convert.
 * @return String representation.
 */
inline std::string categoryToString(PasswordCategory category) {
    switch (category) {
        case PasswordCategory::General:
            return "General";
        case PasswordCategory::Finance:
            return "Finance";
        case PasswordCategory::Work:
            return "Work";
        case PasswordCategory::Personal:
            return "Personal";
        case PasswordCategory::Social:
            return "Social";
        case PasswordCategory::Entertainment:
            return "Entertainment";
        case PasswordCategory::Shopping:
            return "Shopping";
        case PasswordCategory::Email:
            return "Email";
        case PasswordCategory::Development:
            return "Development";
        case PasswordCategory::Server:
            return "Server";
        case PasswordCategory::Other:
            return "Other";
        default:
            return "Unknown";
    }
}

/**
 * @brief Converts string to PasswordCategory.
 * @param str String to convert.
 * @return PasswordCategory enum value.
 */
inline PasswordCategory stringToCategory(const std::string& str) {
    if (str == "General")
        return PasswordCategory::General;
    if (str == "Finance")
        return PasswordCategory::Finance;
    if (str == "Work")
        return PasswordCategory::Work;
    if (str == "Personal")
        return PasswordCategory::Personal;
    if (str == "Social")
        return PasswordCategory::Social;
    if (str == "Entertainment")
        return PasswordCategory::Entertainment;
    if (str == "Shopping")
        return PasswordCategory::Shopping;
    if (str == "Email")
        return PasswordCategory::Email;
    if (str == "Development")
        return PasswordCategory::Development;
    if (str == "Server")
        return PasswordCategory::Server;
    if (str == "Other")
        return PasswordCategory::Other;
    return PasswordCategory::General;
}

/**
 * @brief Converts PasswordStrength to string.
 * @param strength Strength to convert.
 * @return String representation.
 */
inline std::string strengthToString(PasswordStrength strength) {
    switch (strength) {
        case PasswordStrength::VeryWeak:
            return "Very Weak";
        case PasswordStrength::Weak:
            return "Weak";
        case PasswordStrength::Medium:
            return "Medium";
        case PasswordStrength::Strong:
            return "Strong";
        case PasswordStrength::VeryStrong:
            return "Very Strong";
        default:
            return "Unknown";
    }
}

}  // namespace atom::secret

#endif  // ATOM_SECRET_PASSWORD_ENTRY_HPP
