/*
 * uuid.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-4-5

Description: UUID Generator

**************************************************/

#ifndef ATOM_UTILS_UUID_HPP
#define ATOM_UTILS_UUID_HPP

#include <algorithm>
#include <array>
#include <concepts>
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <string_view>

#include <openssl/evp.h>

#include "atom/macro.hpp"
#include "atom/type/expected.hpp"

namespace atom::utils {

// Error types for UUID operations
enum class UuidError {
    InvalidFormat,
    InvalidLength,
    InvalidCharacter,
    ConversionFailed,
    InternalError
};

/**
 * @class UUID
 * @brief Represents a Universally Unique Identifier (UUID).
 *
 * This class provides methods for generating, comparing, and manipulating
 * UUIDs with enhanced C++20 features, robust error handling and performance
 * optimizations.
 */
class UUID {
public:
    /**
     * @brief Constructs a new UUID with a random value.
     * @throws std::runtime_error If the random generator fails
     */
    UUID();

    /**
     * @brief Constructs a UUID from a given 16-byte array.
     * @param data An array of 16 bytes representing the UUID.
     */
    explicit UUID(const std::array<uint8_t, 16>& data);

    /**
     * @brief Constructs a UUID from a span of bytes
     * @param bytes A span of bytes, must be exactly 16 bytes
     * @throws std::invalid_argument If span size is not 16 bytes
     */
    explicit UUID(std::span<const uint8_t> bytes);

    /**
     * @brief Converts the UUID to a string representation.
     * @return A string representation of the UUID.
     */
    ATOM_NODISCARD auto toString() const -> std::string;

    /**
     * @brief Creates a UUID from a string representation.
     * @param str A string representation of a UUID.
     * @return A std::expected containing either a UUID object or an error.
     */
    static auto fromString(std::string_view str)
        -> type::expected<UUID, UuidError>;

    /**
     * @brief Compares this UUID with another for equality.
     * @param other Another UUID to compare with.
     * @return True if both UUIDs are equal, otherwise false.
     */
    auto operator==(const UUID& other) const -> bool;

    /**
     * @brief Compares this UUID with another for inequality.
     * @param other Another UUID to compare with.
     * @return True if both UUIDs are not equal, otherwise false.
     */
    auto operator!=(const UUID& other) const -> bool;

    /**
     * @brief Defines a less-than comparison for UUIDs.
     * @param other Another UUID to compare with.
     * @return True if this UUID is less than the other, otherwise false.
     */
    auto operator<(const UUID& other) const -> bool;

    /**
     * @brief Defines a spaceship operator for UUIDs for three-way comparison
     * @param other Another UUID to compare with
     * @return auto The comparison result
     */
    auto operator<=>(const UUID& other) const = default;

    /**
     * @brief Writes the UUID to an output stream.
     * @param os The output stream to write to.
     * @param uuid The UUID to write.
     * @return The output stream.
     */
    friend auto operator<<(std::ostream& os, const UUID& uuid) -> std::ostream&;

    /**
     * @brief Reads a UUID from an input stream.
     * @param is The input stream to read from.
     * @param uuid The UUID to read into.
     * @return The input stream.
     */
    friend auto operator>>(std::istream& is, UUID& uuid) -> std::istream&;

    /**
     * @brief Retrieves the underlying data of the UUID.
     * @return An array of 16 bytes representing the UUID.
     */
    ATOM_NODISCARD auto getData() const noexcept
        -> const std::array<uint8_t, 16>&;

    /**
     * @brief Gets the version of the UUID.
     * @return The version number of the UUID.
     */
    ATOM_NODISCARD auto version() const noexcept -> uint8_t;

    /**
     * @brief Gets the variant of the UUID.
     * @return The variant number of the UUID.
     */
    ATOM_NODISCARD auto variant() const noexcept -> uint8_t;

    /**
     * @brief Generates a version 3 UUID using the MD5 hashing algorithm.
     * @param namespace_uuid The namespace UUID.
     * @param name The name from which to generate the UUID.
     * @return A version 3 UUID.
     * @throws std::runtime_error If the hash generation fails
     */
    static auto generateV3(const UUID& namespace_uuid,
                           std::string_view name) -> UUID;

    /**
     * @brief Generates a version 5 UUID using the SHA-1 hashing algorithm.
     * @param namespace_uuid The namespace UUID.
     * @param name The name from which to generate the UUID.
     * @return A version 5 UUID.
     * @throws std::runtime_error If the hash generation fails
     */
    static auto generateV5(const UUID& namespace_uuid,
                           std::string_view name) -> UUID;

    /**
     * @brief Generates a version 1, time-based UUID.
     * @return A version 1 UUID.
     * @throws std::runtime_error If the generation fails
     */
    static auto generateV1() -> UUID;

    /**
     * @brief Generates a version 4, random UUID.
     * @return A version 4 UUID.
     * @throws std::runtime_error If the random generator fails
     */
    static auto generateV4() -> UUID;

    /**
     * @brief Checks if a string is a valid UUID format
     * @param str The string to check
     * @return true if valid UUID format, false otherwise
     */
    static auto isValidUUID(std::string_view str) noexcept -> bool;

private:
    /**
     * @brief Generates a random UUID.
     * @throws std::runtime_error If the random generator fails
     */
    void generateRandom();

    /**
     * @brief Template method that generates a name-based UUID using a hashing
     * algorithm.
     * @tparam DIGEST Function to get the digest algorithm
     * @param namespace_uuid The namespace UUID.
     * @param name The name from which to generate the UUID.
     * @param version The version of the UUID to be generated.
     * @return A UUID generated from the name.
     * @throws std::runtime_error If the hash generation fails
     */
    template <const EVP_MD* (*DIGEST)()>
    static auto generateNameBased(const UUID& namespace_uuid,
                                  std::string_view name, int version) -> UUID;

    /**
     * @brief Generates a unique node identifier for version 1 UUIDs.
     * @return A 64-bit node identifier.
     * @throws std::runtime_error If node generation fails
     */
    static auto generateNode() -> uint64_t;

    std::array<uint8_t, 16> data_;  ///< The internal storage of the UUID.
};

template <const EVP_MD* (*DIGEST)()>
auto UUID::generateNameBased(const UUID& namespace_uuid, std::string_view name,
                             int version) -> UUID {
    // Use unique_ptr with custom deleter for automatic cleanup
    struct EVPCtxDeleter {
        void operator()(EVP_MD_CTX* ctx) const {
            if (ctx)
                EVP_MD_CTX_free(ctx);
        }
    };

    std::unique_ptr<EVP_MD_CTX, EVPCtxDeleter> ctx(EVP_MD_CTX_new());
    if (!ctx) {
        throw std::runtime_error("Failed to create EVP_MD_CTX");
    }

    if (EVP_DigestInit_ex(ctx.get(), DIGEST(), nullptr) != 1) {
        throw std::runtime_error("Failed to initialize digest");
    }

    const auto& ns_data = namespace_uuid.getData();
    if (EVP_DigestUpdate(ctx.get(), ns_data.data(), ns_data.size()) != 1) {
        throw std::runtime_error("Failed to update digest with namespace");
    }

    if (EVP_DigestUpdate(ctx.get(), name.data(), name.size()) != 1) {
        throw std::runtime_error("Failed to update digest with name");
    }

    std::array<uint8_t, EVP_MAX_MD_SIZE> hash;
    unsigned int hash_len;
    if (EVP_DigestFinal_ex(ctx.get(), hash.data(), &hash_len) != 1) {
        throw std::runtime_error("Failed to finalize digest");
    }

    std::array<uint8_t, 16> uuid_data;
    std::copy_n(hash.begin(), std::min(hash_len, static_cast<unsigned int>(16)),
                uuid_data.begin());

    uuid_data[6] = (uuid_data[6] & 0x0F) | (version << 4);  // Set version
    uuid_data[8] = (uuid_data[8] & 0x3F) | 0x80;            // Set variant

    return UUID(uuid_data);
}

// Concept for random number generators used in UUID generation
template <typename T>
concept UuidRandomEngine = requires(T& engine) {
    { engine() } -> std::convertible_to<uint64_t>;
    { T(std::declval<uint64_t>()) } -> std::same_as<T>;
};

/**
 * @brief Generates a unique UUID and returns it as a string.
 * @return A unique UUID as a string.
 * @throws std::runtime_error If UUID generation fails
 */
ATOM_NODISCARD auto generateUniqueUUID() -> std::string;

/**
 * @brief Gets the MAC address of the system
 * @return MAC address string or empty if not available
 */
ATOM_NODISCARD auto getMAC() -> std::string;

/**
 * @brief Gets CPU serial information
 * @return CPU serial string or empty if not available
 */
ATOM_NODISCARD auto getCPUSerial() -> std::string;

/**
 * @brief Formats a UUID string with dashes
 * @param uuid Raw UUID string
 * @return Formatted UUID with dashes
 */
ATOM_NODISCARD auto formatUUID(std::string_view uuid) -> std::string;

#if ATOM_USE_SIMD
typedef long long __m128i __attribute__((__vector_size__(16), __aligned__(16)));

/**
 * @class FastUUID
 * @brief High-performance UUID implementation using SIMD instructions
 */
class FastUUID {
public:
    /**
     * @brief Default constructor
     */
    FastUUID();

    /**
     * @brief Copy constructor
     * @param other UUID to copy
     */
    FastUUID(const FastUUID& other);

    /**
     * @brief Construct from SIMD vector
     * @param x The vector containing UUID data
     */
    FastUUID(__m128i x);

    /**
     * @brief Construct from two 64-bit integers
     * @param x First half of UUID
     * @param y Second half of UUID
     */
    FastUUID(uint64_t x, uint64_t y);

    /**
     * @brief Construct from byte array
     * @param bytes Pointer to 16 bytes of UUID data
     * @throws std::invalid_argument If bytes is null
     */
    FastUUID(const uint8_t* bytes);

    /**
     * @brief Construct from string
     * @param bytes String containing 16 bytes of UUID data
     * @throws std::invalid_argument If string doesn't have enough data
     */
    explicit FastUUID(std::string_view bytes);

    /**
     * @brief Create UUID from string representation
     * @param s UUID string (with or without dashes)
     * @return UUID object
     * @throws std::invalid_argument If string format is invalid
     */
    static FastUUID fromStrFactory(std::string_view s);

    /**
     * @brief Create UUID from string representation
     * @param raw C-string of UUID (with or without dashes)
     * @return UUID object
     * @throws std::invalid_argument If string format is invalid
     */
    static FastUUID fromStrFactory(const char* raw);

    /**
     * @brief Parse string into this UUID
     * @param raw C-string to parse
     * @throws std::invalid_argument If string format is invalid
     */
    void fromStr(const char* raw);

    /**
     * @brief Assignment operator
     * @param other UUID to copy
     * @return Reference to this object
     */
    FastUUID& operator=(const FastUUID& other);

    /**
     * @brief Equality comparison
     * @param lhs Left-hand UUID
     * @param rhs Right-hand UUID
     * @return true if UUIDs are equal
     */
    friend bool operator==(const FastUUID& lhs, const FastUUID& rhs);

    /**
     * @brief Less-than comparison
     * @param lhs Left-hand UUID
     * @param rhs Right-hand UUID
     * @return true if lhs is less than rhs
     */
    friend bool operator<(const FastUUID& lhs, const FastUUID& rhs);

    /**
     * @brief Not-equal comparison
     * @param lhs Left-hand UUID
     * @param rhs Right-hand UUID
     * @return true if UUIDs are not equal
     */
    friend bool operator!=(const FastUUID& lhs, const FastUUID& rhs);

    /**
     * @brief Greater-than comparison
     * @param lhs Left-hand UUID
     * @param rhs Right-hand UUID
     * @return true if lhs is greater than rhs
     */
    friend bool operator>(const FastUUID& lhs, const FastUUID& rhs);

    /**
     * @brief Less-than-or-equal comparison
     * @param lhs Left-hand UUID
     * @param rhs Right-hand UUID
     * @return true if lhs is less than or equal to rhs
     */
    friend bool operator<=(const FastUUID& lhs, const FastUUID& rhs);

    /**
     * @brief Greater-than-or-equal comparison
     * @param lhs Left-hand UUID
     * @param rhs Right-hand UUID
     * @return true if lhs is greater than or equal to rhs
     */
    friend bool operator>=(const FastUUID& lhs, const FastUUID& rhs);

    /**
     * @brief Get raw bytes of UUID
     * @return String containing 16 bytes of UUID data
     */
    std::string bytes() const;

    /**
     * @brief Fill string with raw bytes of UUID
     * @param out Output string to fill
     */
    void bytes(std::string& out) const;

    /**
     * @brief Copy raw bytes to buffer
     * @param bytes Output buffer (must be at least 16 bytes)
     * @throws std::invalid_argument If bytes is null
     */
    void bytes(char* bytes) const;

    /**
     * @brief Get string representation of UUID
     * @return UUID string with dashes
     */
    std::string str() const;

    /**
     * @brief Fill string with UUID representation
     * @param s Output string to fill (will be resized to 36)
     */
    void str(std::string& s) const;

    /**
     * @brief Copy string representation to buffer
     * @param res Output buffer (must be at least 36 bytes)
     * @throws std::invalid_argument If res is null
     */
    void str(char* res) const;

    /**
     * @brief Stream output operator
     * @param stream Output stream
     * @param uuid UUID to output
     * @return Reference to stream
     */
    friend std::ostream& operator<<(std::ostream& stream, const FastUUID& uuid);

    /**
     * @brief Stream input operator
     * @param stream Input stream
     * @param uuid UUID to read into
     * @return Reference to stream
     */
    friend std::istream& operator>>(std::istream& stream, FastUUID& uuid);

    /**
     * @brief Get hash of UUID
     * @return Hash value suitable for unordered containers
     */
    size_t hash() const;

    /**
     * @brief Raw UUID data (16 bytes, aligned for SIMD)
     */
    alignas(16) uint8_t data[16];
};

#undef __m128i

/**
 * @class FastUUIDGenerator
 * @brief High-performance UUID generator using SIMD
 * @tparam RNG Random number generator type
 */
template <UuidRandomEngine RNG>
class FastUUIDGenerator {
public:
    /**
     * @brief Default constructor using random seed
     */
    FastUUIDGenerator();

    /**
     * @brief Constructor with specified seed
     * @param seed Random seed value
     */
    FastUUIDGenerator(uint64_t seed);

    /**
     * @brief Constructor with existing random engine
     * @param gen Reference to existing generator
     */
    FastUUIDGenerator(RNG& gen);

    /**
     * @brief Generate a new UUID
     * @return Random UUID
     */
    FastUUID getUUID();

private:
    std::shared_ptr<RNG> generator;
    std::uniform_int_distribution<uint64_t> distribution;
};
#endif
}  // namespace atom::utils

namespace std {
template <>
struct hash<atom::utils::UUID> {
    auto operator()(const atom::utils::UUID& uuid) const -> size_t {
        return std::hash<std::string>{}(uuid.toString());
    }
};

#if ATOM_USE_SIMD
template <>
struct hash<atom::utils::FastUUID> {
    auto operator()(const atom::utils::FastUUID& uuid) const -> size_t {
        return uuid.hash();
    }
};
#endif
}  // namespace std

#endif
