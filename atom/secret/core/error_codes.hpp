#ifndef ATOM_SECRET_CORE_ERROR_CODES_HPP
#define ATOM_SECRET_CORE_ERROR_CODES_HPP

#include <cstdint>
#include <string>

namespace atom::secret {

/**
 * @brief Error codes for the secret module operations.
 */
enum class ErrorCode : uint32_t {
    // Success
    Success = 0,

    // General errors (1-99)
    Unknown = 1,
    InvalidArgument = 2,
    NullPointer = 3,
    OutOfMemory = 4,
    NotImplemented = 5,
    OperationCancelled = 6,
    Timeout = 7,

    // Encryption errors (100-199)
    EncryptionFailed = 100,
    DecryptionFailed = 101,
    InvalidKey = 102,
    InvalidIV = 103,
    InvalidTag = 104,
    KeyDerivationFailed = 105,
    RandomGenerationFailed = 106,
    UnsupportedAlgorithm = 107,
    AuthenticationFailed = 108,
    InvalidCiphertext = 109,
    InvalidPlaintext = 110,

    // Password errors (200-299)
    PasswordTooShort = 200,
    PasswordTooWeak = 201,
    PasswordMissingUppercase = 202,
    PasswordMissingLowercase = 203,
    PasswordMissingDigit = 204,
    PasswordMissingSpecial = 205,
    PasswordInHistory = 206,
    PasswordExpired = 207,
    PasswordBreached = 208,
    InvalidPasswordPolicy = 209,

    // Storage errors (300-399)
    StorageNotInitialized = 300,
    StorageReadFailed = 301,
    StorageWriteFailed = 302,
    StorageDeleteFailed = 303,
    StorageKeyNotFound = 304,
    StorageKeyExists = 305,
    StorageCorrupted = 306,
    StorageAccessDenied = 307,
    StorageFull = 308,
    StorageBackupFailed = 309,
    StorageRestoreFailed = 310,

    // Manager errors (400-499)
    ManagerNotInitialized = 400,
    ManagerLocked = 401,
    ManagerAlreadyUnlocked = 402,
    InvalidMasterPassword = 403,
    SessionExpired = 404,
    EntryNotFound = 405,
    EntryExists = 406,
    ImportFailed = 407,
    ExportFailed = 408,

    // Serialization errors (500-599)
    SerializationFailed = 500,
    DeserializationFailed = 501,
    InvalidFormat = 502,
    UnsupportedVersion = 503,
    DataCorrupted = 504,

    // OTP errors (600-699)
    OtpGenerationFailed = 600,
    OtpVerificationFailed = 601,
    InvalidOtpSecret = 602,
    OtpExpired = 603,
    InvalidOtpAlgorithm = 604,

    // Hash errors (700-799)
    HashFailed = 700,
    HmacFailed = 701,
    InvalidHashAlgorithm = 702,
};

/**
 * @brief Converts an error code to a human-readable string.
 * @param code The error code to convert.
 * @return A string description of the error.
 */
inline std::string errorCodeToString(ErrorCode code) {
    switch (code) {
        case ErrorCode::Success:
            return "Success";
        case ErrorCode::Unknown:
            return "Unknown error";
        case ErrorCode::InvalidArgument:
            return "Invalid argument";
        case ErrorCode::NullPointer:
            return "Null pointer";
        case ErrorCode::OutOfMemory:
            return "Out of memory";
        case ErrorCode::NotImplemented:
            return "Not implemented";
        case ErrorCode::OperationCancelled:
            return "Operation cancelled";
        case ErrorCode::Timeout:
            return "Operation timed out";
        case ErrorCode::EncryptionFailed:
            return "Encryption failed";
        case ErrorCode::DecryptionFailed:
            return "Decryption failed";
        case ErrorCode::InvalidKey:
            return "Invalid encryption key";
        case ErrorCode::InvalidIV:
            return "Invalid initialization vector";
        case ErrorCode::InvalidTag:
            return "Invalid authentication tag";
        case ErrorCode::KeyDerivationFailed:
            return "Key derivation failed";
        case ErrorCode::RandomGenerationFailed:
            return "Random number generation failed";
        case ErrorCode::UnsupportedAlgorithm:
            return "Unsupported algorithm";
        case ErrorCode::AuthenticationFailed:
            return "Authentication failed";
        case ErrorCode::InvalidCiphertext:
            return "Invalid ciphertext";
        case ErrorCode::InvalidPlaintext:
            return "Invalid plaintext";
        case ErrorCode::PasswordTooShort:
            return "Password too short";
        case ErrorCode::PasswordTooWeak:
            return "Password too weak";
        case ErrorCode::PasswordMissingUppercase:
            return "Password missing uppercase letter";
        case ErrorCode::PasswordMissingLowercase:
            return "Password missing lowercase letter";
        case ErrorCode::PasswordMissingDigit:
            return "Password missing digit";
        case ErrorCode::PasswordMissingSpecial:
            return "Password missing special character";
        case ErrorCode::PasswordInHistory:
            return "Password was used recently";
        case ErrorCode::PasswordExpired:
            return "Password has expired";
        case ErrorCode::PasswordBreached:
            return "Password found in data breach";
        case ErrorCode::InvalidPasswordPolicy:
            return "Invalid password policy";
        case ErrorCode::StorageNotInitialized:
            return "Storage not initialized";
        case ErrorCode::StorageReadFailed:
            return "Storage read failed";
        case ErrorCode::StorageWriteFailed:
            return "Storage write failed";
        case ErrorCode::StorageDeleteFailed:
            return "Storage delete failed";
        case ErrorCode::StorageKeyNotFound:
            return "Storage key not found";
        case ErrorCode::StorageKeyExists:
            return "Storage key already exists";
        case ErrorCode::StorageCorrupted:
            return "Storage data corrupted";
        case ErrorCode::StorageAccessDenied:
            return "Storage access denied";
        case ErrorCode::StorageFull:
            return "Storage full";
        case ErrorCode::StorageBackupFailed:
            return "Storage backup failed";
        case ErrorCode::StorageRestoreFailed:
            return "Storage restore failed";
        case ErrorCode::ManagerNotInitialized:
            return "Password manager not initialized";
        case ErrorCode::ManagerLocked:
            return "Password manager is locked";
        case ErrorCode::ManagerAlreadyUnlocked:
            return "Password manager already unlocked";
        case ErrorCode::InvalidMasterPassword:
            return "Invalid master password";
        case ErrorCode::SessionExpired:
            return "Session expired";
        case ErrorCode::EntryNotFound:
            return "Entry not found";
        case ErrorCode::EntryExists:
            return "Entry already exists";
        case ErrorCode::ImportFailed:
            return "Import failed";
        case ErrorCode::ExportFailed:
            return "Export failed";
        case ErrorCode::SerializationFailed:
            return "Serialization failed";
        case ErrorCode::DeserializationFailed:
            return "Deserialization failed";
        case ErrorCode::InvalidFormat:
            return "Invalid format";
        case ErrorCode::UnsupportedVersion:
            return "Unsupported version";
        case ErrorCode::DataCorrupted:
            return "Data corrupted";
        case ErrorCode::OtpGenerationFailed:
            return "OTP generation failed";
        case ErrorCode::OtpVerificationFailed:
            return "OTP verification failed";
        case ErrorCode::InvalidOtpSecret:
            return "Invalid OTP secret";
        case ErrorCode::OtpExpired:
            return "OTP expired";
        case ErrorCode::InvalidOtpAlgorithm:
            return "Invalid OTP algorithm";
        case ErrorCode::HashFailed:
            return "Hash operation failed";
        case ErrorCode::HmacFailed:
            return "HMAC operation failed";
        case ErrorCode::InvalidHashAlgorithm:
            return "Invalid hash algorithm";
        default:
            return "Unknown error code: " +
                   std::to_string(static_cast<uint32_t>(code));
    }
}

/**
 * @brief Checks if an error code represents success.
 * @param code The error code to check.
 * @return True if the code represents success.
 */
inline bool isSuccess(ErrorCode code) noexcept {
    return code == ErrorCode::Success;
}

/**
 * @brief Checks if an error code represents an error.
 * @param code The error code to check.
 * @return True if the code represents an error.
 */
inline bool isError(ErrorCode code) noexcept {
    return code != ErrorCode::Success;
}

}  // namespace atom::secret

#endif  // ATOM_SECRET_CORE_ERROR_CODES_HPP
