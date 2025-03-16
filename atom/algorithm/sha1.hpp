#ifndef ATOM_ALGORITHM_SHA1_HPP
#define ATOM_ALGORITHM_SHA1_HPP

#include <array>
#include <concepts>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

#ifdef __AVX2__
#include <immintrin.h>  // AVX2 instruction set
#endif

namespace atom::algorithm {

/**
 * @brief Concept that checks if a type is a byte container.
 *
 * A type satisfies this concept if it provides access to its data as a
 * contiguous array of `uint8_t` and provides a size.
 *
 * @tparam T The type to check.
 */
template <typename T>
concept ByteContainer = requires(T t) {
    { std::data(t) } -> std::convertible_to<const uint8_t*>;
    { std::size(t) } -> std::convertible_to<size_t>;
};

/**
 * @class SHA1
 * @brief Computes the SHA-1 hash of a sequence of bytes.
 *
 * This class implements the SHA-1 hashing algorithm according to
 * FIPS PUB 180-4. It supports incremental updates and produces a 20-byte
 * digest.
 */
class SHA1 {
public:
    /**
     * @brief Constructs a new SHA1 object with the initial hash values.
     *
     * Initializes the internal state with the standard initial hash values as
     * defined in the SHA-1 algorithm.
     */
    SHA1() noexcept;

    /**
     * @brief Updates the hash with a span of bytes.
     *
     * Processes the input data to update the internal hash state. This function
     * can be called multiple times to hash data in chunks.
     *
     * @param data A span of constant bytes to hash.
     */
    void update(std::span<const uint8_t> data) noexcept;

    /**
     * @brief Updates the hash with a raw byte array.
     *
     * Processes the input data to update the internal hash state. This function
     * can be called multiple times to hash data in chunks.
     *
     * @param data A pointer to the start of the byte array.
     * @param length The number of bytes to hash.
     */
    void update(const uint8_t* data, size_t length);

    /**
     * @brief Updates the hash with a byte container.
     *
     * Processes the input data from a container satisfying the ByteContainer
     * concept to update the internal hash state.
     *
     * @tparam Container A type satisfying the ByteContainer concept.
     * @param container The container of bytes to hash.
     */
    template <ByteContainer Container>
    void update(const Container& container) noexcept {
        update(std::span<const uint8_t>(
            reinterpret_cast<const uint8_t*>(std::data(container)),
            std::size(container)));
    }

    /**
     * @brief Finalizes the hash computation and returns the digest as a byte
     * array.
     *
     * Completes the SHA-1 computation, applies padding, and returns the
     * resulting 20-byte digest.
     *
     * @return A 20-byte array containing the SHA-1 digest.
     */
    [[nodiscard]] auto digest() noexcept -> std::array<uint8_t, 20>;

    /**
     * @brief Finalizes the hash computation and returns the digest as a
     * hexadecimal string.
     *
     * Completes the SHA-1 computation and converts the resulting 20-byte digest
     * into a hexadecimal string representation.
     *
     * @return A string containing the hexadecimal representation of the SHA-1
     * digest.
     */
    [[nodiscard]] auto digestAsString() noexcept -> std::string;

    /**
     * @brief Resets the SHA1 object to its initial state.
     *
     * Clears the internal buffer and resets the hash state to allow for hashing
     * new data.
     */
    void reset() noexcept;

    /**
     * @brief The size of the SHA-1 digest in bytes.
     */
    static constexpr size_t DIGEST_SIZE = 20;

private:
    /**
     * @brief Processes a single 64-byte block of data.
     *
     * Applies the core SHA-1 transformation to a single block of data.
     *
     * @param block A pointer to the 64-byte block to process.
     */
    void processBlock(const uint8_t* block) noexcept;

    /**
     * @brief Rotates a 32-bit value to the left by a specified number of bits.
     *
     * Performs a left bitwise rotation, which is a key operation in the SHA-1
     * algorithm.
     *
     * @param value The 32-bit value to rotate.
     * @param bits The number of bits to rotate by.
     * @return The rotated value.
     */
    [[nodiscard]] static constexpr auto rotateLeft(
        uint32_t value, size_t bits) noexcept -> uint32_t {
        return (value << bits) | (value >> (WORD_SIZE - bits));
    }

#ifdef __AVX2__
    /**
     * @brief Processes a single 64-byte block of data using AVX2 SIMD
     * instructions.
     *
     * This function is an optimized version of processBlock that utilizes AVX2
     * SIMD instructions for faster computation.
     *
     * @param block A pointer to the 64-byte block to process.
     */
    void processBlockSIMD(const uint8_t* block) noexcept;
#endif

    /**
     * @brief The size of a data block in bytes.
     */
    static constexpr size_t BLOCK_SIZE = 64;

    /**
     * @brief The number of 32-bit words in the hash state.
     */
    static constexpr size_t HASH_SIZE = 5;

    /**
     * @brief The number of 32-bit words in the message schedule.
     */
    static constexpr size_t SCHEDULE_SIZE = 80;

    /**
     * @brief The size of the message length in bytes.
     */
    static constexpr size_t LENGTH_SIZE = 8;

    /**
     * @brief The number of bits per byte.
     */
    static constexpr size_t BITS_PER_BYTE = 8;

    /**
     * @brief The padding byte used to pad the message.
     */
    static constexpr uint8_t PADDING_BYTE = 0x80;

    /**
     * @brief The byte mask used for byte operations.
     */
    static constexpr uint8_t BYTE_MASK = 0xFF;

    /**
     * @brief The size of a word in bits.
     */
    static constexpr size_t WORD_SIZE = 32;

    /**
     * @brief The current hash state.
     */
    std::array<uint32_t, HASH_SIZE> hash_;

    /**
     * @brief The buffer to store the current block of data.
     */
    std::array<uint8_t, BLOCK_SIZE> buffer_;

    /**
     * @brief The total number of bits processed so far.
     */
    uint64_t bitCount_;

    /**
     * @brief Flag indicating whether to use SIMD instructions for processing.
     */
    bool useSIMD_ = false;
};

/**
 * @brief Converts an array of bytes to a hexadecimal string.
 *
 * This function takes an array of bytes and converts each byte into its
 * hexadecimal representation, concatenating them into a single string.
 *
 * @tparam N The size of the byte array.
 * @param bytes The array of bytes to convert.
 * @return A string containing the hexadecimal representation of the byte array.
 */
template <size_t N>
[[nodiscard]] auto bytesToHex(const std::array<uint8_t, N>& bytes) noexcept
    -> std::string;

/**
 * @brief Specialization of bytesToHex for SHA1 digest size.
 *
 * This specialization provides an optimized version for converting SHA1 digests
 * (20 bytes) to a hexadecimal string.
 *
 * @param bytes The array of bytes to convert.
 * @return A string containing the hexadecimal representation of the byte array.
 */
template <>
[[nodiscard]] auto bytesToHex<SHA1::DIGEST_SIZE>(
    const std::array<uint8_t, SHA1::DIGEST_SIZE>& bytes) noexcept
    -> std::string;

/**
 * @brief Computes SHA-1 hashes of multiple containers in parallel.
 *
 * This function computes the SHA-1 hash of each container provided as an
 * argument, utilizing parallel execution to improve performance.
 *
 * @tparam Containers A variadic list of types satisfying the ByteContainer
 * concept.
 * @param containers A pack of containers to compute the SHA-1 hashes for.
 * @return A vector of SHA-1 digests, each corresponding to the input
 * containers.
 */
template <ByteContainer... Containers>
[[nodiscard]] auto computeHashesInParallel(const Containers&... containers)
    -> std::vector<std::array<uint8_t, SHA1::DIGEST_SIZE>>;

}  // namespace atom::algorithm

#endif  // ATOM_ALGORITHM_SHA1_HPP