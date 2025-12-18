#ifndef ATOM_SECRET_CORE_TYPES_HPP
#define ATOM_SECRET_CORE_TYPES_HPP

#include <array>
#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "atom/algorithm/encoding/base.hpp"

namespace atom::secret {

// ============================================================================
// Basic Type Aliases
// ============================================================================

/// Byte type for binary data
using Byte = uint8_t;

/// Byte vector for variable-length binary data
using ByteVector = std::vector<Byte>;

/// Byte span for non-owning views of binary data
using ByteSpan = std::span<const Byte>;

/// Mutable byte span
using MutableByteSpan = std::span<Byte>;

/// Fixed-size byte array template
template <size_t N>
using ByteArray = std::array<Byte, N>;

// ============================================================================
// Cryptographic Type Aliases
// ============================================================================

/// 128-bit key (16 bytes)
using Key128 = ByteArray<16>;

/// 256-bit key (32 bytes)
using Key256 = ByteArray<32>;

/// 512-bit key (64 bytes)
using Key512 = ByteArray<64>;

/// AES-GCM IV (12 bytes / 96 bits)
using GcmIV = ByteArray<12>;

/// AES-CBC IV (16 bytes / 128 bits)
using CbcIV = ByteArray<16>;

/// AES-GCM authentication tag (16 bytes)
using GcmTag = ByteArray<16>;

/// SHA-256 hash (32 bytes)
using Sha256Hash = ByteArray<32>;

/// SHA-512 hash (64 bytes)
using Sha512Hash = ByteArray<64>;

/// BLAKE2b-256 hash (32 bytes)
using Blake2b256Hash = ByteArray<32>;

/// BLAKE2b-512 hash (64 bytes)
using Blake2b512Hash = ByteArray<64>;

/// Salt for key derivation (typically 16-32 bytes)
using Salt = ByteVector;

// ============================================================================
// Time Type Aliases
// ============================================================================

/// System clock time point
using TimePoint = std::chrono::system_clock::time_point;

/// Duration in seconds
using Seconds = std::chrono::seconds;

/// Duration in milliseconds
using Milliseconds = std::chrono::milliseconds;

/// Duration in minutes
using Minutes = std::chrono::minutes;

// ============================================================================
// Callback Type Aliases
// ============================================================================

/// Progress callback (current, total) -> bool (return false to cancel)
using ProgressCallback = std::function<bool(size_t current, size_t total)>;

/// Completion callback (success, error_message)
using CompletionCallback =
    std::function<void(bool success, const std::string& error)>;

/// Password prompt callback () -> optional<string>
using PasswordPromptCallback = std::function<std::optional<std::string>()>;

// ============================================================================
// Smart Pointer Type Aliases
// ============================================================================

/// Unique pointer alias
template <typename T>
using UniquePtr = std::unique_ptr<T>;

/// Shared pointer alias
template <typename T>
using SharedPtr = std::shared_ptr<T>;

/// Weak pointer alias
template <typename T>
using WeakPtr = std::weak_ptr<T>;

// ============================================================================
// Constants
// ============================================================================

namespace constants {

/// Default PBKDF2 iteration count
constexpr int DEFAULT_PBKDF2_ITERATIONS = 100000;

/// Minimum PBKDF2 iteration count
constexpr int MIN_PBKDF2_ITERATIONS = 10000;

/// Default Argon2 memory cost (64 MB)
constexpr uint32_t DEFAULT_ARGON2_MEMORY_COST = 65536;

/// Default Argon2 time cost (iterations)
constexpr uint32_t DEFAULT_ARGON2_TIME_COST = 3;

/// Default Argon2 parallelism
constexpr uint32_t DEFAULT_ARGON2_PARALLELISM = 4;

/// Default salt length in bytes
constexpr size_t DEFAULT_SALT_LENGTH = 32;

/// Default key length in bytes (256 bits)
constexpr size_t DEFAULT_KEY_LENGTH = 32;

/// AES block size in bytes
constexpr size_t AES_BLOCK_SIZE = 16;

/// GCM IV size in bytes
constexpr size_t GCM_IV_SIZE = 12;

/// GCM tag size in bytes
constexpr size_t GCM_TAG_SIZE = 16;

/// CBC IV size in bytes
constexpr size_t CBC_IV_SIZE = 16;

/// ChaCha20-Poly1305 nonce size in bytes
constexpr size_t CHACHA20_NONCE_SIZE = 12;

/// Poly1305 tag size in bytes
constexpr size_t POLY1305_TAG_SIZE = 16;

/// TOTP default period in seconds
constexpr int TOTP_DEFAULT_PERIOD = 30;

/// TOTP default digits
constexpr int TOTP_DEFAULT_DIGITS = 6;

/// HOTP default digits
constexpr int HOTP_DEFAULT_DIGITS = 6;

/// Default auto-lock timeout in seconds
constexpr int DEFAULT_AUTO_LOCK_TIMEOUT = 300;

/// Default password expiry in days
constexpr int DEFAULT_PASSWORD_EXPIRY_DAYS = 90;

/// Minimum password length
constexpr int MIN_PASSWORD_LENGTH = 8;

/// Default password length for generation
constexpr int DEFAULT_PASSWORD_LENGTH = 16;

/// Maximum password history entries
constexpr int MAX_PASSWORD_HISTORY = 10;

}  // namespace constants

// ============================================================================
// Utility Functions
// ============================================================================

/**
 * @brief Converts a byte vector to a hexadecimal string.
 * @param data The byte data to convert.
 * @param uppercase Whether to use uppercase letters (default: false).
 * @return Hexadecimal string representation.
 *
 * Uses atom::algorithm::encodeHex internally.
 */
inline std::string bytesToHex(ByteSpan data, bool uppercase = false) {
    return atom::algorithm::encodeHex(data, uppercase);
}

/**
 * @brief Converts a hexadecimal string to a byte vector.
 * @param hex The hexadecimal string to convert.
 * @return Byte vector or empty vector if invalid.
 *
 * Uses atom::algorithm::decodeHex internally.
 */
inline ByteVector hexToBytes(std::string_view hex) {
    auto result = atom::algorithm::decodeHex(hex);
    if (result.has_value()) {
        return result.value();
    }
    return {};
}

/**
 * @brief Gets the current time point.
 * @return Current system time.
 */
inline TimePoint now() noexcept { return std::chrono::system_clock::now(); }

/**
 * @brief Converts a time point to Unix timestamp.
 * @param tp The time point to convert.
 * @return Unix timestamp in seconds.
 */
inline int64_t toUnixTimestamp(TimePoint tp) noexcept {
    return std::chrono::duration_cast<Seconds>(tp.time_since_epoch()).count();
}

/**
 * @brief Converts a Unix timestamp to time point.
 * @param timestamp Unix timestamp in seconds.
 * @return Time point.
 */
inline TimePoint fromUnixTimestamp(int64_t timestamp) noexcept {
    return TimePoint(Seconds(timestamp));
}

}  // namespace atom::secret

#endif  // ATOM_SECRET_CORE_TYPES_HPP
