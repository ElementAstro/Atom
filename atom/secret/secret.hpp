#ifndef ATOM_SECRET_SECRET_HPP
#define ATOM_SECRET_SECRET_HPP

/**
 * @file secret.hpp
 * @brief Unified header for the Atom Secret module.
 *
 * This header includes all components of the secret management library,
 * providing a convenient single-include option for users.
 *
 * Dependencies on other atom modules:
 * - atom::algorithm (Base32/Base64/Hex encoding)
 * - atom::type (nlohmann::json)
 * - atom::utils (UUID generation)
 *
 * @author Max Qian
 * @license GPL3
 */

// Core components
#include "core/error_codes.hpp"
#include "core/result.hpp"
#include "core/types.hpp"
#include "core/utils.hpp"

// Cryptographic primitives
#include "crypto/encryption.hpp"
#include "crypto/hash.hpp"
#include "crypto/hmac.hpp"
#include "crypto/key_derivation.hpp"
#include "crypto/secure_memory.hpp"

// Password management
#include "password/breach_checker.hpp"
#include "password/entry.hpp"
#include "password/generator.hpp"
#include "password/validator.hpp"

// One-Time Password (OTP)
#include "otp/hotp.hpp"
#include "otp/totp.hpp"

// Secure storage
#include "storage/backup.hpp"
#include "storage/file_storage.hpp"
#include "storage/storage.hpp"

// Session and audit management
#include "manager/audit_log.hpp"
#include "manager/password_manager.hpp"
#include "manager/session.hpp"

// Serialization
#include "serialization/json.hpp"

// Legacy headers (for backward compatibility)
#include "common.hpp"
#include "encryption.hpp"
#include "password_entry.hpp"
#include "password_manager.hpp"
#include "password_utils.hpp"
#include "result.hpp"
#include "serialization.hpp"
#include "storage.hpp"

namespace atom::secret {

/**
 * @brief Library version information.
 */
struct Version {
    static constexpr int MAJOR = 2;
    static constexpr int MINOR = 0;
    static constexpr int PATCH = 0;

    static constexpr const char* STRING = "2.0.0";

    /**
     * @brief Gets the full version string.
     * @return Version string.
     */
    static const char* getVersionString() noexcept { return STRING; }

    /**
     * @brief Checks if the library version is at least the specified version.
     * @param major Major version.
     * @param minor Minor version.
     * @param patch Patch version.
     * @return True if library version >= specified version.
     */
    static bool isAtLeast(int major, int minor = 0, int patch = 0) noexcept {
        if (MAJOR > major)
            return true;
        if (MAJOR < major)
            return false;
        if (MINOR > minor)
            return true;
        if (MINOR < minor)
            return false;
        return PATCH >= patch;
    }
};

/**
 * @brief Feature availability checks.
 */
struct Features {
    /**
     * @brief Checks if AES-GCM encryption is available.
     * @return True if available.
     */
    static bool hasAesGcm() noexcept {
        return Encryption::isAvailable(EncryptionAlgorithm::AES_256_GCM);
    }

    /**
     * @brief Checks if ChaCha20-Poly1305 encryption is available.
     * @return True if available.
     */
    static bool hasChaCha20() noexcept {
        return Encryption::isAvailable(EncryptionAlgorithm::ChaCha20_Poly1305);
    }

    /**
     * @brief Checks if Argon2 key derivation is available.
     * @return True if available.
     */
    static bool hasArgon2() noexcept {
        return KeyDerivation::isArgon2Available();
    }

    /**
     * @brief Checks if scrypt key derivation is available.
     * @return True if available.
     */
    static bool hasScrypt() noexcept {
        return KeyDerivation::isScryptAvailable();
    }

    /**
     * @brief Checks if memory locking is available.
     * @return True if available.
     */
    static bool hasMemoryLocking() noexcept {
        return SecureMemory::isMemoryLockingAvailable();
    }

    /**
     * @brief Checks if system keychain storage is available.
     * @return True if available.
     */
    static bool hasSystemKeychain() noexcept {
        return SecureStorage::isBackendAvailable(StorageBackend::System);
    }
};

}  // namespace atom::secret

#endif  // ATOM_SECRET_SECRET_HPP
