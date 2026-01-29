#ifndef ATOM_SECRET_COMMON_HPP
#define ATOM_SECRET_COMMON_HPP
#include <chrono>
#include <cstdint>
#include <string>

namespace atom::secret {

/**
 * @brief Password strength levels.
 */
enum class PasswordStrength { VeryWeak, Weak, Medium, Strong, VeryStrong };

/**
 * @brief Password categories.
 */
enum class PasswordCategory {
    General,
    Finance,
    Work,
    Personal,
    Social,
    Entertainment,
    Other
};

/**
 * @brief Structure for encryption options.
 */
struct EncryptionOptions {
    bool useHardwareAcceleration =
        true;                    ///< Whether to use hardware acceleration.
    int keyIterations = 100000;  ///< PBKDF2 iteration count, increased default.

    /**
     * @brief Encryption method enumeration.
     */
    enum class Method : uint8_t {
        AES_GCM = 0,           ///< AES-GCM (Default, AEAD encryption).
        AES_CBC = 1,           ///< AES-CBC.
        CHACHA20_POLY1305 = 2  ///< ChaCha20-Poly1305.
    };

    Method encryptionMethod{
        Method::AES_GCM};  ///< The encryption method to use.
};

/**
 * @brief Settings for the Password Manager.
 */
struct PasswordManagerSettings {
    int autoLockTimeoutSeconds = 300;  ///< Auto-lock timeout in seconds.
    bool notifyOnPasswordExpiry =
        true;                     ///< Enable password expiry notifications.
    int passwordExpiryDays = 90;  ///< Password validity period in days.
    int minPasswordLength = 12;   ///< Minimum password length requirement.
    bool requireSpecialChars =
        true;                      ///< Require special characters in passwords.
    bool requireNumbers = true;    ///< Require numbers in passwords.
    bool requireMixedCase = true;  ///< Require mixed case letters in passwords.
    EncryptionOptions encryptionOptions;  ///< Encryption options.
};

/**
 * @brief Structure representing a previous password entry with change
 * timestamp.
 */
struct PreviousPassword {
    std::string password;  ///< The previous password value
    std::chrono::system_clock::time_point
        changed;  ///< When the password was changed
};

}  // namespace atom::secret

#endif  // ATOM_SECRET_COMMON_HPP
