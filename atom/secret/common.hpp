#ifndef ATOM_SECRET_COMMON_HPP
#define ATOM_SECRET_COMMON_HPP

#include <chrono>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>
#include <algorithm>
#include <cctype>

namespace atom::secret {

/**
 * @brief Password strength levels.
 */
enum class PasswordStrength {
    VeryWeak = 0,
    Weak = 1,
    Medium = 2,
    Strong = 3,
    VeryStrong = 4
};

/**
 * @brief Password categories.
 */
enum class PasswordCategory {
    General = 0,
    Finance = 1,
    Work = 2,
    Personal = 3,
    Social = 4,
    Entertainment = 5,
    Education = 6,
    Health = 7,
    Travel = 8,
    Shopping = 9,
    Other = 10
};

/**
 * @brief Password generation options.
 */
struct PasswordGenerationOptions {
    int length = 16;                    ///< Password length
    bool includeUppercase = true;       ///< Include uppercase letters
    bool includeLowercase = true;       ///< Include lowercase letters
    bool includeNumbers = true;         ///< Include numbers
    bool includeSpecialChars = true;    ///< Include special characters
    bool excludeSimilarChars = false;   ///< Exclude similar looking characters (0, O, l, 1, etc.)
    bool excludeAmbiguousChars = false; ///< Exclude ambiguous characters
    std::string customCharset;          ///< Custom character set (overrides other options if not empty)
    std::string excludeChars;           ///< Characters to exclude
    bool requireFromEachSet = true;     ///< Require at least one character from each enabled set
};

/**
 * @brief Search and filter options for password entries.
 */
struct SearchOptions {
    std::string query;                  ///< Search query
    PasswordCategory category = PasswordCategory::General; ///< Filter by category
    bool searchInTitle = true;          ///< Search in title
    bool searchInUsername = true;       ///< Search in username
    bool searchInUrl = true;            ///< Search in URL
    bool searchInNotes = true;          ///< Search in notes
    bool searchInTags = true;           ///< Search in tags
    bool caseSensitive = false;         ///< Case sensitive search
    bool useRegex = false;              ///< Use regular expressions
    std::chrono::system_clock::time_point createdAfter;  ///< Filter by creation date
    std::chrono::system_clock::time_point createdBefore; ///< Filter by creation date
    std::chrono::system_clock::time_point expiresAfter;  ///< Filter by expiration date
    std::chrono::system_clock::time_point expiresBefore; ///< Filter by expiration date
    bool includeExpired = true;         ///< Include expired entries
};

/**
 * @brief Structure for encryption options.
 */
struct EncryptionOptions {
    bool useHardwareAcceleration = true;    ///< Whether to use hardware acceleration.
    int keyIterations = 100000;             ///< PBKDF2 iteration count, increased default.
    int saltSize = 16;                      ///< Salt size in bytes.
    int ivSize = 12;                        ///< IV size in bytes (for GCM mode).
    int tagSize = 16;                       ///< Authentication tag size in bytes.
    bool useSecureMemory = true;            ///< Use secure memory allocation when available.

    /**
     * @brief Encryption method enumeration.
     */
    enum class Method : std::uint8_t {
        AES_GCM = 0,           ///< AES-GCM (Default, AEAD encryption).
        AES_CBC = 1,           ///< AES-CBC with HMAC.
        CHACHA20_POLY1305 = 2, ///< ChaCha20-Poly1305.
        AES_256_GCM = 3,       ///< AES-256-GCM (explicit).
        AES_128_GCM = 4        ///< AES-128-GCM.
    };

    Method encryptionMethod{Method::AES_GCM};  ///< The encryption method to use.

    /**
     * @brief Get the key size for the selected encryption method.
     * @return Key size in bytes.
     */
    int getKeySize() const {
        switch (encryptionMethod) {
            case Method::AES_128_GCM:
                return 16;
            case Method::AES_GCM:
            case Method::AES_256_GCM:
            case Method::AES_CBC:
                return 32;
            case Method::CHACHA20_POLY1305:
                return 32;
            default:
                return 32;
        }
    }

    /**
     * @brief Get the IV size for the selected encryption method.
     * @return IV size in bytes.
     */
    int getIVSize() const {
        switch (encryptionMethod) {
            case Method::AES_GCM:
            case Method::AES_256_GCM:
            case Method::AES_128_GCM:
                return 12; // GCM mode
            case Method::AES_CBC:
                return 16; // CBC mode
            case Method::CHACHA20_POLY1305:
                return 12;
            default:
                return ivSize;
        }
    }
};

/**
 * @brief Settings for the Password Manager.
 */
struct PasswordManagerSettings {
    int autoLockTimeoutSeconds = 300;      ///< Auto-lock timeout in seconds.
    bool notifyOnPasswordExpiry = true;    ///< Enable password expiry notifications.
    int passwordExpiryDays = 90;           ///< Password validity period in days.
    int minPasswordLength = 12;            ///< Minimum password length requirement.
    int maxPasswordLength = 128;           ///< Maximum password length.
    bool requireSpecialChars = true;       ///< Require special characters in passwords.
    bool requireNumbers = true;            ///< Require numbers in passwords.
    bool requireMixedCase = true;          ///< Require mixed case letters in passwords.
    bool enablePasswordHistory = true;     ///< Enable password history tracking.
    int maxPasswordHistory = 10;           ///< Maximum number of previous passwords to store.
    bool enableAutoBackup = false;         ///< Enable automatic backups.
    int autoBackupIntervalHours = 24;      ///< Backup interval in hours.
    bool enableBruteForceProtection = true; ///< Enable brute force protection.
    int maxFailedAttempts = 5;             ///< Maximum failed login attempts.
    int lockoutDurationMinutes = 15;       ///< Lockout duration after max failed attempts.
    EncryptionOptions encryptionOptions;   ///< Encryption options.

    /**
     * @brief Validate the settings for consistency.
     * @return True if settings are valid, false otherwise.
     */
    bool isValid() const {
        return minPasswordLength > 0 &&
               maxPasswordLength >= minPasswordLength &&
               passwordExpiryDays > 0 &&
               autoLockTimeoutSeconds >= 0 &&
               maxPasswordHistory >= 0 &&
               autoBackupIntervalHours > 0 &&
               maxFailedAttempts > 0 &&
               lockoutDurationMinutes >= 0;
    }
};

/**
 * @brief Structure representing a previous password entry with change
 * timestamp.
 */
struct PreviousPassword {
    std::string password;  ///< The previous password value
    std::chrono::system_clock::time_point changed;  ///< When the password was changed
    PasswordStrength strength = PasswordStrength::Medium;  ///< Strength of the previous password

    PreviousPassword() = default;
    PreviousPassword(std::string pwd, std::chrono::system_clock::time_point time, PasswordStrength str = PasswordStrength::Medium)
        : password(std::move(pwd)), changed(time), strength(str) {}
};

/**
 * @brief Utility functions for password management.
 */
namespace utils {

/**
 * @brief Convert PasswordStrength enum to string.
 * @param strength The password strength.
 * @return String representation of the strength.
 */
inline std::string strengthToString(PasswordStrength strength) {
    switch (strength) {
        case PasswordStrength::VeryWeak: return "Very Weak";
        case PasswordStrength::Weak: return "Weak";
        case PasswordStrength::Medium: return "Medium";
        case PasswordStrength::Strong: return "Strong";
        case PasswordStrength::VeryStrong: return "Very Strong";
        default: return "Unknown";
    }
}

/**
 * @brief Convert PasswordCategory enum to string.
 * @param category The password category.
 * @return String representation of the category.
 */
inline std::string categoryToString(PasswordCategory category) {
    switch (category) {
        case PasswordCategory::General: return "General";
        case PasswordCategory::Finance: return "Finance";
        case PasswordCategory::Work: return "Work";
        case PasswordCategory::Personal: return "Personal";
        case PasswordCategory::Social: return "Social";
        case PasswordCategory::Entertainment: return "Entertainment";
        case PasswordCategory::Education: return "Education";
        case PasswordCategory::Health: return "Health";
        case PasswordCategory::Travel: return "Travel";
        case PasswordCategory::Shopping: return "Shopping";
        case PasswordCategory::Other: return "Other";
        default: return "Unknown";
    }
}

/**
 * @brief Convert string to PasswordCategory enum.
 * @param categoryStr The category string.
 * @return PasswordCategory enum value.
 */
inline PasswordCategory stringToCategory(std::string_view categoryStr) {
    if (categoryStr == "General") return PasswordCategory::General;
    if (categoryStr == "Finance") return PasswordCategory::Finance;
    if (categoryStr == "Work") return PasswordCategory::Work;
    if (categoryStr == "Personal") return PasswordCategory::Personal;
    if (categoryStr == "Social") return PasswordCategory::Social;
    if (categoryStr == "Entertainment") return PasswordCategory::Entertainment;
    if (categoryStr == "Education") return PasswordCategory::Education;
    if (categoryStr == "Health") return PasswordCategory::Health;
    if (categoryStr == "Travel") return PasswordCategory::Travel;
    if (categoryStr == "Shopping") return PasswordCategory::Shopping;
    return PasswordCategory::Other;
}

/**
 * @brief Analyze password strength based on various criteria.
 * @param password The password to analyze.
 * @return PasswordStrength enum value.
 */
inline PasswordStrength analyzePasswordStrength(std::string_view password) {
    if (password.empty()) return PasswordStrength::VeryWeak;

    int score = 0;
    bool hasLower = false, hasUpper = false, hasDigit = false, hasSpecial = false;

    // Length scoring
    if (password.length() >= 8) score += 1;
    if (password.length() >= 12) score += 1;
    if (password.length() >= 16) score += 1;

    // Character type analysis
    for (char c : password) {
        if (std::islower(c)) hasLower = true;
        else if (std::isupper(c)) hasUpper = true;
        else if (std::isdigit(c)) hasDigit = true;
        else hasSpecial = true;
    }

    if (hasLower) score += 1;
    if (hasUpper) score += 1;
    if (hasDigit) score += 1;
    if (hasSpecial) score += 1;

    // Complexity bonus
    int charTypes = (hasLower ? 1 : 0) + (hasUpper ? 1 : 0) + (hasDigit ? 1 : 0) + (hasSpecial ? 1 : 0);
    if (charTypes >= 3) score += 1;
    if (charTypes == 4) score += 1;

    // Convert score to strength
    if (score <= 2) return PasswordStrength::VeryWeak;
    if (score <= 4) return PasswordStrength::Weak;
    if (score <= 6) return PasswordStrength::Medium;
    if (score <= 8) return PasswordStrength::Strong;
    return PasswordStrength::VeryStrong;
}

/**
 * @brief Check if a password meets the specified requirements.
 * @param password The password to check.
 * @param settings The password manager settings.
 * @return True if password meets requirements, false otherwise.
 */
inline bool validatePassword(std::string_view password, const PasswordManagerSettings& settings) {
    if (password.length() < static_cast<size_t>(settings.minPasswordLength) ||
        password.length() > static_cast<size_t>(settings.maxPasswordLength)) {
        return false;
    }

    bool hasLower = false, hasUpper = false, hasDigit = false, hasSpecial = false;

    for (char c : password) {
        if (std::islower(c)) hasLower = true;
        else if (std::isupper(c)) hasUpper = true;
        else if (std::isdigit(c)) hasDigit = true;
        else hasSpecial = true;
    }

    if (settings.requireMixedCase && (!hasLower || !hasUpper)) return false;
    if (settings.requireNumbers && !hasDigit) return false;
    if (settings.requireSpecialChars && !hasSpecial) return false;

    return true;
}

/**
 * @brief Generate a secure random password.
 * @param options The password generation options.
 * @return Generated password string.
 */
std::string generatePassword(const PasswordGenerationOptions& options);

/**
 * @brief Calculate entropy of a password.
 * @param password The password to analyze.
 * @return Entropy in bits.
 */
double calculatePasswordEntropy(std::string_view password);

} // namespace utils

}  // namespace atom::secret

#endif  // ATOM_SECRET_COMMON_HPP
