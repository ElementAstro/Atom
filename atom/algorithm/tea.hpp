#ifndef ATOM_ALGORITHM_TEA_HPP
#define ATOM_ALGORITHM_TEA_HPP

#include <array>
#include <concepts>
#include <span>
#include <stdexcept>
#include <vector>

#include <spdlog/spdlog.h>
#include "atom/algorithm/rust_numeric.hpp"

namespace atom::algorithm {

/**
 * @brief Custom exception class for TEA-related errors.
 *
 * This class inherits from std::runtime_error and is used to throw exceptions
 * specific to the TEA, XTEA, and XXTEA algorithms.
 */
class TEAException : public std::runtime_error {
public:
    /**
     * @brief Constructs a TEAException with a specified error message.
     *
     * @param message The error message associated with the exception.
     */
    using std::runtime_error::runtime_error;
};

/**
 * @brief Concept that checks if a type is a container of 32-bit unsigned
 * integers.
 *
 * A type satisfies this concept if it is a contiguous range where each element
 * is a 32-bit unsigned integer.
 *
 * @tparam T The type to check.
 */
template <typename T>
concept UInt32Container = std::ranges::contiguous_range<T> && requires(T t) {
    { std::data(t) } -> std::convertible_to<const u32 *>;
    { std::size(t) } -> std::convertible_to<usize>;
    requires sizeof(std::ranges::range_value_t<T>) == sizeof(u32);
};

/**
 * @brief Type alias for a 128-bit key used in the XTEA algorithm.
 *
 * Represents the key as an array of four 32-bit unsigned integers.
 */
using XTEAKey = std::array<u32, 4>;

/**
 * @brief Encrypts two 32-bit values using the TEA (Tiny Encryption Algorithm).
 *
 * The TEA algorithm is a symmetric-key block cipher known for its simplicity.
 * This function encrypts two 32-bit unsigned integers using a 128-bit key.
 *
 * @param value0 The first 32-bit value to be encrypted (modified in place).
 * @param value1 The second 32-bit value to be encrypted (modified in place).
 * @param key A reference to an array of four 32-bit unsigned integers
 * representing the 128-bit key.
 * @throws TEAException if the key is invalid.
 */
auto teaEncrypt(u32 &value0, u32 &value1,
                const std::array<u32, 4> &key) noexcept(false) -> void;

/**
 * @brief Decrypts two 32-bit values using the TEA (Tiny Encryption Algorithm).
 *
 * This function decrypts two 32-bit unsigned integers using a 128-bit key.
 *
 * @param value0 The first 32-bit value to be decrypted (modified in place).
 * @param value1 The second 32-bit value to be decrypted (modified in place).
 * @param key A reference to an array of four 32-bit unsigned integers
 * representing the 128-bit key.
 * @throws TEAException if the key is invalid.
 */
auto teaDecrypt(u32 &value0, u32 &value1,
                const std::array<u32, 4> &key) noexcept(false) -> void;

/**
 * @brief Encrypts a container of 32-bit values using the XXTEA algorithm.
 *
 * The XXTEA algorithm is an extension of TEA, designed to correct some of TEA's
 * weaknesses.
 *
 * @tparam Container A type that satisfies the UInt32Container concept.
 * @param inputData The container of 32-bit values to be encrypted.
 * @param inputKey A span of four 32-bit unsigned integers representing the
 * 128-bit key.
 * @return A vector of encrypted 32-bit values.
 * @throws TEAException if the input data is too small or the key is invalid.
 */
template <UInt32Container Container>
auto xxteaEncrypt(const Container &inputData, std::span<const u32, 4> inputKey)
    -> std::vector<u32>;

/**
 * @brief Decrypts a container of 32-bit values using the XXTEA algorithm.
 *
 * @tparam Container A type that satisfies the UInt32Container concept.
 * @param inputData The container of 32-bit values to be decrypted.
 * @param inputKey A span of four 32-bit unsigned integers representing the
 * 128-bit key.
 * @return A vector of decrypted 32-bit values.
 * @throws TEAException if the input data is too small or the key is invalid.
 */
template <UInt32Container Container>
auto xxteaDecrypt(const Container &inputData, std::span<const u32, 4> inputKey)
    -> std::vector<u32>;

/**
 * @brief Encrypts two 32-bit values using the XTEA (Extended TEA) algorithm.
 *
 * XTEA is a block cipher that corrects some weaknesses of TEA.
 *
 * @param value0 The first 32-bit value to be encrypted (modified in place).
 * @param value1 The second 32-bit value to be encrypted (modified in place).
 * @param key A reference to an XTEAKey representing the 128-bit key.
 * @throws TEAException if the key is invalid.
 */
auto xteaEncrypt(u32 &value0, u32 &value1, const XTEAKey &key) noexcept(false)
    -> void;

/**
 * @brief Decrypts two 32-bit values using the XTEA (Extended TEA) algorithm.
 *
 * @param value0 The first 32-bit value to be decrypted (modified in place).
 * @param value1 The second 32-bit value to be decrypted (modified in place).
 * @param key A reference to an XTEAKey representing the 128-bit key.
 * @throws TEAException if the key is invalid.
 */
auto xteaDecrypt(u32 &value0, u32 &value1, const XTEAKey &key) noexcept(false)
    -> void;

/**
 * @brief Converts a byte array to a vector of 32-bit unsigned integers.
 *
 * This function is used to prepare byte data for encryption or decryption with
 * the XXTEA algorithm.
 *
 * @tparam T A type that satisfies the requirements of a contiguous range of
 * uint8_t.
 * @param data The byte array to be converted.
 * @return A vector of 32-bit unsigned integers.
 */
template <typename T>
    requires std::ranges::contiguous_range<T> &&
             std::same_as<std::ranges::range_value_t<T>, u8>
auto toUint32Vector(const T &data) -> std::vector<u32>;

/**
 * @brief Converts a vector of 32-bit unsigned integers back to a byte array.
 *
 * This function is used to convert the result of XXTEA decryption back into a
 * byte array.
 *
 * @tparam Container A type that satisfies the UInt32Container concept.
 * @param data The vector of 32-bit unsigned integers to be converted.
 * @return A byte array.
 */
template <UInt32Container Container>
auto toByteArray(const Container &data) -> std::vector<u8>;

/**
 * @brief Parallel version of XXTEA encryption for large data sets.
 *
 * This function uses multiple threads to encrypt the input data, which can
 * significantly improve performance for large data sets.
 *
 * @tparam Container A type that satisfies the UInt32Container concept.
 * @param inputData The container of 32-bit values to be encrypted.
 * @param inputKey The 128-bit key used for encryption.
 * @param numThreads The number of threads to use. If 0, the function uses the
 * number of hardware threads available.
 * @return A vector of encrypted 32-bit values.
 */
template <UInt32Container Container>
auto xxteaEncryptParallel(const Container &inputData,
                          std::span<const u32, 4> inputKey,
                          usize numThreads = 0) -> std::vector<u32>;

/**
 * @brief Parallel version of XXTEA decryption for large data sets.
 *
 * This function uses multiple threads to decrypt the input data, which can
 * significantly improve performance for large data sets.
 *
 * @tparam Container A type that satisfies the UInt32Container concept.
 * @param inputData The container of 32-bit values to be decrypted.
 * @param inputKey The 128-bit key used for decryption.
 * @param numThreads The number of threads to use. If 0, the function uses the
 * number of hardware threads available.
 * @return A vector of decrypted 32-bit values.
 */
template <UInt32Container Container>
auto xxteaDecryptParallel(const Container &inputData,
                          std::span<const u32, 4> inputKey,
                          usize numThreads = 0) -> std::vector<u32>;

/**
 * @brief Implementation detail for XXTEA encryption.
 *
 * This function performs the actual XXTEA encryption.
 *
 * @param inputData A span of 32-bit values to encrypt.
 * @param inputKey A span of four 32-bit unsigned integers representing the
 * 128-bit key.
 * @return A vector of encrypted 32-bit values.
 */
auto xxteaEncryptImpl(std::span<const u32> inputData,
                      std::span<const u32, 4> inputKey) -> std::vector<u32>;

/**
 * @brief Implementation detail for XXTEA decryption.
 *
 * This function performs the actual XXTEA decryption.
 *
 * @param inputData A span of 32-bit values to decrypt.
 * @param inputKey A span of four 32-bit unsigned integers representing the
 * 128-bit key.
 * @return A vector of decrypted 32-bit values.
 */
auto xxteaDecryptImpl(std::span<const u32> inputData,
                      std::span<const u32, 4> inputKey) -> std::vector<u32>;

/**
 * @brief Implementation detail for parallel XXTEA encryption.
 *
 * This function performs the actual parallel XXTEA encryption.
 *
 * @param inputData A span of 32-bit values to encrypt.
 * @param inputKey A span of four 32-bit unsigned integers representing the
 * 128-bit key.
 * @param numThreads The number of threads to use for encryption.
 * @return A vector of encrypted 32-bit values.
 */
auto xxteaEncryptParallelImpl(std::span<const u32> inputData,
                              std::span<const u32, 4> inputKey,
                              usize numThreads) -> std::vector<u32>;

/**
 * @brief Implementation detail for parallel XXTEA decryption.
 *
 * This function performs the actual parallel XXTEA decryption.
 *
 * @param inputData A span of 32-bit values to decrypt.
 * @param inputKey A span of four 32-bit unsigned integers representing the
 * 128-bit key.
 * @param numThreads The number of threads to use for decryption.
 * @return A vector of decrypted 32-bit values.
 */
auto xxteaDecryptParallelImpl(std::span<const u32> inputData,
                              std::span<const u32, 4> inputKey,
                              usize numThreads) -> std::vector<u32>;

/**
 * @brief Implementation detail for converting a byte array to a vector of
 * u32.
 *
 * This function performs the actual conversion from a byte array to a vector of
 * 32-bit unsigned integers.
 *
 * @param data A span of bytes to convert.
 * @return A vector of 32-bit unsigned integers.
 */
auto toUint32VectorImpl(std::span<const u8> data) -> std::vector<u32>;

/**
 * @brief Implementation detail for converting a vector of u32 to a byte
 * array.
 *
 * This function performs the actual conversion from a vector of 32-bit unsigned
 * integers to a byte array.
 *
 * @param data A span of 32-bit unsigned integers to convert.
 * @return A vector of bytes.
 */
auto toByteArrayImpl(std::span<const u32> data) -> std::vector<u8>;

/**
 * @brief Encrypts a container of 32-bit values using the XXTEA algorithm.
 *
 * The XXTEA algorithm is an extension of TEA, designed to correct some of TEA's
 * weaknesses.
 *
 * @tparam Container A type that satisfies the UInt32Container concept.
 * @param inputData The container of 32-bit values to be encrypted.
 * @param inputKey A span of four 32-bit unsigned integers representing the
 * 128-bit key.
 * @return A vector of encrypted 32-bit values.
 * @throws TEAException if the input data is too small or the key is invalid.
 */
template <UInt32Container Container>
auto xxteaEncrypt(const Container &inputData, std::span<const u32, 4> inputKey)
    -> std::vector<u32> {
    return xxteaEncryptImpl(
        std::span<const u32>{inputData.data(), inputData.size()}, inputKey);
}

/**
 * @brief Decrypts a container of 32-bit values using the XXTEA algorithm.
 *
 * @tparam Container A type that satisfies the UInt32Container concept.
 * @param inputData The container of 32-bit values to be decrypted.
 * @param inputKey A span of four 32-bit unsigned integers representing the
 * 128-bit key.
 * @return A vector of decrypted 32-bit values.
 * @throws TEAException if the input data is too small or the key is invalid.
 */
template <UInt32Container Container>
auto xxteaDecrypt(const Container &inputData, std::span<const u32, 4> inputKey)
    -> std::vector<u32> {
    return xxteaDecryptImpl(
        std::span<const u32>{inputData.data(), inputData.size()}, inputKey);
}

/**
 * @brief Parallel version of XXTEA encryption for large data sets.
 *
 * This function uses multiple threads to encrypt the input data, which can
 * significantly improve performance for large data sets.
 *
 * @tparam Container A type that satisfies the UInt32Container concept.
 * @param inputData The container of 32-bit values to be encrypted.
 * @param inputKey The 128-bit key used for encryption.
 * @param numThreads The number of threads to use. If 0, the function uses the
 * number of hardware threads available.
 * @return A vector of encrypted 32-bit values.
 */
template <UInt32Container Container>
auto xxteaEncryptParallel(const Container &inputData,
                          std::span<const u32, 4> inputKey, usize numThreads)
    -> std::vector<u32> {
    return xxteaEncryptParallelImpl(
        std::span<const u32>{inputData.data(), inputData.size()}, inputKey,
        numThreads);
}

/**
 * @brief Parallel version of XXTEA decryption for large data sets.
 *
 * This function uses multiple threads to decrypt the input data, which can
 * significantly improve performance for large data sets.
 *
 * @tparam Container A type that satisfies the UInt32Container concept.
 * @param inputData The container of 32-bit values to be decrypted.
 * @param inputKey The 128-bit key used for decryption.
 * @param numThreads The number of threads to use. If 0, the function uses the
 * number of hardware threads available.
 * @return A vector of decrypted 32-bit values.
 */
template <UInt32Container Container>
auto xxteaDecryptParallel(const Container &inputData,
                          std::span<const u32, 4> inputKey, usize numThreads)
    -> std::vector<u32> {
    return xxteaDecryptParallelImpl(
        std::span<const u32>{inputData.data(), inputData.size()}, inputKey,
        numThreads);
}

/**
 * @brief Converts a byte array to a vector of 32-bit unsigned integers.
 *
 * This function is used to prepare byte data for encryption or decryption with
 * the XXTEA algorithm.
 *
 * @tparam T A type that satisfies the requirements of a contiguous range of
 * u8.
 * @param data The byte array to be converted.
 * @return A vector of 32-bit unsigned integers.
 */
template <typename T>
    requires std::ranges::contiguous_range<T> &&
             std::same_as<std::ranges::range_value_t<T>, u8>
auto toUint32Vector(const T &data) -> std::vector<u32> {
    return toUint32VectorImpl(std::span<const u8>{data.data(), data.size()});
}

/**
 * @brief Converts a vector of 32-bit unsigned integers back to a byte array.
 *
 * This function is used to convert the result of XXTEA decryption back into a
 * byte array.
 *
 * @tparam Container A type that satisfies the UInt32Container concept.
 * @param data The vector of 32-bit unsigned integers to be converted.
 * @return A byte array.
 */
template <UInt32Container Container>
auto toByteArray(const Container &data) -> std::vector<u8> {
    return toByteArrayImpl(std::span<const u32>{data.data(), data.size()});
}

}  // namespace atom::algorithm

#endif